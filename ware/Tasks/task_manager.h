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

void Taskmanager_Ctrl(uint8_t task_num,uint8_t task_action,uint8_t IsFromISR);
uint8_t Taskmanager_Wait_Suspended(uint8_t task_num, uint32_t timeout_ms);
	
#endif
