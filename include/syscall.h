#ifndef _SYSCALL_H
#define _SYSCALL_H

#include "types.h"

/* System Call Numbers */
#define SYS_UART_PUTC  1
#define SYS_UART_GETC  2
#define SYS_GET_TICKS  3
#define SYS_YIELD      4
#define SYS_EXIT       5
#define SYS_INFO       6
#define SYS_MEM_STAT   7

/* Kernel Dispatcher */
int64_t sys_call_dispatch(uint64_t num, uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3);

/* User-facing inline assembly syscall wrappers */
static inline int64_t syscall0(uint64_t num) {
    register uint64_t x8 __asm__("x8") = num;
    register uint64_t x0 __asm__("x0");
    __asm__ volatile("svc #0" : "=r"(x0) : "r"(x8) : "memory");
    return (int64_t)x0;
}

static inline int64_t syscall1(uint64_t num, uint64_t a0) {
    register uint64_t x8 __asm__("x8") = num;
    register uint64_t x0 __asm__("x0") = a0;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8) : "memory");
    return (int64_t)x0;
}

static inline int64_t syscall2(uint64_t num, uint64_t a0, uint64_t a1) {
    register uint64_t x8 __asm__("x8") = num;
    register uint64_t x0 __asm__("x0") = a0;
    register uint64_t x1 __asm__("x1") = a1;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8), "r"(x1) : "memory");
    return (int64_t)x0;
}

static inline int64_t syscall3(uint64_t num, uint64_t a0, uint64_t a1, uint64_t a2) {
    register uint64_t x8 __asm__("x8") = num;
    register uint64_t x0 __asm__("x0") = a0;
    register uint64_t x1 __asm__("x1") = a1;
    register uint64_t x2 __asm__("x2") = a2;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2) : "memory");
    return (int64_t)x0;
}

#endif /* _SYSCALL_H */
