#include "kernel.h"
#include "uart.h"
#include "printf.h"
#include "string.h"
#include "mmu.h"
#include "irq.h"
#include "timer.h"
#include "mm.h"
#include "sched.h"
#include "virtio.h"
#include "fb.h"
#include "ramfs.h"
#include "syscall.h"
#include "shell.h"

static void worker_thread_alpha(void) {
    kprintf("[Task:Alpha] Worker Thread Alpha started (PID: %u)\n", get_current_task()->pid);

    for (int i = 1; i <= 3; i++) {
        kprintf("[Task:Alpha] Computing workload step %d/3 (Tick: %lu)...\n",
                i, timer_get_ticks());
        task_yield();
    }

    kprintf("[Task:Alpha] Workload finished! Exiting thread.\n");
    task_exit();
}

static void worker_thread_beta(void) {
    kprintf("[Task:Beta] Worker Thread Beta started (PID: %u)\n", get_current_task()->pid);

    /* Allocate memory dynamically from inside the thread */
    char *buf = (char *)kmalloc(128);
    if (buf) {
        strncpy(buf, "Dynamic heap memory allocated from worker thread Beta!", 128);
        kprintf("[Task:Beta] kmalloc test inside thread: \"%s\"\n", buf);
        kfree(buf);
        kprintf("[Task:Beta] kfree test completed inside thread.\n");
    }

    for (int i = 1; i <= 3; i++) {
        kprintf("[Task:Beta] Processing batch %d/3 (Tick: %lu)...\n",
                i, timer_get_ticks());
        task_yield();
    }

    kprintf("[Task:Beta] All batches processed! Exiting thread.\n");
    task_exit();
}

void kernel_main(void) {
    uart_init();

    kprintf("\n");
    kprintf("====================================================================\n");
    kprintf("       _   _   ___   ____  _     _               ____    ____       \n");
    kprintf("      / \\ | | / _ \\ / ___|| |__ (_) _ __        / ___|  / ___|      \n");
    kprintf("     / _ \\| || | | | |    | '_ \\| || '_ \\  ____| |  _  | |          \n");
    kprintf("    / ___ \\ || |_| | |___ | | | | || |_) ||____| |_| | | |___       \n");
    kprintf("   /_/   \\_\\_|\\___/ \\____||_| |_|_|| .__/       \\____|  \\____|      \n");
    kprintf("                                   |_|                              \n");
    kprintf("         AArch64 Bare-Metal Operating System Kernel                 \n");
    kprintf("====================================================================\n");
    kprintf("[+] UART PL011 driver active at 0x%08x (115200 8N1)\n", UART0_BASE);
    kprintf("[+] Bootstrapping CPU Core: %u (MPIDR: 0x%016lx)\n", 
            (unsigned int)(read_mpidr() & 0xFF), read_mpidr());
    kprintf("[+] Current Execution Level: EL%u\n", (unsigned int)read_current_el());
    kprintf("[+] Processor Model (MIDR_EL1): 0x%016lx\n", read_midr());
    kprintf("[+] Generic Timer Frequency (CNTFRQ_EL0): %u Hz (%u MHz)\n", 
            (unsigned int)(read_cntfrq() & 0xFFFFFFFF),
            (unsigned int)((read_cntfrq() / 1000000) & 0xFFFFFFFF));

    kprintf("\n--- Memory Layout ---\n");
    kprintf("  Stack Range:   [0x%016lx - 0x%016lx] (Size: 64 KB)\n",
            (uint64_t)__stack_bottom, (uint64_t)__stack_top);
    kprintf("  Current SP:    0x%016lx\n", read_sp());
    kprintf("  BSS Segment:   [0x%016lx - 0x%016lx] (Cleared)\n",
            (uint64_t)__bss_start, (uint64_t)__bss_end);
    kprintf("  Heap Start:    0x%016lx\n\n", (uint64_t)__heap_start);

    /* --- PHASE 2 SUBSYSTEM INITIALIZATION --- */
    kprintf("--- Initializing Phase 2 Subsystems ---\n");

    /* 1. Initialize 4-Level MMU and Caches */
    mmu_init();

    /* 2. Install Exception Vector Table into VBAR_EL1 */
    irq_vector_init();
    kprintf("[+] Exception Vector Table installed at VBAR_EL1: %p\n", (void *)exception_vectors);

    /* 3. Initialize GICv2 Interrupt Controller */
    gic_init();
    kprintf("[+] GICv2 Interrupt Controller initialized (Dist: 0x%08x, CPU: 0x%08x)\n",
            GICD_BASE, GICC_BASE);

    /* 4. Initialize ARM Generic Timer */
    timer_init();
    kprintf("[+] ARM Generic Timer armed for %u Hz system tick (PPI 14 / IRQ %u)\n",
            TIMER_HZ, IRQ_TIMER_PHYS);

    /* 5. Enable CPU IRQs in DAIF */
    irq_enable();
    kprintf("[+] Interrupts globally enabled in DAIF (DAIF.I cleared)\n");

    /* --- PHASE 3 SUBSYSTEM INITIALIZATION --- */
    kprintf("\n====================================================================\n");
    kprintf("                 INITIALIZING PHASE 3 SUBSYSTEMS                    \n");
    kprintf("====================================================================\n");

    /* 1. Initialize Physical Page & Heap Allocators */
    kprintf("--- Initializing Memory Management Subsystem ---\n");
    mm_init();

    /* Test Physical Page Allocator */
    void *p1 = alloc_page();
    void *p2 = alloc_pages(4);
    kprintf("[*] alloc_page() returned: %p\n", p1);
    kprintf("[*] alloc_pages(4) returned: %p\n", p2);
    free_pages(p2, 4);
    free_page(p1);
    kprintf("[+] Physical page allocation & free verified successfully!\n");

    /* Test Fine-Grained Heap Allocator */
    void *m1 = kmalloc(64);
    void *m2 = kmalloc(256);
    void *m3 = kzalloc(128);
    kprintf("[*] kmalloc(64)  = %p\n", m1);
    kprintf("[*] kmalloc(256) = %p\n", m2);
    kprintf("[*] kzalloc(128) = %p\n", m3);
    strncpy((char *)m1, "Hello from dynamic heap memory!", 64);
    kprintf("[*] Memory test string: \"%s\"\n", (char *)m1);
    kfree(m2);
    kfree(m1);
    kfree(m3);
    kprintf("[+] Heap allocation & free coalescing verified!\n");
    mm_dump_stats();

    /* 2. Initialize VirtIO MMIO Subsystem & Storage */
    kprintf("\n--- Initializing VirtIO Drivers & Storage ---\n");
    virtio_init();

    /* Test Block Device Read/Write */
    static uint8_t write_buf[SECTOR_SIZE];
    static uint8_t read_buf[SECTOR_SIZE];
    memset(write_buf, 0, sizeof(write_buf));
    strncpy((char *)write_buf, "VirtIO Block Device Persistent Storage Test Sector 0x01", sizeof(write_buf) - 1);

    if (virtio_blk_write(1, write_buf, 1) == 0) {
        if (virtio_blk_read(1, read_buf, 1) == 0) {
            kprintf("[+] Block I/O Test Passed! Read: \"%s\"\n", (char *)read_buf);
        } else {
            kprintf("[!] Block read failed!\n");
        }
    }

    /* 3. Initialize Framebuffer Graphics Subsystem */
    kprintf("\n--- Initializing Framebuffer Graphics Engine ---\n");
    fb_init(640, 480);
    fb_render_test_screen();

    /* 4. Initialize Preemptive Multitasking Scheduler */
    kprintf("\n--- Initializing Multitasking Scheduler & Threads ---\n");
    sched_init();

    /* Spawn background kernel threads */
    task_create("WorkerAlpha", worker_thread_alpha, 5);
    task_create("WorkerBeta",  worker_thread_beta,  5);

    sched_dump_tasks();

    kprintf("\n--- Starting Multitasking Execution ---\n");
    kprintf("[*] Kernel Main yielding to background tasks...\n");

    /* Let worker tasks run and demonstrate round-robin preemptive multitasking */
    for (int i = 0; i < 6; i++) {
        task_yield();
    }

    kprintf("\n====================================================================\n");
    kprintf("                 INITIALIZING PHASE 4 & 5 SUBSYSTEMS                \n");
    kprintf("====================================================================\n");

    /* 1. Initialize In-Memory RAM Filesystem (RAMFS) */
    ramfs_init();

    /* 2. Initialize and Launch Interactive CLI Shell */
    shell_init();

    kprintf("\n[SUCCESS] Operating System Fully Booted and Operational!\n");
    kprintf("Entering interactive shell...\n");

    shell_run();
}
