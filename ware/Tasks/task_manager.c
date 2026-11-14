#include "stm32f4xx.h" 
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "defines.h"
#include "variables.h"
#include "task_manager.h"

/* 任务控制块结构体 */
typedef struct {
    TaskFunction_t      task_func;      // 任务主函数
    const char *        task_name;      // 任务名称
    uint16_t            stack_size;     // 堆栈大小
    UBaseType_t         priority;       // 任务优先级
    TaskHandle_t *      handle_ptr;     // 任务句柄指针
    volatile uint8_t *  status_ptr;     // 任务状态变量指针 (连接 variables.c)
} Task_Registry_t;

/* 任务注册表 (数组索引必须与 Task_N_xxx 宏严格对应) */
static const Task_Registry_t Task_List[] = {
    [Task_N_Basic]  = { Basic_Task, "Basic_Task", BASIC_STACK_SIZE, BASIC_PRIO, &Basic_Task_handler, &Basic_Task_Status },
    [Task_N_LVGL]   = { Lvgl_Task,  "LVGL_Task",  LVGL_STACK_SIZE,  LVGL_PRIO,  &Lvgl_Task_handler,  &LVGL_Task_Status  },
    [Task_N_USB]    = { USB_Task,   "USB_Task",   USB_STACK_SIZE,   USB_PRIO,   &USB_Task_handler,   &USB_Task_Status   },
    [Task_N_Music]  = { Music_Task, "Music_Task", MUSIC_STACK_SIZE, MUSIC_PRIO, &Music_Task_handler, &Music_Task_Status },
    [Task_N_Media]  = { Media_Task,  "Media_Task",  MEDIA_STACK_SIZE,  MEDIA_PRIO,  &Media_Task_handler,  &Media_Task_Status  },
    [Task_N_Game]   = { Game_Task,  "Game_Task",  GAME_STACK_SIZE,  GAME_PRIO,  &Game_Task_handler,  &Game_Task_Status  },
    [Task_N_Font]   = { Font_Task,   "Font_Task",   FONT_STACK_SIZE,   FONT_PRIO,   &Font_Task_handler,   &Font_Task_Status   },
    [Task_N_FileOp] = { FileOp_Task, "FileOp_Task", FILEOP_STACK_SIZE, FILEOP_PRIO, &FileOp_Task_handler, &FileOp_Task_Status },
};

#define TASK_NUM_MAX (sizeof(Task_List) / sizeof(Task_List[0]))//任务数量最大值

/**
 * @brief 触发任务管理器，更改任务状态
 * @param task_num 任务编号 (Task_N_xxx)
 * @param task_action 任务动作 (Task_T_xxx)
 * @param IsFromISR 是否来自中断上下文
 */
void Taskmanager_Ctrl(uint8_t task_num, uint8_t task_action, uint8_t IsFromISR)
{
    if (task_num >= TASK_NUM_MAX) return;

    *(Task_List[task_num].status_ptr) = task_action;

    if (IsFromISR) 
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(xTaskManagerSemaphore, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
    else
    {
        xSemaphoreGive(xTaskManagerSemaphore);
    }
}

/**
 * @brief 等待某个任务真正进入挂起态
 * @param task_num 任务编号 (Task_N_xxx)
 * @param timeout_ms 最长等待时间
 * @return 0-已挂起 1-超时
 *
 * Taskmanager_Ctrl(Task_T_Suspend) 只是把动作写进状态变量 + Give 信号量,
 * 真正的 vTaskSuspend 由 Task_Manager 稍后执行。调用方若紧接着释放该任务正在
 * 使用的资源(比如 LVGL 的显存), 就会撞上"资源已释放但任务还在跑"的窗口。
 * 所有这类调用点必须先等这个函数返回 0。
 */
uint8_t Taskmanager_Wait_Suspended(uint8_t task_num, uint32_t timeout_ms)
{
    TickType_t deadline;
    if (task_num >= TASK_NUM_MAX) return 1;

    deadline = xTaskGetTickCount() + pdMS_TO_TICKS(timeout_ms);
    while (1)
    {
        TaskHandle_t h = *(Task_List[task_num].handle_ptr);
        /* 句柄为空 = 任务本来就没了, 视为已挂起 */
        if (h == NULL || eTaskGetState(h) == eSuspended) return 0;
        if ((int32_t)(xTaskGetTickCount() - deadline) >= 0) return 1;
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

/**
 * @brief 任务管理主线程
 */
void Task_Manager( void * pvParameters )
{
    while (1)
    {
        // 阻塞等待外部使能指令
        xSemaphoreTake(xTaskManagerSemaphore, portMAX_DELAY);

        // 遍历所有注册的任务，处理其状态机
        for (uint8_t i = 0; i < TASK_NUM_MAX; i++)
        {
            uint8_t current_action = *(Task_List[i].status_ptr);
            TaskHandle_t *pHandle = Task_List[i].handle_ptr;

            switch (current_action)
            {
                case Task_T_Creat:
                    /* 注册表里尚未实现的项 task_func 为 NULL, 直接降级为 Task_P_Null,
                       否则 xTaskCreate(NULL, ...) 会创建入口为空指针的任务 */
                    if (Task_List[i].task_func == NULL) {
                        *(Task_List[i].status_ptr) = Task_P_Null;
                        break;
                    }
                    if (*pHandle == NULL) {
                        BaseType_t baseType = xTaskCreate(Task_List[i].task_func,
                                                          Task_List[i].task_name,
                                                          Task_List[i].stack_size,
                                                          NULL,
                                                          Task_List[i].priority,
                                                          pHandle);
                        if (baseType != pdPASS) {
                            /* 创建失败(通常是 CCM 池不足): 保持空态, 绝不谎报 Running,
                               否则后续 Suspend/Resume 判空跳过, 状态机永久说谎 */
                            *(Task_List[i].status_ptr) = Task_P_Null;
                            break;
                        }
                    }
                    *(Task_List[i].status_ptr) = Task_P_Running;
                    break;

                case Task_T_Suspend:
                    if (*pHandle != NULL) {
                        vTaskSuspend(*pHandle);
                    }
                    *(Task_List[i].status_ptr) = Task_P_Stop;
                    break;

                case Task_T_Resume:
                    if (*pHandle != NULL) {
                        vTaskResume(*pHandle);
                    }
                    *(Task_List[i].status_ptr) = Task_P_Running;
                    break;

                case Task_T_Delete:
                    if (*pHandle != NULL) {
                        vTaskDelete(*pHandle);
                        *pHandle = NULL; 
                    }
                    *(Task_List[i].status_ptr) = Task_P_Null;
                    break;

                default:
                    break;
            }
        }
    }
}
