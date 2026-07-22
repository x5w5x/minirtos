/*
 * @Author: 轩
 * @Date: 2026-07-21 10:03:26
 * @LastEditTime: 2026-07-21 10:03:33
 * @FilePath: \minirtos\driver\adc_driver.h
 */
#ifndef ADC_DRIVER
#define ADC_DRIVER

#include "vfs.h"
#include "stm32f10x.h"


#define ADC_CMD_DISABLE         0x00
#define ADC_CMD_ENABLE          0x01
#define ADC_CMD_SET_SAMPLE_TIME 0x02

int adc_register(const char *name, ADC_TypeDef *adcx, uint8_t channel,
                 uint8_t sample_time);

int adc_ioctl(os_device_t *dev, int cmd, void *args);
int adc_open(os_device_t *dev, uint16_t flag);
int adc_close(os_device_t *dev);
int adc_read(os_device_t *dev, uint32_t pos, void *buffer, uint32_t size);
int adc_write(os_device_t *dev, uint32_t pos, const void *buffer, uint32_t size);

#endif // !ADC_DRIVER