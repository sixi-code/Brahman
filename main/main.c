#include "main.h"
#include "stm32f4xx_it.h"   // FAULT_MAGIC / Fault_Report 声明
#include "tsdb_log.h"        // tsdb_printf (故障现场上报)

uint8_t init_res = 0;
int main(void)
{	//！！！
	/* 向量表偏移已经在 SystemInit() 里设好
	 * (start/system_stm32f4xx.c 的 VECT_TAB_OFFSET = 0x00010000),
	 * 这里再写一次只作冗余保险: 真正的设置在 SystemInit() 里,
	 * 避免复位到本行之间 VTOR 仍指向 Bootloader 的向量表。 */
	SCB->VTOR = 0x08010000; // 重定向中断向量表到 APP 起始地址
	__enable_irq();// 重新开启全局中断 (因为 Bootloader 跳转前把它关了)
	//不要手贱删除

	Nvic_Init();    //NVIC
	Systick_init(); //systick
	init_res = tlsf_init(); //mem
	
	if(init_res) while(1);
	xTaskCreate((TaskFunction_t		    )   Start_Task,
				(char *                 )   "Start_Task",
				(configSTACK_DEPTH_TYPE )   START_TASK_STACK_SIZE,
				(void *                 )   NULL,
				(UBaseType_t            )   START_TASK_PRIO,
				(TaskHandle_t *         )   &Start_Task_handler );
	vTaskStartScheduler();
				
	while(1){
	}
}	
		
/* ===== FreeRTOS 钩子 =====
 * configCHECK_FOR_STACK_OVERFLOW=2 与 configUSE_MALLOC_FAILED_HOOK=1 之后必须实现这两个,
 * 否则链接报 L6218E。栈溢出/内存不足都属于"已经出错"的状态, 统一走 Fault_Report
 * 留下现场再复位, 比原地卡死更利于定位。 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;               // 任务名是 RAM 里的字符串, 复位后无意义, 不进备份寄存器
    Fault_Report(NULL, 0, FAULT_REASON_HARD);   // sp 为 NULL 时只记 CFSR/HFSR 与原因
}

void vApplicationMallocFailedHook(void)
{
    Fault_Report(NULL, 0, FAULT_REASON_HARD);
}

/* 读回上次复位前由 Fault_Report 写入的现场, 记一条日志并清魔数。
 * 必须在 fdb_tsdb_init 之后调用, 否则 tsdb_printf 直接返回。 */
static void Fault_Log_Pending(void)
{
    static const char *const reason_name[] = { "HardFault", "MemManage", "BusFault", "UsageFault", "LVGL_ASSERT" };

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    PWR_BackupAccessCmd(ENABLE);

    if (RTC_ReadBackupRegister(RTC_BKP_DR1) != FAULT_MAGIC) return;

    uint32_t pc = RTC_ReadBackupRegister(RTC_BKP_DR2);
    uint32_t lr = RTC_ReadBackupRegister(RTC_BKP_DR3);
    uint32_t xp = RTC_ReadBackupRegister(RTC_BKP_DR4);
    uint32_t cf = RTC_ReadBackupRegister(RTC_BKP_DR5);
    uint32_t hf = RTC_ReadBackupRegister(RTC_BKP_DR6);
    uint32_t rn = RTC_ReadBackupRegister(RTC_BKP_DR7);

    if (rn == FAULT_REASON_LVGL) {
        tsdb_printf("[FAULT] LVGL_ASSERT at lv_conf/user code line %u", (unsigned)pc);
    } else {
        tsdb_printf("[FAULT] %s PC=%08X LR=%08X PSR=%08X CFSR=%08X HFSR=%08X",
                    (rn < (sizeof(reason_name) / sizeof(reason_name[0]))) ? reason_name[rn] : "?",
                    (unsigned)pc, (unsigned)lr, (unsigned)xp, (unsigned)cf, (unsigned)hf);
    }

    RTC_WriteBackupRegister(RTC_BKP_DR1, 0);   // 只上报一次
}

void Start_Task( void * pvParameters )
{
	//此处只初始化上电后不再deinit的硬件
	Wakeup_Key_Init();
	Joystick_Middle_Init();
	Pin_Ctrl_Init();
	RNG_Init();
	ADC1_DMA_Init();

	/* 初始化结果逐项判定, 只保留"第一次失败"的编号, 便于现场定位。
	 * 共用一个变量反复赋值时只有最后一次会被检查, 于是:
	 *  - Flash 初始化失败后仍会在失效器件上初始化 KVDB, 读回全 0xFF;
	 *  - RTC 起振失败被后续返回值覆盖, 之后所有时间戳都错;
	 *  - font_init() 的返回值会把前面所有错误吞掉。 */
	init_res = 0;

	if (RTC_Clock_Init() && !init_res) init_res = 1;          // 1=RTC/LSE

	//w25q128
	if (W25QXX_Init() && !init_res) init_res = 2;             // 2=SPI Flash

	// FlashDB KVDB Init
    fdb_kvdb_control(&kvdb, FDB_KVDB_CTRL_SET_LOCK, (void *)fdb_lock);//设置锁函数
    fdb_kvdb_control(&kvdb, FDB_KVDB_CTRL_SET_UNLOCK, (void *)fdb_unlock);//设置解锁函数
    if (fdb_kvdb_init(&kvdb, "env", "fdb_kvdb1", NULL, NULL) != FDB_NO_ERR && !init_res) init_res = 3;  // 3=KVDB
    kvdb_persist_load();//加载持久化数据
    I2S_Exchange_Ctrl(kv_hdp0_or_spk1);//设置I2S输出模式(从FlashDB读取)
    ES9018_Set_Config((const ES9018_Config_t *)&kv_es9018_cfg);//设置ES9018配置(从FlashDB读取)

    // FlashDB TSDB Init
    fdb_tsdb_control(&tsdb, FDB_TSDB_CTRL_SET_LOCK, (void *)fdb_lock);//设置锁函数
    fdb_tsdb_control(&tsdb, FDB_TSDB_CTRL_SET_UNLOCK, (void *)fdb_unlock);//设置解锁函数
    if (fdb_tsdb_init(&tsdb, "log", "fdb_tsdb1", get_fdb_time, 128, NULL) != FDB_NO_ERR && !init_res) init_res = 4;  // 4=TSDB
	
	//font
	g_font_need_update = font_init();//初始化字体,返回是否需要更新字体

	// 上一轮若发生过 HardFault/断言复位, 这里把现场写进日志(必须在 tsdb 就绪之后)
	Fault_Log_Pending();

	/* 失败时先把错误号记进日志再停住 —— 否则现场只看到"卡死/重启"看不出是哪一步。 */
	if (init_res) {
		tsdb_printf("[INIT] failed at step %u (1=RTC 2=FLASH 3=KVDB 4=TSDB)", (unsigned)init_res);
		while(1) { }   // 由 Basic_Task 的看门狗超时复位
	}
		
	// 信号量必须先于 Task_Manager 创建: Taskmanager_Ctrl 里会 Give 它,
	// 若句柄为 NULL 会命中 configASSERT 并关中断死循环
	xTaskManagerSemaphore = xSemaphoreCreateBinary();

	//creat task manager
    if (xTaskCreate(Task_Manager,"Task_Manager",TASK_MANAGER_STACK_SIZE,NULL,TASK_MANAGER_PRIO,&Task_Manager_handler ) != pdPASS
        || xTaskManagerSemaphore == NULL)
    {
        // 任务管理器起不来 = 后续所有任务都不会被创建, 直接停机等看门狗
        while(1) { }
    }

	Taskmanager_Ctrl(Task_N_Basic, Task_T_Creat, 0);    //basic task
	Taskmanager_Ctrl(Task_N_LVGL, Task_T_Creat, 0);     //LVGL task
	Taskmanager_Ctrl(Task_N_Music, Task_T_Creat, 0);     //MUSIC task
	Taskmanager_Ctrl(Task_N_USB, Task_T_Creat, 0);      //usb task
	Taskmanager_Ctrl(Task_N_Font, Task_T_Creat, 0);     //font task
	Taskmanager_Ctrl(Task_N_FileOp, Task_T_Creat, 0);   //file operation worker

	vTaskDelete(NULL);   //参数为NULL时，表示删除任务自身
}
