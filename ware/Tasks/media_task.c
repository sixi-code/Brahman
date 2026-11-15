#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "systick_conf.h"
#include "debug.h"
#include "variables.h"
#include "defines.h"
#include "video.h"
#include "mjpeg.h"
#include "jpg.h"
#include "gif.h"
#include "png.h"
#include "bmp.h"
#include "task_manager.h"
#include "page_manager.h"
#include "fatfs.h"
#include "lv_port_disp.h"
#include "file_unit.h"
#include <string.h>
#include "avi.h"

void Media_Task( void * pvParameters )
{
    uint8_t res = 0;
    uint8_t last_R = 0;

    Delay_ms(50);

    uint8_t ext_buf[8];
    uint8_t *ext = fatfs_get_extension(chosen_file_path, ext_buf, sizeof(ext_buf));

    g_lcd_user = LCD_USER_MDIA;
    Taskmanager_Ctrl(Task_N_LVGL, Task_T_Suspend, 0);
    /* Suspend 只是"请求": 必须确认 LVGL 任务真的停了, 才能动它的显存。
     * 否则 LVGL 可能正处在 disp_flush 中间(或 lv_timer_handler 里),
     * lv_port_disp_deinit() 释放 buf_2_1/buf_2_2 后 DMA 仍在搬像素 → 花屏/死机。 */
    Taskmanager_Wait_Suspended(Task_N_LVGL, 500);
    Page_Push_History(Page_Get_Current());
    lv_port_disp_deinit();
    Page_Request_Switch(PAGE_MEDIA);

    if (strcmp((char*)ext, "mjpeg") == 0)
    {
        res = video_mjpeg_play_init(chosen_file_path);
        chosen_file_path_free();
        while (!res)
        {
            if (g_key_R_M_RT && !last_R) break; // 右摇杆刚按下(按下沿)即退出播放
            last_R = g_key_R_M_RT;
            res = video_mjpeg_play_task();
        }
        video_mjpeg_play_deinit();
    }
    else if (strcmp((char*)ext, "raw") == 0)
    {
        res = video_play_init(chosen_file_path);
        chosen_file_path_free();
        while (!res)
        {
            if (g_key_R_M_RT && !last_R) break; // 右摇杆刚按下(按下沿)即退出播放
            last_R = g_key_R_M_RT;
            res = video_play_task();
        }
        video_play_deinit();
    }
	else if (strcmp((char*)ext, "avi") == 0)
    {
        res = video_avi_play_init(chosen_file_path);
        chosen_file_path_free();
        while (!res)
        {
            if (g_key_R_M_RT && !last_R) break; // 右摇杆刚按下(按下沿)即退出播放
            last_R = g_key_R_M_RT;
            res = video_avi_play_task();
        }
        video_avi_play_deinit();
    }
    else if (strcmp((char*)ext, "bmp") == 0 ||
             strcmp((char*)ext, "jpeg") == 0 ||
             strcmp((char*)ext, "jpg") == 0 ||
             strcmp((char*)ext, "png") == 0)
    {
        if (strcmp((char*)ext, "bmp") == 0)
			res = Decode_BMP_Picture(chosen_file_path);
        else if (strcmp((char*)ext, "png") == 0)
			res = Decode_PNG_Picture(chosen_file_path);
        else
			res = Decode_JPG_Picture(chosen_file_path);
        chosen_file_path_free();

        while (!res)
        {
            if (g_key_R_M_RT && !last_R) break; // 右摇杆刚按下(按下沿)即退出播放
            last_R = g_key_R_M_RT;
            Delay_ms(20);
        }
    }
    else // gif
    {
        res = Decode_GIF_Init(chosen_file_path);
        chosen_file_path_free();
        while (!res)
        {
            if (g_key_R_M_RT && !last_R) break; // 右摇杆刚按下(按下沿)即退出播放
            last_R = g_key_R_M_RT;
            res = Decode_GIF_Task();
        }
        Decode_GIF_Deinit();
    }

    g_lcd_user = LCD_USER_LVGL;
    /* 显存重建 + 全屏重绘交给 Lvgl_Task 自己在安全点执行:
     * 这里先置请求, 再 Resume, 避免在 LVGL 上下文之外改 draw_buf/inv_areas。 */
    g_lvgl_disp_reinit_req = 1;
    Taskmanager_Ctrl(Task_N_LVGL, Task_T_Resume, 0);
    Page_Back();

    Taskmanager_Ctrl(Task_N_Media, Task_T_Delete, 0);
    while(1) { Delay_ms(20); }
}
