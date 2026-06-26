/*
 * @Author: 轩
 * @Date: 2026-05-17 11:34:46
 * @LastEditTime: 2026-06-26 13:49:31
 * @FilePath: \minirtos\User\main.c
 */
#include "stm32f10x.h"
// #include "OS_Core/os_core.h"
 #include "os_core.h"
// 1. 准备测试变量
volatile uint32_t countA = 0;
volatile uint32_t countB = 0;
volatile uint32_t countC = 0;
// 2. 分配任务堆栈和档案袋 (TCB)
uint32_t TaskA_Stack[64];
uint32_t TaskB_Stack[64];
uint32_t TaskC_Stack[64];
os_tcb_t TaskA_TCB;
os_tcb_t TaskB_TCB;
os_tcb_t TaskC_TCB;
os_sem_t Count;

// 引入底层的跑道变量，方便我们手动测试挂起
extern uint32_t os_ready_bitmap;
// 这是 STM32 硬件规定死的心跳中断入口
void SysTick_Handler(void)
{
    os_tick_handler(); // 呼叫宿管阿姨查房
}
void TaskA(void *param)
{
    while (1)
    {
        os_sem_take(&Count);
        countA++;  
        os_delay(10);
        //os_delay(1000);
    }
}

void TaskB(void *param)
{
    while (1)
    {
        countB++;
         os_delay(10);
        //os_delay(1000); 
    }
}

void TaskC(void *param)
{
    while(1)
    {   countC++;
        if(countC==5)
        {
            countC=0;
            os_sem_give(&Count);
        }
        os_delay(1000);
    }
}

int main(void)
{
    SysTick_Config(SystemCoreClock / 1000);
    os_sched_init();
    os_task_create(&TaskA_TCB, TaskA, NULL, TaskA_Stack, 64, 1,1);
    os_task_create(&TaskB_TCB, TaskB, NULL, TaskB_Stack, 64, 2,1);
    os_task_create(&TaskC_TCB, TaskC, NULL, TaskC_Stack, 64, 2,1);
    os_task_ready(&TaskA_TCB);
    os_task_ready(&TaskB_TCB);
    os_task_ready(&TaskC_TCB);
    os_sem_init(&Count,0);

    // 第 4 步：系统点火！(此函数一调用，CPU 就被 RTOS 接管了)
    // 请确保你在 os_core.c 里加了那个 os_start() 函数哦！
    os_start();

    while (1);
}

