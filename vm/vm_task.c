#include "vm_task.h"
#include "os_core.h"

uint32_t g_slot_index =0;
uint32_t g_version =0;

 static vm_app_context_t ctx[APP_NUM];
// vm_app_context_t ctx[APP_NUM];
static const uint8_t app0_code[]={
    0x05, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00,
    0x05, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x47, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0xE0, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x6D, 0x71, 0x5F, 0x61, 0x70, 0x70, 0x00, 0x70, 0x69, 0x6E, 0x67, 0x00,
}  ;
static const uint8_t app1_code[]={   0x05, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x50, 0x00, 0x00, 0x00,
    0x05, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x58, 0x00, 0x00, 0x00,
    0x05, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00,
    0x05, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0xE0, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x73, 0x79, 0x73, 0x5F, 0x6C, 0x65, 0x64, 0x00, 0x6D, 0x71, 0x5F, 0x61, 0x70, 0x70, 0x00,
};  
// {
//     "CLASSES": {
//         "led": {
//             "OFF": 0,
//             "ON": 1,
//             "TOGGLE": 2
//         }
      
//     },
//     "BINDINGS": {
//         "sys_led": "led",
//         "led" : "led"
       
//     }
// }
static volatile uint8_t app_valid =0;

void vm_unload_apps()
{
    app_valid=0;
    for(int i=2;i<APP_NUM;i++){
        ctx[i].is_run=0;
        ctx[i].pc=0;
        ctx[i].bytecode=NULL;
        for (int j = 0; j < ctx[i].fd_count; j++) {
            vfs_close(ctx[i].fd_table[j]);
        }
        ctx[i].fd_count = 0; // 清空计数器
        ctx[i].wait_fd = -1;
    }
    SEGGER_RTT_printf(0, "[VM] 动态 App 已全部卸载，等待烧录...\r\n");
}

 void vm_load_apps(uint32_t base_addr)
//void vm_load_apps()
{
   // VmBinHeader_t *header =(VmBinHeader_t *)APP_START_ADDR;
   VmBinHeader_t *header =(VmBinHeader_t *)base_addr;
    if(header->magic==VM_MAGIC){
        SEGGER_RTT_printf(0, "[VM] 发现 MQVM 包, 包含 %d 个 App\r\n", header->app_count);
        int slot =2;
        for(uint32_t i=0;i<header->app_count;i++){
            if(slot >=APP_NUM) break;
            // uint8_t *app_addr = (uint8_t *)APP_START_ADDR + header->toc[i].offset;
             uint8_t *app_addr = (uint8_t *)base_addr + header->toc[i].offset;
            ctx[slot].bytecode =app_addr;
            ctx[slot].pc =0;
            ctx[slot].is_run =1;
            ctx[slot].state=1;
            SEGGER_RTT_printf(0, "[VM] 挂载动态 App 到槽位 %d, 地址: %p\r\n", slot, app_addr);
            slot++;
        }
        app_valid=1;
    }
}
static const uint8_t* const raw_app_bytecodes[APP_NUM] = {
    app0_code,  
   
 app1_code, 
    //   NULL,
    NULL,       
    NULL        
};
void vmtask(void *param)
{

    for (int i = 0; i < APP_NUM; i++) {
        ctx[i].bytecode=(uint8_t *)raw_app_bytecodes[i];
        if (ctx[i].bytecode != NULL) {
            ctx[i].pc     = 0;
            ctx[i].is_run = 1;
            ctx[i].state  = 1;
        }else {
           ctx[i].is_run = 0; 
        }
    }
    // vm_load_apps();
    vm_find_apps();

    while (1) {
        for (int i = 0; i < APP_NUM; i++) {
            if (ctx[i].bytecode == NULL) continue;
            if(i>=2&&app_valid==0) continue;
            if (ctx[i].is_run) {
                vm_wakeup(&ctx[i]);
                vm_run(&ctx[i]);
            }
        }
        os_delay(1);
    }
}


void vm_find_apps()
{
    uint32_t max_ver = 0;
    int latest_index = -1;

    for (int i = 0; i < APP_SLOT_NUM; i++) {
        uint32_t slot_addr = APP_START_ADDR + i * APP_SLOT_SIZE;
        VmBinHeader_t *header = (VmBinHeader_t *)slot_addr;
        
        if (header->magic == VM_MAGIC) {
            // 读取存放于槽位末尾的版本号
            uint32_t version = *(uint32_t *)(slot_addr + APP_VERSION_OFFSET);
            if (version == 0xFFFFFFFF) version = 0; // 防止初次擦除后的乱码
            
            if (latest_index == -1 || version >= max_ver) {
                max_ver = version;
                latest_index = i;
            }
        }
    }

    if (latest_index != -1) {
        g_slot_index = latest_index;
        g_version = max_ver;
        SEGGER_RTT_printf(0, "[VM] 找到最新 App: 槽位 %d, 版本 %d\r\n", latest_index, max_ver);
        vm_load_apps(APP_START_ADDR + latest_index * APP_SLOT_SIZE);
    } else {
        SEGGER_RTT_printf(0, "[VM] 未找到有效的动态 App\r\n");
    }
}