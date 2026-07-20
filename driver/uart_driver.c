// #include "uart_driver.h"

// #define UART_MAX_NUM 2

// typedef struct {
//     USART_TypeDef *uartx;
//     int is_used;
//     uint32_t baud;
// } uart_driver_t;

// static os_device_t uart_device[UART_MAX_NUM];
// static uart_driver_t uart_driver[UART_MAX_NUM];

// int uart_open(os_device_t *dev, uint16_t flag)
// {
//     GPIO_InitTypeDef gpio;
//     uart_driver_t *uart = (uart_driver_t *)dev->private_data;

//     if (uart->uartx == USART1) {
//         RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);
//         gpio.GPIO_Pin   = GPIO_Pin_9;
//         gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
//         gpio.GPIO_Speed = GPIO_Speed_50MHz;
//         GPIO_Init(GPIOA, &gpio);
//         gpio.GPIO_Pin   = GPIO_Pin_10;
//         gpio.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
//         GPIO_Init(GPIOA, &gpio);

//     } else if (uart->uartx == USART2) {
//         RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
//         RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
//         gpio.GPIO_Pin   = GPIO_Pin_2;
//         gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
//         gpio.GPIO_Speed = GPIO_Speed_50MHz;
//         GPIO_Init(GPIOA, &gpio);
//         gpio.GPIO_Pin   = GPIO_Pin_3;
//         gpio.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
//         GPIO_Init(GPIOA, &gpio);
//     }

//     USART_InitTypeDef usart;
//     usart.USART_BaudRate            = uart->baud;
//     usart.USART_WordLength          = USART_WordLength_8b;  // 默认8位数据位
//     usart.USART_StopBits            = USART_StopBits_1;     // 默认1位停止位
//     usart.USART_Parity              = USART_Parity_No;      // 无校验
//     usart.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
//     usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None; // 默认无硬件流

//     USART_Init(uart->uartx, &usart);
//     USART_Cmd(uart->uartx, ENABLE);
//     SEGGER_RTT_printf(0,"uart open");
//     return 0;
// }

// int uart_close(os_device_t *dev)
// {
//     uart_driver_t *uart = (uart_driver_t *)dev->private_data;
//     USART_Cmd(uart->uartx, DISABLE);
//     return 0;
// }

// int uart_read(os_device_t *dev, uint32_t pos, void *buffer, uint32_t size)
// {
//     uint8_t *buf = (uint8_t *)buffer;
//     uart_driver_t *uart = (uart_driver_t *)dev->private_data;

//     for (uint32_t i = 0; i < size; i++) {
//         while (USART_GetFlagStatus(uart->uartx, USART_FLAG_RXNE) == RESET);
//         buf[i] = USART_ReceiveData(uart->uartx);
//     }
//     return size;
// }

// int uart_write(os_device_t *dev, uint32_t pos, const void *buffer, uint32_t size)
// {
//     const uint8_t *buf = (const uint8_t *)buffer;
//     uart_driver_t *uart = (uart_driver_t *)dev->private_data;
    
//     for (uint32_t i = 0; i < size; i++) {
//        while (USART_GetFlagStatus(uart->uartx, USART_FLAG_TXE) == RESET);
//         USART_SendData(uart->uartx, buf[i]);
//     }
//     while (USART_GetFlagStatus(uart->uartx, USART_FLAG_TC) == RESET);
//     return size;
// }

// int uart_ioctl(os_device_t *dev, int cmd, void *args)
// {
//     uart_driver_t *uart = (uart_driver_t *)dev->private_data;
//     switch (cmd) {
//         case 0x00:
//             USART_Cmd(uart->uartx, DISABLE);
//             break;
//         case 0x01:
//             USART_Cmd(uart->uartx, ENABLE);
//             break;
//     }
//     return 0;
// }

// int uart_register(const char *name, USART_TypeDef *uartx, uint32_t baud)
// {
//     for (int i = 0; i < UART_MAX_NUM; i++) {
//         if (!uart_driver[i].is_used) {
//             uart_driver[i].is_used      = 1;
//             uart_driver[i].uartx        = uartx;
//             uart_driver[i].baud         = baud;
//             uart_device[i].private_data = &uart_driver[i];
//             uart_device[i].name         = name;
//             uart_device[i].open         = uart_open;
//             uart_device[i].close        = uart_close;
//             uart_device[i].read         = uart_read;
//             uart_device[i].write        = uart_write;
//             uart_device[i].ioctl        = uart_ioctl;
//             vfs_register_device(&uart_device[i]);
//             return 0;
//         }
//     }
//     return -1;
// }
#include "uart_driver.h"

#define UART_MAX_NUM 2
// ==========================================
// 【新增】512 字节的环形缓冲区 
// ==========================================
#define UART_RX_BUF_SIZE 512
static uint8_t  rx_buf[UART_RX_BUF_SIZE];
static volatile uint32_t rx_head = 0; 
static volatile uint32_t rx_tail = 0; 

typedef struct {
    USART_TypeDef *uartx;
    int is_used;
    uint32_t baud;
} uart_driver_t;

static os_device_t uart_device[UART_MAX_NUM];
static uart_driver_t uart_driver[UART_MAX_NUM];

// ==========================================
// 【核心修复】强壮的中断服务函数
// ==========================================
void USART1_IRQHandler(void)
{
    // 1. 正常接收到数据
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
        rx_buf[rx_head] = USART_ReceiveData(USART1);
        rx_head = (rx_head + 1) % UART_RX_BUF_SIZE;
    }
    
    // 2. 【极其关键】清除溢出错误 (ORE) 标志
    // 如果不加这三行，一旦因为高负载丢包，系统就会陷入无限中断死循环！
    if (USART_GetFlagStatus(USART1, USART_FLAG_ORE) != RESET) {
        USART_ReceiveData(USART1); // 连续读两次或者读一次 DR 就可以清空 ORE
    }
}

int uart_open(os_device_t *dev, uint16_t flag)
{
    GPIO_InitTypeDef gpio;
    uart_driver_t *uart = (uart_driver_t *)dev->private_data;

    rx_head = 0;
    rx_tail = 0;

    if (uart->uartx == USART1) {
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);
        gpio.GPIO_Pin   = GPIO_Pin_9;
        gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
        gpio.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_Init(GPIOA, &gpio);
        gpio.GPIO_Pin   = GPIO_Pin_10;
        gpio.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
        GPIO_Init(GPIOA, &gpio);

        // 配置中断优先级，防止中断风暴
        NVIC_InitTypeDef NVIC_InitStructure;
        NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0; 
        NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
        NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
        NVIC_Init(&NVIC_InitStructure);

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
    usart.USART_WordLength          = USART_WordLength_8b;  
    usart.USART_StopBits            = USART_StopBits_1;     
    usart.USART_Parity              = USART_Parity_No;      
    usart.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None; 

    USART_Init(uart->uartx, &usart);
    
    // 只对作为系统的 USART1 开启接收中断
    if (uart->uartx == USART1) {
        USART_ITConfig(uart->uartx, USART_IT_RXNE, ENABLE);
    }
    
    USART_Cmd(uart->uartx, ENABLE);
    SEGGER_RTT_printf(0,"uart open");
    return 0;
}

int uart_close(os_device_t *dev)
{
    uart_driver_t *uart = (uart_driver_t *)dev->private_data;
    if (uart->uartx == USART1) {
        USART_ITConfig(uart->uartx, USART_IT_RXNE, DISABLE);
    }
    USART_Cmd(uart->uartx, DISABLE);
    return 0;
}

int uart_read(os_device_t *dev, uint32_t pos, void *buffer, uint32_t size)
{
    uint8_t *buf = (uint8_t *)buffer;
    uart_driver_t *uart = (uart_driver_t *)dev->private_data;

    // 如果不是 USART1（比如 USART2 没有环形缓冲），仍然走原来的死等逻辑
    if (uart->uartx != USART1) {
        for (uint32_t i = 0; i < size; i++) {
            while (USART_GetFlagStatus(uart->uartx, USART_FLAG_RXNE) == RESET);
            buf[i] = USART_ReceiveData(uart->uartx);
        }
        return size;
    }

    // USART1 走高性能环形缓冲 + os_delay 让出 CPU！
    for (uint32_t i = 0; i < size; i++) {
        while (rx_head == rx_tail) {
            os_delay(1); // 因为有了中断兜底，这句再也不会卡死硬件了！
        }
        buf[i] = rx_buf[rx_tail];
        rx_tail = (rx_tail + 1) % UART_RX_BUF_SIZE;
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
        case 0xFF: // 【新增】彻底清空环形缓冲区，用于 iap 接收失败时抽干垃圾数据
            rx_head = 0;
            rx_tail = 0;
            break;
    }
    return 0;
}

int uart_register(const char *name, USART_TypeDef *uartx, uint32_t baud)
{
    // 这个函数不需要动，保持原样即可[cite: 5]
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