#ifndef _IRQ_H
#define _IRQ_H

#include "types.h"

/* GICv2 Base Addresses for QEMU virt machine */
#define GICD_BASE 0x08000000ULL
#define GICC_BASE 0x08010000ULL

/* GIC Distributor Register Offsets */
#define GICD_CTLR       0x000
#define GICD_TYPER      0x004
#define GICD_IIDR       0x008
#define GICD_IGROUPR    0x080
#define GICD_ISENABLER  0x100
#define GICD_ICENABLER  0x180
#define GICD_ISPENDR    0x200
#define GICD_ICPENDR    0x280
#define GICD_ISACTIVER  0x300
#define GICD_ICACTIVER  0x380
#define GICD_IPRIORITYR 0x400
#define GICD_ITARGETSR  0x800
#define GICD_ICFGR      0xC00

/* GIC CPU Interface Register Offsets */
#define GICC_CTLR       0x000
#define GICC_PMR        0x004
#define GICC_BPR        0x008
#define GICC_IAR        0x00C
#define GICC_EOIR       0x010
#define GICC_RPR        0x014
#define GICC_HPPIR      0x018

/* Interrupt Numbers */
#define IRQ_TIMER_PHYS  30  /* Generic Timer PPI 14 (16 + 14 = 30) */
#define IRQ_UART0       33  /* PL011 UART SPI 1 (32 + 1 = 33) */

/* Trap frame containing CPU state saved by vector stubs */
struct trap_frame {
    uint64_t regs[31]; /* x0 - x30 */
    uint64_t sp;       /* Stack pointer at exception time */
    uint64_t elr;      /* Exception Link Register (PC to return to) */
    uint64_t spsr;     /* Saved Program Status Register */
    uint64_t esr;      /* Exception Syndrome Register */
    uint64_t far;      /* Fault Address Register */
};

extern uint8_t exception_vectors[];

/* Exception handling & GIC API */
void gic_init(void);
void gic_enable_irq(uint32_t irq);
void gic_disable_irq(uint32_t irq);

void irq_vector_init(void);
void irq_enable(void);
void irq_disable(void);

void handle_sync_exception(struct trap_frame *tf);
void handle_irq(struct trap_frame *tf);
void handle_fiq(struct trap_frame *tf);
void handle_serror(struct trap_frame *tf);
uint64_t irq_get_count(void);

#endif /* _IRQ_H */
