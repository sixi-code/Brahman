#ifndef __IMG_CRC_H__
#define __IMG_CRC_H__

#include <stdint.h>

/* APP 镜像完整性校验记录。
 * 记录放在 Bootloader 区末尾的专用页(0x0800FC00), 所在扇区
 * (0x0800C000~0x0800FFFF) 不存放任何代码, 擦写它不会破坏引导程序;
 * 注意 Bootloader 若增长到越过 0x0800C000, 必须把记录页挪走。
 *
 * 记录由 Bootloader 自己在 UF2 全部写完后封盘生成(Img_Crc_Seal),
 * 上电跳转前再用硬件 CRC 单元重算一遍比对。主机侧工具无需参与,
 * 换用任何能生成 UF2 的工具都自动获得校验能力。 */

#define IMG_CRC_RECORD_ADDR   0x0800FC00UL
#define IMG_CRC_RECORD_MAGIC  0x43524331UL   /* "CRC1" */

typedef struct {
    uint32_t magic;       /* IMG_CRC_RECORD_MAGIC 表示记录有效 */
    uint32_t image_len;   /* 参与校验的字节数, 已按 4 字节向上取整 */
    uint32_t crc32;       /* 与主机工具一致的 CRC32 结果 */
    uint32_t reserved;    /* 占满 16 字节, 便于按字读写 */
} img_crc_record_t;

/* 返回值: 0=记录有效且校验一致, 1=记录不存在(未写入过), 2=校验不一致 */
uint8_t Img_Crc_Verify(uint32_t app_addr, uint32_t max_len);

/* 读取记录本身, 供诊断使用; 返回 0 表示记录有效 */
uint8_t Img_Crc_Read(img_crc_record_t *out);

/* 全部块写完后由 Bootloader 封盘: 计算 image_len 字节的 CRC 并落盘。
 * 返回 0 表示记录已写入且回读校验通过。 */
uint8_t Img_Crc_Seal(uint32_t app_addr, uint32_t image_len);

#endif
