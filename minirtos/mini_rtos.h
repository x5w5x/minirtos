/*
 * @Author: 轩
 * @Date: 2026-05-17 11:47:15
 * @LastEditTime: 2026-05-17 13:05:16
 * @FilePath: \minirtos\minirtos\mini_rtos.h
 */
#ifndef MINI_RTOS_H
#define MINI_RTOS_H

#include <stdint.h>
typedef enum{
    TASK_STATE_READY=0,
    TASK_STATE_RUNNING,
    TASK_STATE_BLOCKED
}TaskStatus_t;

typedef struct tcb_struct{
    uint32_t *stack_ptr;
    uint8_t priority;
    TaskStatus_t status;
    struct tcb_struct *next;   
}TCB_t;

void Task_Create(TCB_t *tcb,void(* task_func)(void *),void *param,uint32_t *stack_base,uint32_t stack_size,uint8_t prio);
void OS_Start(void);
#endif
