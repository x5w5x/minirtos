/*
 * @Author: 轩
 * @Date: 2026-05-17 16:16:17
 * @LastEditTime: 2026-07-11 16:40:13
 * @FilePath: \minirtos\OS_Core\os_core.h
 */
#ifndef OS_CORE_H
#define OS_CORE_H

#include "os_port.h"
#include <stdint.h>
#include "os_list.h"
#include <stddef.h>
typedef enum {
    OS_TASK_STATE_READY = 0,
    OS_TASK_STATE_RUNNING,
    OS_TASK_STATE_BLOCKED,
    OS_TASK_STATE_SUSPENDED
} os_task_state_t;

typedef struct os_tcb_t {
    uint32_t *stack_ptr;
    os_list_node_t list_node;
    uint8_t priority;
    os_task_state_t state;
    uint32_t ticks_to_delay;
    uint32_t time_slice_reload; // 初始时间片
    uint32_t time_slice;        // 剩余时间片

    char name[16];
    os_list_node_t global_node;
    uint32_t *stack_base;
    uint16_t stack_size;

} os_tcb_t;

typedef struct {
    uint8_t count;
    os_list_node_t wait_node;
} os_sem_t;

typedef struct {
    uint8_t lock;
    os_tcb_t *owner;
    uint8_t original_pri;
    os_list_node_t wait_node;
} os_mutex_t;

typedef struct {
    uint8_t *msg_pool;
    uint16_t msg_size;
    uint16_t max_msg;

    uint16_t msg_count;
    uint16_t head; // 读索引
    uint16_t tail; // 写索引

    os_list_node_t wait_node;

} os_msg_queue_t;
typedef void (*os_timer_cb_t)(void *arg);
typedef struct {
    os_list_node_t list_node;
    const char *name;

    os_timer_cb_t cb;
    void *arg;
    uint32_t timeout_tick;
    uint32_t period;
    uint8_t state;
}os_timer_t;


#define OS_TCB_FROM_NODE(node_ptr) \
    ((os_tcb_t *)((uint8_t *)(node_ptr) - offsetof(os_tcb_t, list_node)))

#define offset_of(type, member)         ((uint32_t)&(((type *)0)->member))

#define container_of(ptr, type, member) ((type *)((char *)(ptr) - offset_of(type, member)))

// ==================== 核心组件 API 声明 ====================
void os_sched_init(void);
void os_task_create(os_tcb_t *tcb,
                    const char *name,
                    void (*task_func)(void *),
                    void *param,
                    uint32_t *stack_base,
                    uint32_t stack_size,
                    uint8_t prio, uint32_t time_slice);
void os_task_ready(os_tcb_t *tcb);
void os_sched(void);
void os_start(void);
void os_delay(uint32_t ticks);
void os_tick_handler(void);
void os_idle_task(void *param);

// ==================== 信号量 ====================
void os_sem_init(os_sem_t *sem, uint8_t value);
void os_sem_take(os_sem_t *sem);
void os_sem_give(os_sem_t *sem);
// ==================== 互斥锁 ====================
void os_mutex_init(os_mutex_t *mutex);
void os_mutex_take(os_mutex_t *mutex);
void os_mutex_give(os_mutex_t *mutex);
// ==================== 消息队列 ====================
void os_msg_init(os_msg_queue_t *msg, uint8_t *pool, uint16_t msg_size, uint16_t max_msg);
void os_msg_send(os_msg_queue_t *msg, void *data);
void os_msg_recv(os_msg_queue_t *msg, void *data);
// ==================== 系统信息 ====================
void os_system_info();
// ==================== 软件定时 ====================
void os_timer_init();
void os_timer_start(os_timer_t *timer, uint32_t delay, uint32_t period);
#endif // DEBUG