#include "stm32f4xx.h"
#include <stddef.h>   // NULL
#include "img_crc.h"

/* 硬件 CRC 单元: 多项式 0x04C11DB7, 初值 0xFFFFFFFF, 无输出异或 ——
 * 与 bin2uf2/uf2conv.py 里的软件实现逐位一致。
 * 每写一次 DR 会按字节处理, 顺序是高位到低位; 为了让结果与"按地址顺序
 * 逐字节喂入"一致, 读出的字先 __REV 再写 DR。 */
static void crc_unit_begin(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_CRCEN;
    (void)RCC->AHB1ENR;
    CRC->CR = CRC_CR_RESET;      /* 清零到初值 */
}

/* 对一段 Flash 内容求 CRC。尾部不足一个字时按擦除态 0xFF 补齐 ——
 * 那正是 Flash 中未写入字节的实际内容。 */
static uint32_t crc_unit_calc(const uint8_t *p, uint32_t len)
{
    uint32_t i;

    crc_unit_begin();

    for (i = 0; i + 4 <= len; i += 4) {
        uint32_t w = (uint32_t)p[i]
                   | ((uint32_t)p[i + 1] << 8)
                   | ((uint32_t)p[i + 2] << 16)
                   | ((uint32_t)p[i + 3] << 24);
        CRC->DR = __REV(w);
    }

    if (i < len) {
        uint32_t w = 0xFFFFFFFFu;
        uint32_t k;
        for (k = 0; i + k < len; k++) {
            w &= ~(0xFFu << (8 * k));
            w |= (uint32_t)p[i + k] << (8 * k);
        }
        CRC->DR = __REV(w);
    }

    return CRC->DR;
}

uint8_t Img_Crc_Read(img_crc_record_t *out)
{
    const img_crc_record_t *rec = (const img_crc_record_t *)IMG_CRC_RECORD_ADDR;

    if (out == NULL) return 1;
    out->magic     = rec->magic;
    out->image_len = rec->image_len;
    out->crc32     = rec->crc32;
    out->reserved  = rec->reserved;

    return (out->magic == IMG_CRC_RECORD_MAGIC) ? 0 : 1;
}

uint8_t Img_Crc_Verify(uint32_t app_addr, uint32_t max_len)
{
    img_crc_record_t rec;

    if (Img_Crc_Read(&rec) != 0) {
        return 1;                     /* 没有记录: 旧流程烧录的固件, 由调用方决定 */
    }

    /* 记录里的长度同样不可信, 必须落在可升级区内 */
    if (rec.image_len == 0 || rec.image_len > max_len) {
        return 2;
    }

    if (crc_unit_calc((const uint8_t *)app_addr, rec.image_len) != rec.crc32) {
        return 2;
    }

    return 0;
}

/* 全部块写完后由 Bootloader 自己封盘: 计算并落盘 CRC 记录。
 * 顺序上先擦该页、写 length/crc, 最后写 magic —— magic 一旦有效, 其余字段
 * 必然已经写完, 不会出现"记录有效但内容残缺"的中间态。
 * 期间"升级中"标记一直有效, 掉电后下次上电仍会停在 UF2。 */
uint8_t Img_Crc_Seal(uint32_t app_addr, uint32_t image_len)
{
    uint32_t crc;
    uint32_t sector;

    if (image_len == 0) return 1;

    crc = crc_unit_calc((const uint8_t *)app_addr, image_len);

    /* 记录页所在扇区: 0x0800C000 ~ 0x0800FFFF, 该扇区不存放代码, 完全空闲。
     * 若将来 Bootloader 增长到越过 0x0800C000, 必须把记录页与 IMG_CRC_RECORD_ADDR
     * 一起挪到新的空闲页, 否则封盘时的擦除会把引导程序自己擦掉。 */
    sector = FLASH_Sector_3;

    /* 擦写期间 Flash 总线被占用, 任何取指或中断向量读取都会停摆;
     * 本函数运行在 Flash 上, 必须先关中断再动 Flash。 */
    __disable_irq();

    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                    FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    if (FLASH_EraseSector(sector, VoltageRange_3) != FLASH_COMPLETE) {
        FLASH_Lock();
        __enable_irq();
        return 2;
    }

    /* 先写长度与 CRC */
    if (FLASH_ProgramWord(IMG_CRC_RECORD_ADDR + 4, image_len) != FLASH_COMPLETE ||
        FLASH_ProgramWord(IMG_CRC_RECORD_ADDR + 8, crc)       != FLASH_COMPLETE) {
        FLASH_Lock();
        __enable_irq();
        return 3;
    }

    /* 最后写 magic: 它有效就意味着前面两个字段已经落盘 */
    if (FLASH_ProgramWord(IMG_CRC_RECORD_ADDR, IMG_CRC_RECORD_MAGIC) != FLASH_COMPLETE) {
        FLASH_Lock();
        __enable_irq();
        return 4;
    }

    FLASH_Lock();
    __enable_irq();

    /* 回读校验: 写完立刻验证一遍, 避免"以为写了其实没写" */
    if (Img_Crc_Verify(app_addr, image_len) != 0) {
        return 5;
    }

    return 0;
}
