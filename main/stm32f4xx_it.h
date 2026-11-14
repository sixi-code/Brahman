/**
  ******************************************************************************
  * @file    Project/STM32F4xx_StdPeriph_Templates/stm32f4xx_it.h 
  * @author  MCD Application Team
  * @version V1.8.1
  * @date    27-January-2022
  * @brief   This file contains the headers of the interrupt handlers.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __STM32F4xx_IT_H
#define __STM32F4xx_IT_H

#ifdef __cplusplus
 extern "C" {
#endif 

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */

void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);

/* 故障现场采集 ------------------------------------------------------------
 * Fault_Report 把 PC/LR/xPSR/CFSR/HFSR 写进 RTC 备份寄存器后软复位;
 * Start_Task 下次启动时读到 FAULT_MAGIC 就上报一条日志并清标志。
 * 备份寄存器: DR0=时钟有效(rtc_clock.c 占用) DR1=魔数 DR2=PC DR3=LR
 *             DR4=xPSR DR5=CFSR DR6=HFSR DR7=故障类型 */
#define FAULT_MAGIC        0x5AA5F017UL
#define FAULT_REASON_HARD  0u
#define FAULT_REASON_MM    1u
#define FAULT_REASON_BUS   2u
#define FAULT_REASON_USAGE 3u
#define FAULT_REASON_LVGL  4u   /* LVGL 断言: DR2=行号 */
#define LVGL_ASSERT_NO_LINE  0u

void Fault_Report(uint32_t *sp, uint32_t lr, uint32_t reason);
void SVC_Handler(void);
void DebugMon_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __STM32F4xx_IT_H */

