#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "music.h"
#include "systick_conf.h"
#include "variables.h"
#include "defines.h"
#include "es9018k2m.h"
#include "pin_ctrl.h"
#include "task_manager.h"
#include "page_manager.h"

void Music_Task( void * pvParameters )
{
	while(1)
	{
		/* 消费外部(GUI)请求。
		 * 解码器状态机里有大量 "if(Music_Status==Song_Next){... Music_Status=Song_Prepare;}"
		 * 这类非原子判断, GUI 直接写 Music_Status 会被下一次写入覆盖:
		 *  - 连点两次"下一首" → 后一次丢失("点了没反应");
		 *  - 点"退出" → Song_Error 被改回 Music_Exit, 回桌面后还在放歌。
		 * 这里在任务上下文的循环顶部一次性提交, 与解码器互不交错。 */
		if (g_music_req != 0)
		{
			uint8_t req;
			taskENTER_CRITICAL();
			req = g_music_req;
			g_music_req = 0;
			taskEXIT_CRITICAL();

			/* 只在"允许改变播放状态"的阶段落地: Song_Error/退出请求任何阶段都要生效,
			 * 切歌请求则在 None/Init 阶段没有意义(此时还没开始播)。 */
			if (req == Song_Error || req == Music_Exit || Music_Status != Music_None)
			{
				Music_Status = req;
			}
		}

		if(Music_Status == Music_None)
		{
			// 双重判断，确保当前和目标页面都是音乐页时才触发自动播放
			if(Page_Get_Current() == PAGE_MUSIC && Page_Get_Next() == PAGE_MUSIC)
			{
				Music_Status = Music_Init;
			}
			else
			{
				Delay_ms(20);
			}
		}
		else if(Music_Status == Music_Init)
		{
			if(audio_play_prepare()) Music_Status = Music_Exit;
			else Music_Status = Song_Prepare;
		}
		else if(Music_Status == Music_Exit)
		{
			audio_play_clear();
			Music_Status = Music_None;
		}
		else
		{
			static uint32_t last_page = PAGE_NONE;
			static uint32_t current_page = PAGE_NONE;
            
			uint32_t temp_page = Page_Get_Current();
			
			if(temp_page != current_page) 
			{
				last_page = current_page;
				current_page = temp_page;
			}
			
			//从桌面进入到不是音乐的页面
			if(current_page != PAGE_MUSIC && last_page == PAGE_DESKTOP)
			{
				Music_Status = Music_Exit;
			}

			//从音乐进入文件页面
			if(current_page == PAGE_FILE && last_page == PAGE_MUSIC) 
			{
				if(g_file_chosen)
				{
					Music_Status = Song_File;
					Page_Back();
					g_file_chosen = 0;
				}
			}
			
			/* 进度条 seek 请求在这里执行 —— 只有本任务拥有音乐文件句柄。
			 * 不能在 UI 回调里直接 seek: 会与这里的 f_read 并发, 还阻塞 LVGL 40ms。
			 * Music_Suspend_Flag 保持原语义: 置 1 后本任务会跳过本轮读盘,
			 * 给底层 f_read 一个退出的机会再改文件指针。 */
			if (g_music_seek_pending)
			{
				uint32_t target = g_music_seek_req;
				uint8_t old_suspend = Music_Suspend_Flag;
				g_music_seek_pending = 0;

				Music_Suspend_Flag = 1;
				Delay_ms(40);          // 在本任务里阻塞没有副作用
				audio_seek(target);
				Music_Suspend_Flag = old_suspend;
			}

			audio_play_task();
			/* 本任务优先级高于 LVGL/USB/BASIC, 而 audio_play_task()
			 * 里的 f_read 是阻塞 IO(SD 卡一次 5KB 可能 5-20ms)。解码器等 DMA 时
			 * 虽然会阻塞, 但在"消费完信号量立刻补读"的窗口里不会让出 CPU,
			 * 表现为界面掉帧、USB 音频断流。主动让出一次, 让低优先级任务有机会跑。 */
			taskYIELD();
		}
	}
}
