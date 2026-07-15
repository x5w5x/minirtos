/*
 * @Author: 轩
 * @Date: 2026-07-15 23:22:16
 * @LastEditTime: 2026-07-15 23:38:16
 * @FilePath: \minirtos\vm\vm_isa.h
 */
#ifndef VM_ISA_H
#define VM_ISA_H
#include "stdint.h"
typedef enum {
    op_load=0x00,
    op_mov,
    op_add,
    op_sub,
    op_jump,
    op_syscall,
    op_delay

}vm_opcode_t;

typedef struct {
    vm_opcode_t opcode;
    uint32_t reg[3]; 
}vm_inst_t;


#endif // !VM_ISA_H