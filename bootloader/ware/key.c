#include "stm32f4xx.h"
#include "systick_conf.h"   // Delay_ms (去抖需要)

void Wakeup_Key_Init(void) 
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE); // 使能GPIOA时钟

    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN;        // 输入模式
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_DOWN;      // 下拉
    GPIO_Init(GPIOA, &GPIO_InitStruct);
}

//按下1 不按0
uint8_t Read_Wakeup_Key(void)
{
	return GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0);
}

/* 上电去抖: 连续 50ms 内 90% 采样点为高才认定"要进 UF2"。
 * 单次采样会被弹片抖动干扰 —— 既可能漏按导致直接跳 APP, 也可能被毛刺误入。 */
uint8_t Read_Wakeup_Key_Debounced(void)
{
	uint8_t cnt = 0;

	for (uint8_t i = 0; i < 50; i++) {
		if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0)) {
			cnt++;
		} else {
			cnt = 0;            // 出现低电平就重新计
		}
		if (cnt >= 45) return 1; // 连续 45/50 为高, 认定"要进 UF2"
		Delay_ms(1);
	}
	return 0;
}

