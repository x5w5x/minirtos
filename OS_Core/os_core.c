#include "os_core.h"
#include "stm32f10x.h"

os_list_node_t os_delay_list_head;
os_tcb_t os_idle_task_tcb;
uint32_t os_idle_task_stack[64];
os_list_node_t os_ready_queue[32];
uint32_t os_ready_bitmap  = 0x00;
os_tcb_t *os_current_task = NULL;
os_tcb_t *os_next_task    = NULL;

/*空闲任务*/
void os_idle_task(void *param)
{
    while (1) {
    }
}
/*任务创建*/
void os_task_create(os_tcb_t *tcb, void (*task_func)(void *), void *param, uint32_t *stack_base, uint32_t stack_size, uint8_t prio, uint32_t time_slice)
{
    tcb->priority       = prio;
    tcb->ticks_to_delay = 0;
    tcb->state          = OS_TASK_STATE_SUSPENDED;
    os_list_init(&tcb->list_node);
    uint32_t *sp = stack_base + stack_size;
    sp -= 16;
    sp[15]                 = (0x01 << 24);
    sp[14]                 = (uint32_t)task_func;
    sp[13]                 = 0x14141414;
    sp[8]                  = (uint32_t)param;
    tcb->stack_ptr         = sp;
    tcb->time_slice_reload = time_slice;
    tcb->time_slice        = time_slice;
}
/*初始化调度器*/
void os_sched_init(void)
{
    for (int i = 0; i < 32; i++) {
        os_list_init(&os_ready_queue[i]);
    }
    os_list_init(&os_delay_list_head);
    os_task_create(&os_idle_task_tcb, os_idle_task, NULL, os_idle_task_stack, 64, 0, 0);
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
/*任务调度*/
void os_sched(void)
{
    uint8_t highest_prio       = 31 - __clz(os_ready_bitmap);
    os_list_node_t *first_node = os_ready_queue[highest_prio].next;
    os_tcb_t *next_task        = OS_TCB_FROM_NODE(first_node);
    if (next_task == os_current_task)
        return;
    if (os_current_task != NULL) {
        if (os_current_task->state == OS_TASK_STATE_RUNNING) {
            os_current_task->state = OS_TASK_STATE_READY;
        }
    }
    next_task->state = OS_TASK_STATE_RUNNING;
    os_next_task     = next_task;
    SCB->ICSR |= (1 << 28); // 触发上下文切换

    // while(1);
}

void os_start(void)
{
    NVIC_SetPriority(PendSV_IRQn, 0xFF);
    __set_PSP(0);
    os_sched();
    while (1);
}

void os_delay(uint32_t ticks)
{
    if (ticks == 0)
        return;
    os_current_task->ticks_to_delay = ticks;
    os_current_task->state          = OS_TASK_STATE_BLOCKED;

    os_list_remove(&os_current_task->list_node);
    if (os_ready_queue[os_current_task->priority].next == &os_ready_queue[os_current_task->priority]) {
        os_ready_bitmap &= ~(0x01 << os_current_task->priority);
    }
    os_list_add(&os_delay_list_head, &os_current_task->list_node);
    os_sched();
}

void os_tick_handler(void)
{
    if (os_current_task == NULL)
        return;
    os_list_node_t *curr = os_delay_list_head.next;
    os_list_node_t *next_node;
    if (os_current_task->time_slice_reload > 0)
        if (os_current_task->time_slice > 0) {
            os_current_task->time_slice--;
            if (os_current_task->time_slice == 0) {
                os_current_task->time_slice = os_current_task->time_slice_reload;
                os_list_remove(&os_current_task->list_node);
                os_list_add(&os_ready_queue[os_current_task->priority], &os_current_task->list_node);
            }
        }
    while (curr != &os_delay_list_head) {
        next_node     = curr->next;
        os_tcb_t *tcb = OS_TCB_FROM_NODE(curr);
        if (tcb->ticks_to_delay > 0) {
            tcb->ticks_to_delay--;
            if (tcb->ticks_to_delay == 0) {
                os_list_remove(curr);
                os_task_ready(tcb);
            }
        }
        curr = next_node;
    }

    os_sched();
}

// ==================== 信号量 ====================

void os_sem_init(os_sem_t *sem, uint8_t value)
{
    sem->count = value;
    os_list_init(&sem->wait_node);
}

void os_sem_take(os_sem_t *sem)
{
    if (sem->count > 0) {
        sem->count--;
    } else {
        os_current_task->state = OS_TASK_STATE_BLOCKED;
        os_list_remove(&os_current_task->list_node);

        if (os_list_is_empty(&os_ready_queue[os_current_task->priority]))
            os_ready_bitmap &= ~(0x01 << os_current_task->priority);
        os_list_add(&sem->wait_node, &os_current_task->list_node);

        os_sched();
    }
}

void os_sem_give(os_sem_t *sem)
{
    if (!(os_list_is_empty(&sem->wait_node))) {
        os_list_node_t *node = sem->wait_node.next;
        os_list_remove(node);
        os_tcb_t *task = container_of(node, os_tcb_t, list_node);
        os_task_ready(task);

        os_sched();
    } else {
        sem->count = 1;
    }
}

// ==================== 互斥锁 ====================

void os_mutex_init(os_mutex_t *mutex)
{
    mutex->lock         = 0;
    mutex->owner        = NULL;
    mutex->original_pri = 0;
    os_list_init(&mutex->wait_node);
}

void os_mutex_take(os_mutex_t *mutex)
{
    if (mutex->lock == 0) {
        mutex->lock         = 1;
        mutex->owner        = os_current_task;
        mutex->original_pri = os_current_task->priority;
    } else {

        if (os_current_task->priority > mutex->owner->priority) {
            uint8_t old_pri = mutex->owner->priority;
            os_list_remove(&mutex->owner->list_node);
            if (os_list_is_empty(&os_ready_queue[old_pri]))
                os_ready_bitmap &= ~(0x01 << old_pri);
            mutex->owner->priority = os_current_task->priority;
            os_list_add(&os_ready_queue[mutex->owner->priority], &mutex->owner->list_node);
            os_ready_bitmap |= (0x01 << mutex->owner->priority);
        }

        os_current_task->state = OS_TASK_STATE_BLOCKED;
        os_list_remove(&os_current_task->list_node);
        if (os_list_is_empty(&os_ready_queue[os_current_task->priority]))
            os_ready_bitmap &= ~(0x01 << os_current_task->priority);
        os_list_add(&mutex->wait_node, &os_current_task->list_node);
        os_sched();
    }
}

void os_mutex_give(os_mutex_t *mutex)
{
    if (os_current_task->priority != mutex->original_pri) { // if(mutex->owner->priority!=mutex->original_pri)
        os_list_remove(&os_current_task->list_node);
        if (os_list_is_empty(&os_ready_queue[os_current_task->priority]))
            os_ready_bitmap &= ~(0x01 << os_current_task->priority);
        os_current_task->priority = mutex->original_pri;
        os_list_add(&os_ready_queue[os_current_task->priority], &os_current_task->list_node);
        os_ready_bitmap |= (0x01 << os_current_task->priority);
    }
    if (!(os_list_is_empty(&mutex->wait_node))) {
        os_list_node_t *node = mutex->wait_node.next;
        os_list_remove(node);
        os_tcb_t *task = container_of(node, os_tcb_t, list_node);
        os_task_ready(task);
        mutex->owner=task;
        mutex->original_pri=task->priority;
        os_sched();
    }else{
        mutex->lock=0;
    }
}