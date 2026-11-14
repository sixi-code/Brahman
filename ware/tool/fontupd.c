#include "fontupd.h"
#include "ff.h"
#include "w25q128.h"
#include "lcd_bsp.h"
#include "string.h"
#include "malloc.h"
#include "systick_conf.h"
#include "FreeRTOS.h"
#include "task.h"
#include "variables.h"
#include "iwdg_feed.h"

//lvgl字库读取缓冲区
uint8_t __g_font_buf[210]; 

// 字库配置结构
typedef struct {
    char* path;        			
    uint8_t type;      			
    __packed uint32_t *addr;     
    __packed uint32_t *size;    
} FontConfig;

#define FONTSECSIZE     (1024 * 1024 * 7)   // 字库占用空间大小 7MB
#define FONTINFOADDR    (1024 * 1024 * 8)   // 字库存储起始地址 8MB
#define Checksum        0xAA

_font_info ftinfo;

// 全局变量区
static const FontConfig fontConfigs[] = {
    {"/FONT/UNIGBK.bin",  0, &ftinfo.unigbkaddr,   &ftinfo.unigbksize},
    {"/FONT/Font8.bin",   1, &ftinfo.font8addr,    &ftinfo.font8size},
    {"/FONT/Font12.bin",  2, &ftinfo.font12addr,   &ftinfo.font12size},
	{"/FONT/Font16.bin",  3, &ftinfo.font16addr,   &ftinfo.font16size},
	{"/FONT/Font24.bin",  4, &ftinfo.font24addr,   &ftinfo.font24size},
};

// 计算字库配置数量
#define FONT_CONFIG_COUNT (sizeof(fontConfigs) / sizeof(fontConfigs[0]))

// 内部使用的单文件更新函数
//fxpath: 字体文件路径指针
//fx: 字体索引
//start_addr: 写入Flash的起始地址
// 返回值: 0表示成功，其他表示错误码（1：内存分配失败，2：文件打开失败，3：文件状态获取失败，4：索引越界，5：文件读取失败）
static uint8_t updata_fontx(uint8_t *fxpath, uint8_t fx, uint32_t start_addr)
{
    uint32_t flashaddr = start_addr;// Flash写入起始地址
    FIL *fftemp = NULL;// FatFs 文件对象指针
    uint8_t *tempbuf = NULL;// 临时缓冲区指针
    uint8_t res;// FatFs 函数返回值
    uint8_t ret = 0;// 返回值，0表示成功，其他表示错误码
    uint16_t bread;// 实际读取的字节数
    uint32_t offx = 0;// 当前写入偏移量
    uint32_t new_addr = 0;// 本次要提交的新字库地址(成功后才写回全局)
    uint32_t new_size = 0;// 本次要提交的新字库大小
    FILINFO fileInfo;// 文件信息结构体

    fftemp = (FIL *)malloc_bsc(sizeof(FIL));
    tempbuf = malloc_bsc(4096);
    
    if (!fftemp || !tempbuf) {
        ret = 1; 
        goto __exit; // 统一出口，防止内存泄漏
    }

    res = f_open(fftemp, (const char *)fxpath, FA_READ);
    if (res != FR_OK) {
        ret = 2;
        goto __exit;
    }

    res = f_stat((const char *)fxpath, &fileInfo);
    if (res != FR_OK) {
        ret = 3;
        goto __exit;
    }
	
    /* 全局字库信息必须先"记在本地", 等整个文件写成功之后才提交。
     * 若进口就改 fontConfigs[fx].addr/size, 而 ftinfo.fontok 要到最后才写:
     * 中途掉电或卡读失败时, 重启后 font_init() 看到 size != 0 但数据是空的
     * → 全屏乱码; 且 size 已"有效", 自动升级逻辑也不再触发, 只能手动修。 */
    if (fx >= FONT_CONFIG_COUNT) {
        ret = 4;
        goto __exit;
    }
    new_addr = flashaddr;
    new_size = fileInfo.fsize;

    // 循环写入
    while (1) {
        IWDG_FEED_SAFE();   // 字库文件可达数 MB, 持续写 Flash 必须喂狗
        res = f_read(fftemp, tempbuf, 4096, (uint32_t *)&bread);
        if (res != FR_OK) {
            ret = 5;
            break;
        }
        // 写入 Flash (越界检查: 不允许越过字库区末尾)
        if ((uint32_t)offx + flashaddr + bread > (uint32_t)FONTINFOADDR + (uint32_t)FONTSECSIZE) {
            ret = 6;
            break;
        }
        W25QXX_Write(tempbuf, offx + flashaddr, bread);
        offx += bread;

        if (new_size > 0) {
            g_font_update_progress = (uint8_t)((offx * 100) / new_size);
        }

        if (bread != 4096) break;
    }
    
    f_close(fftemp);

    /* 只有完整写完才提交新的 addr/size —— 与上面的"先记本地"配对 */
    if (ret == 0 && offx == new_size) {
        *(fontConfigs[fx].addr) = new_addr;
        *(fontConfigs[fx].size) = new_size;
    } else if (ret == 0) {
        ret = 7;   // 读到的字节数与文件大小不符(文件被截断/卡读错误)
    }

__exit:
    if (fftemp) free_bsc(fftemp);
    if (tempbuf) free_bsc(tempbuf);
    return ret;
}

// 更新所有字体
// src: 字体文件所在目录路径
// 返回值: 0表示成功，其他表示错误码（1：内存分配失败，10+idx：第idx个文件不存在，20+idx：第idx个文件写入失败）
uint8_t update_font(uint8_t* src)
{
    uint8_t ret = 0;
    uint8_t *pname = NULL; // 字体文件路径缓冲区
    FIL *fftemp = NULL; // FatFs 文件对象指针
    uint8_t res; // FatFs 函数返回值
    uint32_t current_flash_addr = FONTINFOADDR + sizeof(ftinfo); // 当前写入Flash的起始地址，初始为FONTINFOADDR + ftinfo结构体大小

    g_font_update_state = 0; //更新状态  0: 未开始，1: 擦除中，2: 写入中，3: 完成，0xFF: 错误
    g_font_update_progress = 0; //更新进度 0-100
    g_font_update_file_index = 0;// 当前更新文件索引
    g_font_update_error = 0;// 字库更新错误码

    pname = malloc_bsc(100);
    fftemp = (FIL*)malloc_bsc(sizeof(FIL));
    
    if (!pname || !fftemp) {
        ret = 1;
        goto __error;
    }

    // 1. 预检查：所有文件必须都存在，否则不开始擦除
    for (uint8_t idx = 0; idx < FONT_CONFIG_COUNT; idx++) {
        strcpy((char*)pname, (char*)src);
        strcat((char*)pname, fontConfigs[idx].path);
        res = f_open(fftemp, (const char*)pname, FA_READ);
        if (res != FR_OK) {
            ret = 10 + idx;
            goto __error;
        }
        f_close(fftemp);
    }

	// 2. 擦除区域：从 FONTINFOADDR 开始的连续 FONTSECSIZE 字节（7MB）
	{
		uint32_t total_sectors = FONTSECSIZE / 4096; //擦除的总扇区数
		uint32_t sector_count = 0; //已擦除的扇区数
		g_font_update_state = 1; // 更新状态为擦除中

		for (uint32_t addr = FONTINFOADDR; addr < FONTINFOADDR + FONTSECSIZE; addr += 4096)
		{
			W25QXX_Erase_Sector(addr);
			sector_count++;
			g_font_update_progress = (uint8_t)((sector_count * 100) / total_sectors);
			// 7MB 字库区共 1792 个扇区, 全程远超看门狗超时: 每擦 16 个扇区喂一次狗并让出 CPU
			if ((sector_count & 0x0F) == 0) {
				IWDG_FEED_SAFE();
				vTaskDelay(pdMS_TO_TICKS(1));
			}
		}
	}

    // 3. 写入文件
    g_font_update_state = 2; // 更新状态为写入中
    g_font_update_progress = 0; // 更新进度为0
    for (uint8_t idx = 0; idx < FONT_CONFIG_COUNT; idx++) {
        g_font_update_file_index = idx;// 当前更新文件索引
        g_font_update_progress = 0;// 当前文件更新进度为0
        strcpy((char*)pname, (char*)src);
        strcat((char*)pname, fontConfigs[idx].path);

        // 传入当前计算好的地址
        res = updata_fontx(pname, idx, current_flash_addr);
        if (res) {
            ret = 20 + idx; // 20+: 写入失败错误码
            goto __error;
        }

        // 计算下一个文件的起始地址 = 当前地址 + 当前文件大小
        current_flash_addr += *(fontConfigs[idx].size);
    }

    // 4. 写入头部信息
    g_font_update_state = 3;// 更新状态为完成
    g_font_update_progress = 100;// 更新进度为100
    ftinfo.fontok = Checksum;// 设置字库完好标志
    W25QXX_Write((uint8_t*)&ftinfo, FONTINFOADDR, sizeof(ftinfo));// 写入字库信息到Flash

__error:
    if (ret) {
        g_font_update_state = 0xFF;// 更新状态为错误
        g_font_update_error = ret;// 设置错误码
    }
    if (pname) free_bsc(pname);
    if (fftemp) free_bsc(fftemp);

    return ret;
}

//初始化字体
//返回值:0,字库完好.其他,字库丢失
uint8_t font_init(void)
{	
    uint8_t t = 0;
    while (t < 3) 
	{
        t++;
        W25QXX_Read((uint8_t*)&ftinfo, FONTINFOADDR, sizeof(ftinfo));
        if (ftinfo.fontok == Checksum) break;
        Delay_ms(20);
    }
    return (ftinfo.fontok != Checksum) ? 1 : 0;
}
