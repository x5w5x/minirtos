/*
 * @Author: 轩
 * @Date: 2026-07-21 11:57:57
 * @LastEditTime: 2026-07-21 11:58:02
 * @FilePath: \minirtos\driver\key_driver.h
 */

#ifndef KEY_DRIVER
#define KEY_DRIVER

#include "vfs.h"
#include "stm32f10x.h"

int key_register(const char *name, GPIO_TypeDef *gpiox, uint16_t pin,
                 uint8_t act_val, uint8_t pull_up);

int key_ioctl(os_device_t *dev, int cmd, void *args);

int key_open(os_device_t *dev, uint16_t flag);
int key_close(os_device_t *dev);
int key_read(os_device_t *dev, uint32_t pos, void *buffer, uint32_t size);
int key_write(os_device_t *dev, uint32_t pos, const void *buffer, uint32_t size);

#endif // !KEY_DRIVER