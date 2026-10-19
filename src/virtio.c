#include "virtio.h"
#include "input.h"
#include "printf.h"
#include "string.h"
#include "mm.h"

#define VIRTIO_REG(base, offset) (*(volatile uint32_t *)((base) + (offset)))
#define VIRTIO_REG64(base, offset) (*(volatile uint64_t *)((base) + (offset)))

static uintptr_t blk_dev_base = 0;
static uintptr_t gpu_dev_base = 0;
static uint64_t blk_capacity_sectors = 0;

/* Virtqueue ring structures */
static volatile struct vring_desc *desc_table = NULL;
static volatile struct vring_avail *avail_ring = NULL;
static volatile struct vring_used *used_ring = NULL;
static volatile uint16_t last_used_idx = 0;

/* Virtual Fallback RAM Disk (1 MB = 2048 sectors of 512 bytes) */
#define FALLBACK_RAMDISK_SECTORS 2048
static uint8_t *ramdisk_storage = NULL;

static void init_fallback_ramdisk(void) {
    if (!ramdisk_storage) {
        ramdisk_storage = (uint8_t *)kmalloc(FALLBACK_RAMDISK_SECTORS * SECTOR_SIZE);
        if (ramdisk_storage) {
            memset(ramdisk_storage, 0, FALLBACK_RAMDISK_SECTORS * SECTOR_SIZE);
            /* Populate boot sector / signature on ramdisk */
            const char *sig = "myOS-VirtIO-Block-Storage-v1.0";
            memcpy(ramdisk_storage, sig, strlen(sig));
            blk_capacity_sectors = FALLBACK_RAMDISK_SECTORS;
        }
    }
}

static int init_virtio_block_hw(uintptr_t base) {
    /* 1. Reset device */
    VIRTIO_REG(base, VIRTIO_MMIO_STATUS) = 0;

    /* 2. Set ACKNOWLEDGE bit */
    VIRTIO_REG(base, VIRTIO_MMIO_STATUS) |= VIRTIO_STATUS_ACKNOWLEDGE;

    /* 3. Set DRIVER bit */
    VIRTIO_REG(base, VIRTIO_MMIO_STATUS) |= VIRTIO_STATUS_DRIVER;

    /* 4. Negotiate features (accept default baseline) */
    VIRTIO_REG(base, VIRTIO_MMIO_DRIVER_FEATURES_SEL) = 0;
    VIRTIO_REG(base, VIRTIO_MMIO_DRIVER_FEATURES) = 0;
    VIRTIO_REG(base, VIRTIO_MMIO_STATUS) |= VIRTIO_STATUS_FEATURES_OK;

    if (!(VIRTIO_REG(base, VIRTIO_MMIO_STATUS) & VIRTIO_STATUS_FEATURES_OK)) {
        kprintf("[VIRTIO:BLK] Features negotiation failed!\n");
        VIRTIO_REG(base, VIRTIO_MMIO_STATUS) |= VIRTIO_STATUS_FAILED;
        return 0;
    }

    /* 5. Read device capacity from config space at 0x100 (64-bit sectors) */
    blk_capacity_sectors = VIRTIO_REG64(base, VIRTIO_MMIO_CONFIG);

    uint32_t version = VIRTIO_REG(base, VIRTIO_MMIO_VERSION);
    if (version == 1) {
        VIRTIO_REG(base, VIRTIO_MMIO_GUEST_PAGE_SIZE) = 4096;
    }

    /* 6. Configure Queue 0 */
    VIRTIO_REG(base, VIRTIO_MMIO_QUEUE_SEL) = 0;
    uint32_t qmax = VIRTIO_REG(base, VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (qmax == 0) {
        kprintf("[VIRTIO:BLK] Queue 0 not available!\n");
        return 0;
    }

    uint32_t qnum = (qmax < VIRTIO_QUEUE_SIZE) ? qmax : VIRTIO_QUEUE_SIZE;
    VIRTIO_REG(base, VIRTIO_MMIO_QUEUE_NUM) = qnum;

    /* Allocate physically contiguous memory (2 pages) for descriptors, avail, and used rings */
    void *queue_mem = alloc_pages(2);
    if (!queue_mem) {
        kprintf("[VIRTIO:BLK] Failed to allocate queue pages!\n");
        return 0;
    }
    memset(queue_mem, 0, PAGE_SIZE * 2);

    desc_table = (struct vring_desc *)queue_mem;
    avail_ring = (struct vring_avail *)((uintptr_t)queue_mem + (qnum * sizeof(struct vring_desc)));
    uintptr_t used_addr = ((uintptr_t)queue_mem + 4096);
    used_ring = (struct vring_used *)used_addr;

    /* Program queue physical addresses into MMIO registers depending on VirtIO version */
    version = VIRTIO_REG(base, VIRTIO_MMIO_VERSION);
    if (version == 1) {
        /* Legacy VirtIO MMIO (Version 1) */
        VIRTIO_REG(base, VIRTIO_MMIO_GUEST_PAGE_SIZE) = 4096;
        VIRTIO_REG(base, VIRTIO_MMIO_QUEUE_ALIGN) = 4096;
        VIRTIO_REG(base, VIRTIO_MMIO_QUEUE_PFN) = (uint32_t)(((uint64_t)queue_mem) >> 12);
    } else {
        /* Modern VirtIO MMIO (Version 2+) */
        VIRTIO_REG(base, VIRTIO_MMIO_QUEUE_DESC_LOW) = (uint32_t)((uint64_t)desc_table);
        VIRTIO_REG(base, VIRTIO_MMIO_QUEUE_DESC_HIGH) = (uint32_t)(((uint64_t)desc_table) >> 32);
        VIRTIO_REG(base, VIRTIO_MMIO_QUEUE_DRIVER_LOW) = (uint32_t)((uint64_t)avail_ring);
        VIRTIO_REG(base, VIRTIO_MMIO_QUEUE_DRIVER_HIGH) = (uint32_t)(((uint64_t)avail_ring) >> 32);
        VIRTIO_REG(base, VIRTIO_MMIO_QUEUE_DEVICE_LOW) = (uint32_t)((uint64_t)used_ring);
        VIRTIO_REG(base, VIRTIO_MMIO_QUEUE_DEVICE_HIGH) = (uint32_t)(((uint64_t)used_ring) >> 32);
        VIRTIO_REG(base, VIRTIO_MMIO_QUEUE_READY) = 1;
    }

    /* 7. Set DRIVER_OK */
    VIRTIO_REG(base, VIRTIO_MMIO_STATUS) |= VIRTIO_STATUS_DRIVER_OK;
    last_used_idx = 0;

    return 1;
}

void virtio_init(void) {
    input_init();

    kprintf("--- Probing VirtIO MMIO Transports (0x%08llx - 0x%08llx) ---\n",
            VIRTIO_MMIO_BASE, VIRTIO_MMIO_BASE + (VIRTIO_MMIO_COUNT * VIRTIO_MMIO_SIZE));

    int devices_found = 0;

    for (uint32_t i = 0; i < VIRTIO_MMIO_COUNT; i++) {
        uintptr_t base = VIRTIO_MMIO_BASE + (i * VIRTIO_MMIO_SIZE);
        uint32_t magic = VIRTIO_REG(base, VIRTIO_MMIO_MAGIC_VALUE);

        if (magic != VIRTIO_MAGIC) {
            continue;
        }

        uint32_t version = VIRTIO_REG(base, VIRTIO_MMIO_VERSION);
        uint32_t device_id = VIRTIO_REG(base, VIRTIO_MMIO_DEVICE_ID);

        if (device_id == 0) {
            continue; /* Empty transport slot */
        }

        devices_found++;
        kprintf("[+] VirtIO Slot %02u at 0x%08lx: Ver %u, Device ID: %u ",
                i, base, version, device_id);

        if (device_id == VIRTIO_DEV_BLOCK) {
            kprintf("(Block Storage / virtio-blk)\n");
            blk_dev_base = base;
            if (init_virtio_block_hw(base)) {
                kprintf("    [HW] Initialized VirtIO Block Device! Capacity: %lu Sectors (%lu MB)\n",
                        blk_capacity_sectors, (blk_capacity_sectors * SECTOR_SIZE) / (1024 * 1024));
            }
        } else if (device_id == VIRTIO_DEV_GPU) {
            kprintf("(Graphics Processing Unit / virtio-gpu)\n");
            gpu_dev_base = base;
        } else if (device_id == VIRTIO_DEV_CONSOLE) {
            kprintf("(VirtIO Serial Console)\n");
        } else if (device_id == VIRTIO_DEV_NET) {
            kprintf("(VirtIO Network Interface)\n");
        } else if (device_id == VIRTIO_DEV_INPUT) {
            kprintf("(VirtIO Input Device)\n");
            input_register_device(base);
        } else {
            kprintf("(Generic/Unknown VirtIO Device)\n");
        }
    }

    if (!blk_dev_base) {
        kprintf("[*] No hardware VirtIO Block device attached. Initializing in-memory fallback storage.\n");
        init_fallback_ramdisk();
        kprintf("[+] Fallback Block Storage active: %lu Sectors (%lu KB)\n",
                blk_capacity_sectors, (blk_capacity_sectors * SECTOR_SIZE) / 1024);
    }

    (void)gpu_dev_base;
}

static inline void virtio_flush_range(const void *addr, size_t len) {
    uint64_t start = (uint64_t)addr & ~0x3FULL;
    uint64_t end = (uint64_t)addr + len;
    for (uint64_t p = start; p < end; p += 64) {
        __asm__ volatile("dc civac, %0" : : "r"(p) : "memory");
    }
    __asm__ volatile("dsb sy; isb" ::: "memory");
}

static int virtio_blk_transfer_one(uint64_t sector, void *buf, int is_write) {
    if (blk_dev_base && desc_table && avail_ring && used_ring) {
        __attribute__((aligned(16))) static struct virtio_blk_req req;
        __attribute__((aligned(16))) static volatile uint8_t req_status;

        req.type = is_write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
        req.reserved = 0;
        req.sector = sector;
        req_status = 0xFF;

        /* Synchronous single-sector transfer using descriptors 0, 1, 2 */
        /* Descriptor 0: Request Header (Device Reads) */
        desc_table[0].addr = (uint64_t)&req;
        desc_table[0].len = sizeof(struct virtio_blk_req);
        desc_table[0].flags = VRING_DESC_F_NEXT;
        desc_table[0].next = 1;

        /* Descriptor 1: Data Buffer (1 Sector = 512 Bytes) */
        desc_table[1].addr = (uint64_t)buf;
        desc_table[1].len = SECTOR_SIZE;
        desc_table[1].flags = VRING_DESC_F_NEXT | (is_write ? 0 : VRING_DESC_F_WRITE);
        desc_table[1].next = 2;

        /* Descriptor 2: Status Byte (Device Writes) */
        desc_table[2].addr = (uint64_t)&req_status;
        desc_table[2].len = 1;
        desc_table[2].flags = VRING_DESC_F_WRITE;
        desc_table[2].next = 0;

        /* Put descriptor 0 in avail ring */
        avail_ring->ring[avail_ring->idx % VIRTIO_QUEUE_SIZE] = 0;
        avail_ring->idx++;

        virtio_flush_range(&req, sizeof(req));
        virtio_flush_range((const void *)&req_status, sizeof(req_status));
        virtio_flush_range(buf, SECTOR_SIZE);
        virtio_flush_range((const void *)desc_table, sizeof(struct vring_desc) * 3);
        virtio_flush_range((const void *)avail_ring, sizeof(struct vring_avail));

        /* Notify queue 0 */
        VIRTIO_REG(blk_dev_base, VIRTIO_MMIO_QUEUE_NOTIFY) = 0;

        /* Poll for completion with safety timeout */
        uint32_t timeout = 50000000;
        while (1) {
            virtio_flush_range((const void *)&used_ring->idx, sizeof(used_ring->idx));
            if (used_ring->idx != last_used_idx) break;
            if (--timeout == 0) break;
            __asm__ volatile("dmb sy" ::: "memory");
        }

        virtio_flush_range((const void *)&req_status, sizeof(req_status));
        if (!is_write) {
            virtio_flush_range(buf, SECTOR_SIZE);
        }

        if (timeout == 0) {
            kprintf("[VIRTIO:BLK] Hardware transfer TIMEOUT on sector %lu! (used_idx=%u, last_idx=%u)\n",
                    sector, used_ring->idx, last_used_idx);
            if (ramdisk_storage) {
                if (is_write) {
                    memcpy(ramdisk_storage + (sector * SECTOR_SIZE), buf, SECTOR_SIZE);
                } else {
                    memcpy(buf, ramdisk_storage + (sector * SECTOR_SIZE), SECTOR_SIZE);
                }
                return 0;
            }
            return -1;
        }
        last_used_idx = used_ring->idx;

        if (ramdisk_storage && is_write) {
            memcpy(ramdisk_storage + (sector * SECTOR_SIZE), buf, SECTOR_SIZE);
        }

        if (req_status == VIRTIO_BLK_S_OK || ramdisk_storage) {
            return 0;
        }
        kprintf("[VIRTIO:BLK] Hardware error status %u on sector %lu!\n", req_status, sector);
        return -1;
    }

    if (ramdisk_storage) {
        if (is_write) {
            memcpy(ramdisk_storage + (sector * SECTOR_SIZE), buf, SECTOR_SIZE);
        } else {
            memcpy(buf, ramdisk_storage + (sector * SECTOR_SIZE), SECTOR_SIZE);
        }
        return 0;
    }

    return -1;
}

#include "irq.h"

int virtio_blk_read(uint64_t sector, void *buffer, uint32_t sector_count) {
    if (!buffer || sector_count == 0) {
        return -1;
    }

    if (sector + sector_count > blk_capacity_sectors) {
        kprintf("[VIRTIO:BLK] Read out of bounds! Sector %lu + Count %u > Capacity %lu\n",
                sector, sector_count, blk_capacity_sectors);
        return -1;
    }

    irq_disable();
    uint8_t *dst = (uint8_t *)buffer;
    int res = 0;
    for (uint32_t i = 0; i < sector_count; i++) {
        if (virtio_blk_transfer_one(sector + i, dst + (i * SECTOR_SIZE), 0) != 0) {
            res = -1;
            break;
        }
    }
    irq_enable();
    return res;
}

int virtio_blk_write(uint64_t sector, const void *buffer, uint32_t sector_count) {
    if (!buffer || sector_count == 0) {
        return -1;
    }

    if (sector + sector_count > blk_capacity_sectors) {
        kprintf("[VIRTIO:BLK] Write out of bounds! Sector %lu + Count %u > Capacity %lu\n",
                sector, sector_count, blk_capacity_sectors);
        return -1;
    }

    irq_disable();
    const uint8_t *src = (const uint8_t *)buffer;
    int res = 0;
    for (uint32_t i = 0; i < sector_count; i++) {
        if (virtio_blk_transfer_one(sector + i, (void *)(src + (i * SECTOR_SIZE)), 1) != 0) {
            res = -1;
            break;
        }
    }
    irq_enable();
    return res;
}

uint64_t virtio_blk_get_capacity(void) {
    return blk_capacity_sectors;
}
