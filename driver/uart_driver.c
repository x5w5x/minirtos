#include "uart_driver.h"

#define UART_MAX_NUM 2

typedef struct {
    USART_TypeDef *uartx;
    int is_used;
    uint32_t baud;
} uart_driver_t;

static os_device_t uart_device[UART_MAX_NUM];
static uart_driver_t uart_driver[UART_MAX_NUM];

int uart_open(os_device_t *dev, uint16_t flag)
{
    GPIO_InitTypeDef gpio;
    uart_driver_t *uart = (uart_driver_t *)dev->private_data;

    if (uart->uartx == USART1) {
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);
        gpio.GPIO_Pin   = GPIO_Pin_9;
        gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
        gpio.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_Init(GPIOA, &gpio);
        gpio.GPIO_Pin   = GPIO_Pin_10;
        gpio.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
        GPIO_Init(GPIOA, &gpio);

    } else if (uart->uartx == USART2) {
        RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
        gpio.GPIO_Pin   = GPIO_Pin_2;
        gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
        gpio.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_Init(GPIOA, &gpio);
        gpio.GPIO_Pin   = GPIO_Pin_3;
        gpio.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
        GPIO_Init(GPIOA, &gpio);
    }

    USART_InitTypeDef usart;
    usart.USART_BaudRate            = uart->baud;
    usart.USART_WordLength          = USART_WordLength_8b;  // 默认8位数据位
    usart.USART_StopBits            = USART_StopBits_1;     // 默认1位停止位
    usart.USART_Parity              = USART_Parity_No;      // 无校验
    usart.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None; // 默认无硬件流

    USART_Init(uart->uartx, &usart);
    USART_Cmd(uart->uartx, ENABLE);
    SEGGER_RTT_printf(0,"uart open");
    return 0;
}

int uart_close(os_device_t *dev)
{
    uart_driver_t *uart = (uart_driver_t *)dev->private_data;
    USART_Cmd(uart->uartx, DISABLE);
    return 0;
}

int uart_read(os_device_t *dev, uint32_t pos, void *buffer, uint32_t size)
{
    uint8_t *buf = (uint8_t *)buffer;
    uart_driver_t *uart = (uart_driver_t *)dev->private_data;

    for (uint32_t i = 0; i < size; i++) {
        while (USART_GetFlagStatus(uart->uartx, USART_FLAG_RXNE) == RESET);
        buf[i] = USART_ReceiveData(uart->uartx);
    }
    return size;
}

int uart_write(os_device_t *dev, uint32_t pos, const void *buffer, uint32_t size)
{
    const uint8_t *buf = (const uint8_t *)buffer;
    uart_driver_t *uart = (uart_driver_t *)dev->private_data;
    
    for (uint32_t i = 0; i < size; i++) {
       while (USART_GetFlagStatus(uart->uartx, USART_FLAG_TXE) == RESET);
        USART_SendData(uart->uartx, buf[i]);
    }
    while (USART_GetFlagStatus(uart->uartx, USART_FLAG_TC) == RESET);
    return size;
}

int uart_ioctl(os_device_t *dev, int cmd, void *args)
{
    uart_driver_t *uart = (uart_driver_t *)dev->private_data;
    switch (cmd) {
        case 0x00:
            USART_Cmd(uart->uartx, DISABLE);
            break;
        case 0x01:
            USART_Cmd(uart->uartx, ENABLE);
            break;
    }
    return 0;
}

int uart_register(const char *name, USART_TypeDef *uartx, uint32_t baud)
{
    for (int i = 0; i < UART_MAX_NUM; i++) {
        if (!uart_driver[i].is_used) {
            uart_driver[i].is_used      = 1;
            uart_driver[i].uartx        = uartx;
            uart_driver[i].baud         = baud;
            uart_device[i].private_data = &uart_driver[i];
            uart_device[i].name         = name;
            uart_device[i].open         = uart_open;
            uart_device[i].close        = uart_close;
            uart_device[i].read         = uart_read;
            uart_device[i].write        = uart_write;
            uart_device[i].ioctl        = uart_ioctl;
            vfs_register_device(&uart_device[i]);
            return 0;
        }
    }
    return -1;
}