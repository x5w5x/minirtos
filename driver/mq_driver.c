/*
 * @Author: 轩
 * @Date: 2026-07-17 16:23:15
 * @LastEditTime: 2026-07-17 16:23:24
 * @FilePath: \minirtos\driver\mq_driver.c
 */
#include "mq_driver.h"
#include <string.h>

#define MQ_MAX_NUM 2

typedef struct {
    os_msg_queue_t *queue;
    int is_used;
} mq_driver_t;

static os_device_t mq_device[MQ_MAX_NUM];
static mq_driver_t mq_driver[MQ_MAX_NUM];

static int mq_open(os_device_t *dev, uint16_t flag) {
    return 0; 
}

static int mq_close(os_device_t *dev) {
    return 0;
}

static int mq_read(os_device_t *dev, uint32_t pos, void *buffer, uint32_t size) {
    mq_driver_t *mq = (mq_driver_t *)dev->private_data;

   
    if (mq->queue->msg_count == 0) {
        return 0; 
    }
    os_msg_recv(mq->queue, buffer);
    return 1; 
}

static int mq_write(os_device_t *dev, uint32_t pos, const void *buffer, uint32_t size) {
    mq_driver_t *mq = (mq_driver_t *)dev->private_data;
    os_msg_send(mq->queue, (void *)buffer);
    return size;
}

static int mq_ioctl(os_device_t *dev, int cmd, void *args) {
    mq_driver_t *mq = (mq_driver_t *)dev->private_data;
    
    
    if (cmd == 0xFF) { 
        return mq->queue->msg_count;
    }
    return 0;
}

int mq_register(const char *name, os_msg_queue_t *queue) {
    for (int i = 0; i < MQ_MAX_NUM; i++) {
        if (!mq_driver[i].is_used) {
            mq_driver[i].is_used = 1;
            mq_driver[i].queue   = queue;

            mq_device[i].private_data = &mq_driver[i];
            mq_device[i].name         = name;
            mq_device[i].open         = mq_open;
            mq_device[i].close        = mq_close;
            mq_device[i].read         = mq_read;
            mq_device[i].write        = mq_write;
            mq_device[i].ioctl        = mq_ioctl;

            vfs_register_device(&mq_device[i]);
            return 0;
        }
    }
    return -1;
}