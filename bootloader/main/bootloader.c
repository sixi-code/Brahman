#include "stm32f4xx.h" 
#include "pin_ctrl.h"
#include "key.h"
#include "nvic_conf.h"
#include "usbd_msc_conf.h"
#include "bootuf2.h"
#include "bootuf2_config.h"
#include "boot_marker.h"
#include "systick_conf.h"
#include "lcd_bsp.h"
#include "malloc.h"
#include "FreeRTOS.h"
#include "task.h"

#define APP_ADDRESS CONFIG_BOOTUF2_APP_START   // 与 UF2/Flash 写入校验共用
typedef void (*pFunction)(void);

/* 跳转前三道校验: 栈顶在 SRAM、复位向量在 APP 区、复位向量是 Thumb 地址。
 * 半擦除的镜像常出现栈顶合法但复位向量为 0xFFFFFFFF 或奇数, 只查栈顶会跳进
 * 无效地址, 表现为 HardFault 死循环或跑半截固件。 */
void Jump_To_Main(void)
{
    uint32_t app_sp = *(__IO uint32_t *)APP_ADDRESS;
    uint32_t app_pc = *(__IO uint32_t *)(APP_ADDRESS + 4);

    uint8_t sp_ok = ((app_sp & 0x2FFE0000) == 0x20000000);                 // SRAM 0x20000000~0x2001FFFF
    uint8_t pc_ok = (app_pc >= APP_ADDRESS) && (app_pc < 0x08100000);      // 必须在 APP 区且为 1MB Flash 内
    uint8_t thumb_ok = ((app_pc & 1u) != 0);                               // Cortex-M 的向量必须是 Thumb(bit0=1)

    if (sp_ok && pc_ok && thumb_ok)
    {
        /* 跳转前清干净 Bootloader 用过的中断与外设状态:
         * OTG_HS 的 DMA 仍在跑、NVIC 还有 pending 位、SysTick 仍在计数,
         * APP 若不立刻重置这些模块, 会出现 "USB 不认设备 / 一进 main 就进中断"。 */
        __disable_irq();

        SysTick->CTRL = 0;      // 停 SysTick
        SysTick->LOAD = 0;
        SysTick->VAL  = 0;

        /* 关掉所有外设中断并使 pending 位归零(0~95 覆盖 STM32F4 的全部 IRQ) */
        for (uint32_t i = 0; i < 8; i++) {
            NVIC->ICER[i] = 0xFFFFFFFFu;
            NVIC->ICPR[i] = 0xFFFFFFFFu;
        }

        /* 复位 USB OTG HS 控制器与它的 DMA, 避免 APP 接管时外设处于半工作态 */
        RCC->AHB1RSTR |=  RCC_AHB1RSTR_OTGHRST;
        RCC->AHB1RSTR &= ~RCC_AHB1RSTR_OTGHRST;

        /* 把 Bootloader 打开过的 GPIO 拉到复位态, 防止 APP 初始化前的短暂误动作 */
        uint32_t gpio_rst = RCC_AHB1RSTR_GPIOARST | RCC_AHB1RSTR_GPIOBRST | RCC_AHB1RSTR_GPIOCRST
                          | RCC_AHB1RSTR_GPIODRST | RCC_AHB1RSTR_GPIOERST | RCC_AHB1RSTR_GPIOHRST;
        RCC->AHB1RSTR |=  gpio_rst;
        RCC->AHB1RSTR &= ~gpio_rst;

        /* 镜像校验通过: 清掉"升级中"标记, 之后再掉电也不会被误判为半截固件 */
        Boot_Marker_Set(BOOT_MARK_NONE);

        __set_MSP(app_sp);
        pFunction JumpToApplication = (pFunction)app_pc;
        JumpToApplication();
    }
	else
	{
		/* 镜像无效: 留在 Bootloader 等主机重新拖入 UF2 */
		while(1) { }
	}
}

TaskHandle_t    Task_handler;
void Task( void * pvParameters );

int main(void)
{
	Nvic_Init();    //NVIC
	Systick_init(); //systick
	tlsf_init();    //mem
    Pin_Ctrl_Init();  //pin
    Wakeup_Key_Init();//wakeup

	Boot_Marker_Init();

	/* 上一次升级没写完(标记仍是 UPGRADING)时, APP 区可能是半截固件。
	 * 这时不跳转, 直接进 UF2 等待重新拖入, 否则用户看到的是"变砖"。
	 * 标记为 DONE/NONE 表示镜像完整或从未升级过, 正常跳转。 */
	/* 三个进入 UF2 的条件: 上次升级未完成 / 上电按住按键 / (否则) 跳 APP */
	if ((Boot_Marker_Get() == BOOT_MARK_UPGRADING) || Read_Wakeup_Key_Debounced())
	{
		msc_bootuf2_init(0, 0x40040000UL);
		xTaskCreate(Task,"Task",512,NULL,3,&Task_handler);
		vTaskStartScheduler();
    }
	else
	{
		Jump_To_Main();
	}
}

void Task( void * pvParameters )
{
	LCD_Init();
	for(uint8_t i=0;i<240;i+=20)
	{
		Display_None(i);
	}
	Display_Icon(100);
	while(1)
	{
		uint32_t total = bootuf2_get_total_blocks();
		uint32_t written = bootuf2_get_written_blocks();
		if (total == 0 || total == 0xFFFFFFFF) 
			Progress_Bar(130, 0);
		else
			Progress_Bar(130, written*140/total);
		
		if (bootuf2_is_write_done()) 
			NVIC_SystemReset();
	}
}
