/*
 * @Author: 轩
 * @Date: 2026-07-20 20:43:50
 * @LastEditTime: 2026-07-20 20:45:16
 * @FilePath: \minirtos\driver\pwm_driver.h
 */

#ifndef PWM_DRIVER
#define PWM_DRIVER

#include "vfs.h"
#include "stm32f10x.h"

// IOCTL 推荐宏定义
#define PWM_CMD_DISABLE  0x00
#define PWM_CMD_ENABLE   0x01
#define PWM_CMD_SET_DUTY 0x02

int pwm_register(const char *name, TIM_TypeDef *timx, uint8_t channel,
                 uint16_t period, uint16_t prescaler);

int pwm_ioctl(os_device_t *dev, int cmd, void *args);
int pwm_open(os_device_t *dev, uint16_t flag);
int pwm_close(os_device_t *dev);
int pwm_read(os_device_t *dev, uint32_t pos, void *buffer, uint32_t size);
int pwm_write(os_device_t *dev, uint32_t pos, const void *buffer, uint32_t size);

#endif // !PWM_DRIVER