#ifndef OS_PORT_H
#define OS_PORT_H

/*
==底层库引入==
*/
#include "stm32f10x.h"

#define OS_PORT_GET_HIGHEST_PRI(bitmap) (31-__clz(bitmap))

#define OS_PORT_YIELD() (SCB->ICSR |= (1 << 28))

void os_port_systick_init();
#endif // !OS_PORT_H
