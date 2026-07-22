/*
 * @Author: 轩
 * @Date: 2026-07-21 10:04:08
 * @LastEditTime: 2026-07-21 10:05:24
 * @FilePath: \minirtos\driver\adc_driver.c
 */

#include "adc_driver.h"

#define ADC_MAX_NUM 4

// 利用位域压缩结构体，剔除无用临时变量，总占用仅 6 字节
typedef struct {
    ADC_TypeDef *adcx;           // 4 bytes
    uint8_t      channel;        // 1 byte: 通道号 0~15
    uint8_t      sample_time : 7;// 7 bits: ADC_SampleTime_x 
    uint8_t      is_used     : 1;// 1 bit:  注册标记
} adc_driver_t;

static os_device_t  adc_device[ADC_MAX_NUM];
static adc_driver_t adc_driver[ADC_MAX_NUM];

// ==========================================
// 把 channel 翻译成 GPIO 引脚
// ==========================================
static void adc_get_pin(uint8_t ch, GPIO_TypeDef **port, uint16_t *pin)
{
    if (ch <= 7) {
        *port = GPIOA;
        *pin  = (uint16_t)(GPIO_Pin_0 << ch);
    } else if (ch <= 9) {
        *port = GPIOB;
        *pin  = (uint16_t)(GPIO_Pin_0 << (ch - 8));
    } else if (ch <= 15) {
        *port = GPIOC;
        *pin  = (uint16_t)(GPIO_Pin_0 << (ch - 10));
    } else {
        *port = GPIOA;
        *pin  = GPIO_Pin_0;
    }
}

// ==========================================
// VFS 回调实现
// ==========================================
int adc_open(os_device_t *dev, uint16_t flag)
{
    adc_driver_t *adc = (adc_driver_t *)dev->private_data;
    GPIO_TypeDef *port;
    uint16_t      pin;

    // 1. 使能对应 GPIO 时钟，并配置成模拟输入
    adc_get_pin(adc->channel, &port, &pin);
    if (port == GPIOA)
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    else if (port == GPIOB)
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    else
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin  = pin;
    gpio.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(port, &gpio);

    // 2. 安全易读的时钟使能
    if (adc->adcx == ADC1)
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
    else if (adc->adcx == ADC2)
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC2, ENABLE);
    else if (adc->adcx == ADC3)
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC3, ENABLE);

    // 【关键修复】：必须配置 ADC 时钟分频，STM32F103 的 ADC 极限是 14MHz (72MHz / 6 = 12MHz)
    RCC_ADCCLKConfig(RCC_PCLK2_Div6); 

    // 3. ADC 基本配置
    ADC_InitTypeDef adc_init;
    adc_init.ADC_Mode               = ADC_Mode_Independent;
    adc_init.ADC_ScanConvMode       = DISABLE;
    adc_init.ADC_ContinuousConvMode = DISABLE;
    adc_init.ADC_ExternalTrigConv   = ADC_ExternalTrigConv_None;
    adc_init.ADC_DataAlign          = ADC_DataAlign_Right;
    adc_init.ADC_NbrOfChannel       = 1;
    ADC_Init(adc->adcx, &adc_init);

    // 4. 使能并复位校准
    ADC_Cmd(adc->adcx, ENABLE);
    ADC_ResetCalibration(adc->adcx);
    while (ADC_GetResetCalibrationStatus(adc->adcx));
    ADC_StartCalibration(adc->adcx);
    while (ADC_GetCalibrationStatus(adc->adcx));

    return 0;
}

int adc_close(os_device_t *dev)
{
    adc_driver_t *adc = (adc_driver_t *)dev->private_data;
    ADC_Cmd(adc->adcx, DISABLE);
    return 0;
}

// read: 零缓存直接读取，提高实时性
int adc_read(os_device_t *dev, uint32_t pos, void *buffer, uint32_t size)
{
    uint16_t     *buf = (uint16_t *)buffer;
    adc_driver_t *adc = (adc_driver_t *)dev->private_data;

    if (size < sizeof(uint16_t))
        return -1;

    // 单次配置并触发
    ADC_RegularChannelConfig(adc->adcx, adc->channel, 1, adc->sample_time);
    ADC_SoftwareStartConvCmd(adc->adcx, ENABLE);
    
    // 等待转换完成
    while (!ADC_GetFlagStatus(adc->adcx, ADC_FLAG_EOC));
    ADC_ClearFlag(adc->adcx, ADC_FLAG_EOC);
    
    // 直接将结果存入应用层传来的 buffer
    *buf = ADC_GetConversionValue(adc->adcx);
    return sizeof(uint16_t);
}

int adc_write(os_device_t *dev, uint32_t pos, const void *buffer, uint32_t size)
{
    return -1; // ADC 是纯输入设备，不支持写操作，拦截返回 -1
}

int adc_ioctl(os_device_t *dev, int cmd, void *args)
{
    adc_driver_t *adc = (adc_driver_t *)dev->private_data;
    switch (cmd) {
        case ADC_CMD_DISABLE:  // 停止
            ADC_Cmd(adc->adcx, DISABLE);
            break;
        case ADC_CMD_ENABLE:  // 启动
            ADC_Cmd(adc->adcx, ENABLE);
            break;
        case ADC_CMD_SET_SAMPLE_TIME:  // 修改采样时间
            // 【关键修复】统一采用强转传数值，杜绝 HardFault
            adc->sample_time = (uint32_t)args; 
            break;
        default:
            return -1;
    }
    return 0;
}

int adc_register(const char *name, ADC_TypeDef *adcx, uint8_t channel,
                 uint8_t sample_time)
{
    if (channel > 15) return -1;

    for (int i = 0; i < ADC_MAX_NUM; i++) {
        if (!adc_driver[i].is_used) {
            adc_driver[i].is_used      = 1;
            adc_driver[i].adcx         = adcx;
            adc_driver[i].channel      = channel;
            // 缺省默认采样时间，避免填 0 导致配置异常
            adc_driver[i].sample_time  = sample_time ? sample_time : ADC_SampleTime_55Cycles5;

            adc_device[i].private_data = &adc_driver[i];
            adc_device[i].name         = name;
            adc_device[i].open         = adc_open;
            adc_device[i].close        = adc_close;
            adc_device[i].read         = adc_read;
            adc_device[i].write        = adc_write;
            adc_device[i].ioctl        = adc_ioctl;
            
            vfs_register_device(&adc_device[i]);
            return 0;
        }
    }
    return -1;
}