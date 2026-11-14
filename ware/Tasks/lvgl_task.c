#include "FreeRTOS.h"
#include "task.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "page_manager.h"
#include "variables.h"

void Lvgl_Task( void * pvParameters )
{   // 初始化 LVGL 库和显示输入设备
	lv_init();
	lv_port_disp_init();
	lv_port_indev_init();
    
    Page_Manager_Init(); 


    TickType_t xLastWakeTime = xTaskGetTickCount();
	
    while(1) 
    {
        /* 显存重建只能在 LVGL 自己的上下文里做。
         * 由媒体/游戏/副屏任务在 Resume 之前直接调 lv_port_disp_reinit() 会
         * lv_obj_invalidate(), 会和 LVGL 的 draw_buf/inv_areas 抢状态 —— 偶发花屏
         * 与半幅错位。改为请求-消费: 请求方置位后 Resume, 这里先重建再进主循环。 */
        if (g_lvgl_disp_reinit_req)
        {
            g_lvgl_disp_reinit_req = 0;
            lv_port_disp_reinit();
            if (lv_scr_act() != NULL) lv_obj_invalidate(lv_scr_act());
        }

        Page_Manager_Loop(); 
        
        lv_timer_handler(); 
		
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(20));
    }
}
