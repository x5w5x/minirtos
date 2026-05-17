/*
 * @Author: 轩
 * @Date: 2026-05-17 16:16:17
 * @LastEditTime: 2026-05-17 17:25:21
 * @FilePath: \minirtos\OS_Core\os_core.h
 */
#ifndef OS_CORE_H
#define OS_CORE_H

#include<stdint.h>
#include"os_list.h"
#include <stddef.h>
typedef enum{
    OS_TASK_STATE_READY=0,
    OS_TASK_STATE_RUNNING,
    OS_TASK_STATE_BLOCKED,
    OS_TASK_STATE_SUSPENDED
}os_task_state_t;

typedef struct os_tcb_t{
    uint32_t *stack_ptr;
    os_list_node_t list_node;
    uint8_t priority;
    os_task_state_t state;
}os_tcb_t;
#define OS_TCB_FROM_NODE(node_ptr) \
    ((os_tcb_t *)((uint8_t *)(node_ptr)-offsetof(os_tcb_t,list_node)))


    // ==================== 核心组件 API 声明 ====================
void os_sched_init(void);
void os_task_create(os_tcb_t *tcb, 
                    void (*task_func)(void *), 
                    void *param, 
                    uint32_t *stack_base, 
                    uint32_t stack_size, 
                    uint8_t prio);
void os_task_ready(os_tcb_t *tcb);
void os_sched(void);
void os_start(void);
#endif // DEBUG