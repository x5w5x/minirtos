/*
 * @Author: 轩
 * @Date: 2026-05-17 12:23:21
 * @LastEditTime: 2026-05-17 17:41:07
 * @FilePath: \minirtos\minirtos\os_core.c
 */
// /*
//  * @Author: 轩
//  * @Date: 2026-05-17 12:23:21
//  * @LastEditTime: 2026-05-17 15:22:10
//  * @FilePath: \minirtos\minirtos\os_core.c
//  */

// #include"mini_rtos.h"
// #include "stm32f10x.h"
// #define MAX_PRIORITIES 32
// TCB_t *ReadyQueue[MAX_PRIORITIES];
// TCB_t *CurrentTask;
// uint32_t ReadyBitMap=0x00;

// void Error_loop(void)
// {
//     while(1);
// }
// void Task_Create(TCB_t *tcb,void(* task_func)(void *),void *param,uint32_t *stack_base,uint32_t stack_size,uint8_t prio)
// {
//     uint32_t *sp=stack_base+stack_size; 
//     sp-=16;
//     sp[15]=(0x01<<24);
//     sp[14]=(uint32_t)task_func;
//     sp[13]=(uint32_t)Error_loop;
//     sp[8]=(uint32_t)param;
//     tcb->priority=prio;
//     tcb->stack_ptr=sp;
//     tcb->status=TASK_STATE_READY;
//     ReadyQueue[prio]=tcb;
//     ReadyBitMap|=(1<<prio);
// }

// void OS_Start(void){
//     uint8_t prio=31-__clz(ReadyBitMap);
//     CurrentTask=ReadyQueue[prio];
//     __set_PSP(0);
//     SCB->ICSR |= (1 << 28);
//     while(1);
// }
