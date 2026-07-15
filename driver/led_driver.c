/*
 * @Author: 轩
 * @Date: 2026-07-15 21:54:49
 * @LastEditTime: 2026-07-15 22:48:37
 * @FilePath: \minirtos\driver\led_driver.c
 */
#include "led_driver.h"
#define LED_MAX_NUM 4

typedef struct {
    GPIO_TypeDef *gpiox;
    uint16_t pin;
    int is_used;
    uint8_t act_value;
} led_driver_t;

static os_device_t led_device[LED_MAX_NUM];
static led_driver_t led_driver[LED_MAX_NUM];

int led_open(os_device_t *dev, uint16_t flag)
{
    led_driver_t *led = (led_driver_t *)dev->private_data;
    if (led->gpiox == GPIOA)
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    else if (led->gpiox == GPIOB)
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    else
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    GPIO_InitTypeDef led_init;
    led_init.GPIO_Mode  = GPIO_Mode_Out_PP;
    led_init.GPIO_Speed = GPIO_Speed_50MHz;
    led_init.GPIO_Pin   = led->pin;
    GPIO_Init(led->gpiox, &led_init);
    return 0;
}
int led_write(os_device_t *dev, uint32_t pos, const void *buffer, uint32_t size)
{
    uint8_t status    = *(const uint8_t *)buffer;
    led_driver_t *led = (led_driver_t *)dev->private_data;

    if (status == 1) {

        GPIO_WriteBit(led->gpiox, led->pin, led->act_value == 1);
    } else {

        GPIO_WriteBit(led->gpiox, led->pin, led->act_value == 0);
    }

    return size;
}

int led_ioctl(os_device_t *dev, int cmd, void *args)
{
    led_driver_t *led = (led_driver_t *)dev->private_data;
    switch (cmd) {
        case 0x00:
            GPIO_WriteBit(led->gpiox, led->pin, led->act_value == 0);
            break;
        case 0x01:
            GPIO_WriteBit(led->gpiox, led->pin, led->act_value == 1);
            break;
        case 0x02:
            GPIO_WriteBit(led->gpiox, led->pin, (BitAction)(1 - GPIO_ReadOutputDataBit(led->gpiox, led->pin)));
    }

    return 0;
}

int led_close(os_device_t *dev)
{
    led_driver_t *led = (led_driver_t *)dev->private_data;
    if (led->act_value == 1)
        GPIO_WriteBit(led->gpiox, led->pin,  0);
    else
        GPIO_WriteBit(led->gpiox, led->pin, 1);
    return 0;
}
// int led_driver_init(void)
// {
//     led_device.name         = "sys_led";
//     led_device.private_data = &sys_led;
//     led_device.write        = led_write;
//     led_device.ioctl        = led_ioctl;
//     vfs_register_device(&led_device);
//     return 0;
// }

int led_register(const char *name, GPIO_TypeDef *gpiox, uint16_t pin, uint8_t act_val)
{
    for (int i = 0; i < LED_MAX_NUM; i++) {
        if (!led_driver[i].is_used) {
            led_driver[i].is_used      = 1;
            led_driver[i].gpiox        = gpiox;
            led_driver[i].pin          = pin;
            led_driver[i].act_value    = act_val;
            led_device[i].private_data = &led_driver[i];
            led_device[i].name         = name;
            led_device[i].open         = led_open;
            led_device[i].write        = led_write;
            led_device[i].ioctl        = led_ioctl;
            vfs_register_device(&led_device[i]);
            return 0;
        }
    }
    return -1;
}
