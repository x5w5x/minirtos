#include "os_port.h"
#include "os_core.h"
void os_port_systick_init()
{
    SysTick_Config(SystemCoreClock / 1000);
}
void SysTick_Handler(void)
{
    os_tick_handler();
}