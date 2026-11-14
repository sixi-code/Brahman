#include <stm32f4xx.h>

void Nvic_Init(void)
{
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

	/* 先把所有中断的优先级都压到最低(0xFF)。
	 * 复位值是 0 —— 也就是"最高优先级, 高于 configMAX_SYSCALL_INTERRUPT_PRIORITY"。
	 * 本工程在别处(EXTI/USART/I2C 等)有直接调 NVIC_EnableIRQ 却不设优先级的地方,
	 * 那些中断一旦在里面调用任何 FreeRTOS 的 FromISR API, 会命中
	 * configASSERT(ucCurrentPriority >= ucMaxSysCallPriority) 并把中断永久关掉;
	 * 即使不调 API, 优先级 0 也能抢占 SysTick, 让调度节拍漂移。
	 * 下面会逐个把用到的通道设成实际需要的优先级。 */
	for (uint32_t i = 0; i < (sizeof(NVIC->IP) / sizeof(NVIC->IP[0])); i++) {
		NVIC->IP[i] = 0xFF;
	}

	//系统滴答定时器中断优先级为6
	NVIC_SetPriority(SysTick_IRQn, 6);

	NVIC_InitTypeDef NVIC_InitStructure;

	//配置RTOS系统定时器中断
    NVIC_InitStructure.NVIC_IRQChannel = TIM1_BRK_TIM9_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 15;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
	
	//配置ADC DMA传输完成中断
	NVIC_InitStructure.NVIC_IRQChannel = DMA2_Stream0_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 14;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	//配置LCD SPI DMA中断
    NVIC_InitStructure.NVIC_IRQChannel = DMA2_Stream5_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 13;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
	
	//配置Flash DMA1 Stream0中断（SPI3_RX）
    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Stream0_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 12;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    //配置Flash DMA1 Stream5中断（SPI3_TX）
    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Stream5_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 12;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
	
	//配置SD卡DMA中断
	NVIC_InitStructure.NVIC_IRQChannel = DMA2_Stream3_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 11;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;	
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;		
	NVIC_Init(&NVIC_InitStructure);
	
	NVIC_InitStructure.NVIC_IRQChannel = SDIO_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 10;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;	
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;		
	NVIC_Init(&NVIC_InitStructure);
	
	//配置USB OTG中断
	NVIC_InitStructure.NVIC_IRQChannel = OTG_HS_IRQn; 
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 9;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
	
	//配置music i2s中断
	NVIC_InitStructure.NVIC_IRQChannel = DMA1_Stream4_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 8;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}
