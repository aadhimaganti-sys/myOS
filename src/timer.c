#include "timer.h"
#include "irq.h"
#include "kernel.h"
#include "printf.h"
#include "sched.h"

volatile uint64_t system_ticks = 0;
static uint32_t timer_interval = 0;

static inline void write_cntp_tval(uint32_t val) {
    __asm__ volatile("msr cntp_tval_el0, %0" :: "r"((uint64_t)val));
}

static inline void write_cntp_ctl(uint32_t val) {
    __asm__ volatile("msr cntp_ctl_el0, %0" :: "r"((uint64_t)val));
}

static inline uint32_t read_cntp_ctl(void) {
    uint64_t val;
    __asm__ volatile("mrs %0, cntp_ctl_el0" : "=r"(val));
    return (uint32_t)val;
}

void timer_init(void) {
    uint64_t cntfrq = read_cntfrq();
    timer_interval = (uint32_t)(cntfrq / TIMER_HZ);

    /* Disable timer before setting interval */
    write_cntp_ctl(0);

    /* Program initial countdown value */
    write_cntp_tval(timer_interval);

    /* Enable Physical Timer: bit 0 = ENABLE (1), bit 1 = IMASK (0 = unmask) */
    write_cntp_ctl(1);

    /* Enable IRQ 30 (Physical Timer PPI 14) on GIC */
    gic_enable_irq(IRQ_TIMER_PHYS);
}

void timer_handler(void) {
    /* Re-arm the timer countdown */
    write_cntp_tval(timer_interval);

    system_ticks++;
    sched_tick();

    /* Heartbeat every 100 ticks (1 second) */
    if (system_ticks % 100 == 0) {
        kprintf("[TICK] System Uptime: %u sec (%lu ticks)\n",
                (unsigned int)(system_ticks / 100), system_ticks);
    }
}

uint64_t timer_get_ticks(void) {
    return system_ticks;
}

void timer_sleep_ms(uint32_t ms) {
    uint64_t ticks_to_wait = (uint64_t)ms / (1000 / TIMER_HZ);
    if (ticks_to_wait == 0) ticks_to_wait = 1;
    uint64_t target = system_ticks + ticks_to_wait;
    while (system_ticks < target) {
        task_yield();
        __asm__ volatile("wfi");
    }
}
