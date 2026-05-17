/*
 * @Author: 轩
 * @Date: 2026-02-06 19:12:36
 * @LastEditTime: 2026-05-17 14:58:30
 * @FilePath: \minirtos\User\main.c
 */
#include "stm32f10x.h"                  // Device header
#include"minirtos/mini_rtos.h"
#include"Serial.h"
extern uint32_t ReadyBitMap;
uint32_t TaskA_Stack[64];
uint32_t TaskB_Stack[64];
TCB_t TaskA_TCB;
TCB_t TaskB_TCB;
// 1. 加上 volatile，并且改名叫 countA 和 countB
volatile uint32_t countA = 0;
volatile uint32_t countB = 0;

// 2. 任务 A 的剧本：改成疯狂加 1
void TaskA(void *param)
{
    while (1)
    {
        countA++;  // 每次执行到这里，值都会变大！
        ReadyBitMap |= (1 << 1);
        // 强行手动触发 PendSV 中断，把 CPU 让给任务 B
        SCB->ICSR |= (1 << 28); 
    }
}

// 3. 任务 B 的剧本：也是疯狂加 1
void TaskB(void *param)
{
    while (1)
    {
        countB++;  // 每次执行到这里，值都会变大！
        ReadyBitMap &= ~(1 << 1);
        // 强行手动触发 PendSV 中断，把 CPU 让回给任务 A
        SCB->ICSR |= (1 << 28); 
    }
}

int main(void)
{
	Serial_Init();
    // 1. 制造任务 A，放在优先级 0（假设这是高优先级）
    Task_Create(&TaskA_TCB, TaskA, (void *)0, TaskA_Stack, 64, 0);
    
    // 2. 制造任务 B，放在优先级 1（假设这是另一个就绪优先级）
    // 注意：在我们现在纯汇编数前导零的代码里，它会自动永远去执行目前最高优先级的任务
    // 为了让这两个任务都能跑，我们这里把它们设为同一个优先级（比如都设为 0）
    // 这样位图里的 Bit0 会被置 1，两个任务都在 ReadyQueue 里，通过手动切流可以相互覆盖！
    Task_Create(&TaskB_TCB, TaskB, (void *)0, TaskB_Stack, 64, 1);

    // 3. 终极点火，启动世界！
    OS_Start();

    while (1)
    {
        // 这里永远不会被执行到了
    }
}


