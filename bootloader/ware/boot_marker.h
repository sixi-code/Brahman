#ifndef __BOOT_MARKER_H__
#define __BOOT_MARKER_H__

#include <stdint.h>

/* 升级过程的掉电标记, 存放在 RTC 备份寄存器(软复位与短时掉电都不清零)。
 * 目的: 上一次升级写到一半就掉电时, 下次上电不要直接跳 APP ——
 * 那时 APP 区是半截固件, 跳过去只会 HardFault 或跑飞, 用户看到的是"变砖"。
 * 留在 Bootloader 里, 用户重新拖入 UF2 即可救回来。 */

#define BOOT_MARK_NONE        0u   /* 无标记: 正常上电, 直接跳 APP */
#define BOOT_MARK_UPGRADING   1u   /* 正在升级: 掉电即视为镜像不完整 */
#define BOOT_MARK_DONE        2u   /* 本次升级已完整写入 */

void     Boot_Marker_Init(void);       /* 使能备份域访问, 只做一次 */
uint32_t Boot_Marker_Get(void);
void     Boot_Marker_Set(uint32_t state);

#endif
