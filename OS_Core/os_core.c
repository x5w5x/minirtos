#include "os_core.h"
#include "stm32f10x.h"
#include "string.h"

os_list_node_t os_delay_list_head;
os_list_node_t os_global_task_list;
os_tcb_t os_idle_task_tcb;
uint32_t os_idle_task_stack[64];
uint32_t os_timer_task_stack[128]; 
os_tcb_t os_timer_task_tcb;
os_list_node_t os_ready_queue[32];
uint32_t os_ready_bitmap  = 0x00;
os_tcb_t *os_current_task = NULL;
os_tcb_t *os_next_task    = NULL;

volatile uint32_t os_idle_count = 0;
volatile uint8_t os_cpu_usage   = 0;
uint32_t os_idle_max            = 0;
uint8_t os_is_calibrated        = 0;

volatile uint32_t os_sys_tick = 0;

os_list_node_t os_timer_list;

/*空闲任务*/
void os_idle_task(void *param)
{
    while (1) {
        os_idle_count++;
    }
}
/*任务创建*/
void os_task_create(os_tcb_t *tcb, const char *name, void (*task_func)(void *), void *param, uint32_t *stack_base, uint32_t stack_size, uint8_t prio, uint32_t time_slice)
{
    tcb->priority       = prio;
    tcb->ticks_to_delay = 0;
    tcb->state          = OS_TASK_STATE_SUSPENDED;
    os_list_init(&tcb->list_node);
    uint32_t *sp = stack_base + stack_size;
    sp -= 16;
    uint32_t *paint_ptr = stack_base;
    while (paint_ptr < sp) {
        *paint_ptr = 0xDEADBEEF;
        paint_ptr++;
    }
    sp[15]                 = (0x01 << 24);
    sp[14]                 = (uint32_t)task_func;
    sp[13]                 = 0x14141414;
    sp[8]                  = (uint32_t)param;
    tcb->stack_ptr         = sp;
    tcb->time_slice_reload = time_slice;
    tcb->time_slice        = time_slice;

    tcb->stack_base = stack_base;
    tcb->stack_size = stack_size;

    int i = 0;
    while (name[i] != '\0' && i < 14) {
        tcb->name[i] = name[i];
        i++;
    }
    while (i < 14) {
        tcb->name[i] = ' ';
        i++;
    }
    tcb->name[14] = '\0';
    os_list_init(&tcb->global_node);
    os_list_add(&os_global_task_list, &tcb->global_node);
}
/*初始化调度器*/
void os_sched_init(void)
{
    os_port_systick_init();
    for (int i = 0; i < 32; i++) {
        os_list_init(&os_ready_queue[i]);
    }
    os_list_init(&os_delay_list_head);
    os_list_init(&os_global_task_list);
    os_task_create(&os_idle_task_tcb, "IDLE", os_idle_task, NULL, os_idle_task_stack, 64, 0, 0);
    os_task_ready(&os_idle_task_tcb);
    os_timer_init();
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
    // uint8_t highest_prio       = 31 - __clz(os_ready_bitmap);
    uint8_t highest_prio       = OS_PORT_GET_HIGHEST_PRI(os_ready_bitmap);
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
    // SCB->ICSR |= (1 << 28); // 触发上下文切换
    OS_PORT_YIELD();

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
    static uint32_t tick_counter = 0;
    os_sys_tick++;
    tick_counter++;
    if (tick_counter >= 1000) {
        tick_counter = 0;
        if (os_is_calibrated)
            if (os_idle_count > os_idle_max) os_idle_count = os_idle_max;
        os_cpu_usage  = 100 - (os_idle_count * 100 / os_idle_max);
        os_idle_count = 0;
    }
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
        mutex->owner        = task;
        mutex->original_pri = task->priority;
        os_sched();
    } else {
        mutex->lock = 0;
    }
}

// ==================== 消息队列 ====================

void os_msg_init(os_msg_queue_t *msg, uint8_t *pool, uint16_t msg_size, uint16_t max_msg)
{
    msg->msg_pool = pool;
    msg->msg_size = msg_size;
    msg->max_msg  = max_msg;

    msg->msg_count = 0;
    msg->head      = 0;
    msg->tail      = 0;
    os_list_init(&msg->wait_node);
}
void os_msg_send(os_msg_queue_t *msg, void *data)
{
    if (msg->msg_count >= msg->max_msg)
        return;

    uint8_t *dest = msg->msg_pool + (msg->tail * msg->msg_size);

    memcpy(dest, data, msg->msg_size);

    msg->tail = (msg->tail + 1) % msg->max_msg;

    msg->msg_count++;
    if (!os_list_is_empty(&msg->wait_node)) {
        os_list_node_t *node = msg->wait_node.next;
        os_list_remove(node);
        os_tcb_t *task = container_of(node, os_tcb_t, list_node);
        os_task_ready(task);
        os_sched();
    }
}

void os_msg_recv(os_msg_queue_t *msg, void *data)
{
    if (msg->msg_count == 0) {
        os_current_task->state = OS_TASK_STATE_BLOCKED;
        os_list_remove(&os_current_task->list_node);
        if (os_list_is_empty(&os_ready_queue[os_current_task->priority]))
            os_ready_bitmap &= ~(0x01 << os_current_task->priority);
        os_list_add(&msg->wait_node, &os_current_task->list_node);
        os_sched();
    }

    uint8_t *src = msg->msg_pool + (msg->head * msg->msg_size);

    memcpy(data, src, msg->msg_size);
    msg->head = (msg->head + 1) % msg->max_msg;
    msg->msg_count--;
}
// ==================== 系统信息 ====================

void os_system_info()
{
    SEGGER_RTT_WriteString(0, "\x1B[2J\x1B[H");
    SEGGER_RTT_printf(0, "========================================================\r\n");
    SEGGER_RTT_printf(0, "                MiniRTOS System Info                    \r\n");
    SEGGER_RTT_printf(0, "========================================================\r\n");
    SEGGER_RTT_printf(0, "Task Name       | State | Prio | Stack Used / Total (Words)\r\n");
    SEGGER_RTT_printf(0, "--------------------------------------------------------\r\n");

    os_list_node_t *node = os_global_task_list.next;

    while (node != &os_global_task_list) {
        os_tcb_t *tcb         = container_of(node, os_tcb_t, global_node);
        const char *state_str = "UNK";
        if (tcb == os_current_task)
            state_str = "RUN";
        else if (tcb->state == OS_TASK_STATE_READY)
            state_str = "RDY";
        else if (tcb->state == OS_TASK_STATE_BLOCKED)
            state_str = "BLK";
        else if (tcb->state == OS_TASK_STATE_SUSPENDED)
            state_str = "SUS";

        uint32_t unused_words = 0;
        uint32_t *stack_ptr   = tcb->stack_base;
        while (stack_ptr < (tcb->stack_base + tcb->stack_size)) {
            if (*stack_ptr == 0xDEADBEEF) {
                unused_words++;
                stack_ptr++;
            } else
                break;
        }
        uint32_t used_words = tcb->stack_size - unused_words;
        SEGGER_RTT_printf(0, "%s |  %s  |  %02d  |  %03d / %03d\r\n",
                          tcb->name, state_str, tcb->priority, used_words, tcb->stack_size);

        node = node->next;
    }
    SEGGER_RTT_printf(0, "========================================================\r\n");
    SEGGER_RTT_printf(0, "CPU Usage: %d%%\r\n", os_cpu_usage); // 打印全局 CPU 变量
    SEGGER_RTT_printf(0, "========================================================\r\n");
}

// ==================== 软件定时 ====================

void os_timer_start(os_timer_t *timer, uint32_t delay, uint32_t period)
{
    timer->period       = period;
    timer->state        = 1;
    timer->timeout_tick = os_sys_tick + delay;
    os_list_add(&os_timer_list, &timer->list_node);
}

void os_timer_task(void *param)
{
    while (1) {
        uint32_t next_delay  = 0xFFFFFFFF;
        os_list_node_t *node = os_timer_list.next;

        while (node != &os_timer_list) {
            os_timer_t *timer = container_of(node, os_timer_t, list_node);
            if (timer->state == 1) {
                if (os_sys_tick >= timer->timeout_tick) {
                    timer->cb(timer->arg);
                    if (timer->period == 0)
                        timer->state = 0;
                    else
                        timer->timeout_tick += timer->period;
                    }
                    if (timer->state == 1) {
                      next_delay= ((timer->timeout_tick-os_sys_tick)<next_delay) ? timer->timeout_tick-os_sys_tick :next_delay;
                        }
                }
                node =node->next;
            }
            os_delay(next_delay);
        }
    }


void os_timer_init()
{
    os_list_init(&os_timer_list);
    os_task_create(&os_timer_task_tcb, 
                   "TimerTask",      
                   os_timer_task,    
                   NULL,             
                   os_timer_task_stack, 
                   128,              
                   31,              
                   1);               
    os_task_ready(&os_timer_task_tcb);

}