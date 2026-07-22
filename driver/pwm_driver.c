/*
 * @Author: 轩
 * @Date: 2026-07-20 20:44:36
 * @LastEditTime: 2026-07-20 20:44:42
 * @FilePath: \minirtos\driver\pwm_driver.c
 */

#include "pwm_driver.h"

#define PWM_MAX_NUM 4

// 【优化1】调整成员顺序防对齐浪费，使用位域，砍掉多余的 duty 成员
typedef struct {
    TIM_TypeDef *timx;           // 4 bytes
    uint16_t     period;         // 2 bytes: 自动重装载值 (ARR)
    uint16_t     prescaler;      // 2 bytes: 预分频值 (PSC)
    uint8_t      channel;        // 1 byte: 1~4
    uint8_t      is_used : 1;    // 1 bit: 标记是否被注册 (复用 1 byte 空间)
} pwm_driver_t; 

static os_device_t  pwm_device[PWM_MAX_NUM];
static pwm_driver_t pwm_driver[PWM_MAX_NUM];

// ==========================================
// 把 (timx, channel) 翻译成对应的 GPIO 引脚（默认映射，未开启重映射）
// ==========================================
static void pwm_get_pin(TIM_TypeDef *timx, uint8_t ch,
                        GPIO_TypeDef **port, uint16_t *pin)
{
    *port = GPIOA;     // 给个默认值
    *pin  = GPIO_Pin_0;

    if (timx == TIM1) {
        switch (ch) {
            case 1: *pin = GPIO_Pin_8;  break;
            case 2: *pin = GPIO_Pin_9;  break;
            case 3: *pin = GPIO_Pin_10; break;
            case 4: *pin = GPIO_Pin_11; break;
            default: break;
        }
    } else if (timx == TIM2) {
        switch (ch) {
            case 1: *pin = GPIO_Pin_0;  break;
            case 2: *pin = GPIO_Pin_1;  break;
            case 3: *pin = GPIO_Pin_2;  break;
            case 4: *pin = GPIO_Pin_3;  break;
            default: break;
        }
    } else if (timx == TIM3) {
        switch (ch) {
            case 1: *pin = GPIO_Pin_6;  break;
            case 2: *pin = GPIO_Pin_7;  break;
            case 3: *port = GPIOB, *pin = GPIO_Pin_0; break;
            case 4: *port = GPIOB, *pin = GPIO_Pin_1; break;
            default: break;
        }
    } else if (timx == TIM4) {
        *port = GPIOB;
        switch (ch) {
            case 1: *pin = GPIO_Pin_6;  break;
            case 2: *pin = GPIO_Pin_7;  break;
            case 3: *pin = GPIO_Pin_8;  break;
            case 4: *pin = GPIO_Pin_9;  break;
            default: break;
        }
    }
}

// 获取通道对应的 CCR 寄存器 (非常棒的硬件解耦封装)
static volatile uint16_t *pwm_get_ccr(TIM_TypeDef *timx, uint8_t ch)
{
    switch (ch) {
        case 1: return &timx->CCR1;
        case 2: return &timx->CCR2;
        case 3: return &timx->CCR3;
        case 4: return &timx->CCR4;
        default: return &timx->CCR1;
    }
}

// ==========================================
// VFS 回调实现
// ==========================================

int pwm_open(os_device_t *dev, uint16_t flag)
{
    pwm_driver_t *pwm = (pwm_driver_t *)dev->private_data;
    GPIO_TypeDef *port;
    uint16_t      pin;

    // 1. 打开对应 GPIO 时钟
    pwm_get_pin(pwm->timx, pwm->channel, &port, &pin);
    if (port == GPIOA)
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    else if (port == GPIOB)
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    else
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    // 2. GPIO 复用推挽输出
    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin   = pin;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(port, &gpio);

    // 3. 打开 TIM 时钟
    if (pwm->timx == TIM1)
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
    else if (pwm->timx == TIM2)
        RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    else if (pwm->timx == TIM3)
        RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
    else
        RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);

    // 4. 时基单元
    TIM_TimeBaseInitTypeDef tim;
    tim.TIM_Prescaler         = pwm->prescaler;
    tim.TIM_Period            = pwm->period;
    tim.TIM_CounterMode       = TIM_CounterMode_Up;
    tim.TIM_ClockDivision     = TIM_CKD_DIV1;
    tim.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(pwm->timx, &tim);

    // 5. PWM 模式配置
    TIM_OCInitTypeDef oc;
    oc.TIM_OCMode      = TIM_OCMode_PWM1;
    oc.TIM_OutputState = TIM_OutputState_Enable;
    oc.TIM_OCPolarity  = TIM_OCPolarity_High;
    oc.TIM_Pulse       = 0; // 默认初始占空比为 0 (无需从 RAM 中取值)

    switch (pwm->channel) {
        case 1: TIM_OC1Init(pwm->timx, &oc); break;
        case 2: TIM_OC2Init(pwm->timx, &oc); break;
        case 3: TIM_OC3Init(pwm->timx, &oc); break;
        case 4: TIM_OC4Init(pwm->timx, &oc); break;
        default: return -1;
    }

    // 6. 高级定时器 (TIM1) 需要打开 MOE
    if (pwm->timx == TIM1)
        TIM_CtrlPWMOutputs(pwm->timx, ENABLE);

    // 7. 启动定时器
    TIM_Cmd(pwm->timx, ENABLE);
    return 0;
}

int pwm_close(os_device_t *dev)
{
    pwm_driver_t *pwm = (pwm_driver_t *)dev->private_data;
    TIM_Cmd(pwm->timx, DISABLE);
    if (pwm->timx == TIM1)
        TIM_CtrlPWMOutputs(pwm->timx, DISABLE);
    return 0;
}

// write: buffer 里放的是新的占空比（0 ~ period 之间）
int pwm_write(os_device_t *dev, uint32_t pos, const void *buffer, uint32_t size)
{
    const uint16_t *buf = (const uint16_t *)buffer;
    pwm_driver_t   *pwm = (pwm_driver_t *)dev->private_data;

    if (size < sizeof(uint16_t))
        return -1;

    uint16_t duty = *buf;
    if (duty > pwm->period)
        duty = pwm->period;

    // 【优化2】不再依赖 RAM 副本，直接写硬件！
    *pwm_get_ccr(pwm->timx, pwm->channel) = duty;

    // 立刻把比较值预装载进影子寄存器
    switch (pwm->channel) {
        case 1: TIM_OC1PreloadConfig(pwm->timx, TIM_OCPreload_Enable); break;
        case 2: TIM_OC2PreloadConfig(pwm->timx, TIM_OCPreload_Enable); break;
        case 3: TIM_OC3PreloadConfig(pwm->timx, TIM_OCPreload_Enable); break;
        case 4: TIM_OC4PreloadConfig(pwm->timx, TIM_OCPreload_Enable); break;
        default: return -1;
    }
    return size;
}

// read: 回读当前占空比
int pwm_read(os_device_t *dev, uint32_t pos, void *buffer, uint32_t size)
{
    uint16_t      *buf = (uint16_t *)buffer;
    pwm_driver_t  *pwm = (pwm_driver_t *)dev->private_data;

    if (size < sizeof(uint16_t))
        return -1;

    // 【优化3】绝对的实时性：直接读寄存器里的占空比，而不是读 RAM
    *buf = *pwm_get_ccr(pwm->timx, pwm->channel);
    return sizeof(uint16_t);
}

int pwm_ioctl(os_device_t *dev, int cmd, void *args)
{
    pwm_driver_t *pwm = (pwm_driver_t *)dev->private_data;
    switch (cmd) {
        case PWM_CMD_DISABLE:  // 停止
            TIM_Cmd(pwm->timx, DISABLE);
            break;
            
        case PWM_CMD_ENABLE:  // 启动
            TIM_Cmd(pwm->timx, ENABLE);
            break;
            
        case PWM_CMD_SET_DUTY:  // 设置占空比
            // 【优化4】修复指针陷阱：直接把 void* 当作无符号整数值使用，而不是取地址
            {
                uint16_t duty = (uint32_t)args; 
                if (duty > pwm->period) duty = pwm->period;
                *pwm_get_ccr(pwm->timx, pwm->channel) = duty;
            }
            break;
            
        default:
            return -1;
    }
    return 0;
}

int pwm_register(const char *name, TIM_TypeDef *timx, uint8_t channel,
                 uint16_t period, uint16_t prescaler)
{
    for (int i = 0; i < PWM_MAX_NUM; i++) {
        if (!pwm_driver[i].is_used) {
            pwm_driver[i].is_used      = 1;
            pwm_driver[i].timx         = timx;
            pwm_driver[i].channel      = channel;
            pwm_driver[i].period       = period;
            pwm_driver[i].prescaler    = prescaler;

            pwm_device[i].private_data = &pwm_driver[i];
            pwm_device[i].name         = name;
            pwm_device[i].open         = pwm_open;
            pwm_device[i].close        = pwm_close;
            pwm_device[i].read         = pwm_read;
            pwm_device[i].write        = pwm_write;
            pwm_device[i].ioctl        = pwm_ioctl;
            vfs_register_device(&pwm_device[i]);
            return 0;
        }
    }
    return -1;
}