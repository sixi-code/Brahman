/*
 * Copyright (c) 2024, sakumisu
 * Copyright (c) 2024, Egahp
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "stm32f4xx.h"
#include "bootuf2.h"
#include "boot_marker.h"
#include "usbd_core.h"

/* ===== 升级掉电标记 =====
 * 存放在 RTC 备份寄存器: 软复位与短时掉电都不清零。
 * 实现放在本文件是因为它已参与编译; 声明在 boot_marker.h 供 main 使用。 */
#define BOOT_MARK_BKP   RTC->BKP10R

void Boot_Marker_Init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    (void)RCC->APB1ENR;          /* 读回一次, 确保 PWR 时钟已生效 */
    PWR->CR |= PWR_CR_DBP;       /* 解除备份域写保护 */
}

uint32_t Boot_Marker_Get(void)
{
    return BOOT_MARK_BKP;
}

void Boot_Marker_Set(uint32_t state)
{
    PWR->CR |= PWR_CR_DBP;
    BOOT_MARK_BKP = state;
}

char file_INFO[] = {
    "Brahman UF2 BOOT Powered By CherryUSB\r\n"
};

const char file_ID__[12] = BOOTUF2_FAMILYID_ARRAY;

static struct bootuf2_FILE files[] = {
    [0] = { .Name = file_ID__, .Content = NULL, .FileSize = 0 },
    [1] = { .Name = "INFO_UF2TXT", .Content = file_INFO, .FileSize = sizeof(file_INFO) - 1 },
};

struct bootuf2_data {
    const struct bootuf2_DBR *const DBR;
    struct bootuf2_STATE *const STATE;
    uint8_t *const fbuff;
    uint8_t *const erase;
    size_t page_count;
    uint8_t *const cache;
    const size_t cache_size;
    uint32_t cached_address;
    size_t cached_bytes;
};

/*!< define DBRs */
static const struct bootuf2_DBR bootuf2_DBR = {
    .JMPInstruction = { 0xEB, 0x3C, 0x90 },
    .OEM = "UF2 UF2 ",
    .BPB = {
        .BytesPerSector = CONFIG_BOOTUF2_SECTOR_SIZE,
        .SectorsPerCluster = CONFIG_BOOTUF2_SECTOR_PER_CLUSTER,
        .ReservedSectors = CONFIG_BOOTUF2_SECTOR_RESERVED,
        .NumberOfFAT = CONFIG_BOOTUF2_NUM_OF_FAT,
        .RootEntries = CONFIG_BOOTUF2_ROOT_ENTRIES,
        .Sectors = (BOOTUF2_SECTORS(0) > 0xFFFF) ? 0 : BOOTUF2_SECTORS(0),
        .MediaDescriptor = 0xF8,
        .SectorsPerFAT = BOOTUF2_SECTORS_PER_FAT(0),
        .SectorsPerTrack = 1,
        .Heads = 1,
        .HiddenSectors = 0,
        .SectorsOver32MB = (BOOTUF2_SECTORS(0) > 0xFFFF) ? BOOTUF2_SECTORS(0) : 0,
        .BIOSDrive = 0x80,
        .Reserved = 0,
        .ExtendBootSignature = 0x29,
        .VolumeSerialNumber = 0x00420042,
        .VolumeLabel = "CHERRYUF2",
        .FileSystem = "FAT16   ",
    },
};

/*!< define mask */
static uint8_t __attribute__((aligned(4))) bootuf2_mask[BOOTUF2_BLOCKSMAX / 8 + 1] = { 0 };

/*!< define state */
static struct bootuf2_STATE bootuf2_STATE = {
    .NumberOfBlock = 0,
    .NumberOfWritten = 0,
    .Mask = bootuf2_mask,
    .Enable = 1,
};

/*!< define flash cache */
static uint8_t __attribute__((aligned(4))) bootuf2_disk_cache[CONFIG_BOOTUF2_CACHE_SIZE];

/*!< define flash buff */
static uint8_t __attribute__((aligned(4))) bootuf2_disk_fbuff[256];

/*!< define erase flag buff */
static uint8_t __attribute__((aligned(4))) bootuf2_disk_erase[BOOTUF2_DIVCEIL(CONFIG_BOOTUF2_PAGE_COUNTMAX, 8)];

/*!< define disk */
static struct bootuf2_data bootuf2_disk = {
    .DBR = &bootuf2_DBR,
    .STATE = &bootuf2_STATE,
    .fbuff = bootuf2_disk_fbuff,
    .erase = bootuf2_disk_erase,
    .cache = bootuf2_disk_cache,
    .cache_size = sizeof(bootuf2_disk_cache),
};

static void fname_copy(char *dst, char const *src, uint16_t len)
{
    for (size_t i = 0; i < len; ++i) {
        if (*src)
            *dst++ = *src++;
        else
            *dst++ = ' ';
    }
}

static void fcalculate_cluster(struct bootuf2_data *ctx)
{
    /*!< init files cluster */
    uint16_t cluster_beg = 2;
    for (int i = 0; i < ARRAY_SIZE(files); i++) {
        files[i].ClusterBeg = cluster_beg;
        files[i].ClusterEnd = -1 + cluster_beg +
                              BOOTUF2_DIVCEIL(files[i].FileSize,
                                              ctx->DBR->BPB.BytesPerSector *
                                                  ctx->DBR->BPB.SectorsPerCluster);
        cluster_beg = files[i].ClusterEnd + 1;
    }
}

static int ffind_by_cluster(uint32_t cluster)
{
    if (cluster >= 0xFFF0) {
        return -1;
    }

    for (uint32_t i = 0; i < ARRAY_SIZE(files); i++) {
        if ((files[i].ClusterBeg <= cluster) &&
            (cluster <= files[i].ClusterEnd)) {
            return i;
        }
    }

    return -1;
}

/* 判断这一块是否可以写入。
 * 块头字段来自主机, 一律当不可信输入:
 *  1) NumberOfBlock 为 0 或超过上限 —— 会被 state_check 误判为完成;
 *  2) PayloadSize 为 0 或超过 Data[476] —— 越界读, 还会写穿 4KB cache;
 *  3) TargetAddress 与 PayloadSize 之和越过 APP 区 —— 会擦掉 Bootloader 自己;
 *  4) BlockIndex 超过块数上限 —— Mask[pos] 越界写, 而 mask 只有 481 字节。
 * 剩余情况交给 Mask 判重。 */
static bool bootuf2block_check_writable(struct bootuf2_STATE *STATE,
                                        struct bootuf2_BLOCK *uf2, uint32_t block_max)
{
    if ((uf2->NumberOfBlock == 0) || (uf2->NumberOfBlock > BOOTUF2_BLOCKSMAX)) {
        USB_LOG_ERR("UF2 NumberOfBlock %u out of range\r\n", (unsigned)uf2->NumberOfBlock);
        return false;
    }
    if ((uf2->PayloadSize == 0) || (uf2->PayloadSize > sizeof(uf2->Data))) {
        USB_LOG_ERR("UF2 PayloadSize %u invalid\r\n", (unsigned)uf2->PayloadSize);
        return false;
    }
    if ((uf2->TargetAddress < CONFIG_BOOTUF2_APP_START) ||
        (uf2->TargetAddress + uf2->PayloadSize >
         CONFIG_BOOTUF2_APP_START + CONFIG_BOOTUF2_FLASHMAX)) {
        USB_LOG_ERR("UF2 TargetAddress %08x out of app region\r\n", (unsigned)uf2->TargetAddress);
        return false;
    }

    {
        if (uf2->BlockIndex < block_max) {
            uint8_t mask = 1 << (uf2->BlockIndex % 8);
            uint32_t pos = uf2->BlockIndex / 8;

            if ((STATE->Mask[pos] & mask) == 0) {
                return true;
            }
        }
    }

    return false;
}

/* 会话状态维护。
 * 同一上电周期内允许先后刷入两个不同的固件: 块数一变就当作"换了文件",
 * 清空掩码与进度重新计。只有块数本身非法(超过上限)才把会话打成永不完成 ——
 * 那种情况下主机发的块索引本身就不可信, 不能靠它判断进度。
 * 清空掩码是必须的: 否则新文件的块会被上一轮的掩码位跳过, 结果是固件缺块
 * 却仍然自动重启。 */
static void bootuf2block_state_update(struct bootuf2_STATE *STATE,
                                      struct bootuf2_BLOCK *uf2, uint32_t block_max)
{
    if (uf2->NumberOfBlock) {
        if (STATE->NumberOfBlock != uf2->NumberOfBlock) {
            if (uf2->NumberOfBlock > BOOTUF2_BLOCKSMAX) {
                /* 块数超出可升级区容量: 主机数据不可信, 让会话永不完成 */
                STATE->NumberOfBlock = 0xffffffff;
            } else {
                /* 换了新的升级文件: 清掉上一轮进度与掩码, 从 0 重新开始 */
                memset(STATE->Mask, 0, (BOOTUF2_BLOCKSMAX / 8) + 1);
                STATE->NumberOfWritten = 0;
                STATE->NumberOfBlock = uf2->NumberOfBlock;
            }
        }

        if (uf2->BlockIndex < block_max) {
            uint8_t mask = 1 << (uf2->BlockIndex % 8);
            uint32_t pos = uf2->BlockIndex / 8;

            if ((STATE->Mask[pos] & mask) == 0) {
                STATE->Mask[pos] |= mask;
                STATE->NumberOfWritten++;
            }
        }
    }

    USB_LOG_DBG("UF2 block total %d written %d index %d\r\n",
                uf2->NumberOfBllock, STATE->NumberOfWritten, uf2->BlockIndex);
}

static bool bootuf2block_state_check(struct bootuf2_STATE *STATE)
{
    return (STATE->NumberOfWritten >= STATE->NumberOfBlock) &&
           STATE->NumberOfBlock;
}

static int bootuf2_flash_flush(struct bootuf2_data *ctx)
{
    int err;

    if (ctx->cached_bytes == 0) {
        return 0;
    }

    err = bootuf2_flash_write(ctx->cached_address, ctx->cache, ctx->cached_bytes);

    if (err) {
        USB_LOG_ERR("UF2 slot flash write error %d at offset %08lx len %d\r\n",
                    err, ctx->cached_address, ctx->cached_bytes);
        return -1;
    }

    ctx->cached_bytes = 0;

    return 0;
}

int bootuf2_flash_write_internal(struct bootuf2_data *ctx, struct bootuf2_BLOCK *uf2)
{
    /* PayloadSize 是主机字段, 直接 memcpy 会有两种越界:
     * 超过 Data[476] 时把块尾之后的内存当固件烧进去;
     * 叠加 cached_bytes 超过 cache_size 时写穿 4KB 缓存。 */
    if (uf2->PayloadSize == 0 || uf2->PayloadSize > sizeof(uf2->Data)) {
        USB_LOG_ERR("UF2 payload size %d invalid (max %d)\r\n",
                    (int)uf2->PayloadSize, (int)sizeof(uf2->Data));
        return -1;
    }
    if (ctx->cached_bytes + uf2->PayloadSize > ctx->cache_size) {
        USB_LOG_ERR("UF2 payload overflow cache (%d + %d > %d)\r\n",
                    (int)ctx->cached_bytes, (int)uf2->PayloadSize, (int)ctx->cache_size);
        return -1;
    }

    /*!< 1.cache not empty and address not continue */
    /*!< 2.cache full */
    if ((ctx->cached_bytes && ((ctx->cached_address + ctx->cached_bytes) != uf2->TargetAddress)) ||
        (ctx->cached_bytes == ctx->cache_size)) {
        int err = bootuf2_flash_flush(ctx);
        if (err)
            return err;
    }

    memcpy(ctx->cache + ctx->cached_bytes, uf2->Data, uf2->PayloadSize);

    ctx->cached_address = uf2->TargetAddress - ctx->cached_bytes;
    ctx->cached_bytes += uf2->PayloadSize;

    return 0;
}

void bootuf2_init(void)
{
    struct bootuf2_data *ctx;

    ctx = &bootuf2_disk;

    fcalculate_cluster(ctx);

    ctx->cached_bytes = 0;
    ctx->cached_address = 0;

    /* 显式清空升级状态与掩码。
     * 上电周期内软件复位会重新执行到这里, 残留的 Mask 位会让主机重发的块被判为
     * "已写过"而跳过, NumberOfWritten 不再增长, 进度条永远走不到 100%。 */
    memset(bootuf2_mask, 0, sizeof(bootuf2_mask));
    bootuf2_STATE.NumberOfBlock   = 0;
    bootuf2_STATE.NumberOfWritten = 0;
    bootuf2_STATE.Enable          = 1;

    boot2uf2_flash_init();   // 解锁并清理 Flash 标志 + 清空擦除位图
}

int boot2uf2_read_sector(uint32_t start_sector, uint8_t *buff, uint32_t sector_count)
{
    struct bootuf2_data *ctx;

    ctx = &bootuf2_disk;

    while (sector_count) {
        memset(buff, 0, ctx->DBR->BPB.BytesPerSector);

        uint32_t sector_relative = start_sector;

        /*!< DBR sector */
        if (start_sector == BOOTUF2_SECTOR_DBR_END) {
            memcpy(buff, ctx->DBR, sizeof(struct bootuf2_DBR));
            buff[510] = 0x55;
            buff[511] = 0xaa;
        }
        /*!< FAT sector */
        else if (start_sector < BOOTUF2_SECTOR_FAT_END(ctx->DBR)) {
            uint16_t *buff16 = (uint16_t *)buff;

            sector_relative -= BOOTUF2_SECTOR_RSVD_END(ctx->DBR);

            /*!< Perform the same operation on all FAT tables */
            while (sector_relative >= ctx->DBR->BPB.SectorsPerFAT) {
                sector_relative -= ctx->DBR->BPB.SectorsPerFAT;
            }

            uint16_t cluster_unused = files[ARRAY_SIZE(files) - 1].ClusterEnd + 1;
            uint16_t cluster_absolute_first = sector_relative *
                                              BOOTUF2_FAT16_PER_SECTOR(ctx->DBR);

            /*!< cluster used link to chain, or unsed */
            for (uint16_t i = 0, cluster_absolute = cluster_absolute_first;
                 i < BOOTUF2_FAT16_PER_SECTOR(ctx->DBR);
                 i++, cluster_absolute++) {
                if (cluster_absolute >= cluster_unused)
                    buff16[i] = 0;
                else
                    buff16[i] = cluster_absolute + 1;
            }

            /*!< cluster 0 and 1 */
            if (sector_relative == 0) {
                buff[0] = ctx->DBR->BPB.MediaDescriptor;
                buff[1] = 0xff;
                buff16[1] = 0xffff;
            }

            /*!< cluster end of file */
            for (uint32_t i = 0; i < ARRAY_SIZE(files); i++) {
                uint16_t cluster_file_last = files[i].ClusterEnd;

                if (cluster_file_last >= cluster_absolute_first) {
                    uint16_t idx = cluster_file_last - cluster_absolute_first;
                    if (idx < BOOTUF2_FAT16_PER_SECTOR(ctx->DBR)) {
                        buff16[idx] = 0xffff;
                    }
                }
            }
        }
        /*!< root entries */
        else if (start_sector < BOOTUF2_SECTOR_ROOT_END(ctx->DBR)) {
            sector_relative -= BOOTUF2_SECTOR_FAT_END(ctx->DBR);

            struct bootuf2_ENTRY *ent = (void *)buff;
            int remain_entries = BOOTUF2_ENTRY_PER_SECTOR(ctx->DBR);

            uint32_t file_index_first;

            /*!< volume label entry */
            if (sector_relative == 0) {
                fname_copy(ent->Name, (char const *)ctx->DBR->BPB.VolumeLabel, 11);
                ent->Attribute = 0x28;
                ent++;
                remain_entries--;
                file_index_first = 0;
            } else {
                /*!< -1 to account for volume label in first sector */
                file_index_first = sector_relative * BOOTUF2_ENTRY_PER_SECTOR(ctx->DBR) - 1;
            }

            for (uint32_t idx = file_index_first;
                 (remain_entries > 0) && (idx < ARRAY_SIZE(files));
                 idx++, ent++) {
                const uint32_t cluster_beg = files[idx].ClusterBeg;

                const struct bootuf2_FILE *f = &files[idx];

                if ((0 == f->FileSize) &&
                    (0 != idx)) {
                    continue;
                }

                fname_copy(ent->Name, f->Name, 11);
                ent->Attribute = 0x05;
                ent->CreateTimeTeenth = BOOTUF2_SECONDS_INT % 2 * 100;
                ent->CreateTime = BOOTUF2_DOS_TIME;
                ent->CreateDate = BOOTUF2_DOS_DATE;
                ent->LastAccessDate = BOOTUF2_DOS_DATE;
                ent->FirstClustH16 = cluster_beg >> 16;
                ent->UpdateTime = BOOTUF2_DOS_TIME;
                ent->UpdateDate = BOOTUF2_DOS_DATE;
                ent->FirstClustL16 = cluster_beg & 0xffff;
                ent->FileSize = f->FileSize;
            }
        }
        /*!< data */
        else if (start_sector < BOOTUF2_SECTOR_DATA_END(ctx->DBR)) {
            sector_relative -= BOOTUF2_SECTOR_ROOT_END(ctx->DBR);

            int fid = ffind_by_cluster(2 + sector_relative / ctx->DBR->BPB.SectorsPerCluster);

            if (fid >= 0) {
                const struct bootuf2_FILE *f = &files[fid];

                uint32_t sector_relative_file =
                    sector_relative -
                    (files[fid].ClusterBeg - 2) * ctx->DBR->BPB.SectorsPerCluster;

                size_t fcontent_offset = sector_relative_file * ctx->DBR->BPB.BytesPerSector;
                size_t fcontent_length = f->FileSize;

                if (fcontent_length > fcontent_offset) {
                    const void *src = (void *)((uint8_t *)(f->Content) + fcontent_offset);
                    size_t copy_size = fcontent_length - fcontent_offset;

                    if (copy_size > ctx->DBR->BPB.BytesPerSector) {
                        copy_size = ctx->DBR->BPB.BytesPerSector;
                    }

                    memcpy(buff, src, copy_size);
                }
            }
        }
        /*!< unknown sector, ignore */

        start_sector++;
        sector_count--;
        buff += ctx->DBR->BPB.BytesPerSector;
    }

    return 0;
}

int bootuf2_write_sector(uint32_t start_sector, const uint8_t *buff, uint32_t sector_count)
{
    struct bootuf2_data *ctx;

    ctx = &bootuf2_disk;

    while (sector_count) {
        struct bootuf2_BLOCK *uf2 = (void *)buff;

        if (!((uf2->MagicStart0 == BOOTUF2_MAGIC_START0) &&
              (uf2->MagicStart1 == BOOTUF2_MAGIC_START1) &&
              (uf2->MagicEnd == BOOTUF2_MAGIC_END) &&
              (uf2->Flags & BOOTUF2_FLAG_FAMILID_PRESENT) &&
              !(uf2->Flags & BOOTUF2_FLAG_NOT_MAIN_FLASH))) {
            goto next;
        }

        if (uf2->FamilyID == CONFIG_BOOTUF2_FAMILYID) {
            /* 第三个参数是"块数上限", 必须传 BOOTUF2_BLOCKSMAX。
             * 若误传字节数(0xF0000), BlockIndex/8 可超过 mask 的 481 字节, 越界写。 */
            if (bootuf2block_check_writable(ctx->STATE, uf2, BOOTUF2_BLOCKSMAX)) {
                /* 第一块即将落盘: 打上"升级中", 此后掉电都不会再跳 APP。
                 * 放在这里而不是 init 里 —— 只是进 UF2 模式看看就断电的场合
                 * 不应把设备留在 Bootloader。 */
                Boot_Marker_Set(BOOT_MARK_UPGRADING);

                if (bootuf2_flash_write_internal(ctx, uf2) != 0) {
                    /* 写失败不更新 mask, 主机可重发这一块;
                     * NumberOfWritten 也保持旧值, 避免进度虚满。 */
                    USB_LOG_ERR("UF2 block %d write failed at %08x\r\n",
                                (int)uf2->BlockIndex, (unsigned)uf2->TargetAddress);
                    goto next;
                }
                bootuf2block_state_update(ctx->STATE, uf2, BOOTUF2_BLOCKSMAX);
            } else {
                USB_LOG_DBG("UF2 block %d already written\r\n",
                            uf2->BlockIndex);
            }
        } else {
            USB_LOG_DBG("UF2 block illegal id %08x\r\n", uf2->FamilyID);
        }

    next:
        start_sector++;
        sector_count--;
        buff += ctx->DBR->BPB.BytesPerSector;
    }

    return 0;
}

uint16_t bootuf2_get_sector_size(void)
{
    return bootuf2_disk.DBR->BPB.BytesPerSector;
}

uint32_t bootuf2_get_sector_count(void)
{
    return bootuf2_disk.DBR->BPB.SectorsOver32MB + bootuf2_disk.DBR->BPB.Sectors;
}

uint32_t bootuf2_get_total_blocks(void)
{
    return bootuf2_disk.STATE->NumberOfBlock;
}

uint32_t bootuf2_get_written_blocks(void)
{
    return bootuf2_disk.STATE->NumberOfWritten;
}

bool bootuf2_is_write_done(void)
{
    if (bootuf2block_state_check(bootuf2_disk.STATE)) {
        /* 末段缓存写成功才算完成。
         * flush 失败时不能让主机看到"成功", 否则设备带着缺尾的固件复位,
         * 而原固件已被部分擦除。 */
        if (bootuf2_flash_flush(&bootuf2_disk) != 0) {
            USB_LOG_ERR("UF2 final flush failed, stay in bootloader\r\n");
            bootuf2_disk.STATE->NumberOfBlock = 0xFFFFFFFF;  // 标记永不完成, 主机重拖
            return false;
        }
        /* 全部块写完整: 标记完成, 下次上电允许跳转 APP */
        Boot_Marker_Set(BOOT_MARK_DONE);
        USB_LOG_DBG("UF2 update ok\r\n");
        return true;
    } else {
        return false;
    }
}
