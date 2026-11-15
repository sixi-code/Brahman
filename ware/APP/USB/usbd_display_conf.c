    /*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "usbd_display.h"
#include "usbd_display_conf.h"   // usb_display_deinit 声明, 供初始化失败时回退
#include "FreeRTOS.h"
#include "semphr.h"
#include "malloc.h"
#include "lcd_bsp.h"
#include "variables.h"
#include "defines.h"
#include "task_manager.h"
#include "lvgl.h"
#include "task_manager.h"
#include "page_manager.h"
#include "lv_port_disp.h"

#define DISPLAY_IN_EP  0x81
#define DISPLAY_OUT_EP 0x01

#define USBD_VID           0x303A
#define USBD_PID           0x2987
#define USBD_MAX_POWER     100
#define USBD_LANGID_STRING 1033

#define USB_CONFIG_SIZE (9 + 9 + 7 + 7)

#ifdef CONFIG_USB_HS
#define DISPLAY_EP_MPS 512
#else
#define DISPLAY_EP_MPS 64
#endif

static const uint8_t device_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0x00, 0x00, 0x00, USBD_VID, USBD_PID, 0x0101, 0x01)
};

static const uint8_t config_descriptor[] = {
    USB_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE, 0x01, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    USB_INTERFACE_DESCRIPTOR_INIT(0x00, 0x00, 0x02, 0xff, 0x00, 0x00, 0x00),
    USB_ENDPOINT_DESCRIPTOR_INIT(DISPLAY_IN_EP, 0x02, DISPLAY_EP_MPS, 0x00),
    USB_ENDPOINT_DESCRIPTOR_INIT(DISPLAY_OUT_EP, 0x02, DISPLAY_EP_MPS, 0x00),
};

static const uint8_t device_quality_descriptor[] = {
    0x0a,
    USB_DESCRIPTOR_TYPE_DEVICE_QUALIFIER,
    0x00,
    0x02,
    0x00,
    0x00,
    0x00,
    0x40,
    0x00,
    0x00,
};


/* 字符串描述符 */
static const char *string_descriptors[] = {
    (const char[]){ 0x09, 0x04 },           /* Langid */
    "STM32",                                /* Manufacturer */
    "STM32_R240x240_Ergb16_Fps60_Bl16",     /* Product - 修改为240x240 */
    "2022123456",                           /* Serial Number */
    "STM32_display_only"                    /* Display Interface String */
};

static const uint8_t *device_descriptor_callback(uint8_t speed)
{
    return device_descriptor;
}

static const uint8_t *config_descriptor_callback(uint8_t speed)
{
    return config_descriptor;
}

static const uint8_t *device_quality_descriptor_callback(uint8_t speed)
{
    return device_quality_descriptor;
}

static const char *string_descriptor_callback(uint8_t speed, uint8_t index)
{
    if (index >= (sizeof(string_descriptors) / sizeof(char *))) {
        return NULL;
    }
    return string_descriptors[index];
}

const struct usb_descriptor usbdisplay_descriptor = {
    .device_descriptor_callback = device_descriptor_callback,
    .config_descriptor_callback = config_descriptor_callback,
    .device_quality_descriptor_callback = device_quality_descriptor_callback,
    .string_descriptor_callback = string_descriptor_callback
};

static void usbd_event_handler(uint8_t busid, uint8_t event)
{
    switch (event) {
        default:
            break;
    }
}

struct usbd_interface intf0;

#define DISPLAY_CHUNK_SIZE  8192 // 每个显示数据块的大小
#define DISPLAY_CHUNK_COUNT 4    // 每次接收的显示数据块数量

struct usbd_display_frame frame_pool[DISPLAY_CHUNK_COUNT];

void usb_display_init(uint8_t busid, uintptr_t reg_base)
{
	g_lcd_user = LCD_USER_DISP;
	Taskmanager_Ctrl(Task_N_LVGL, Task_T_Suspend, 0);
	/* 必须确认 LVGL 真挂起了才动显存: 超时说明它还在跑 disp_flush/DMA,
	   此时 deinit 会回收在用的 draw_buf —— 在途传输读已释放内存。
	   超时就整体回退, 副屏开启失败比花屏/HardFault 好得多。 */
	if (Taskmanager_Wait_Suspended(Task_N_LVGL, 500) != 0) {
		g_lcd_user = LCD_USER_LVGL;
		return;
	}
	Page_Push_History(Page_Get_Current());
	lv_port_disp_deinit();
	Page_Request_Switch(PAGE_DISPLAY);
	
    for (int i = 0; i < DISPLAY_CHUNK_COUNT; i++) 
	{
        frame_pool[i].frame_buf = (uint8_t *)malloc_bsc(DISPLAY_CHUNK_SIZE);
        if (frame_pool[i].frame_buf == NULL) {
            /* 4 块 8KB 共 32KB, 副屏开启时若 BSC 池不够就整体回退:
               不能死等, 否则连带 LVGL 一起被挂在这一帧上 */
            for (int k = 0; k < i; k++) {
                if (frame_pool[k].frame_buf) { free_bsc(frame_pool[k].frame_buf); frame_pool[k].frame_buf = NULL; }
            }
            usb_display_deinit();      // 恢复 LVGL 显示与页面
            return;
        }
        frame_pool[i].frame_bufsize = DISPLAY_CHUNK_SIZE;
    }

    usbd_desc_register(busid, &usbdisplay_descriptor);

    usbd_add_interface(busid, usbd_display_init_intf(&intf0, DISPLAY_OUT_EP, DISPLAY_IN_EP, frame_pool, DISPLAY_CHUNK_COUNT));
    usbd_initialize(busid, reg_base, usbd_event_handler);
}

void usb_display_deinit(void)
{
	g_lcd_user = LCD_USER_LVGL;
	g_lvgl_disp_reinit_req = 1;// 由 Lvgl_Task 在安全点重建显存并重绘
	Taskmanager_Ctrl(Task_N_LVGL, Task_T_Resume, 0);
	Page_Back();
	
    for (int i = 0; i < DISPLAY_CHUNK_COUNT; i++) 
    {
        if (frame_pool[i].frame_buf != NULL) {
            free_bsc(frame_pool[i].frame_buf);
            frame_pool[i].frame_buf = NULL;
        }
    }
	usbd_ep_close(0, DISPLAY_IN_EP);
    usbd_ep_close(0, DISPLAY_OUT_EP);
	usbd_deinitialize(0);
}

void usb_display_task(void)
{
    static uint8_t last_R = 0;

    if (g_key_R_M_RT && !last_R) { // 右摇杆刚按下(按下沿)退出 USB 显示模式
        g_usb_function = USB_NONE;
    }
    last_R = g_key_R_M_RT;

    struct usbd_display_frame *frame;
    int ret;

    ret = usbd_display_dequeue(&frame, pdMS_TO_TICKS(100));
    if (ret < 0) {return;}

    uint8_t *data_ptr = frame->frame_buf;
    uint32_t data_len = frame->frame_size;

    if (frame->is_sof) 
	{
        struct usbd_disp_frame_header *header = (struct usbd_disp_frame_header *)data_ptr;

        /* 主机下发的 header 完全不受信: 帧长度不足时 data_len -= sizeof(header)
           会让 uint32_t 下溢成 ~4G, 直接让 DMA 搬穿整个内存; 坐标越界则把 LCD
           窗口设到屏幕外. 这里先校验, 不合法就丢弃该帧并回队. */
        if (data_len < sizeof(struct usbd_disp_frame_header)) {
            usbd_display_enqueue(frame);
            return;
        }
        if (header->x >= LCD_W || header->y >= LCD_H ||
            header->width == 0 || header->height == 0 ||
            (uint32_t)header->x + header->width  > LCD_W ||
            (uint32_t)header->y + header->height > LCD_H) {
            usbd_display_enqueue(frame);
            return;
        }

        LCD_Address_Set(header->x, header->y, header->x + header->width - 1, header->y + header->height - 1);
        data_ptr += sizeof(struct usbd_disp_frame_header);
        data_len -= sizeof(struct usbd_disp_frame_header);
    }

    if (data_len > 0) 
	{
        LCD_Write_DMA((uint16_t *)data_ptr, data_len / 2);
		
        xEventGroupWaitBits(xLcdEventGroup,   		// 事件组句柄
							LCD_USER_DISP,    		// 等待显示事件位
							pdTRUE,           		// 退出时自动清除该位
							pdFALSE,          		// 无需等待所有位（单一位）
							pdMS_TO_TICKS(100));  	// 100ms
    }

    usbd_display_enqueue(frame);
}
