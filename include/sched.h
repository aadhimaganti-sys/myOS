#ifndef _SCHED_H
#define _SCHED_H

#include "types.h"
#include "stddef.h"

#define MAX_TASKS       16
#define TASK_STACK_SIZE 8192
#define TASK_TIME_SLICE 5 /* Ticks per time slice (50 ms at 100 Hz) */

typedef enum {
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_SLEEPING,
    TASK_DEAD
} task_state_t;

/* Callee-saved registers for AArch64 context switching */
struct cpu_context {
    uint64_t x19;
    uint64_t x20;
    uint64_t x21;
    uint64_t x22;
    uint64_t x23;
    uint64_t x24;
    uint64_t x25;
    uint64_t x26;
    uint64_t x27;
    uint64_t x28;
    uint64_t fp; /* x29 */
    uint64_t lr; /* x30 */
    uint64_t sp;
};

struct task {
    struct cpu_context context;
    uint32_t pid;
    task_state_t state;
    char name[32];
    uint64_t priority;
    uint64_t ticks;
    uint64_t time_slice;
    void (*entry)(void);
    uint8_t *stack;
    size_t stack_size;
};

/* Assembly Context Switch */
extern void cpu_switch_to(struct task *prev, struct task *next);

/* Scheduler API */
void sched_init(void);
int task_create(const char *name, void (*entry)(void), uint64_t priority);
void schedule(void);
void task_yield(void);
void task_exit(void);
struct task *get_current_task(void);
void sched_tick(void);
int sched_should_preempt(void);
void sched_dump_tasks(void);

#endif /* _SCHED_H */
