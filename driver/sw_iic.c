/*
 * @Author: 轩
 * @Date: 2026-07-19 16:38:00
 * @LastEditTime: 2026-07-19 16:38:14
 * @FilePath: \minirtos\driver\sw_iic.c
 */
#include "sw_iic.h"
#include "stm32f10x.h"

/* 
 * 硬件连接约定：
 * SCL -> PB6
 * SDA -> PB7
 */
#define I2C_SCL_PORT  GPIOB
#define I2C_SCL_PIN   GPIO_Pin_6
#define I2C_SDA_PORT  GPIOB
#define I2C_SDA_PIN   GPIO_Pin_7

#define SCL_H()  GPIO_SetBits(I2C_SCL_PORT, I2C_SCL_PIN)
#define SCL_L()  GPIO_ResetBits(I2C_SCL_PORT, I2C_SCL_PIN)
#define SDA_H()  GPIO_SetBits(I2C_SDA_PORT, I2C_SDA_PIN)
#define SDA_L()  GPIO_ResetBits(I2C_SDA_PORT, I2C_SDA_PIN)

// 微秒级短延时，控制 I2C 波特率 (约 400kHz)
static void sw_i2c_delay(void) {
    uint8_t i = 10;
    while(i--);
}

void sw_i2c_init(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    // 配置为开漏输出模式 (推荐外部接 4.7K 上拉电阻。如果没有，可改为推挽 Out_PP)
    GPIO_InitStructure.GPIO_Pin = I2C_SCL_PIN | I2C_SDA_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    SCL_H();
    SDA_H();
}

void sw_i2c_start(void) {
    SDA_H();
    SCL_H();
    sw_i2c_delay();
    SDA_L();
    sw_i2c_delay();
    SCL_L();
}

void sw_i2c_stop(void) {
    SDA_L();
    SCL_H();
    sw_i2c_delay();
    SDA_H();
    sw_i2c_delay();
}

void sw_i2c_send_byte(uint8_t byte) {
    for (uint8_t i = 0; i < 8; i++) {
        if (byte & 0x80) SDA_H();
        else             SDA_L();
        byte <<= 1;
        
        sw_i2c_delay();
        SCL_H();
        sw_i2c_delay();
        SCL_L();
        sw_i2c_delay();
    }
    
    // OLED 通常不需要严格处理设备的 ACK 应答，直接发一个时钟跳过应答位即可
    SDA_H(); 
    sw_i2c_delay();
    SCL_H();
    sw_i2c_delay();
    SCL_L();
}