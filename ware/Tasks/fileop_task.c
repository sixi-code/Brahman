// 后台文件操作任务 (复制/删除)
// 通过 Task Notification 接收 UI 请求，避免阻塞 LVGL 任务
#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "defines.h"
#include "variables.h"
#include "task_manager.h"
#include "file_unit.h"
#include "malloc.h"

// 记事本写文本文件入口 (实现在 note_unit.c)
extern int do_write_text(const char * path, const char * text);

/* ===== 提交/完成接口 =====
 * cmd / src / dst / busy 是一组必须一起生效的字段: UI 先写 cmd 再写 busy 的话,
 * 后台任务可能刚好在两者之间读到"有新命令但 busy 还是 0", 于是按空闲路径处理,
 * 把命令连同两个指针一起丢掉。反过来读侧也一样。
 * 所有读写都经由下面的接口, 在临界区内一次性完成。 */

uint8_t FileOp_Submit(uint8_t cmd, char *src, char *dst)
{
    uint8_t ok = 0;

    if (FileOp_Task_handler == NULL) return 0;   // 任务还没起来

    taskENTER_CRITICAL();
    if (!g_file_op_busy) {
        g_async_src    = src;
        g_async_dst    = dst;
        g_file_op_cmd  = cmd;
        g_file_op_result = FILEOP_RES_FAIL;
        g_file_op_done = 0;
        g_file_op_busy = 1;
        ok = 1;
    }
    taskEXIT_CRITICAL();

    if (ok) xTaskNotifyGive(FileOp_Task_handler);
    return ok;
}

uint8_t FileOp_Is_Busy(void)
{
    uint8_t busy;
    taskENTER_CRITICAL();
    busy = g_file_op_busy;
    taskEXIT_CRITICAL();
    return busy;
}

uint8_t FileOp_Get_Cmd(void)
{
    uint8_t cmd;
    taskENTER_CRITICAL();
    cmd = g_file_op_cmd;
    taskEXIT_CRITICAL();
    return cmd;
}

/* 取一次操作的结果。返回 1 表示本次调用拿到了新结果, *res 有效。
 * 读完即清 done, 避免同一结果被两个页面各处理一遍。 */
uint8_t FileOp_Take_Result(uint8_t *res)
{
    uint8_t got = 0;

    taskENTER_CRITICAL();
    if (g_file_op_done && !g_file_op_busy) {
        if (res) *res = g_file_op_result;
        g_file_op_done = 0;
        got = 1;
    }
    taskEXIT_CRITICAL();

    return got;
}

void FileOp_Task(void *pvParameters)
{
    /* 等待 SD 卡初始化完成。
     * 必须有等待上限: 卡一直不插时若无限等, 这个任务会永久挂在
     * 这里, Taskmanager_Ctrl 的 Suspend/Delete 也管不到它(它不响应状态机),
     * 结果任务槽位与栈永久被占。改成最多等 60s, 之后自我注销。 */
    for (uint32_t w = 0; w < 600 && !g_TFcard_inited; w++)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (!g_TFcard_inited)
    {
        /* 自愿退出: 先把句柄与状态收回空态, 再自删。
         * 若只调 Taskmanager_Ctrl(Delete) 然后 vTaskDelete(NULL),
         * 管理器会对同一个任务再删一次, 或保留一个指向已释放 TCB 的句柄。 */
        taskENTER_CRITICAL();
        FileOp_Task_handler = NULL;
        FileOp_Task_Status  = Task_P_Null;
        taskEXIT_CRITICAL();
        vTaskDelete(NULL);
    }

    while (1)
    {
        // 阻塞等待 UI 发送任务通知
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        uint8_t res = FILEOP_RES_FAIL;
        uint8_t cmd;
        char *src = NULL;
        char *dst = NULL;

        // 命令与两个指针在临界区内一次性取走, 之后只用局部副本
        taskENTER_CRITICAL();
        cmd = g_file_op_cmd;
        src = g_async_src;
        dst = g_async_dst;
        g_async_src = NULL;
        g_async_dst = NULL;
        taskEXIT_CRITICAL();

        switch (cmd)
        {
            case 1: // 复制
                if (src && dst)
                {
                    res = (do_copy(src, dst) == 0) ? FILEOP_RES_OK : FILEOP_RES_FAIL;
                }
                break;

            case 2: // 删除
                if (src)
                {
                    res = (do_delete(src) == 0) ? FILEOP_RES_OK : FILEOP_RES_FAIL;
                }
                break;

            case 3: // 写文本文件 (src 是路径, dst 是正文)
                if (src && dst)
                {
                    res = (uint8_t)do_write_text(src, dst);
                }
                break;

            default:
                break;
        }

        // 缓冲由本任务独占, 操作结束后统一释放
        if (src) free_bsc(src);
        if (dst) free_bsc(dst);

        // 结果与"空闲"一次性提交, UI 不会看到"没结果但已空闲"的中间态
        taskENTER_CRITICAL();
        g_file_op_result = res;
        g_file_op_cmd    = 0;
        g_file_op_done   = 1;
        g_file_op_busy   = 0;
        taskEXIT_CRITICAL();

        /* 页面在操作期间被退出时, Remove_File_Unit() 不能释放 op_progress
         * (后台正在累加进度)。现在操作结束, 由这里回收, 避免内存泄漏。 */
        file_op_progress_reclaim();
    }
}
