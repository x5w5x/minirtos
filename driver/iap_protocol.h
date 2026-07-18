/*
 * @Author: 轩
 * @Date: 2026-07-17 20:33:32
 * @LastEditTime: 2026-07-17 21:01:26
 * @FilePath: \minirtos\driver\iap_protocol.h
 */
/*
 * iap_task.c/.h  —— 串口IAP接收任务（轮询版，无中断依赖）
 *
 * 帧格式(变长发送)：
 *   [header:0xA5][cmd:1B][size:2B LE][addr:4B LE][crc:4B LE][data:size字节][tail:0x5A]
 *   注意：上位机只发 size 个data字节，不是固定256！
 *
 * 流程：
 *   START(带总大小+总CRC) -> 擦除 -> ACK
 *   DATA × n (每帧校验CRC后写入)  -> 每帧ACK
 *   FINISH -> 回读Flash算总CRC比对 -> ACK/NACK
 */
#ifndef IAP_TASK_H
#define IAP_TASK_H

#include <stdint.h>

#define IAP_HEADER      0xA5
#define IAP_TAIL        0x5A

#define CMD_START       0x01
#define CMD_DATA        0x02
#define CMD_FINISH      0x03

#define IAP_ACK         0x06
#define IAP_NACK        0x15

#define IAP_DATA_MAX    256

#pragma pack(1)
typedef struct {
    uint8_t  header;            /* 0xA5 */
    uint8_t  cmd;               /* CMD_START / CMD_DATA / CMD_FINISH */
    uint16_t size;              /* data有效长度 */
    uint32_t addr;              /* DATA: 写入地址 */
    uint32_t crc;               /* START: 固件总CRC  DATA: 本帧data的CRC */
    uint8_t  data[IAP_DATA_MAX];
    uint8_t  tail;              /* 0x5A */
} IAP_Frame_t;
#pragma pack()

/* 帧头固定部分长度: header+cmd+size+addr+crc = 12字节 */
#define IAP_FRAME_HEAD_LEN  12

void Task_IAP(void *param);

#endif
