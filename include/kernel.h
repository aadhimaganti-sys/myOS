#ifndef _KERNEL_H
#define _KERNEL_H

#include "types.h"

/* Linker symbols */
extern uint8_t __bss_start[];
extern uint8_t __bss_end[];
extern uint8_t __stack_bottom[];
extern uint8_t __stack_top[];
extern uint8_t __heap_start[];

/* System register helpers */
static inline uint64_t read_current_el(void) {
    uint64_t el;
    __asm__ volatile("mrs %0, CurrentEL" : "=r"(el));
    return (el >> 2) & 0x3;
}

static inline uint64_t read_midr(void) {
    uint64_t midr;
    __asm__ volatile("mrs %0, midr_el1" : "=r"(midr));
    return midr;
}

static inline uint64_t read_mpidr(void) {
    uint64_t mpidr;
    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
    return mpidr;
}

static inline uint64_t read_cntfrq(void) {
    uint64_t frq;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(frq));
    return frq;
}

static inline uint64_t read_sp(void) {
    uint64_t sp;
    __asm__ volatile("mov %0, sp" : "=r"(sp));
    return sp;
}

void kernel_main(void);

#endif /* _KERNEL_H */
