#include <string.h>
#include "iap_protocol.h"
#include "flash_iap.h"
#include "uart_driver.h"
// #include "vm_task.h"
#include "stm32f10x.h"
#include "SEGGER_RTT.h"

static uint32_t g_target_addr = 0;
static uint32_t g_target_version = 0;
/* ------------------------------------------------------------------ */
/* 内部状态                                                            */
/* ------------------------------------------------------------------ */
typedef struct {
    uint32_t total_size;    /* START帧给的固件总大小 */
    uint32_t total_crc;     /* START帧给的固件总CRC  */
    uint32_t received;      /* 已写入字节数          */
    uint8_t  started;       /* 收到START且擦除成功    */
} iap_ctx_t;

static iap_ctx_t   g_ctx;
static IAP_Frame_t g_frame;     /* 静态帧缓冲，不占任务栈 */
static int         g_fd = -1;   /* 串口设备fd */

/* ------------------------------------------------------------------ */
/* 硬件CRC32 (STM32 CRC单元, 小端拼32bit字, 末尾不足4字节补0)           */
/* ------------------------------------------------------------------ */
static uint32_t crc32_hw(const uint8_t *data, uint32_t len)
{
    uint32_t i = 0;

    CRC->CR = CRC_CR_RESET;

    for (; i + 4 <= len; i += 4) {
        CRC->DR = (uint32_t)data[i]
                | ((uint32_t)data[i + 1] << 8)
                | ((uint32_t)data[i + 2] << 16)
                | ((uint32_t)data[i + 3] << 24);
    }
    if (i < len) {
        uint32_t last = 0;
        for (uint32_t j = 0; i + j < len; j++)
            last |= (uint32_t)data[i + j] << (8 * j);
        CRC->DR = last;
    }
    return CRC->DR;
}

/* ------------------------------------------------------------------ */
/* 串口收发封装 —— 走VFS的fd接口                                       */
/* ------------------------------------------------------------------ */
static void iap_recv(uint8_t *buf, uint32_t len)
{
    vfs_read(g_fd, 0, buf, len);
}

static void iap_reply(uint8_t code)
{
    vfs_write(g_fd, 0, &code, 1);
}

/* ------------------------------------------------------------------ */
/* 收一帧: 逐字节同步帧头 -> 固定部分 -> data[size] -> tail             */
/* ------------------------------------------------------------------ */
static int iap_recv_frame(IAP_Frame_t *f)
{
    uint8_t *p = (uint8_t *)f;

    /* 1. 滑动同步帧头 */
    do {
        iap_recv(&f->header, 1);
    } while (f->header != IAP_HEADER);

    /* 2. 固定部分: cmd + size + addr + crc (11字节) */
    iap_recv(p + 1, IAP_FRAME_HEAD_LEN - 1);

    if (f->size > IAP_DATA_MAX)
        return -1;

    /* 3. 变长data (线上只有size字节, 不是固定256!) */
    if (f->size > 0)
        iap_recv(f->data, f->size);

    /* 4. 帧尾单独收 */
    iap_recv(&f->tail, 1);
    if (f->tail != IAP_TAIL)
        return -1;

    return 0;
}
extern uint32_t g_slot_index;
extern uint32_t g_version;
#define APP_SLOT_SIZE (2*1024)
#define APP_SLOT_NUM  8
#define APP_VERSION_OFFSET (APP_SLOT_SIZE - 4)
/* ------------------------------------------------------------------ */
/* 命令处理: 返回0 -> ACK, 负数 -> NACK                                */
/* ------------------------------------------------------------------ */
// static int handle_start(IAP_Frame_t *f)
// {
//     if (f->size < 4)
//         return -1;

//     uint32_t total = (uint32_t)f->data[0]
//                    | ((uint32_t)f->data[1] << 8)
//                    | ((uint32_t)f->data[2] << 16)
//                    | ((uint32_t)f->data[3] << 24);

//     if (total == 0 || total > APP_MAX_SIZE)
//         return -2;
//         vm_unload_apps();
//     if (flash_iap_erase(APP_START_ADDR, total) != 0)
//         return -3;

//     g_ctx.total_size = total;
//     g_ctx.total_crc  = f->crc;
//     g_ctx.received   = 0;
//     g_ctx.started    = 1;

//     SEGGER_RTT_printf(0, "[IAP] start, total=%d crc=%08x\r\n", total, f->crc);
//     return 0;
// }

// static int handle_data(IAP_Frame_t *f)
// {
//     if (!g_ctx.started || f->size == 0)
//         return -1;

//     /* 本帧CRC校验 */
//     if (crc32_hw(f->data, f->size) != f->crc)
//         return -2;

//     /* 补齐到偶数字节, flash按半字写 */
//     uint16_t wlen = f->size;
//     if (wlen & 1)
//         f->data[wlen++] = 0xFF;

//     if (flash_iap_write(f->addr, f->data, wlen) != 0)
//         return -3;

//     /* 回读比对, 确认真正写进去了 */
//     if (memcmp((void *)f->addr, f->data, f->size) != 0)
//         return -4;

//     g_ctx.received += f->size;
//     SEGGER_RTT_printf(0, "[IAP] data @%08x len=%d (%d/%d)\r\n",
//                       f->addr, f->size, g_ctx.received, g_ctx.total_size);
//     return 0;
// }

// static int handle_finish(void)
// {
//     if (!g_ctx.started || g_ctx.received < g_ctx.total_size)
//         return -1;

//     /* 从Flash回读整个固件算总CRC */
//     uint32_t crc = crc32_hw((const uint8_t *)APP_START_ADDR, g_ctx.total_size);

//     g_ctx.started = 0;

//     if (crc != g_ctx.total_crc) {
//         SEGGER_RTT_printf(0, "[IAP] crc fail: %08x != %08x\r\n",
//                           crc, g_ctx.total_crc);
//         return -2;
//     }

//     SEGGER_RTT_printf(0, "[IAP] finish ok, crc=%08x\r\n", crc);
//     /* 这里可以: 写升级标志 / NVIC_SystemReset() */
//     vm_load_apps();
//     return 0;
// }
static int handle_start(IAP_Frame_t *f)
{
    if (f->size < 4)
        return -1;

    uint32_t total = (uint32_t)f->data[0]
                   | ((uint32_t)f->data[1] << 8)
                   | ((uint32_t)f->data[2] << 16)
                   | ((uint32_t)f->data[3] << 24);

    // 必须确保固件大小装得进 2KB 槽位，并留出末尾 4 字节放版本号
    if (total == 0 || total > (APP_SLOT_SIZE - 4))
        return -2;
        
    vm_unload_apps();

    // ---------- 核心：轮转到下一个槽位 ----------
    uint32_t next_slot = (g_slot_index + 1) % APP_SLOT_NUM;
    g_target_addr = APP_START_ADDR + next_slot * APP_SLOT_SIZE;
    g_target_version = g_version + 1;

    // 擦除这整个 2KB 槽位
    if (flash_iap_erase(g_target_addr, APP_SLOT_SIZE) != 0)
        return -3;

    g_ctx.total_size = total;
    g_ctx.total_crc  = f->crc;
    g_ctx.received   = 0;
    g_ctx.started    = 1;

    SEGGER_RTT_printf(0, "[IAP] start, slot=%d target=%08x total=%d\r\n", next_slot, g_target_addr, total);
    return 0;
}

static int handle_data(IAP_Frame_t *f)
{
    if (!g_ctx.started || f->size == 0)
        return -1;

    if (crc32_hw(f->data, f->size) != f->crc)
        return -2;

    uint16_t wlen = f->size;
    if (wlen & 1)
        f->data[wlen++] = 0xFF;

    // ---------- 核心：将 PC 下发的绝对地址映射到新槽位 ----------
    // PC 发来的 f->addr 永远是以 0x08004000 算起的，我们求出相对偏移，加上新槽位基址
    uint32_t write_offset = f->addr - APP_START_ADDR;
    uint32_t real_write_addr = g_target_addr + write_offset;

    // 写入真实的目标地址
    if (flash_iap_write(real_write_addr, f->data, wlen) != 0)
        return -3;

    if (memcmp((void *)real_write_addr, f->data, f->size) != 0)
        return -4;

    g_ctx.received += f->size;
    SEGGER_RTT_printf(0, "[IAP] data @%08x -> 映射到 @%08x\r\n", f->addr, real_write_addr);
    return 0;
}

static int handle_finish(void)
{
    if (!g_ctx.started || g_ctx.received < g_ctx.total_size)
        return -1;

    // ---------- 核心：从新槽位地址回读算总 CRC ----------
    uint32_t crc = crc32_hw((const uint8_t *)g_target_addr, g_ctx.total_size);

    g_ctx.started = 0;

    if (crc != g_ctx.total_crc) {
        SEGGER_RTT_printf(0, "[IAP] crc fail: %08x != %08x\r\n", crc, g_ctx.total_crc);
        return -2;
    }

    // ---------- 校验成功后，将版本号写入槽位末尾 ----------
    uint32_t version_addr = g_target_addr + APP_VERSION_OFFSET;
    flash_iap_write(version_addr, (uint8_t *)&g_target_version, 4);

    // 更新系统当前环境参数
    g_slot_index = (g_target_addr - APP_START_ADDR) / APP_SLOT_SIZE;
    g_version = g_target_version;

    SEGGER_RTT_printf(0, "[IAP] finish ok, 升级至版本 %d\r\n", g_version);
    
    // 直接用新地址挂载代码
    vm_load_apps(g_target_addr);
    return 0;
}
/* ------------------------------------------------------------------ */
/* IAP任务入口                                                         */
/* ------------------------------------------------------------------ */
void Task_IAP(void *param)
{
    (void)param;

    /* 名字对应 uart_register("uart1", USART1, 115200) */
    g_fd = vfs_open("sys_uart");
    if (g_fd < 0) {
        SEGGER_RTT_printf(0, "[IAP] no uart1 device!\r\n");
        while (1) os_delay(1000);
    }

    flash_iap_init();
    memset(&g_ctx, 0, sizeof(g_ctx));

    SEGGER_RTT_printf(0, "[IAP] task ready, fd=%d\r\n", g_fd);

    while (1) {
        if (iap_recv_frame(&g_frame) != 0) {
            iap_reply(IAP_NACK);
            os_delay(1);
            continue;
        }

        int ret;
        switch (g_frame.cmd) {
        case CMD_START:  ret = handle_start(&g_frame);  break;
        case CMD_DATA:   ret = handle_data(&g_frame);   break;
        case CMD_FINISH: ret = handle_finish();         break;
        default:         ret = -1;                      break;
        }

        iap_reply(ret == 0 ? IAP_ACK : IAP_NACK);
        if (ret != 0)
            SEGGER_RTT_printf(0, "[IAP] cmd=%02x err=%d\r\n",
                              g_frame.cmd, ret);
    
    }
}
