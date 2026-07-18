/*
 * @Author: 轩
 * @Date: 2026-07-15 23:22:16
 * @LastEditTime: 2026-07-18 19:18:41
 * @FilePath: \minirtos\vm\vm_isa.h
 */
#ifndef VM_ISA_H
#define VM_ISA_H
#include "stdint.h"

#define SYS_VFS_OPEN   1
#define SYS_VFS_CLOSE  2
#define SYS_VFS_IOCTL  3
#define SYS_VFS_WRITE  4
#define SYS_VFS_READ   5

#define VFS_CMD_POLL_READ  0xFF

extern volatile uint32_t os_sys_tick;
typedef enum {
    op_load=0x00,
    op_mov,
    op_add,
    op_sub,
    op_jump,
    op_syscall,
    op_delay,

    op_cmp,
    op_jeq, 
    op_jne,
    op_jgt,
    op_jlt,

}vm_opcode_t;

typedef struct {
    vm_opcode_t opcode;
    int32_t reg[3]; 
}vm_inst_t;

typedef struct {
    uint8_t is_run;
    uint8_t state;
    uint32_t wack_up_tick;
    
    int wait_fd;
    uint8_t *bytecode;
    uint32_t pc;
    int32_t reg[16];

     
}vm_app_context_t;

void vm_run(vm_app_context_t *ctx);
void vm_wakeup(vm_app_context_t *ctx);
#endif // !VM_ISA_H