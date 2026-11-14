#ifndef __SYSTICK_CONF_H__
#define __SYSTICK_CONF_H__
#include "stm32f4xx.h"                  // Device header

void Systick_init(void);
void Delay_us(uint16_t us);
void Delay_ms(uint32_t ms);   // 必须是 32 位, 否则 ms*1000 会按 65536 取模

#endif
