#ifndef VFS_H
#define VFS_H

#include "stdint.h"
#include "stddef.h"

#include "os_list.h"
#include "os_core.h"

#define  MAX_DEVICE_NUM 16
typedef struct os_device{
    const char *name;
    uint8_t type;
    uint8_t is_open;

    void *private_data;

    int (*init)(struct os_device *dedv);
    int (*open)(struct os_device *dev,uint16_t flag);
    int (*read)(struct os_device *dev,uint32_t pos,void *buffer,uint32_t size);
    int (*write)(struct os_device *dev,uint32_t pos,const void *bufferr,uint32_t size);
    int (*ioctl)(struct os_device *dev,int cmd,void *args);
    int (*close)(struct os_device *dev,uint16_t flag);

    os_list_node_t list_node;

    os_mutex_t mutex;

} os_device_t;

void vfs_init();
int vfs_register_device(os_device_t *dev);
int vfs_open(const char *name);
int vfs_write(int fd, uint32_t pos, const void *buffer, uint32_t size);
int vfs_read(int fd, uint32_t pos, void *buffer, uint32_t size);
int vfs_close(int fd);
int vfs_ioctl(int fd, int cmd, void *args);
#endif // !VFS_H

