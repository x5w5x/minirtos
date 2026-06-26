// /*
//  * @Author: 轩
//  * @Date: 2026-05-17 11:34:46
//  * @LastEditTime: 2026-06-26 17:19:19
//  * @FilePath: \minirtos\User\main.c
//  */
// #include "SEGGER_RTT.h"
// #include "stm32f10x.h"
// //#include "OS_Core/os_core.h"
// #include "os_core.h"
// // 1. 准备测试变量
// volatile uint32_t countA = 0;
// volatile uint32_t countB = 0;
// volatile uint32_t countC = 0;
// // 2. 分配任务堆栈和档案袋 (TCB)
// uint32_t TaskA_Stack[64];
// uint32_t TaskB_Stack[64];
// uint32_t TaskC_Stack[64];
// os_tcb_t TaskA_TCB;
// os_tcb_t TaskB_TCB;
// os_tcb_t TaskC_TCB;
// // os_sem_t Count;
// os_mutex_t Count;

// // 引入底层的跑道变量，方便我们手动测试挂起
// extern uint32_t os_ready_bitmap;
// // 这是 STM32 硬件规定死的心跳中断入口
// void SysTick_Handler(void)
// {
//     os_tick_handler(); // 呼叫宿管阿姨查房
// }
// // void TaskA(void *param)
// //{
// //     while (1)
// //     {
// //         os_sem_take(&Count);
// //         countA++;
// //         os_delay(10);
// //         //os_delay(1000);
// //     }
// // }

// // void TaskB(void *param)
// //{
// //     while (1)
// //     {
// //         countB++;
// //          os_delay(10);
// //         //os_delay(1000);
// //     }
// // }

// // void TaskC(void *param)
// //{
// //     while(1)
// //     {   countC++;
// //         if(countC==5)
// //         {
// //             countC=0;
// //             os_sem_give(&Count);
// //         }
// //         os_delay(1000);
// //     }
// // }
// void TaskC(void *param) // 低优先级 (打工人)
// {
//     while (1) {
//         // os_sem_take(&Count); // 1. 打工人先拿到锁
//         os_mutex_take(&Count);
//         // 模拟拿着锁进行真实的物理运算 (不睡觉！)
//         for (int i = 0; i < 5000000; i++) {
//             countC++;
//         }

//         os_mutex_give(&Count);

//         // os_sem_give(&Count); // 干完活，释放锁
//         os_delay(1000); // 休息一下，让出 CPU
//     }
// }

// void TaskA(void *param) // 高优先级 (长官)
// {
//     while (1) {
//         os_delay(10); // 1. 故意晚点出场，让 C 先拿到锁
//         // os_sem_take(&Count); // 2. 长官出场要锁，被 C 阻塞
//         os_mutex_take(&Count);
//         countA++; // 拿到锁后干活
//         os_mutex_give(&Count);
//         // os_sem_give(&Count);
//         os_delay(10);
//     }
// }

// void TaskB(void *param) // 中优先级 (捣乱分子)
// {
//     os_delay(20); // 1. 晚点出场，确保 C 已经拿了锁，A 已经被阻塞

//     while (1) {
//         // 2. 捣乱分子开始死循环！不调用 os_delay，霸占 CPU！
//         countB++;
//         os_delay(10);
//     }
// }
// int main(void)
// {
//     SysTick_Config(SystemCoreClock / 1000);
//     SEGGER_RTT_Init();

//     os_sched_init();
//     os_task_create(&TaskA_TCB, TaskA, NULL, TaskA_Stack, 64, 3, 1);
//     os_task_create(&TaskB_TCB, TaskB, NULL, TaskB_Stack, 64, 2, 1);
//     os_task_create(&TaskC_TCB, TaskC, NULL, TaskC_Stack, 64, 1, 1);
//     os_task_ready(&TaskA_TCB);
//     os_task_ready(&TaskB_TCB);
//     os_task_ready(&TaskC_TCB);
//     // os_sem_init(&Count,1);
//     os_mutex_init(&Count);
//     SEGGER_RTT_WriteString(0, "Hello RTT!\r\n");
//     // 第 4 步：系统点火！(此函数一调用，CPU 就被 RTOS 接管了)
//     // 请确保你在 os_core.c 里加了那个 os_start() 函数哦！
//     os_start();

//     while (1);
// }


#include "SEGGER_RTT.h"
#include "stm32f10x.h"
#include "os_core.h"

// ==========================================
// 1. 邮局基础设施筹备
// ==========================================
// 定义一个真实的包裹类型（比如传感器数据）
typedef struct {
    uint8_t  sensor_id;  // 传感器编号
    uint32_t value;      // 传感器读数
} sensor_msg_t;

#define MAX_MSGS 5 // 邮局货架最多放 5 个包裹

// 分配邮局和货架内存
os_msg_queue_t my_queue;
uint8_t my_queue_pool[MAX_MSGS * sizeof(sensor_msg_t)]; 

// 2. 分配任务堆栈和档案袋 (TCB)
uint32_t TaskA_Stack[256];
uint32_t TaskB_Stack[256];
uint32_t TaskC_Stack[256];
os_tcb_t TaskA_TCB;
os_tcb_t TaskB_TCB;
os_tcb_t TaskC_TCB;

extern uint32_t os_ready_bitmap;

void SysTick_Handler(void)
{
    os_tick_handler();
}

// ==========================================
// 3. 任务剧本
// ==========================================

void TaskA(void *param) // 发送者 (中优先级 2)
{
    sensor_msg_t send_msg;
    send_msg.sensor_id = 1;
    send_msg.value = 1000;

    while (1) {
        send_msg.value += 10; // 模拟传感器数据变化
        
        SEGGER_RTT_printf(0, "[Task A] 准备发货... ID:%d, Value:%d\r\n", send_msg.sensor_id, send_msg.value);
        
        // 往邮局塞包裹
        os_msg_send(&my_queue, &send_msg);
        
        os_delay(1000); // 每秒发一次
    }
}

void TaskB(void *param) // 接收者 (高优先级 3)
{
    sensor_msg_t recv_msg;

    while (1) {
        // 收件人在这里死等！如果没有包裹，他会被关进小黑屋，绝对不占用 CPU！
        os_msg_recv(&my_queue, &recv_msg);
        
        // 一旦代码走到这里，说明绝对拿到包裹了，立刻打印！
        SEGGER_RTT_printf(0, "[Task B] 收到包裹啦！ID:%d, Value:%d\r\n", recv_msg.sensor_id, recv_msg.value);
    }
}

void TaskC(void *param) // 心跳打工人 (低优先级 1)
{
    while (1) {
        SEGGER_RTT_printf(0, "[Task C] 系统心跳滴答...\r\n");
        os_delay(500); 
    }
}

// ==========================================
// 4. 初始化与点火
// ==========================================
int main(void)
{
    SysTick_Config(SystemCoreClock / 1000);
    SEGGER_RTT_Init();

    os_sched_init();
    
    // 初始化消息队列 (关键步骤！)
    // os_msg_queue_init(邮局指针, 货架内存, 每个包裹大小, 货架容量);
    // 注意：这里需要你之前写的 init 函数名字匹配，如果叫 os_msg_init 就改一下
    os_msg_init(&my_queue, my_queue_pool, sizeof(sensor_msg_t), MAX_MSGS);

    // 优先级分配：收件人(3) > 发件人(2) > 心跳(1)
    os_task_create(&TaskB_TCB, TaskB, NULL, TaskB_Stack, 256, 3, 1);
    os_task_create(&TaskA_TCB, TaskA, NULL, TaskA_Stack, 256, 2, 1);
    os_task_create(&TaskC_TCB, TaskC, NULL, TaskC_Stack, 256, 1, 1);
    
    os_task_ready(&TaskA_TCB);
    os_task_ready(&TaskB_TCB);
    os_task_ready(&TaskC_TCB);
    
    SEGGER_RTT_WriteString(0, "MiniRTOS Booting with Message Queue...\r\n");
    
    os_start();

    while (1);
}
