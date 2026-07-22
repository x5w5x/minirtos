/*
 * @Author: 轩
 * @Date: 2026-07-17 13:31:46
 * @LastEditTime: 2026-07-21 11:22:05
 * @FilePath: \minirtos\vm\vm_task.h
 */
#ifndef VM_TASK_H
#define VM_TASK_H

#include "vm_isa.h"
#define VM_MAGIC 0x4D56514D
#define APP_START_ADDR     (0x08000000 + 24 * 1024) 
#define APP_SLOT_SIZE (1*1024)
#define APP_SLOT_NUM  8
#define APP_VERSION_OFFSET (APP_SLOT_SIZE - 4)




#define APP_NUM 4



#pragma pack(push, 1)
typedef struct {
    uint32_t offset;
    uint32_t size;
} AppTocEntry_t;

typedef struct {
    uint32_t magic;
    uint32_t app_count;
    AppTocEntry_t toc[]; 
} VmBinHeader_t;
#pragma pack(pop)

void vmtask(void *param);

void vm_unload_apps();
// void vm_load_apps(uint32_t base_addr);
void vm_load_apps();
 void vm_find_apps();
#endif // !VM_TASK_H
