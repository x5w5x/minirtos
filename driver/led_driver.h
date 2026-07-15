/*
 * @Author: 轩
 * @Date: 2026-07-15 22:55:15
 * @LastEditTime: 2026-07-15 22:55:20
 * @FilePath: \minirtos\driver\led_driver.h
 */
#ifndef LED_DRIVER_H
#define LED_DRIVER_H

#include "vfs.h"
#include "stm32f10x.h"
int led_register(const char *name, GPIO_TypeDef *gpiox, uint16_t pin, uint8_t act_val);

int led_ioctl(os_device_t *dev, int cmd, void *args);

int led_open(os_device_t *dev, uint16_t flag);
int led_close(os_device_t *dev);


#endif // !LED_DRIVER
