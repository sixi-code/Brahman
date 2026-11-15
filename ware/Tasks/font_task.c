#include "FreeRTOS.h"
#include "task.h"
#include "fontupd.h"
#include "pin_ctrl.h"
#include "sdio_sdcard.h"
#include "fatfs.h"
#include "defines.h"
#include "variables.h"
#include "page_manager.h"

void Font_Task(void *pvParameters)
{
	if (!g_font_need_update)
	{
		/* 自愿退出: 先把句柄与状态收回到空态再自删。
		 * 若留着句柄, 任务管理器后续的删除分支会对已删除的任务调用 vTaskDelete。 */
		taskENTER_CRITICAL();
		Font_Task_handler = NULL;
		Font_Task_Status = Task_P_Null;
		taskEXIT_CRITICAL();
		vTaskDelete(NULL);
		return;
	}

	while (!g_TFcard_inited)
	{
		vTaskDelay(pdMS_TO_TICKS(100));
	}

	uint8_t res = update_font((uint8_t*)"0:");

	if (!res)
	{
		g_font_need_update = 0;
		Page_Request_Switch(PAGE_START);
	}
	
	/* 同上: 结束前把句柄与状态收回空态 */
	taskENTER_CRITICAL();
	Font_Task_handler = NULL;
	Font_Task_Status = Task_P_Null;
	taskEXIT_CRITICAL();
	vTaskDelete(NULL);
}
