#ifndef __FATFS_H__
#define __FATFS_H__

#include "stm32f4xx.h"
#include "ff.h"
#include <stddef.h>   // size_t

uint8_t fatfs_mount(uint8_t pdrv);
uint8_t fatfs_unmount(uint8_t pdrv);

/* 容量查询, 单位 MB。参数用 uint32_t: 64GB 以上的卡总量会超过 65535 MB。 */
uint8_t fatfs_get_space(const char *drv, uint32_t *total, uint32_t *free);
/* 扩展名提取(重入): 结果写入调用方提供的 ext[cap], 不保存任何静态状态。
 * 缓冲至少 8 字节(最多 7 字符后缀 + '\0')。 */
uint8_t* fatfs_get_extension(const char *path, uint8_t *ext, size_t cap);
	
#endif
