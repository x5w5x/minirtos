#include "os_core.h"
#include "stm32f10x.h"

os_list_node_t os_ready_queue[32];
uint32_t os_ready_bitmap  = 0x00;
os_tcb_t *os_current_task = NULL;
os_tcb_t *os_next_task    = NULL;

void os_task_create(os_tcb_t *tcb, void (*task_func)(void *), void *param, uint32_t *stack_base, uint32_t stack_size, uint8_t prio)
{
    tcb->priority = prio;
    tcb->state    = OS_TASK_STATE_SUSPENDED;
    os_list_init(&tcb->list_node);
    uint32_t *sp = stack_base + stack_size;
    sp -= 16;
    sp[15]         = (0x01 << 24);
    sp[14]         = (uint32_t)task_func;
    sp[13]         = 0x14141414;
    sp[8]          = (uint32_t)param;
    tcb->stack_ptr = sp;
}
void os_sched_init(void)
{
    for (int i = 0; i < 32; i++) {
        os_list_init(&os_ready_queue[i]);
    }
}

void os_task_ready(os_tcb_t *tcb)
{
    if (tcb->state == OS_TASK_STATE_READY)
        return;
    tcb->state = OS_TASK_STATE_READY;
    os_list_add(&os_ready_queue[tcb->priority], &tcb->list_node);
    os_ready_bitmap |= (0x01 << (tcb->priority));
}

void os_sched(void)
{
    if (os_ready_bitmap == 0)
        return;
    uint8_t highest_prio= 31-__clz(os_ready_bitmap);
    os_list_node_t *first_node=os_ready_queue[highest_prio].next;
    os_tcb_t *next_task=OS_TCB_FROM_NODE(first_node);
    if(next_task ==os_current_task)
    return;
    if(os_current_task !=NULL){
        os_current_task->state=OS_TASK_STATE_READY;
    }
    next_task->state=OS_TASK_STATE_RUNNING;
    os_next_task=next_task;
    SCB->ICSR |=(1<<28);
    // while(1);

}

void os_start(void)
{
    __set_PSP(0);
    os_sched();
    while(1);
}