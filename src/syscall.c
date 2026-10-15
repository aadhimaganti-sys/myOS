#include "syscall.h"
#include "uart.h"
#include "timer.h"
#include "sched.h"
#include "mm.h"
#include "printf.h"

int64_t sys_call_dispatch(uint64_t num, uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3) {
    (void)a2;
    (void)a3;

    switch (num) {
        case SYS_UART_PUTC:
            uart_putc((char)a0);
            return 0;

        case SYS_UART_GETC:
            return (int64_t)uart_getc();

        case SYS_GET_TICKS:
            return (int64_t)timer_get_ticks();

        case SYS_YIELD:
            task_yield();
            return 0;

        case SYS_EXIT:
            task_exit();
            return 0;

        case SYS_INFO:
            kprintf("[SYSCALL] sys_info called (ticks=%lu, free_pages=%lu)\n",
                    timer_get_ticks(), mm_get_free_pages());
            return 0;

        case SYS_MEM_STAT:
            if (a0) {
                *(uint64_t *)a0 = mm_get_free_pages();
            }
            if (a1) {
                *(uint64_t *)a1 = mm_get_total_pages();
            }
            return 0;

        default:
            kprintf("[SYSCALL] Unknown syscall #%lu (args: 0x%lx, 0x%lx)\n", num, a0, a1);
            return -1;
    }
}
