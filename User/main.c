/*
 * @Author: 轩
 * @Date: 2026-05-17 11:34:46
 * @LastEditTime: 2026-06-26 15:51:00
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
// os_sem_t Count;
os_mutex_t Count;

// 引入底层的跑道变量，方便我们手动测试挂起
extern uint32_t os_ready_bitmap;
// 这是 STM32 硬件规定死的心跳中断入口
void SysTick_Handler(void)
{
    os_tick_handler(); // 呼叫宿管阿姨查房
}
// void TaskA(void *param)
//{
//     while (1)
//     {
//         os_sem_take(&Count);
//         countA++;
//         os_delay(10);
//         //os_delay(1000);
//     }
// }

// void TaskB(void *param)
//{
//     while (1)
//     {
//         countB++;
//          os_delay(10);
//         //os_delay(1000);
//     }
// }

// void TaskC(void *param)
//{
//     while(1)
//     {   countC++;
//         if(countC==5)
//         {
//             countC=0;
//             os_sem_give(&Count);
//         }
//         os_delay(1000);
//     }
// }
void TaskC(void *param) // 低优先级 (打工人)
{
    while (1) {
        // os_sem_take(&Count); // 1. 打工人先拿到锁
        os_mutex_take(&Count);
        // 模拟拿着锁进行真实的物理运算 (不睡觉！)
        for (int i = 0; i < 5000000; i++) {
            countC++;
            
        }

        os_mutex_give(&Count);
		
        // os_sem_give(&Count); // 干完活，释放锁
        os_delay(1000); // 休息一下，让出 CPU
    }
}

void TaskA(void *param) // 高优先级 (长官)
{
    while (1) {
        os_delay(10); // 1. 故意晚点出场，让 C 先拿到锁
        // os_sem_take(&Count); // 2. 长官出场要锁，被 C 阻塞
        os_mutex_take(&Count);
        countA++; // 拿到锁后干活
        os_mutex_give(&Count);
        // os_sem_give(&Count);
        os_delay(10);
    }
}

void TaskB(void *param) // 中优先级 (捣乱分子)
{
    os_delay(20); // 1. 晚点出场，确保 C 已经拿了锁，A 已经被阻塞

    while (1) {
        // 2. 捣乱分子开始死循环！不调用 os_delay，霸占 CPU！
        countB++;
         os_delay(10);
    }
}
int main(void)
{
    SysTick_Config(SystemCoreClock / 1000);
    os_sched_init();
    os_task_create(&TaskA_TCB, TaskA, NULL, TaskA_Stack, 64, 3, 1);
    os_task_create(&TaskB_TCB, TaskB, NULL, TaskB_Stack, 64, 2, 1);
    os_task_create(&TaskC_TCB, TaskC, NULL, TaskC_Stack, 64, 1, 1);
    os_task_ready(&TaskA_TCB);
    os_task_ready(&TaskB_TCB);
    os_task_ready(&TaskC_TCB);
    // os_sem_init(&Count,1);
    os_mutex_init(&Count);

    // 第 4 步：系统点火！(此函数一调用，CPU 就被 RTOS 接管了)
    // 请确保你在 os_core.c 里加了那个 os_start() 函数哦！
    os_start();

    while (1);
}
