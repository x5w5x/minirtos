#ifndef SW_IIC_H
#define SW_IIC_H

#include <stdint.h>


void sw_i2c_init(void);
void sw_i2c_start(void);
void sw_i2c_stop(void);
void sw_i2c_send_byte(uint8_t byte);

#endif // SW_I2C_H