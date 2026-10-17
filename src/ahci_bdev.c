/*
 * CherryAHCI - CherryVFS Block Device Bridge (/dev/sda)
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include "cherry_ahci.h"
#include "cherry_vfs.h"
#include "vfs_dev.h"
#include "cherry_printk.h"

extern int snprintf(char *buf, size_t size, const char *fmt, ...);

static int ahci_bdev_open(struct vfs_bdev *bdev, int flags)
{
    (void)bdev;
    (void)flags;
    return 0;
}

static int ahci_bdev_close(struct vfs_bdev *bdev)
{
    (void)bdev;
    return 0;
}

static int ahci_bdev_read(struct vfs_bdev *bdev, void *buf, uint64_t lba, size_t num_blocks)
{
    struct ahci_port *port = (struct ahci_port *)bdev->dev.priv_data;
    if (!port) return -1;
    return ahci_port_read_blocks(port, lba, (uint32_t)num_blocks, buf);
}

static int ahci_bdev_write(struct vfs_bdev *bdev, const void *buf, uint64_t lba, size_t num_blocks)
{
    struct ahci_port *port = (struct ahci_port *)bdev->dev.priv_data;
    if (!port) return -1;
    return ahci_port_write_blocks(port, lba, (uint32_t)num_blocks, buf);
}

static int ahci_bdev_sync(struct vfs_bdev *bdev)
{
    struct ahci_port *port = (struct ahci_port *)bdev->dev.priv_data;
    if (!port) return -1;
    return ahci_port_flush(port);
}

static int ahci_bdev_ioctl(struct vfs_bdev *bdev, unsigned int cmd, unsigned long arg)
{
    struct ahci_port *port = (struct ahci_port *)bdev->dev.priv_data;
    if (!port) return -1;

    switch (cmd) {
    case BLOCK_IOCTL_GET_SECTOR_SIZE:
    case BLOCK_IOCTL_GET_BLOCK_SIZE:
        if (arg) *(uint32_t *)arg = port->block_size;
        return 0;
    case BLOCK_IOCTL_GET_SECTOR_COUNT:
        if (arg) *(uint64_t *)arg = port->block_count;
        return 0;
    case BLOCK_IOCTL_SYNC:
        return ahci_port_flush(port);
    default:
        return -22;
    }
}

static const struct vfs_bdev_ops s_ahci_bdev_ops = {
    .open         = ahci_bdev_open,
    .close        = ahci_bdev_close,
    .read_blocks  = ahci_bdev_read,
    .write_blocks = ahci_bdev_write,
    .sync         = ahci_bdev_sync,
    .ioctl        = ahci_bdev_ioctl,
};

int ahci_bdev_register(struct ahci_port *port)
{
    uint32_t blk_size = port->block_size ? port->block_size : 512;
    uint64_t blk_count = port->block_count;
    char name[16];
    char alias[16];

    snprintf(name, sizeof(name), "sd%c", 'a' + (int)port->port_no);
    snprintf(alias, sizeof(alias), "sata%u", port->port_no);

    port->bdev.ops = &s_ahci_bdev_ops;
    port->bdev.dev.priv_data = port;
    vfs_bdev_register(&port->bdev, name, blk_size, blk_count);

    port->alias_bdev.ops = &s_ahci_bdev_ops;
    port->alias_bdev.dev.priv_data = port;
    vfs_bdev_register(&port->alias_bdev, alias, blk_size, blk_count);

    pr_info("cherry_ahci: registered block device /dev/%s (%llu sectors, %u bytes/sector)\n",
            name, (unsigned long long)blk_count, blk_size);
    return 0;
}
