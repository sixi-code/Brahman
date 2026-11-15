//user file
#include "stm32f4xx.h"
#include "sdio_sdcard.h"
#include "w25q128.h"
#include "malloc.h"		  
#include "ff.h"
#include "string.h"
#include <ctype.h>

// 全局指针数组，最好初始化为NULL以防野指针
FATFS *fs[FF_VOLUMES] = {NULL};
FIL *file;	  				

/**
 * @brief  统一的 FatFs 挂载函数 (包含自动内存分配)
 * @param  pdrv: 0 - SD卡, 1 - U盘 (原外部Flash已删除)
 * @retval 0:成功, 1:失败
 */
uint8_t fatfs_mount(uint8_t pdrv)
{
    uint8_t res;
    // 动态生成挂载路径字符串，例如 "0:", "1:"
    char path[3] = {(char)(pdrv + '0'), ':', '\0'};

    if(pdrv >= FF_VOLUMES) return 1; // 越界保护

    // 1. 自动分配内存 (如果还未分配)
    if(fs[pdrv] == NULL)
    {
        fs[pdrv] = (FATFS*)malloc_bsc(sizeof(FATFS));
        if(fs[pdrv] == NULL) return 1; // 内存不足
    }

    // 2. 尝试挂载
    res = f_mount(fs[pdrv], path, 1); 	

    return (res == FR_OK) ? 0 : 1;
}

/**
 * @brief  统一的 FatFs 卸载函数 (包含自动内存释放)
 * @param  pdrv: 0 - SD卡, 1 - U盘 (原外部Flash已删除)
 * @retval 0:成功, 其他:失败
 */
uint8_t fatfs_unmount(uint8_t pdrv)
{
    uint8_t res;
    char path[3] = {(char)(pdrv + '0'), ':', '\0'};

    if(pdrv >= FF_VOLUMES) return 1; // 越界保护

    // 1. 卸载文件系统
    res = f_mount(NULL, path, 0);

    /* 2. 不能 free FATFS 结构。
     * FatFs 在 FF_FS_LOCK=0 时不跟踪打开的 FIL: 音乐播放中拔卡时, 另一任务
     * 手里可能还攥着指向这个 FATFS 的 FIL -> fs 指针, free 之后它就成了
     * 指向已释放堆块的野指针, 后续任何 f_read 都是堆损坏。
     * 改为保留结构体复用: f_mount(NULL) 只是让卷失效, 下次 mount 时
     * f_mount 会重新初始化这块内存, 复用是安全的, 且省一次分配/释放。 */
    if(fs[pdrv] == NULL)
    {
        fs[pdrv] = (FATFS *)malloc_bsc(sizeof(FATFS));
    }

    return res;
}

// ================= 保留原来的函数 =================

/* 计算容量: "0:" / "1:" 的总容量与剩余容量, 单位 MB。
 * 用 uint32_t 而不是 uint16_t —— FAT32 上 64GB 卡的总量超过 65535 MB, 会被截断。 */
uint8_t fatfs_get_space(const char *drv, uint32_t *total, uint32_t *free)
{
	FATFS *fs1;
	uint8_t res;
    uint32_t fre_clust = 0, fre_sect = 0, tot_sect = 0;

    if (drv == NULL || total == NULL || free == NULL) return 1;

    res = (uint32_t)f_getfree((const char*)drv, (uint32_t*)&fre_clust, &fs1);
    if(res == 0)
	{
	    tot_sect = (fs1->n_fatent - 2) * fs1->csize;
	    fre_sect = fre_clust * fs1->csize;
		*total = (uint32_t)(tot_sect >> 11); // >>11 = 除以 2048, 即 512B 扇区折算成 MB
		*free  = (uint32_t)(fre_sect >> 11);
 	}
	return res;
}

/**
 * @brief 从路径或文件名中提取文件扩展名（后缀）
 * @param path 文件路径（可包含目录）或纯文件名
 * @param ext  调用方提供的输出缓冲, 至少 8 字节
 * @param cap  输出缓冲字节数; 最多写入 cap-1 个字符再补 '\0'
 * @return 传入的 ext 指针（便于链式调用）, 若没有扩展名则内容为空串
 *
 * 输出由调用方提供, 函数本身不保存任何静态状态, 因此可被多个任务同时调用。
 */
uint8_t* fatfs_get_extension(const char *path, uint8_t *ext, size_t cap)
{
    const char *fname = path;
    const char *last_sep = NULL;
    const char *dot;
    size_t copy_len;

    if (ext == NULL || cap == 0) return ext;
    memset(ext, 0, cap);

    if (path == NULL) return ext;

    // 1. 提取文件名（最后一个 '/' 或 '\\' 之后的部分）
    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\') {
            last_sep = p;
        }
    }
    if (last_sep != NULL) {
        fname = last_sep + 1;
    }

    // 2. 查找最后一个 '.'
    dot = strrchr(fname, '.');
    if (dot && dot != fname && *(dot+1) != '\0') {
        const char *ext_start = dot + 1;
        size_t ext_len = strlen(ext_start);
        copy_len = ext_len > (cap - 1) ? (cap - 1) : ext_len;

        for (size_t i = 0; i < copy_len; i++) {
            ext[i] = (uint8_t)tolower(ext_start[i]);
        }
        ext[copy_len] = '\0';
    }
    return ext;
}
