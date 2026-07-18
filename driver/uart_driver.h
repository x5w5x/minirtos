#ifndef UART_DRIVER 
#define UART_DRIVER

#include "vfs.h"
#include "stm32f10x.h"
int uart_register(const char *name, USART_TypeDef *uartx, uint32_t baud);

int uart_ioctl(os_device_t *dev, int cmd, void *args);

int uart_open(os_device_t *dev, uint16_t flag);
int uart_close(os_device_t *dev);
int uart_read(os_device_t *dev, uint32_t pos, void *buffer, uint32_t size);
int uart_write(os_device_t *dev, uint32_t pos, const void *buffer, uint32_t size);
#endif // !UART_DRIVER 