#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "variables.h"

#define MCU_CLK_SPD_MHZ 168
extern void xPortSysTickHandler(void);
// systick中断服务函数
void SysTick_Handler(void)
{
    //判断调度器是否启动
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)
    {   
        RTOS_OK = 0;
    }
    else 
    {
        xPortSysTickHandler();
        RTOS_OK = 1;
    }
}

// 初始化系统滴答定时器
void Systick_init(void)
{
    SysTick->VAL=0;//清空计数器,防止第一次中断延时过长
    uint32_t reload;
    SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK);	/* SYSTICK使用外部时钟源,频率为HCLK */
    reload = MCU_CLK_SPD_MHZ*1000000/configTICK_RATE_HZ;/* 根据delay_ostickspersec设定溢出时间,reload为24位寄存器*/
    SysTick->LOAD = reload;                             /* 先写 LOAD: 若先开 TICKINT 再写 LOAD,
                                                           COUNTFLAG 可能已置位, 会立刻多进一次中断 */
    SysTick->VAL  = 0;
    SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;          /* 开启SYSTICK中断 */
    SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;           /* 开启SYSTICK */
}

// 微秒级延时函数
void Delay_us(uint16_t us)
{
    uint32_t ticks;
    uint32_t told, tnow, tcnt = 0;
    uint32_t reload = SysTick->LOAD;
    ticks = us * MCU_CLK_SPD_MHZ;
    told = SysTick->VAL;
    while (1)
    {
        tnow = SysTick->VAL;
        if (tnow != told)
        {
            if (tnow < told) tcnt += told - tnow;
            else tcnt += reload - tnow + told;
            told = tnow;
            if (tcnt >= ticks) break;
        }
    }
}

// 毫秒级延时函数
/* 注意: 形参必须是 uint32_t。
 * 若用 uint16_t, 实参 ms*1000 是 int, >65535 会按 65536 取模:
 * Delay_ms(120) → Delay_us(120000 & 0xFFFF = 54464) → 实际只有 54.5ms。
 * LCD 复位后要求 >=120ms 才能发 0x11, 时序不足会冷启动偶发白屏/花屏。 */
void Delay_ms(uint32_t ms)
{
    if (__get_IPSR() == 0 && RTOS_OK)
    {
        vTaskDelay(ms);
    }
    else
    {
        /* 分次累加, 既不截断也不会让 Delay_us 内部 us*MCU_CLK 溢出 */
        while (ms--) Delay_us(1000);
    }
}