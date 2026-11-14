#include "kvdb_ctrl.h"
#include "flashdb.h"
#include "variables.h"
#include <string.h>

typedef struct {
    const char *key; // 持久化条目的键名
    void       *ptr; // 指向持久化条目数据的指针
    uint8_t     size;// 持久化条目数据的大小
} persist_entry_t;

/** 定义持久化条目 */
#define KV(name, type) {#name, (void *)&name, sizeof(name)}, // 定义持久化条目
static const persist_entry_t persist_entries[] = { PERSIST_LIST };// 定义持久化条目数组
#undef KV

#define DIRTY_WORDS ((PERSIST_COUNT + 31) / 32)// 计算需要多少个32位整数来存储所有持久化条目的脏标记
static uint32_t persist_dirty[DIRTY_WORDS];// 位图，标记哪些持久化条目已被修改

// 读取所有持久化条目
void kvdb_persist_load(void)
{
    if (!kvdb.parent.init_ok) return;// 如果kvdb未初始化，则直接返回

    for (int i = 0; i < PERSIST_COUNT; i++) {
        struct fdb_blob blob;
        size_t len = fdb_kv_get_blob(&kvdb, persist_entries[i].key,
                                      fdb_blob_make(&blob, persist_entries[i].ptr, persist_entries[i].size));
        (void)len;
    }
}

// 标记持久化条目为已修改
void kvdb_persist_mark(int index)
{
    if (index >= 0 && index < PERSIST_COUNT) {
        GLOBAL(persist_dirty[index / 32] |= (1U << (index % 32)));// 设置对应的位为1，表示该条目已被修改
    }
}

// 将所有已修改的持久化条目写入kvdb
void kvdb_persist_flush(void)
{
    if (!kvdb.parent.init_ok) return;

    uint32_t dirty[DIRTY_WORDS];
    /* 只取快照, 不清脏位。
     * 不能 copy 完立刻 memset(persist_dirty, 0):
     * 写循环每条 fdb_kv_set_blob 都要几十 ms(含 flash 擦写), 期间别的任务
     * 调 kvdb_persist_mark() 置的脏位不在快照里, 写完又不会被清 —— 结果
     * 那次修改永远留不下("音量/输出通道重启后复位")。
     * 现在改为"成功才清位", 期间的新标记自然保留到下一轮。 */
    GLOBAL(memcpy(dirty, persist_dirty, sizeof(dirty)););

    for (int i = 0; i < PERSIST_COUNT; i++) {
        if (dirty[i / 32] & (1U << (i % 32))) {
            const persist_entry_t *e = &persist_entries[i];
            struct fdb_blob blob;
            fdb_err_t ret = fdb_kv_set_blob(&kvdb, e->key,
                            fdb_blob_make(&blob, e->ptr, e->size));
            if (ret == FDB_NO_ERR) {
                /* 写入成功才清脏位; 清之前若期间又被标记, 位仍为 1 也不错,
                   因为那一轮的值已经进到 flash 了(值引用的是同一块内存)。 */
                GLOBAL(persist_dirty[i / 32] &= ~(1U << (i % 32)););
            }
            /* 失败: 保留脏位, 下一轮重试 */
        }
    }
}