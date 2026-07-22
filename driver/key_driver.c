/*
 * @Author: 轩
 * @Date: 2026-07-21 11:58:49
 * @LastEditTime: 2026-07-21 11:59:56
 * @FilePath: \minirtos\driver\key_driver.c
 */

#include "key_driver.h"

#define KEY_MAX_NUM       4
#define KEY_DEBOUNCE_MS   20      // 默认 20ms 消抖

// 由 RTOS 提供（毫秒级 tick），与 uart_driver 用 os_delay(1) 同源
extern uint32_t os_tick_get(void);

typedef struct {
    GPIO_TypeDef *gpiox;
    uint16_t      pin;
    uint8_t       act_value;     // 按下时引脚电平：0 或 1
    uint8_t       pull_up;       // 1 = 上拉输入(IPU)，0 = 下拉输入(IPD)
    uint8_t       state;         // 当前消抖后的状态
    uint8_t       last_raw;      // 上一次采样的原始电平
    uint32_t      last_tick;     // 上一次状态变化的 tick
    uint32_t      press_cnt;     // 按下累计次数
    uint32_t      release_cnt;   // 释放累计次数
    uint8_t       is_used;
} key_driver_t;

static os_device_t  key_device[KEY_MAX_NUM];
static key_driver_t key_driver[KEY_MAX_NUM];

// 读取当前原始电平（返回 0 / 1）
static uint8_t key_read_raw(key_driver_t *key)
{
    return GPIO_ReadInputDataBit(key->gpiox, key->pin) == Bit_SET ? 1 : 0;
}

// 判定此次采样是否按键按下
static uint8_t key_is_pressed(key_driver_t *key, uint8_t raw)
{
    return (raw == key->act_value) ? 1 : 0;
}

// ==========================================
// VFS 回调实现
// ==========================================

int key_open(os_device_t *dev, uint16_t flag)
{
    key_driver_t *key = (key_driver_t *)dev->private_data;

    // 使能对应 GPIO 时钟
    if (key->gpiox == GPIOA)
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    else if (key->gpiox == GPIOB)
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    else
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    // 配置 GPIO：上拉输入 / 下拉输入
    GPIO_InitTypeDef key_init;
    key_init.GPIO_Pin  = key->pin;
    key_init.GPIO_Mode = key->pull_up ? GPIO_Mode_IPU : GPIO_Mode_IPD;
    key_init.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(key->gpiox, &key_init);

    // 初始状态
    key->last_raw     = key_read_raw(key);
    key->state        = key_is_pressed(key, key->last_raw);
    key->last_tick    = os_tick_get();
    key->press_cnt    = 0;
    key->release_cnt  = 0;
    return 0;
}

int key_close(os_device_t *dev)
{
    (void)dev;
    return 0;
}

// 带消抖的状态读取 + 事件计数
static void key_debounce_update(key_driver_t *key)
{
    uint8_t  raw      = key_read_raw(key);
    uint32_t now_tick = os_tick_get();

    if (raw != key->last_raw) {
        // 电平变化 → 重新计时
        key->last_raw  = raw;
        key->last_tick = now_tick;
    }

    if ((now_tick - key->last_tick) >= KEY_DEBOUNCE_MS) {
        uint8_t new_state = key_is_pressed(key, raw);
        if (new_state != key->state) {
            // 状态翻转 → 记一次事件
            if (new_state)  key->press_cnt++;
            else            key->release_cnt++;
            key->state = new_state;
        }
    }
}

// ==========================================
// 核心改造 1: 适配 VM 的 32 位寄存器与异步挂起机制
// ==========================================
int key_read(os_device_t *dev, uint32_t pos, void *buffer, uint32_t size)
{
    // VM 传入的 buffer 实际上是 &(ctx->reg[x])，是一个 32 位的指针
    uint32_t *reg = (uint32_t *)buffer;
    key_driver_t *key = (key_driver_t *)dev->private_data;

    key_debounce_update(key);

    if (key->state == 1) {
        // 如果按键按下，填入干净的 32 位整型 1
        *reg = 1;
        // 返回 4 伪装成读取成功，满足 VM 的 sizeof(int)
        return 4; 
    } else {
        // 【关键】：如果没按下，直接返回 0！
        // 这一步会触发 VM 挂起，当前 App 放弃 CPU，进入极致省电和让权状态
        return 0;
    }
}

int key_write(os_device_t *dev, uint32_t pos, const void *buffer, uint32_t size)
{
    (void)dev;
    (void)buffer;
    (void)size;
    return -1;
}

// ==========================================
// 核心改造 2: 适配 VFS 轮询唤醒机制
// ==========================================
int key_ioctl(os_device_t *dev, int cmd, void *args)
{
    key_driver_t *key = (key_driver_t *)dev->private_data;
    
    // 拦截 VFS_CMD_POLL_READ (0xFF)，供 vm_wakeup 轮询是否需要唤醒 App
    if (cmd == 0xFF) {
        key_debounce_update(key);
        // 若按下则返回 1 (触发唤醒)，否则返回 0 (继续睡)
        return (key->state == 1) ? 1 : 0;
    }

    switch (cmd) {
        case 0x00: {  // 重新读取一次（强行刷新，不等消抖窗）
            uint8_t *out = (uint8_t *)args;
            if (out) {
                key_debounce_update(key);
                *out = key->state;
            }
            break;
        }
        case 0x01: {  // args = uint32_t*：返回按下计数
            if (args) *(uint32_t *)args = key->press_cnt;
            break;
        }
        case 0x02: {  // args = uint32_t*：返回释放计数
            if (args) *(uint32_t *)args = key->release_cnt;
            break;
        }
        case 0x03:  // 清零计数
            key->press_cnt   = 0;
            key->release_cnt = 0;
            break;
        default:
            return -1;
    }
    return 0;
}

int key_register(const char *name, GPIO_TypeDef *gpiox, uint16_t pin,
                 uint8_t act_val, uint8_t pull_up)
{
    for (int i = 0; i < KEY_MAX_NUM; i++) {
        if (!key_driver[i].is_used) {
            key_driver[i].is_used      = 1;
            key_driver[i].gpiox        = gpiox;
            key_driver[i].pin          = pin;
            key_driver[i].act_value    = act_val ? 1 : 0;
            key_driver[i].pull_up      = pull_up ? 1 : 0;
            key_driver[i].state        = 0;
            key_driver[i].last_raw     = 0;
            key_driver[i].last_tick    = 0;
            key_driver[i].press_cnt    = 0;
            key_driver[i].release_cnt  = 0;

            key_device[i].private_data = &key_driver[i];
            key_device[i].name         = name;
            key_device[i].open         = key_open;
            key_device[i].close        = key_close;
            key_device[i].read         = key_read;
            key_device[i].write        = key_write;
            key_device[i].ioctl        = key_ioctl;
            vfs_register_device(&key_device[i]);
            return 0;
        }
    }
    return -1;
}