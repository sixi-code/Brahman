#ifndef __TASK_MANAGER_H__
#define __TASK_MANAGER_H__

#include "stm32f4xx.h" 

void Start_Task(void *pvParameters);
void Task_Manager(void *pvParameters);
void Basic_Task(void *pvParameters);
void Lvgl_Task(void *pvParameters);
void USB_Task(void *pvParameters);
void Music_Task(void *pvParameters);
void Media_Task(void *pvParameters);
void Game_Task(void *pvParameters);
void Font_Task(void *pvParameters);
void FileOp_Task(void *pvParameters);

/* 提交一条控制请求。请求进入内部的固定环形队列, 由 Task_Manager 逐条执行;
 * 同一任务的新请求会覆盖尚未处理的旧请求(最新意图优先)。 */
void Taskmanager_Ctrl(uint8_t task_num,uint8_t task_action,uint8_t IsFromISR);
uint8_t Taskmanager_Wait_Suspended(uint8_t task_num, uint32_t timeout_ms);
uint8_t Taskmanager_Get_Dropped(void);   // 队列满被丢弃的请求数(诊断用)
	
#endif
