#include "stm32f4xx.h"

#ifndef __FILE_UNIT_H__
#define __FILE_UNIT_H__

void Create_File_Unit(void);

void Update_File_Unit(void);

void Remove_File_Unit(void);

void chosen_file_path_free(void);

// 底层文件操作引擎 (供后台任务调用)
int do_copy(const char* src, const char* dst);
int do_delete(const char* path);

// 后台操作收尾时回收进度结构体 (FileOp_Task 在操作结束后调用)
void file_op_progress_reclaim(void);

/* ===== 后台文件操作的提交/完成接口 =====
 * 命令、两个路径指针与 busy 标志必须一起生效, 所以不直接读写全局变量,
 * 一律经由下面的接口(内部在临界区内完成整组字段的读写)。
 *   FileOp_Submit  提交一条命令; 成功返回 1, 此时 src/dst 的所有权交给后台任务
 *   FileOp_Is_Busy 查询是否有操作在进行
 *   FileOp_Get_Cmd 查询当前命令号(用于界面文案)
 *   FileOp_Take_Result 取一次结果; 返回 1 表示拿到新结果并已清完成标志 */
uint8_t FileOp_Submit(uint8_t cmd, char *src, char *dst);
uint8_t FileOp_Is_Busy(void);
uint8_t FileOp_Get_Cmd(void);
uint8_t FileOp_Take_Result(uint8_t *res);

#endif
