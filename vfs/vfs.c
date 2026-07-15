/*
 * @Author: 轩
 * @Date: 2026-07-12 21:43:55
 * @LastEditTime: 2026-07-14 19:02:24
 * @FilePath: \minirtos\vfs\vfs.c
 */
#include "vfs.h"
#include "string.h"

static os_list_node_t device_list_head;
static os_device_t *device[MAX_DEVICE_NUM];

void vfs_init()
{
    os_list_init(&device_list_head);
}

int vfs_register_device(os_device_t *dev)
{
    if (dev == NULL) return -1;

    os_list_add(&device_list_head, &dev->list_node);
    os_mutex_init(&dev->mutex);

    return 0;
}

int vfs_open(const char *name)
{
    os_list_node_t *node;
    os_device_t *dev;

    for (node = device_list_head.next; node != &device_list_head; node = node->next) {
        dev = container_of(node, os_device_t, list_node);

        if (strcmp(dev->name, name) == 0) {
            if (dev->open != NULL)
                dev->open(dev, 0);
            dev->is_open = 1;
            for (int i = 0; i < MAX_DEVICE_NUM; i++)
                if (device[i] == NULL) {
                    device[i] = dev;
                    return i;
                }
        }
    }
    return -1;
}

int vfs_close(int fd)
{
    if (device[fd] == NULL) return -1;
    if (device[fd]->close != NULL)
        device[fd]->close(device[fd], 0);
    device[fd]->is_open = 0;
    device[fd]          = NULL;
    return 0;
}

int vfs_read(int fd, uint32_t pos, void *buffer, uint32_t size)
{
    if (fd < 0 || fd > MAX_DEVICE_NUM - 1) return -1;
    if (device[fd] != NULL) {
        if (device[fd]->read != NULL) {
            os_mutex_take(&device[fd]->mutex);
            int ret = device[fd]->read(device[fd], pos, buffer, size);
            os_mutex_give(&device[fd]->mutex);
            return ret;
        }
    }
    return -1;
}
int vfs_write(int fd, uint32_t pos, const void *buffer, uint32_t size)
{
    if (fd < 0 || fd > MAX_DEVICE_NUM - 1) return -1;
    if (device[fd] != NULL) {
        if (device[fd]->write != NULL) {
            os_mutex_take(&device[fd]->mutex);
            int ret = device[fd]->write(device[fd], pos, buffer, size);
            os_mutex_give(&device[fd]->mutex);

            return ret;
        }
    }
    return -1;
}

int vfs_ioctl(int fd, int cmd, void *args)
{
    if (fd < 0 || fd > MAX_DEVICE_NUM - 1) return -1;
    if (device[fd] != NULL) {
        if (device[fd]->ioctl != NULL) {
            os_mutex_take(&device[fd]->mutex);
            int ret = device[fd]->ioctl(device[fd], cmd, args);
            os_mutex_give(&device[fd]->mutex);
            return ret;
        }
    }
    return -1;
}