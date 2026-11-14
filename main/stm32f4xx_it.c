/**
  ******************************************************************************
  * @file    Project/STM32F4xx_StdPeriph_Templates/stm32f4xx_it.c 
  * @author  MCD Application Team
  * @version V1.8.1
  * @date    27-January-2022
  * @brief   Main Interrupt Service Routines.
  *          This file provides template for all exceptions handler and 
  *          peripherals interrupt service routine.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2016 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_it.h"

#define Use_FreeRTOS 1

/** @addtogroup Template_Project
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/******************************************************************************/
/*            Cortex-M4 Processor Exceptions Handlers                         */
/******************************************************************************/

/**
  * @brief  This function handles NMI exception.
  * @param  None
  * @retval None
  */
void NMI_Handler(void)
{
}

/* ============================ 故障现场采集 ============================
 * 四个 Fault 处理必须留下现场: 若只是 while(1), 重启后无从判断故障点;
 * 表现就是黑屏但不断电, 完全无法定位。改为:
 *   1) 汇编入口取出故障栈帧指针(自动区分 MSP/PSP), 同时拿到 EXC_RETURN;
 *   2) 把 PC/LR/xPSR/CFSR/HFSR 写进 RTC 备份寄存器(软复位不清零);
 *   3) 立即软复位; 下次启动由 Start_Task 读回并写进 TSDB 日志。
 * 备份寄存器分配(DR0 已被 rtc_clock.c 用作时钟有效标志, 不复用):
 *   DR1=魔数  DR2=PC  DR3=LR  DR4=xPSR  DR5=CFSR  DR6=HFSR  DR7=故障类型
 * ==================================================================== */
// FAULT_MAGIC / FAULT_REASON_* 定义在 stm32f4xx_it.h, main.c 也要用

volatile uint32_t g_fault_sp   = 0;   // 故障时刻栈指针, 便于调试器直接查看
volatile uint32_t g_fault_exc  = 0;   // 故障时刻 EXC_RETURN

void Fault_Report(uint32_t *sp, uint32_t lr, uint32_t reason);

__asm void Fault_Handler_Entry(void)
{
    IMPORT  Fault_Report
    TST     LR, #4
    ITE     EQ
    MRSEQ   R0, MSP
    MRSNE   R0, PSP
    MOV     R1, LR
    MOV     R2, R3
    B       Fault_Report
}

__asm void HardFault_Handler(void)
{
    MOV     R3, #0
    B       Fault_Handler_Entry
}

__asm void MemManage_Handler(void)
{
    MOV     R3, #1
    B       Fault_Handler_Entry
}

__asm void BusFault_Handler(void)
{
    MOV     R3, #2
    B       Fault_Handler_Entry
}

__asm void UsageFault_Handler(void)
{
    MOV     R3, #3
    B       Fault_Handler_Entry
}

/* 记录现场并复位。sp 指向异常入栈帧: [0]=R0 [1]=R1 [2]=R2 [3]=R3
 * [4]=R12 [5]=LR [6]=PC [7]=xPSR */
void Fault_Report(uint32_t *sp, uint32_t lr, uint32_t reason)
{
    uint32_t cfsr = SCB->CFSR;      // 读 CFSR 会顺带锁存 MMFAR/BFAR
    uint32_t hfsr = SCB->HFSR;

    g_fault_sp  = (uint32_t)sp;
    g_fault_exc = lr;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    PWR_BackupAccessCmd(ENABLE);

    RTC_WriteBackupRegister(RTC_BKP_DR2, sp ? sp[6] : 0u);   // PC
    RTC_WriteBackupRegister(RTC_BKP_DR3, sp ? sp[5] : 0u);   // LR
    RTC_WriteBackupRegister(RTC_BKP_DR4, sp ? sp[7] : 0u);   // xPSR
    RTC_WriteBackupRegister(RTC_BKP_DR5, cfsr);
    RTC_WriteBackupRegister(RTC_BKP_DR6, hfsr);
    RTC_WriteBackupRegister(RTC_BKP_DR7, reason);
    RTC_WriteBackupRegister(RTC_BKP_DR1, FAULT_MAGIC);

    NVIC_SystemReset();
    while (1) { }                   // 复位未生效时兜底, 等看门狗
}

/* LVGL 断言钩子 (lv_conf.h 的 LV_ASSERT_HANDLER 调用)。
 * LV_USE_LOG=0 时 LVGL 自己什么都不打, 这里只把行号留下来, 然后复位;
 * 现场由 Start_Task 的 Fault_Log_Pending 统一上报, 避免在断言点做重活。 */
void lvgl_assert_hook(const char *file, uint32_t line)
{
    (void)file;                     // 文件名是常量字符串, 断电后地址无意义, 只留行号

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    PWR_BackupAccessCmd(ENABLE);

    RTC_WriteBackupRegister(RTC_BKP_DR2, line);
    RTC_WriteBackupRegister(RTC_BKP_DR3, 0);
    RTC_WriteBackupRegister(RTC_BKP_DR4, 0);
    RTC_WriteBackupRegister(RTC_BKP_DR5, 0);
    RTC_WriteBackupRegister(RTC_BKP_DR6, 0);
    RTC_WriteBackupRegister(RTC_BKP_DR7, FAULT_REASON_LVGL);
    RTC_WriteBackupRegister(RTC_BKP_DR1, FAULT_MAGIC);

    NVIC_SystemReset();
    while (1) { }
}

/**
  * @brief  This function handles SVCall exception.
  * @param  None
  * @retval None
  */
#if (!Use_FreeRTOS)
void SVC_Handler(void)
{
}
#endif

/**
  * @brief  This function handles Debug Monitor exception.
  * @param  None
  * @retval None
  */
void DebugMon_Handler(void)
{
}

/**
  * @brief  This function handles PendSVC exception.
  * @param  None
  * @retval None
  */
#if (!Use_FreeRTOS)
void PendSV_Handler(void)
{
}
#endif

/**
  * @brief  This function handles SysTick Handler.
  * @param  None
  * @retval None
  */
#if (!Use_FreeRTOS)
void SysTick_Handler(void)
{
  //TimingDelay_Decrement();
}
#endif

/******************************************************************************/
/*                 STM32F4xx Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32f4xx.s).                                               */
/******************************************************************************/

/**
  * @brief  This function handles PPP interrupt request.
  * @param  None
  * @retval None
  */
/*void PPP_IRQHandler(void)
{
}*/

/**
  * @}
  */ 


