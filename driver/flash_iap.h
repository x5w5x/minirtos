/*
 * @Author: 轩
 * @Date: 2026-07-17 19:05:38
 * @LastEditTime: 2026-07-17 19:10:28
 * @FilePath: \minirtos\driver\flash_iap.h
 */
#ifndef FLASH_IAP_H
#define FLASH_IAP_H

#include "stm32f10x.h"


#define FLASH_PAGE_SIZE    (1024) 

#define APP_START_ADDR     (0x08000000 + 16 * 1024) 
#define APP_MAX_SIZE       (16 * 1024) 

void flash_iap_init(void);
int flash_iap_erase(uint32_t addr, uint32_t size);
int flash_iap_write(uint32_t addr, uint8_t *data, uint32_t size);

#endif