#include "irq.h"
#include "syscall.h"
#include "timer.h"
#include "printf.h"
#include "sched.h"

#define GICD_REG(offset) (*(volatile uint32_t *)(GICD_BASE + (offset)))
#define GICC_REG(offset) (*(volatile uint32_t *)(GICC_BASE + (offset)))

void gic_init(void) {
    /* Disable Distributor while configuring */
    GICD_REG(GICD_CTLR) = 0;

    /* Read number of interrupt lines supported (in groups of 32) */
    uint32_t typer = GICD_REG(GICD_TYPER);
    uint32_t it_lines = (typer & 0x1F) + 1;

    /* Disable and clear all pending interrupts */
    for (uint32_t i = 0; i < it_lines; i++) {
        GICD_REG(GICD_ICENABLER + (i * 4)) = 0xFFFFFFFF;
        GICD_REG(GICD_ICPENDR + (i * 4))   = 0xFFFFFFFF;
    }

    /* Set default priority (0xA0) for all interrupts */
    for (uint32_t i = 0; i < it_lines * 8; i++) {
        GICD_REG(GICD_IPRIORITYR + (i * 4)) = 0xA0A0A0A0;
    }

    /* Direct all SPIs (interrupts >= 32) to CPU core 0 (affinity mask 0x01) */
    for (uint32_t i = 8; i < it_lines * 8; i++) {
        GICD_REG(GICD_ITARGETSR + (i * 4)) = 0x01010101;
    }

    /* Enable Distributor (Group 0 & Group 1) */
    GICD_REG(GICD_CTLR) = 0x1;

    /* Configure CPU Interface */
    /* PMR: unmask all interrupt priority levels below 0xF0 */
    GICC_REG(GICC_PMR) = 0xF0;

    /* Enable CPU Interface signaling */
    GICC_REG(GICC_CTLR) = 0x1;
}

void gic_enable_irq(uint32_t irq) {
    uint32_t reg_index = irq / 32;
    uint32_t bit_index = irq % 32;
    GICD_REG(GICD_ISENABLER + (reg_index * 4)) = (1U << bit_index);
}

void gic_disable_irq(uint32_t irq) {
    uint32_t reg_index = irq / 32;
    uint32_t bit_index = irq % 32;
    GICD_REG(GICD_ICENABLER + (reg_index * 4)) = (1U << bit_index);
}

void handle_sync_exception(struct trap_frame *tf) {
    uint32_t ec = (uint32_t)((tf->esr >> 26) & 0x3F);
    uint32_t iss = (uint32_t)(tf->esr & 0x01FFFFFF);

    /* Check if exception was an SVC call from EL1 or EL0 */
    if (ec == 0x15 || ec == 0x11) { /* 0x15: SVC in AArch64, 0x11: SVC in AArch32 */
        uint32_t imm = iss & 0xFFFF;
        if (imm == 0 && tf->regs[8] != 0) {
            /* Standard AArch64 syscall calling convention: syscall num in x8, args in x0-x3 */
            tf->regs[0] = (uint64_t)sys_call_dispatch(tf->regs[8], tf->regs[0], tf->regs[1], tf->regs[2], tf->regs[3]);
        } else {
            kprintf("[EXC:SVC] Syscall #%u trapped successfully! (x0=0x%lx, x1=0x%lx)\n",
                    imm, tf->regs[0], tf->regs[1]);
            tf->regs[0] = 0; /* Standard test return */
        }
        return;
    }

    /* Unhandled Synchronous Exception Panic */
    kprintf("\n============================================================\n");
    kprintf("                 KERNEL SYNCHRONOUS EXCEPTION PANIC         \n");
    kprintf("============================================================\n");
    kprintf("ESR_EL1: 0x%016lx | EC: 0x%02x | ISS: 0x%08x\n", tf->esr, ec, iss);
    kprintf("FAR_EL1: 0x%016lx (Fault Virtual Address)\n", tf->far);
    kprintf("ELR_EL1: 0x%016lx (Exception PC)\n", tf->elr);
    kprintf("SPSR_EL1:0x%016lx\n", tf->spsr);
    kprintf("SP:      0x%016lx\n\n", tf->sp);

    kprintf("--- General Purpose Registers ---\n");
    for (int i = 0; i < 30; i += 2) {
        kprintf("  x%02d: 0x%016lx   x%02d: 0x%016lx\n", i, tf->regs[i], i + 1, tf->regs[i + 1]);
    }
    kprintf("  x30: 0x%016lx\n", tf->regs[30]);
    kprintf("============================================================\n");
    kprintf("[HALT] System halted due to unhandled exception.\n");

    while (1) {
        __asm__ volatile("wfe");
    }
}

static volatile uint64_t irq_total_count = 0;

void handle_irq(struct trap_frame *tf) {
    (void)tf;
    uint32_t iar = GICC_REG(GICC_IAR);
    uint32_t intid = iar & 0x3FF;

    if (intid == 1023) {
        /* Spurious interrupt */
        return;
    }

    irq_total_count++;

    if (intid == IRQ_TIMER_PHYS) {
        timer_handler();
    } else {
        kprintf("[IRQ] Received interrupt ID: %u\n", intid);
    }

    /* Signal End of Interrupt */
    GICC_REG(GICC_EOIR) = iar;

    /* Preemption: if time slice expired and runnable tasks exist, context switch */
    if (sched_should_preempt()) {
        schedule();
    }
}

uint64_t irq_get_count(void) {
    return irq_total_count;
}

void handle_fiq(struct trap_frame *tf) {
    (void)tf;
    kprintf("[PANIC] Unhandled FIQ Exception!\n");
    while (1) {
        __asm__ volatile("wfe");
    }
}

void handle_serror(struct trap_frame *tf) {
    (void)tf;
    kprintf("[PANIC] Unhandled System Error (SError) Exception!\n");
    while (1) {
        __asm__ volatile("wfe");
    }
}
