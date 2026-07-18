/*
 * @Author: 轩
 * @Date: 2026-07-17 16:22:15
 * @LastEditTime: 2026-07-17 16:22:22
 * @FilePath: \minirtos\driver\mq_driver.h
 */
#ifndef MQ_DRIVER_H
#define MQ_DRIVER_H

#include "os_core.h" 
#include "vfs.h"   


int mq_register(const char *name, os_msg_queue_t *queue);

#endif