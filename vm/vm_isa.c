
#include "vm_isa.h"
#include "vfs.h"
#include "string.h"
static void do_syscall(vm_app_context_t *ctx, vm_inst_t *inst)
{
    int sys_num = inst->reg[0];

    switch (sys_num) {
        case SYS_VFS_OPEN: {
            const char *dev_name   = (const char *)(ctx->bytecode + inst->reg[2]);
            int fd                 = vfs_open(dev_name);
            ctx->reg[inst->reg[1]] = fd;
            if (fd >= 0 && ctx->fd_count < 8) {
                ctx->fd_table[ctx->fd_count++] = fd;
            }
            break;
        }
        case SYS_VFS_IOCTL: {
            int fd  = ctx->reg[inst->reg[1]];
            int cmd = inst->reg[2];
            int ret = vfs_ioctl(fd, cmd, (void*)ctx->reg[0]);
            break;
        }
        case SYS_VFS_WRITE: {
            int fd             = ctx->reg[inst->reg[1]];
            const char *buffer = (const char *)(ctx->bytecode + inst->reg[2]);
            int ret            = vfs_write(fd, 0, buffer, strlen(buffer));
            break;
        }
        case SYS_VFS_READ: {
            int fd        = ctx->reg[inst->reg[1]];
            int *dest_reg = &ctx->reg[inst->reg[2]];
            int ret       = vfs_read(fd, 0, dest_reg, sizeof(int));
            if (ret <= 0) {
                ctx->state = 2;
                ctx->wait_fd           = fd;
                ctx->pc -= 16;
            }
            break;
        }
        case SYS_VFS_CLOSE: {
            int fd                 = ctx->reg[inst->reg[1]];
            int ret                = vfs_close(fd);
            ctx->reg[inst->reg[1]] = ret; // 返回执行结果给 App
            break;
        }
    }
}

void vm_run(vm_app_context_t *ctx)
{
    int inst_count = 0;
    while (ctx->is_run && (ctx->state == 1)) {

        vm_inst_t *inst    = (vm_inst_t *)(ctx->bytecode + ctx->pc);
        vm_opcode_t opcode = inst->opcode;

        if (inst_count >= 50) {
            return;
        }
        inst_count++;
        ctx->pc += 16;
        switch (opcode) {
            case op_load:
                ctx->reg[inst->reg[0]] = inst->reg[1];
                break;
            case op_mov:
                ctx->reg[inst->reg[0]] = ctx->reg[inst->reg[1]];
                break;
            case op_add:
                ctx->reg[inst->reg[0]] = ctx->reg[inst->reg[1]] + ctx->reg[inst->reg[2]];
                break;
            case op_sub:
                ctx->reg[inst->reg[0]] = ctx->reg[inst->reg[1]] - ctx->reg[inst->reg[2]];
                break;
            case op_jump:
                ctx->pc += (inst->reg[0] - 16);
                break;
            case op_delay:
                ctx->wack_up_tick = os_sys_tick + inst->reg[0];
                ctx->state        = 0;
                break;
            case op_syscall:
                do_syscall(ctx, inst);
                break;
            case op_cmp:
                if (ctx->reg[inst->reg[0]] == inst->reg[1])
                    ctx->reg[15] = 0;
                else if (ctx->reg[inst->reg[0]] > inst->reg[1])
                    ctx->reg[15] = 1;
                else
                    ctx->reg[15] = -1;
                break;
            case op_jeq:
                if (ctx->reg[15] == 0)
                    ctx->pc += (inst->reg[0] - 16);
                break;
            case op_jne:
                if (ctx->reg[15] != 0)
                    ctx->pc += (inst->reg[0] - 16);
                break;
            case op_jgt:
                if (ctx->reg[15] == 1)
                    ctx->pc += (inst->reg[0] - 16);
                break;
            case op_jlt:
                if (ctx->reg[15] == -1)
                    ctx->pc += (inst->reg[0] - 16);
                break;
            case op_and:
                ctx->reg[inst->reg[0]] = ctx->reg[inst->reg[1]] & ctx->reg[inst->reg[2]];
                break;
            case op_or:
                ctx->reg[inst->reg[0]] = ctx->reg[inst->reg[1]] | ctx->reg[inst->reg[2]];
                break;
            case op_xor:
                ctx->reg[inst->reg[0]] = ctx->reg[inst->reg[1]] ^ ctx->reg[inst->reg[2]];
                break;
            case op_lshift:
                ctx->reg[inst->reg[0]] = ctx->reg[inst->reg[1]] << ctx->reg[inst->reg[2]];
                break;
            case op_rshift:
                ctx->reg[inst->reg[0]] = ctx->reg[inst->reg[1]] >> ctx->reg[inst->reg[2]];
                break;
                case op_call:
                if(ctx->sp <8){
                    ctx->call_stack[ctx->sp++] =ctx->pc;
                    ctx->pc +=(inst->reg[0] - 16);;
                }else{
                    ctx->state=0;
                }
                break;
                case op_ret:
                if(ctx->sp >0)
                ctx->pc =ctx->call_stack[--ctx->sp];
                else ctx->state=0;
                break;

            default:
                ctx->is_run = 0;
                break;
        }
    }
}

void vm_wakeup(vm_app_context_t *ctx)
{
    if ((ctx->state == 0) && (ctx->wack_up_tick <= os_sys_tick)) {
        ctx->state = 1;
    } else if (ctx->state == 2) {
        int msg_count = vfs_ioctl(ctx->wait_fd, VFS_CMD_POLL_READ, NULL);

        if (msg_count > 0) {
            ctx->state = 1;
        }
    }
}
