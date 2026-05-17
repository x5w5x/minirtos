/*
 * @Author: 轩
 * @Date: 2026-02-06 19:12:36
 * @LastEditTime: 2026-05-17 17:22:04
 * @FilePath: \minirtos\User\main.c
 */
#include "stm32f10x.h"
#include "OS_Core/os_core.h"
// #include "os_core.h"
// 1. 准备测试变量
volatile uint32_t countA = 0;
volatile uint32_t countB = 0;

// 2. 分配任务堆栈和档案袋 (TCB)
uint32_t TaskA_Stack[64];
uint32_t TaskB_Stack[64];
os_tcb_t TaskA_TCB;
os_tcb_t TaskB_TCB;

// 引入底层的跑道变量，方便我们手动测试挂起
extern uint32_t os_ready_bitmap;

// 3. 任务 A 的剧本 (优先级 0)
void TaskA(void *param)
{
    while (1)
    {
        countA++;
        
        // 测试逻辑：唤醒任务 B (优先级 1)，并触发调度
        os_ready_bitmap |= (0x01 << 1); 
        os_sched(); 
    }
}

// 4. 任务 B 的剧本 (优先级 1，比 A 高)
void TaskB(void *param)
{
    while (1)
    {
        countB++;
        
        // 测试逻辑：任务 B 跑完一次后，把自己从位图中抹除 (挂起)，并触发调度把 CPU 还给 A
        os_ready_bitmap &= ~(0x01 << 1); 
        os_sched(); 
    }
}

int main(void)
{
    // 第 1 步：初始化调度器 (铺设跑道)
    os_sched_init();

    // 第 2 步：创建任务 (造血)
    os_task_create(&TaskA_TCB, TaskA, NULL, TaskA_Stack, 64, 0);
    os_task_create(&TaskB_TCB, TaskB, NULL, TaskB_Stack, 64, 1);

    // 第 3 步：把任务请上跑道 (就绪)
    os_task_ready(&TaskA_TCB);
    os_task_ready(&TaskB_TCB);

    // 第 4 步：系统点火！(此函数一调用，CPU 就被 RTOS 接管了)
    // 请确保你在 os_core.c 里加了那个 os_start() 函数哦！
    os_start();

    while (1);
}

