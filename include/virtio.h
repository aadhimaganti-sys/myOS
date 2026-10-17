#ifndef _VIRTIO_H
#define _VIRTIO_H

#include "types.h"
#include "stddef.h"

#define VIRTIO_MMIO_BASE        0x0a000000ULL
#define VIRTIO_MMIO_SIZE        0x200ULL
#define VIRTIO_MMIO_COUNT       32

/* VirtIO MMIO Register Offsets */
#define VIRTIO_MMIO_MAGIC_VALUE         0x000
#define VIRTIO_MMIO_VERSION             0x004
#define VIRTIO_MMIO_DEVICE_ID           0x008
#define VIRTIO_MMIO_VENDOR_ID           0x00c
#define VIRTIO_MMIO_DEVICE_FEATURES     0x010
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL 0x014
#define VIRTIO_MMIO_DRIVER_FEATURES     0x020
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL 0x024
#define VIRTIO_MMIO_QUEUE_SEL           0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX       0x034
#define VIRTIO_MMIO_QUEUE_NUM           0x038
#define VIRTIO_MMIO_QUEUE_READY         0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY        0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS    0x060
#define VIRTIO_MMIO_INTERRUPT_ACK       0x064
#define VIRTIO_MMIO_STATUS              0x070
#define VIRTIO_MMIO_QUEUE_DESC_LOW      0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH     0x084
#define VIRTIO_MMIO_QUEUE_DRIVER_LOW    0x090
#define VIRTIO_MMIO_QUEUE_DRIVER_HIGH   0x094
#define VIRTIO_MMIO_QUEUE_DEVICE_LOW    0x0a0
#define VIRTIO_MMIO_QUEUE_DEVICE_HIGH   0x0a4
#define VIRTIO_MMIO_CONFIG              0x100

#define VIRTIO_MAGIC                    0x74726976U /* "virt" */

/* Device IDs */
#define VIRTIO_DEV_NET                  1
#define VIRTIO_DEV_BLOCK                2
#define VIRTIO_DEV_CONSOLE              3
#define VIRTIO_DEV_GPU                  16
#define VIRTIO_DEV_INPUT                18

/* Device Status Bits */
#define VIRTIO_STATUS_ACKNOWLEDGE       1
#define VIRTIO_STATUS_DRIVER            2
#define VIRTIO_STATUS_DRIVER_OK         4
#define VIRTIO_STATUS_FEATURES_OK       8
#define VIRTIO_STATUS_FAILED            128

/* Virtqueue Descriptor Flags */
#define VRING_DESC_F_NEXT               1
#define VRING_DESC_F_WRITE              2

#define VIRTIO_QUEUE_SIZE               16

struct vring_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

struct vring_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[VIRTIO_QUEUE_SIZE];
} __attribute__((packed));

struct vring_used_elem {
    uint32_t id;
    uint32_t len;
} __attribute__((packed));

struct vring_used {
    uint16_t flags;
    uint16_t idx;
    struct vring_used_elem ring[VIRTIO_QUEUE_SIZE];
} __attribute__((packed));

/* VirtIO Block Request */
#define VIRTIO_BLK_T_IN                 0 /* Read */
#define VIRTIO_BLK_T_OUT                1 /* Write */

#define VIRTIO_BLK_S_OK                 0
#define VIRTIO_BLK_S_IOERR              1
#define VIRTIO_BLK_S_UNSUPP             2

#define SECTOR_SIZE                     512

struct virtio_blk_req {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
};

/* VirtIO API */
void virtio_init(void);
int virtio_blk_read(uint64_t sector, void *buffer, uint32_t sector_count);
int virtio_blk_write(uint64_t sector, const void *buffer, uint32_t sector_count);
uint64_t virtio_blk_get_capacity(void);

#endif /* _VIRTIO_H */
