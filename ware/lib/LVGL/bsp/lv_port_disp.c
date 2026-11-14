#if 1

#include "lv_port_disp.h"
#include "lvgl.h"
#include "lcd_bsp.h"
#include "lcd_pwm.h"
#include "malloc.h"
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"

#define MY_DISP_HOR_RES (240) //宽度
#define MY_DISP_VER_RES (240) //高度

static lv_disp_draw_buf_t draw_buf_dsc_2; // 保存缓冲区的描述符
static lv_disp_drv_t disp_drv;// 保存显示驱动的描述符

// 2 个缓冲区，分别用于双缓冲显示
static lv_color_t *buf_2_1 = NULL;                  
static lv_color_t *buf_2_2 = NULL;  

static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p);

/* 当前正在等待 lv_disp_flush_ready() 的驱动描述符; NULL 表示没有在飞搬运。
 * 原定义在文件中部, 因为 lv_port_disp_wait_idle() 要用, 提前到这里。 */
static lv_disp_drv_t *current_disp_drv = NULL;

/* 等待"当前没有正在进行的显存搬运"。
 * LCD_Color_Fill() 是 DMA 搬运, disp_flush 返回时像素还在从 buf_2_x 里往外搬;
 * 这时如果 free_bsc(buf_2_1/buf_2_2), DMA 会继续从已释放的块读数据 —— 花屏、
 * 半幅错位, 极端情况整机卡死。返回 0=空闲 1=超时(仍有搬运在进行)。 */
uint8_t lv_port_disp_wait_idle(uint32_t timeout_ms)
{
    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(timeout_ms);
    while (current_disp_drv != NULL)
    {
        if ((int32_t)(xTaskGetTickCount() - deadline) >= 0) return 1;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return 0;
}

//释放缓冲区
void lv_port_disp_deinit(void)
{
    /* 先等在飞的 DMA 收尾, 再放内存。这里做统一收口,
     * 所有调用点(媒体/游戏/副屏)都不用各自关心时序。 */
    lv_port_disp_wait_idle(200);

    draw_buf_dsc_2.buf1 = NULL;
    draw_buf_dsc_2.buf2 = NULL;

    if(buf_2_1) {
        free_bsc(buf_2_1);
        buf_2_1 = NULL;
    }
    if(buf_2_2) {
        free_bsc(buf_2_2);
        buf_2_2 = NULL;
    }
}

//重新分配缓冲区
void lv_port_disp_reinit(void)
{
    buf_2_1 = (lv_color_t *)malloc_bsc(MY_DISP_HOR_RES * 20 * sizeof(lv_color_t));
    buf_2_2 = (lv_color_t *)malloc_bsc(MY_DISP_HOR_RES * 20 * sizeof(lv_color_t));

    if(buf_2_1 != NULL && buf_2_2 != NULL) {
        lv_disp_draw_buf_init(&draw_buf_dsc_2, buf_2_1, buf_2_2, MY_DISP_HOR_RES * 20);
    }
}

//初始化显示驱动
void lv_port_disp_init(void)
{
    LCD_Init();

    lv_port_disp_reinit();

    if(!buf_2_1 || !buf_2_2) return;

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = MY_DISP_HOR_RES;//宽度
    disp_drv.ver_res = MY_DISP_VER_RES;//高度
    disp_drv.flush_cb = disp_flush;//刷新回调函数
    disp_drv.draw_buf = &draw_buf_dsc_2; // draw_buf_dsc_2 的地址是不变的
    lv_disp_drv_register(&disp_drv);//注册显示驱动
}

// 刷新回调函数
// disp_drv: 显示驱动的描述符
// area: 需要刷新的区域
// color_p: 颜色数据的指针
static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
	current_disp_drv = disp_drv;
    LCD_Color_Fill(area->x1, area->y1, area->x2, area->y2, (uint16_t *)color_p);
}

// DMA传输完成回调函数
void LCD_DMA_TransferComplete(void)
{
    if(current_disp_drv != NULL)
    {
        lv_disp_flush_ready(current_disp_drv);
        current_disp_drv = NULL;
    }
}

#else
typedef int keep_pedantic_happy;
#endif
