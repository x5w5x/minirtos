/*
 * @Author: 轩
 * @Date: 2026-07-17 13:31:46
 * @LastEditTime: 2026-07-18 12:46:41
 * @FilePath: \minirtos\vm\vm_task.h
 */
#ifndef VM_TASK_H
#define VM_TASK_H

#include "vm_isa.h"
#define VM_MAGIC 0x4D56514D
#define APP_START_ADDR     (0x08000000 + 16 * 1024) 
#define APP_NUM 4

static vm_app_context_t ctx[APP_NUM];

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
void vm_load_apps();
#endif // !VM_TASK_H
