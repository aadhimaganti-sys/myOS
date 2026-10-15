#include "ramfs.h"
#include "string.h"
#include "printf.h"

static struct ramfs_file files[RAMFS_MAX_FILES];

void ramfs_init(void) {
    memset(files, 0, sizeof(files));

    /* Pre-populate default files */
    const char *readme_content =
        "Welcome to myOS! A complete 64-bit ARM bare-metal operating system.\n"
        "Features:\n"
        " - 4-Level MMU Identity Paging (TTBR0_EL1)\n"
        " - ARM GICv2 Interrupt Controller\n"
        " - ARM Generic Timer (100 Hz Preemptive Tick)\n"
        " - Preemptive Round-Robin Multitasking Scheduler\n"
        " - Dynamic Heap (kmalloc/kfree) & 64MB Page Frame Allocator\n"
        " - VirtIO Block Driver\n"
        " - In-Memory RAM File System (RAMFS)\n"
        " - Interactive CLI Shell with System Calls\n";

    const char *sysinfo_content =
        "OS Name:       myOS Bare-Metal\n"
        "Architecture:  AArch64 (ARMv8-A 64-bit)\n"
        "Processor:     ARM Cortex-A57\n"
        "Target:        QEMU virt machine\n"
        "Kernel EL:     EL1\n"
        "Page Size:     4096 bytes (4-Level Paging, 48-bit VA, 40-bit PA)\n"
        "Heap Pool:     64 MB Dynamic Heap\n"
        "Timer:         Generic Physical Timer (10ms tick / 100 Hz)\n";

    ramfs_create("readme.txt");
    ramfs_write("readme.txt", readme_content, strlen(readme_content));

    ramfs_create("sysinfo.txt");
    ramfs_write("sysinfo.txt", sysinfo_content, strlen(sysinfo_content));

    kprintf("[+] RAMFS initialized (pre-populated 'readme.txt' and 'sysinfo.txt')\n");
}

struct ramfs_file *ramfs_find(const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (files[i].is_used && strcmp(files[i].name, name) == 0) {
            return &files[i];
        }
    }
    return NULL;
}

int ramfs_create(const char *name) {
    if (!name || strlen(name) == 0 || strlen(name) >= RAMFS_MAX_NAME) {
        return -1;
    }
    /* Check if already exists */
    if (ramfs_find(name) != NULL) {
        return 0; /* Already exists */
    }
    /* Find free slot */
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (!files[i].is_used) {
            strncpy(files[i].name, name, RAMFS_MAX_NAME - 1);
            files[i].name[RAMFS_MAX_NAME - 1] = '\0';
            files[i].size = 0;
            files[i].is_used = 1;
            memset(files[i].data, 0, RAMFS_MAX_SIZE);
            return 0;
        }
    }
    return -2; /* Disk full */
}

int ramfs_write(const char *name, const char *data, size_t len) {
    struct ramfs_file *file = ramfs_find(name);
    if (!file) {
        if (ramfs_create(name) != 0) return -1;
        file = ramfs_find(name);
    }
    if (!file) return -1;

    if (len >= RAMFS_MAX_SIZE) {
        len = RAMFS_MAX_SIZE - 1;
    }
    memcpy(file->data, data, len);
    file->data[len] = '\0';
    file->size = len;
    return (int)len;
}

int ramfs_read(const char *name, char *buf, size_t buf_size) {
    struct ramfs_file *file = ramfs_find(name);
    if (!file || !buf || buf_size == 0) return -1;

    size_t to_read = file->size;
    if (to_read >= buf_size) {
        to_read = buf_size - 1;
    }
    memcpy(buf, file->data, to_read);
    buf[to_read] = '\0';
    return (int)to_read;
}

int ramfs_delete(const char *name) {
    struct ramfs_file *file = ramfs_find(name);
    if (!file) return -1;

    file->is_used = 0;
    file->size = 0;
    memset(file->name, 0, sizeof(file->name));
    memset(file->data, 0, sizeof(file->data));
    return 0;
}

void ramfs_list(void) {
    int count = 0;
    size_t total_size = 0;
    kprintf("\n  INDEX  NAME                 SIZE (Bytes)\n");
    kprintf("  -----------------------------------------\n");
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (files[i].is_used) {
            kprintf("  [%02d]   %-20s %lu\n", i, files[i].name, files[i].size);
            count++;
            total_size += files[i].size;
        }
    }
    kprintf("  -----------------------------------------\n");
    kprintf("  Total: %d file(s), %lu bytes used (%lu max per file)\n\n",
            count, total_size, (size_t)RAMFS_MAX_SIZE);
}
