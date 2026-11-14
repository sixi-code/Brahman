#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "systick_conf.h"
#include "variables.h"
#include "defines.h"
#include "nes_main.h"
#include "lvgl.h"
#include "task_manager.h"
#include "page_manager.h"
#include "fatfs.h"
#include "lv_port_disp.h"
#include "file_unit.h"
#include <string.h>

void Game_Task( void * pvParameters )
{
    uint8_t last_R = 0;

    Delay_ms(50);

    g_lcd_user = LCD_USER_GAME;
    Taskmanager_Ctrl(Task_N_LVGL, Task_T_Suspend, 0);
    Taskmanager_Wait_Suspended(Task_N_LVGL, 500);// 等 LVGL 真正挂起, 见 media_task.c 说明
    Page_Push_History(Page_Get_Current());
    lv_port_disp_deinit();
    Page_Request_Switch(PAGE_GAME);

    /* 必须检查 nes_init 的返回值: 非法 .nes(id 不是 "NES")、未实现的 mapper
     * 或内存不足时它返回非 0, 但这里照样 while(1) 全速跑模拟器 ——
     * PRG/PPU 全是垃圾, CPU 跑进未定义 opcode, 表现为黑屏 + Game 任务占满 CPU,
     * 而且 LVGL 已被挂起, 用户只能断电。
     * 初始化失败时立即按"退出播放"的路径返回列表页。 */
    uint8_t nes_res = nes_init(chosen_file_path);
    chosen_file_path_free();

    if (nes_res)
    {
        nes_deinit();
        g_lcd_user = LCD_USER_LVGL;
        g_lvgl_disp_reinit_req = 1;
        Taskmanager_Ctrl(Task_N_LVGL, Task_T_Resume, 0);
        Page_Back();
        Taskmanager_Ctrl(Task_N_Game, Task_T_Delete, 0);
        while(1) { Delay_ms(20); }
    }

    while (1)
    {
        if (!g_key_R_M_RT && last_R) break;
        last_R = g_key_R_M_RT;
        nes_task();
    }

    nes_deinit();

    g_lcd_user = LCD_USER_LVGL;
    g_lvgl_disp_reinit_req = 1;// 由 Lvgl_Task 在安全点重建显存并重绘
    Taskmanager_Ctrl(Task_N_LVGL, Task_T_Resume, 0);
    Page_Back();

    Taskmanager_Ctrl(Task_N_Game, Task_T_Delete, 0);
    while(1) { Delay_ms(20); }
}
