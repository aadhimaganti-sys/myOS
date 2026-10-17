#include "sched.h"
#include "mm.h"
#include "string.h"
#include "printf.h"
#include "irq.h"

static struct task task_table[MAX_TASKS];
static struct task *current_task = NULL;
static uint32_t next_pid = 1;
static int sched_initialized = 0;
static volatile int preempt_requested = 0;

static void task_entry_trampoline(void) {
    /* Globally enable interrupts for the new task */
    irq_enable();

    if (current_task && current_task->entry) {
        current_task->entry();
    }

    task_exit();
}

void sched_init(void) {
    memset(task_table, 0, sizeof(task_table));

    /* Initialize Task 0 (Kernel Master Thread) */
    struct task *t0 = &task_table[0];
    t0->pid = 0;
    t0->state = TASK_RUNNING;
    t0->priority = 10;
    t0->ticks = 0;
    t0->time_slice = TASK_TIME_SLICE;
    t0->entry = NULL;
    t0->stack = NULL; /* Uses boot/kernel stack */
    t0->stack_size = 0x10000;
    strncpy(t0->name, "kmain", sizeof(t0->name) - 1);

    current_task = t0;
    sched_initialized = 1;
    preempt_requested = 0;

    kprintf("[+] Preemptive Multitasking Scheduler initialized (Max Tasks: %u, Quantum: %u ticks)\n",
            MAX_TASKS, TASK_TIME_SLICE);
}

int task_create(const char *name, void (*entry)(void), uint64_t priority) {
    if (!entry) {
        return -1;
    }

    /* Find an available slot */
    int slot = -1;
    for (int i = 1; i < MAX_TASKS; i++) {
        if (task_table[i].state == TASK_UNUSED || task_table[i].state == TASK_DEAD) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        kprintf("[SCHED:ERROR] Task table full, cannot create '%s'!\n", name);
        return -1;
    }

    struct task *t = &task_table[slot];
    memset(t, 0, sizeof(struct task));

    t->pid = next_pid++;
    t->state = TASK_READY;
    t->priority = priority;
    t->ticks = 0;
    t->time_slice = TASK_TIME_SLICE;
    t->entry = entry;
    t->stack_size = TASK_STACK_SIZE;

    t->stack = (uint8_t *)kmalloc(TASK_STACK_SIZE);
    if (!t->stack) {
        kprintf("[SCHED:ERROR] Failed to allocate %u bytes stack for task '%s'!\n",
                TASK_STACK_SIZE, name);
        t->state = TASK_UNUSED;
        return -1;
    }

    if (name) {
        strncpy(t->name, name, sizeof(t->name) - 1);
    } else {
        strncpy(t->name, "task", sizeof(t->name) - 1);
    }

    /* Setup CPU context for switch_to */
    uintptr_t stack_top = ((uintptr_t)t->stack + TASK_STACK_SIZE) & ~15ULL;
    t->context.sp = stack_top;
    t->context.lr = (uintptr_t)task_entry_trampoline;
    t->context.fp = 0;

    kprintf("[+] Created Task PID %u ('%s') at stack [0x%016lx - 0x%016lx]\n",
            t->pid, t->name, (uint64_t)t->stack, (uint64_t)stack_top);

    return t->pid;
}

static int get_current_index(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (&task_table[i] == current_task) {
            return i;
        }
    }
    return 0;
}

static int count_ready_tasks(void) {
    int count = 0;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (task_table[i].state == TASK_READY) {
            count++;
        }
    }
    return count;
}

void schedule(void) {
    if (!sched_initialized) {
        return;
    }

    preempt_requested = 0;

    int curr_idx = get_current_index();
    int next_idx = -1;

    /* Scan round-robin for next TASK_READY task */
    for (int i = 1; i <= MAX_TASKS; i++) {
        int candidate = (curr_idx + i) % MAX_TASKS;
        if (task_table[candidate].state == TASK_READY) {
            next_idx = candidate;
            break;
        }
    }

    /* If no other ready task is found, keep running current task if it is running */
    if (next_idx == -1) {
        if (current_task->state == TASK_RUNNING) {
            current_task->time_slice = TASK_TIME_SLICE;
            return;
        }
        /* Fall back to idle task if current task exited or blocked */
        next_idx = 0;
    }

    struct task *prev = current_task;
    struct task *next = &task_table[next_idx];

    if (prev == next) {
        return;
    }

    if (prev->state == TASK_RUNNING) {
        prev->state = TASK_READY;
    }

    next->state = TASK_RUNNING;
    next->time_slice = TASK_TIME_SLICE;
    current_task = next;

    cpu_switch_to(prev, next);
}

void task_yield(void) {
    schedule();
}

void task_exit(void) {
    if (!current_task) {
        return;
    }

    kprintf("[*] Task PID %u ('%s') completed execution. Terminating.\n",
            current_task->pid, current_task->name);

    current_task->state = TASK_DEAD;

    if (current_task->stack) {
        kfree(current_task->stack);
        current_task->stack = NULL;
    }

    schedule();

    /* If reached, no runnable tasks remain */
    while (1) {
        __asm__ volatile("wfe");
    }
}

struct task *get_current_task(void) {
    return current_task;
}

void sched_tick(void) {
    if (!sched_initialized || !current_task) {
        return;
    }

    current_task->ticks++;

    if (current_task->time_slice > 0) {
        current_task->time_slice--;
    }

    if (current_task->time_slice == 0) {
        if (count_ready_tasks() > 0) {
            preempt_requested = 1;
        } else {
            current_task->time_slice = TASK_TIME_SLICE;
        }
    }
}

int sched_should_preempt(void) {
    return (sched_initialized && preempt_requested && count_ready_tasks() > 0);
}

int sched_get_active_task_count(void) {
    int count = 0;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (task_table[i].state != TASK_UNUSED && task_table[i].state != TASK_DEAD) {
            count++;
        }
    }
    return count;
}

void sched_dump_tasks(void) {
    static const char *state_names[] = {
        "UNUSED", "READY", "RUNNING", "SLEEPING", "DEAD"
    };

    kprintf("--- Active Process & Thread Table ---\n");
    kprintf("  PID  | Name             | State    | Prio | Ticks    | Stack Base\n");
    kprintf("-------+------------------+----------+------+----------+--------------------\n");

    for (int i = 0; i < MAX_TASKS; i++) {
        struct task *t = &task_table[i];
        if (t->state != TASK_UNUSED) {
            kprintf("  %-4u | %-16s | %-8s | %-4u | %-8lu | %p\n",
                    t->pid, t->name, state_names[t->state],
                    (unsigned int)t->priority, t->ticks, (void *)t->stack);
        }
    }
}
