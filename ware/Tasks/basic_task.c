#include "FreeRTOS.h"
#include "task.h"
#include "pin_ctrl.h"
#include "key.h"
#include "adc.h"
#include "es9018k2m.h"
#include "max98357A.h"
#include "rtc_clock.h"
#include "v2p_bat.h"
#include "main.h"
#include "lcd_pwm.h"
#include "sdio_sdcard.h"
#include "iwdg_feed.h"
#include "tsdb_log.h"
#include "kvdb_ctrl.h"

//此task完成以下轮询读取和处理

//充电检测
//耳机插入检测
//TF卡插入检测

//USB插入检测
//按键读取处理
//摇杆读取处理

//es9018k2m gpio状态
//es9018k2m 更新到芯片
//音乐频谱计算

static uint8_t last_music_bitdepth = 0;// 记录上一次的音乐位深度
static uint8_t last_hdp_value = 0;// 记录上一次的耳机音量值
static uint8_t last_brightness = 0;// 记录上一次的屏幕亮度值

void Basic_Task( void * pvParameters )
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    static uint8_t count_tick = 0;// 20ms 节拍计数器(5 拍=100ms, 25 拍=500ms)
	
    IWTG_Init();   // 独立看门狗: LSI 32kHz/128 分频, 重装 938 -> 约 3.7s 超时
                   // 必须放在调度器已启动的任务里, 才能在死循环时真正复位
                   // 注意: 长时间阻塞的循环(字库擦写/文件复制)必须自行喂狗

    while(1) 
    {
		IWDG_Feed();
		
		//adc读取
		//电量计算
        if(g_adc_dma_finished) 
        {
            g_adc_dma_finished = 0;
            ADC_Calculate_Voltage();// 电池电压计算
            VoltageToPercent();// 电池电量百分比计算
        }
		ADC_StartConversion();// 开始下一次ADC转换
		
		//引脚读取
        Is_Battery_Charging();// 充电状态检测
        Is_Headphone_Connecting();// 耳机插入状态检测
        Is_TFcard_Connecting(); // TF卡插入状态检测
        Read_Wakeup_Key();// 唤醒按键读取
        Get_Joystick_Middle();// 摇杆中间位置读取
		
		//tf卡插入卸载
		if(!g_TFcard_inited && g_TFcard_status)
		{
			/* 热插拔处理必须与在途传输互斥: MUSIC/FILEOP 可能正持有 sdcard_lock
			   做 DMA 读写, 此时 SD_Init 会复位 SDIO 外设并重配 DMA 流, 直接竞踩。
			   diskio.c 的重试路径用的就是这对接口。 */
			uint8_t locked = sdcard_lock_external();
			uint8_t ok = (!SD_Init());
			if (ok) {
				ok = (fatfs_mount(DEV_SD) == 0);   // 挂载失败(坏卡/BPB 读不出)不能当卡可用
				if (!ok) SD_Deinit();
			} else {
				SD_Deinit();
			}
			sdcard_unlock_external(locked);
			if (!ok) vTaskDelay(pdMS_TO_TICKS(500));   // 失败别在全速循环里连轴重试
		}
		if(g_TFcard_inited && !g_TFcard_status)// TF卡拔出则卸载文件系统
		{
			/* 顺序很重要: 先摘卷(此后不会再有新的 f_* 打开文件), 再关硬件。
			   若先 SD_Deinit 后 fatfs_unmount, 窗口期内仍有任务在按 fs 指针读写,
			   而 SD_Deinit 已经把外设关了 → 介质消失 + 野指针。 */
			uint8_t locked = sdcard_lock_external();
			fatfs_unmount(DEV_SD);
			SD_Deinit();
			sdcard_unlock_external(locked);
		}

		//pwm ctrl
		if(!g_pwm_inited && kv_screen_status)// 如果PWM未初始化且屏幕状态为开启，则初始化PWM
		{
			LCD_TIM8_PWM_Init();// 初始化PWM
			LCD_PWM_SetFrequency(10000);// 设置PWM频率为10kHz
		}
		if(g_pwm_inited && !kv_screen_status)// 如果PWM已初始化且屏幕状态为关闭，则卸载PWM
		{
			LCD_PWM_DeInit();// 卸载PWM
		}
		
		//max98357 ctrl
		if(!g_max98357_inited && kv_max98357_ststus)// 如果MAX98357A未初始化且状态为开启，则初始化MAX98357A
		{
			MAX98357_Init();
		}
		if(g_max98357_inited && !kv_max98357_ststus)// 如果MAX98357A已初始化且状态为关闭，则卸载MAX98357A
		{
			MAX98357_Deinit();
		}
		
		//es9018 ctrl
		if(!g_es9018_inited && kv_es9018_status)// 如果ES9018未初始化且状态为开启，则初始化ES9018
		{
			if(ES9018_Init()) ES9018_Deinit();
		}
		if(g_es9018_inited && !kv_es9018_status)// 如果ES9018已初始化且状态为关闭，则卸载ES9018
		{
			ES9018_Deinit();
		}
		
		//es9018
		if(last_music_bitdepth != music_bitdepth) // 如果音乐位深度发生变化，则更新ES9018的位深度
		{
			if(!ES9018_Set_BitDepth(music_bitdepth))
				last_music_bitdepth = music_bitdepth;
		}
		if(last_hdp_value != kv_hdp_value) // 如果耳机音量值发生变化，则更新ES9018的音量
		{
			if(!ES9018_Set_Volume(kv_hdp_value,kv_hdp_value))
				last_hdp_value = kv_hdp_value;
		}
		ES9018_Update_Register();// 更新ES9018的寄存器配置，如果配置参数发生变化，则写入新的配置
		if(last_brightness != kv_brightness) // 如果屏幕亮度值发生变化，则更新PWM的占空比
		{
			LCD_PWM_SetDutyCycle(kv_brightness);
			last_brightness = kv_brightness;
		}
		
		//rtc
		static uint8_t last_M = 0;// 记录上一次的唤醒按键状态
		// 如果唤醒按键从按下状态变为未按下状态，则更新RTC时钟
		if(!g_key_WKP_RT && last_M)
		{
			//进入休眠模式 TODO
		}
		last_M = g_key_WKP_RT;
		/* 每 500ms 更新一次时钟。
		 * 原写法 if(++count100ms > 4) 实际是 20ms x 5 = 100ms, 与注释不符,
		 * 且把"节拍计数"当成"500ms 计数器"用, 后续想加 100ms 分频时没有依据。
		 * 改成 count_tick 做 20ms 节拍, 5 个节拍 = 100ms, 25 个 = 500ms。 */
		count_tick++;
		if (count_tick >= 25)
		{
			count_tick = 0;
			RTC_Clock_Update();
		}

		/* Flash 刷写从"每 20ms"降到"每 100ms"。
		 * 这两次调用都要拿 xFlashMutex, 最坏会触发扇区擦除(几十~数百 ms);
		 * 20ms 一次等于让 Flash 子系统长期占住, 音频/UI 任务被牵连,
		 * vTaskDelayUntil 也会因超时失去节拍意义。 */
		if ((count_tick % 5) == 0)
		{
			/* 两次 flush 最坏会连续触发 FlashDB 的 GC/扇区合并, 多个扇区擦除
			   累计可超过 IWDG 的 3.7s 周期 —— 复位发生在 KVDB 写入中途会损坏
			   记录。flush 前喂一次狗, 把本周期的时间预算重新开始计。 */
			IWDG_FEED_SAFE();
			tsdb_log_flush();// 将日志缓冲区中的数据写入存储设备
			kvdb_persist_flush();// 将所有已修改的持久化条目写入kvdb
			IWDG_FEED_SAFE();
		}

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(20));
    }
}
