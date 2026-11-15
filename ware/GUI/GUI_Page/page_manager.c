#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "page_manager.h"
#include "start_page.h"
#include "desktop_page.h"
#include "variables.h"
#include <string.h>
#include <stdarg.h>
#include "malloc.h"
#include "defines.h"
#include "debug.h"      // debug_printf: 页面 init 失败要留一条可查的记录
#include "file_page.h"
#include "album_page.h"
#include "music_page.h"
#include "debug_page.h"
#include "usb_page.h"
#include "settings_page.h"
#include "mem_monitor_page.h"
#include "note_list_page.h"
#include "note_page.h"
#include "about_page.h"
#include "time_set_page.h"
#include "key_test_page.h"
#include "log_ctrl_page.h"
#include "es9018_page.h"
#include "text_page.h"
#include "canvas_page.h"
#include "canvas_list_page.h"
#include "clock_page.h"
#include "calendar_page.h"
#include "words_page.h"
#include "calculator_page.h"
#include "lots_page.h"
#include "cmd_page.h"
#include "serial_page.h"
#include "font_update_page.h"

// 不受lvgl管理的页面
static const Page_Interface_t page_game_interface = { .id = PAGE_GAME };
static const Page_Interface_t page_media_interface = { .id = PAGE_MEDIA };
static const Page_Interface_t page_display_interface = { .id = PAGE_DISPLAY };


// 1. 静态映射表：直接将页面接口指针按 ID 顺序放入数组
static const Page_Interface_t* const page_registry[PAGE_MAX_ID] = {
    [PAGE_NONE]       = NULL,
    [PAGE_START]      = &page_start_interface,
    [PAGE_DESKTOP]    = &page_desktop_interface,
    [PAGE_FILE]       = &page_file_interface,
    [PAGE_FONT_UPDATE]= &page_font_update_interface,
    [PAGE_GAME]       = &page_game_interface,
    [PAGE_MEDIA]      = &page_media_interface,
    [PAGE_DISPLAY]    = &page_display_interface,
    [PAGE_MUSIC]      = &page_music_interface,
    [PAGE_DEBUG]      = &page_debug_interface,
    [PAGE_USB_CTRL]   = &page_usb_interface,
    [PAGE_ALBUM]      = &page_album_interface,
    [PAGE_SETTINGS]   = &page_settings_interface,
    [PAGE_MEM]        = &page_mem_interface,
    [PAGE_NOTE]       = &page_note_interface,
    [PAGE_NOTE_LIST]  = &page_note_list_interface,
    [PAGE_ABOUT]      = &page_about_interface,
    [PAGE_TIME_SET]   = &page_time_set_interface,
    [PAGE_KEY_TEST]   = &page_key_test_interface,
    [PAGE_LOG_CTRL]   = &page_log_ctrl_interface,
    [PAGE_ES9018]     = &page_es9018_interface,
    [PAGE_TEXT]       = &page_text_interface,
    [PAGE_CANVAS]     = &page_canvas_interface,
    [PAGE_CANVAS_LIST]= &page_canvas_list_interface,
    [PAGE_CLOCK]      = &page_clock_interface,
    [PAGE_CALENDAR]   = &page_calendar_interface,
    [PAGE_WORDS]      = &page_words_interface,
    [PAGE_CALCULATOR] = &page_calculator_interface,
    [PAGE_LOTS]       = &page_lots_interface,
    [PAGE_CMD]        = &page_cmd_interface,
    [PAGE_SERIAL]     = &page_serial_interface,
};

// 2. 将状态单独提取出来，放在 SRAM 中
static Page_State_t page_states[PAGE_MAX_ID] = {PAGE_STATE_UNINIT};// 初始化为未初始化状态

static volatile uint32_t current_page_id = PAGE_NONE;// 当前页面ID
static volatile uint32_t next_page_id = PAGE_START;// 下一个页面ID

// 页面传参缓冲区大小 (current_path 与 page_pick_name 共用)
// 与 FILE_PATH_MAX 对齐, 否则装不下 NOTE_PATH_MAX(272) 的长路径
#define PAGE_ARG_BUF_MAX FILE_PATH_MAX

// 页面传参缓冲区的统一分配/填充: 首次用到才申请, 之后复用同一块
// (current_path 与 page_pick_name 都定义在 variables.c, 由本函数统一分配)
static void page_arg_set(char ** buf, const char * src)
{
    if (*buf == NULL) {
        *buf = (char *)malloc_bsc(PAGE_ARG_BUF_MAX);
    }
    if (*buf == NULL) return;// 申请失败就保持 NULL, 由页面按"空名"处理

    strncpy(*buf, src ? src : "", PAGE_ARG_BUF_MAX - 1);
    (*buf)[PAGE_ARG_BUF_MAX - 1] = '\0';
}

// 历史记录相关变量
static volatile uint32_t page_history_stack[PAGE_HISTORY_MAX_DEPTH];// 历史记录栈
static volatile uint16_t history_count = 0;// 历史记录栈计数器
static volatile bool is_back_action = false;// 是否是后退操作

// O(1) 的快速查找函数
// page_id : 页面ID
// 返回值 : 对应的页面接口指针，如果不存在则返回 NULL
static inline const Page_Interface_t* Page_Get_Interface(uint32_t page_id) {
    if (page_id < PAGE_MAX_ID) {
        return page_registry[page_id];
    }
    return NULL;
}

// 获取当前页面ID
uint32_t Page_Get_Current(void)
{
    return current_page_id;
}

// 获取即将切换的目标页面ID 
uint32_t Page_Get_Next(void)
{
    return next_page_id;
}


// 初始化
void Page_Manager_Init(void)
{
	if (g_font_need_update)
		Page_Request_Switch(PAGE_FONT_UPDATE);
	else
		Page_Request_Switch(PAGE_START);
}

void _Page_Request_Switch_Impl(uint32_t new_page_id, const char *path, ...)
{
    if (Page_Get_Interface(new_page_id) != NULL) {
        // 页面传参: PAGE_FILE 传目录路径(文件管理器长期持有 current_path),
        // PAGE_NOTE / PAGE_CANVAS 传条目名(page_pick_name), 两者共用同一套按需分配
        // (page_arg_set 内部会 malloc, 必须留在临界区外)
        if (new_page_id == PAGE_FILE) {
            page_arg_set(&current_path, path);
        } else if (new_page_id == PAGE_NOTE || new_page_id == PAGE_CANVAS) {
            page_arg_set(&page_pick_name, path);
        }
        /* next_page_id/is_back_action 是与 Page_Manager_Loop(LVGL 任务)及
         * 其它任务的 Page_Back 并发读改写的共享状态, 成对更新必须原子 */
        taskENTER_CRITICAL();
        next_page_id = new_page_id;
        is_back_action = false;
        taskEXIT_CRITICAL();
    }
}

/* can_exit 连续拦下的轮数(20ms/轮), 超过 150(约 3s) 放弃本次切换。
 * 文件级静态: Page_Cancel_Switch 也要清它, 否则取消后的第一次新切换
 * 会在弹窗刚弹出的同一帧就被放弃。 */
static uint16_t block_ticks = 0;

/* 只把目标页面拉回当前页面, 不碰页面传参缓冲。
 * 用于 can_exit 拦下切换 / init 失败回退这类"本次切换作废"的场合。 */
void Page_Cancel_Switch(void)
{
    taskENTER_CRITICAL();
    next_page_id = current_page_id;
    is_back_action = false;   // 取消不算后退, 历史栈保持不动
    block_ticks = 0;          // 取消也清宽限计数, 否则下次切换第一次被拦就被放弃
    taskEXIT_CRITICAL();
}

// 触发后退功能
void Page_Back(void)
{
    /* 摇杆(LVGL indev)与媒体/游戏/USB 任务都可能调本函数,
     * history_count 的读-用-写与 next_page_id/is_back_action 的成对更新
     * 必须原子, 否则两个并发 Page_Back 会双弹栈 */
    taskENTER_CRITICAL();
    if (history_count > 0) {
        // 这里只"看一眼"栈顶, 真正弹栈留到 Page_Manager_Loop 里切换真的发生时做
        // (页面可以在切换前拦下并取消本次切换, 那时返回链不能被白白吃掉)
        next_page_id = page_history_stack[history_count - 1];
        is_back_action = true; 
    } else if (current_page_id > PAGE_DESKTOP) {
        next_page_id = PAGE_DESKTOP;
        is_back_action = true;
    }
    taskEXIT_CRITICAL();
}

// 手动推入历史栈（用于 LVGL 被挂起前保存当前页面）
// 用于不在 LVGL 管理的页面中手动保存当前页面状态
void Page_Push_History(uint32_t page_id)
{
    /* 调用方在媒体/游戏/USB 任务里(LVGL 已挂起), 与 Loop 的历史记录逻辑并发,
       计数与栈写入必须原子 */
    taskENTER_CRITICAL();
    if (page_id > PAGE_DESKTOP && history_count < PAGE_HISTORY_MAX_DEPTH) {
        page_history_stack[history_count++] = page_id;
    }
    taskEXIT_CRITICAL();
}

// 清除历史记录
void Page_Clear_History(void)
{
    history_count = 0;
}

// 清除页面管理器的所有状态
void Page_Manager_Deinit(void)
{
    return;
}

// 页面管理主循环
void Page_Manager_Loop(void)
{
    uint32_t target_next_id = next_page_id;// 读取目标页面ID
    bool current_is_back = is_back_action;// 读取是否是后退操作

    // 切换前先询问当前页面是否允许离开(例如未保存的内容需要用户确认)
    // 被拦下时本帧不切换, 但仍继续执行下面的刷新逻辑, 让页面自己把确认弹窗处理完
    bool allow_switch = true;
    if (current_page_id != target_next_id) {
        const Page_Interface_t* leaving_page = Page_Get_Interface(current_page_id);
        if (leaving_page && leaving_page->can_exit && !leaving_page->can_exit()) {
            allow_switch = false;
            /* 被拦下后如果页面的确认弹窗自己也失效了(被别的对象盖住、
             * 被 click_catcher 顶掉却没清 ask 标记), can_exit 会永远返回 false,
             * LVGL 任务每 20ms 调一次、next_page_id 既不清也不作废 ——
             * 用户按任何键都出不去, 只能断电。
             * 约 3s 仍无明确表态就放弃这次切换, 让用户回到可操作状态。 */
            if (++block_ticks > 150) {
                next_page_id   = current_page_id;
                is_back_action = false;
                block_ticks    = 0;
            }
        } else {
            block_ticks = 0;
        }
    }

    if (allow_switch && current_page_id != target_next_id)// 如果当前页面ID与目标页面ID不同，则进行切换
    {
        // 历史记录逻辑 (history_count/栈与其它任务的 Page_Back 并发, 计数变更须原子)
        taskENTER_CRITICAL();
        if (!current_is_back) {
            if (target_next_id > PAGE_DESKTOP) {
                if (current_page_id != 0 && history_count < PAGE_HISTORY_MAX_DEPTH) {
                    page_history_stack[history_count++] = current_page_id;
                }
            } else {
                history_count = 0; // 回到桌面/启动页清空栈
            }
        } else if (history_count > 0) {
            history_count--; // 后退: Page_Back 只看了栈顶, 到这里才真的弹栈
        }
        
        is_back_action = false;
        taskEXIT_CRITICAL();

        const Page_Interface_t* current_page = Page_Get_Interface(current_page_id);
        const Page_Interface_t* next_page = Page_Get_Interface(target_next_id);
        
        // 1. 退出当前页面
        if (current_page) {
            if (current_page->exit) {
                current_page->exit(); 
            }
            page_states[current_page_id] = PAGE_STATE_UNINIT; // 修改独立的状态数组
        }
        
        // 2. 初始化新页面
        if (next_page) {
            if (page_states[target_next_id] == PAGE_STATE_UNINIT && next_page->init) {
                next_page->init();
            }

            /* OOM 失败的探测。
             * 各页面 init 在 malloc_bsc/malloc_ccm 失败时都是"直接 return",
             * 而此时原页面已经 exit 删完了对象, 活动屏幕上不会有任何子对象。
             * 若此时仍标 ACTIVE, update() 会每帧照跑而页面空空 → 永久白屏。
             * 判据用"活动屏幕子对象数 == 0": 所有会创建 UI 的页面都至少建 1 个
             * 对象; init 为 NULL 的页面(游戏/媒体/副屏)不在此列。 */
            if (next_page->init != NULL && lv_obj_get_child_cnt(lv_scr_act()) == 0) {
                page_states[target_next_id] = PAGE_STATE_UNINIT;
                next_page_id = current_page_id;          // 撤销本次切换请求
                debug_printf("page %u init failed (no widget), stay at %u\r\n",
                             (unsigned)target_next_id, (unsigned)current_page_id);
                if (current_page && current_page->init) { // 把原页面重建回来, 不留白屏
                    current_page->init();
                    if (lv_obj_get_child_cnt(lv_scr_act()) > 0)
                        page_states[current_page_id] = PAGE_STATE_ACTIVE;
                }
                return;
            }

            page_states[target_next_id] = PAGE_STATE_ACTIVE;
        }
        
        current_page_id = target_next_id;
    }
    
    // 4. 执行当前页面的刷新逻辑
    if (current_page_id < PAGE_MAX_ID) {
        const Page_Interface_t* current_page = Page_Get_Interface(current_page_id);
        if (current_page && current_page->update && page_states[current_page_id] == PAGE_STATE_ACTIVE) {
            current_page->update();
        }
    }
}
