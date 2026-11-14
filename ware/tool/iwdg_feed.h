#ifndef __IWDG_FEED_H__
#define __IWDG_FEED_H__

#include "stm32f4xx.h"

/* 长循环里的喂狗封装。
 *
 * 背景: Basic_Task 已启用独立看门狗(约 3.7s)。凡是在单个任务里连续运行
 * 超过 1~2 秒的循环(字库擦写、大目录复制/删除、批量导出)都必须周期性调用
 * 下面的宏, 否则会在操作中途被复位, 反而可能把文件系统写坏。
 *
 * 用法: 在循环体内按"每次迭代都调"或"每 N 次调一次"均可, 开销约 1us。 */

#define IWDG_FEED_SAFE()   do { IWDG_ReloadCounter(); } while (0)

#endif
