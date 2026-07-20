/*
 * @Author: 轩
 * @Date: 2026-07-17 19:05:27
 * @LastEditTime: 2026-07-20 13:15:58
 * @FilePath: \minirtos\driver\flash_iap.c
 */
#include "flash_iap.h"
#include "stm32f10x_flash.h"
__attribute__((section(".ramfunc")))
static int is_address_safe(uint32_t addr, uint32_t size) {
    if (addr < APP_START_ADDR || (addr + size) > (APP_START_ADDR + APP_MAX_SIZE)) {
        return 0; 
    }
    return 1;
}

void flash_iap_init(void) {
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_CRC, ENABLE); 
    CRC->CR = CRC_CR_RESET;
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
    FLASH_Lock();
}
__attribute__((section(".ramfunc")))
int flash_iap_erase(uint32_t addr, uint32_t size) {
    if (!is_address_safe(addr, size)) return -1;

    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

    uint32_t erase_addr = addr;
    
    while (erase_addr < addr + size) {
        if (FLASH_ErasePage(erase_addr) != FLASH_COMPLETE) {
            FLASH_Lock();
            return -2; 
        }
        erase_addr += FLASH_PAGE_SIZE;
    }

    FLASH_Lock();
    return 0;
}
__attribute__((section(".ramfunc")))
int flash_iap_write(uint32_t addr, uint8_t *data, uint32_t size) {
    if (!is_address_safe(addr, size)) return -3;

    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

    
    for (uint32_t i = 0; i < size; i += 2) {
        uint16_t halfword = data[i] | (data[i + 1] << 8);
        if (FLASH_ProgramHalfWord(addr + i, halfword) != FLASH_COMPLETE) {
            FLASH_Lock();
            return -4;
        }
    }

    FLASH_Lock();
    return 0;
}


// int flash_iap_erase(uint32_t addr, uint32_t size) {
//     if (!is_address_safe(addr, size)) return -1;

//     __disable_irq(); // 1. 进入临界区，锁死中断

//     FLASH_Unlock();
//     FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

//     uint32_t erase_addr = addr;
//     int status = 0; // 用变量记录状态，统一出口

//     while (erase_addr < addr + size) {
//         if (FLASH_ErasePage(erase_addr) != FLASH_COMPLETE) {
//             status = -2;
//             break; // 跳出循环去 Lock 和恢复中断
//         }
//         erase_addr += FLASH_PAGE_SIZE;
//     }

//     FLASH_Lock();
//     __enable_irq(); // 2. 统一恢复中断
//     return status;
// }

// int flash_iap_write(uint32_t addr, uint8_t *data, uint32_t size) {
//     if (!is_address_safe(addr, size)) return -3;

//     __disable_irq(); // 1. 进入临界区

//     FLASH_Unlock();
//     FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

//     int status = 0;
//     for (uint32_t i = 0; i < size; i += 2) {
//         uint16_t halfword = data[i] | (data[i + 1] << 8);
//         if (FLASH_ProgramHalfWord(addr + i, halfword) != FLASH_COMPLETE) {
//             status = -4;
//             break; // 跳出循环去 Lock 和恢复中断
//         }
//     }

//     FLASH_Lock();
//     __enable_irq(); // 2. 统一恢复中断
//     return status;
// }

static uint8_t crc_buffer[4];
static uint8_t crc_index = 0;

void CRC_FeedData(uint8_t byte) {
    crc_buffer[crc_index++] = byte;

    if (crc_index == 4) {
        uint32_t combined_data = (uint32_t)crc_buffer[0] |
                                 ((uint32_t)crc_buffer[1] << 8) |
                                 ((uint32_t)crc_buffer[2] << 16) |
                                 ((uint32_t)crc_buffer[3] << 24);
         
        CRC->DR = combined_data;
         
        crc_index = 0;
    }
}

void CRC_Finish(void) {
    if (crc_index > 0) {

        uint32_t last_data = 0;
        for(int i = 0; i < crc_index; i++) {
            last_data |= ((uint32_t)crc_buffer[i] << (8 * i));
        }
        CRC->DR = last_data;
    }
}