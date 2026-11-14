#include "stm32f4xx.h"                  // Device header

#ifndef __KEY_H__
#define __KEY_H__

void Wakeup_Key_Init(void);
uint8_t Read_Wakeup_Key(void);
uint8_t Read_Wakeup_Key_Debounced(void);   // 上电去抖版: 连续 45/50ms 为高才认

#endif
