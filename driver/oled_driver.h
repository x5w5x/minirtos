/*
 * @Author: 轩
 * @Date: 2026-07-19 16:39:09
 * @LastEditTime: 2026-07-19 16:53:12
 * @FilePath: \minirtos\driver\oled_driver.h
 */
#ifndef OLED_DRIVER_H
#define OLED_DRIVER_H

#include <stdint.h>

/* --- OLED VFS IOCTL 命令字 --- */
#define OLED_IOC_UPDATE_SCREEN  0x01  // 刷入全屏显存
#define OLED_IOC_CLEAR_BUFFER   0x02  // 清空内存缓冲
#define OLED_IOC_DRAW_POINT     0x03  // 画点 (需传入 oled_point_t)
#define OLED_IOC_DRAW_STRING    0x04
// 画点坐标结构体
typedef struct {
    uint8_t x;
    uint8_t y;
    uint8_t color; // 1:亮, 0:灭
} oled_point_t;

typedef struct {
    uint8_t x;
    uint8_t y;
    const char *str;
} oled_string_t;
// 驱动注册入口
int oled_register(const char *name);

#endif // OLED_DRIVER_H