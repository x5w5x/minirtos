#include "os_core.h"
#include "stm32f10x.h"

os_list_node_t os_delay_list_head;
os_tcb_t os_idle_task_tcb;
uint32_t os_idle_task_stack[64];
os_list_node_t os_ready_queue[32];
uint32_t os_ready_bitmap  = 0x00;
os_tcb_t *os_current_task = NULL;
os_tcb_t *os_next_task    = NULL;
void os_idle_task(void *param)
{
    while(1){

    }
}
void os_task_create(os_tcb_t *tcb, void (*task_func)(void *), void *param, uint32_t *stack_base, uint32_t stack_size, uint8_t prio)
{
    tcb->priority = prio;
    tcb->ticks_to_delay=0;
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
    os_list_init(&os_delay_list_head);
    os_task_create(&os_idle_task_tcb,os_idle_task,NULL,os_idle_task_stack,64,0);
    os_task_ready(&os_idle_task_tcb);
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
    uint8_t highest_prio= 31-__clz(os_ready_bitmap);
    os_list_node_t *first_node=os_ready_queue[highest_prio].next;
    os_tcb_t *next_task=OS_TCB_FROM_NODE(first_node);
    if(next_task ==os_current_task)
    return;
    if(os_current_task !=NULL){
        if (os_current_task->state == OS_TASK_STATE_RUNNING) {
            os_current_task->state = OS_TASK_STATE_READY;
        }
    }
    next_task->state=OS_TASK_STATE_RUNNING;
    os_next_task=next_task;
    SCB->ICSR |=(1<<28);  //触发上下文切换
    
    // while(1);

}

void os_start(void)
{
    NVIC_SetPriority(PendSV_IRQn, 0xFF);
    __set_PSP(0);
    os_sched();
    while(1);
}

void os_delay(uint32_t ticks)
{
    if(ticks==0)
    return;
    os_current_task->ticks_to_delay=ticks;
    os_current_task->state=OS_TASK_STATE_BLOCKED;

    os_list_remove(&os_current_task->list_node);
    if(os_ready_queue[os_current_task->priority].next==&os_ready_queue[os_current_task->priority]){
        os_ready_bitmap &=~(0x01<<os_current_task->priority);
    }
    os_list_add(&os_delay_list_head,&os_current_task->list_node);
    os_sched();
}

void os_tick_handler(void)
{
    os_list_node_t *curr=os_delay_list_head.next;
    os_list_node_t *next_node;
    while(curr!=&os_delay_list_head){
        next_node=curr->next;
        os_tcb_t *tcb=OS_TCB_FROM_NODE(curr);
        if(tcb->ticks_to_delay>0){
            tcb->ticks_to_delay--;
        if(tcb->ticks_to_delay == 0){
            os_list_remove(curr);
            os_task_ready(tcb);
        }}
        curr=next_node;
    }
    os_sched();
}

