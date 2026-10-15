#include "shell.h"
#include "uart.h"
#include "printf.h"
#include "string.h"
#include "kernel.h"
#include "ramfs.h"
#include "mm.h"
#include "sched.h"
#include "timer.h"
#include "syscall.h"

static void cmd_help(int argc, char **argv);
static void cmd_info(int argc, char **argv);
static void cmd_clear(int argc, char **argv);
static void cmd_echo(int argc, char **argv);
static void cmd_ls(int argc, char **argv);
static void cmd_cat(int argc, char **argv);
static void cmd_write(int argc, char **argv);
static void cmd_touch(int argc, char **argv);
static void cmd_rm(int argc, char **argv);
static void cmd_alloc(int argc, char **argv);
static void cmd_free(int argc, char **argv);
static void cmd_ps(int argc, char **argv);
static void cmd_spawn(int argc, char **argv);
static void cmd_yield(int argc, char **argv);
static void cmd_svc(int argc, char **argv);
static void cmd_reboot(int argc, char **argv);

struct shell_cmd {
    const char *name;
    const char *desc;
    void (*func)(int argc, char **argv);
};

static const struct shell_cmd commands[] = {
    {"help",   "Display this list of available commands",      cmd_help},
    {"info",   "Print CPU registers, hardware info, and memory", cmd_info},
    {"clear",  "Clear the terminal screen",                   cmd_clear},
    {"echo",   "Echo arguments to terminal",                  cmd_echo},
    {"ls",     "List all files in the RAM filesystem",        cmd_ls},
    {"cat",    "Display contents of a file: cat <file>",      cmd_cat},
    {"write",  "Write text to a file: write <file> <text>",   cmd_write},
    {"touch",  "Create an empty file: touch <file>",          cmd_touch},
    {"rm",     "Delete a file: rm <file>",                    cmd_rm},
    {"alloc",  "Allocate heap memory: alloc <bytes>",         cmd_alloc},
    {"free",   "Free heap memory: free <hex_addr>",           cmd_free},
    {"ps",     "List active multitasking tasks",              cmd_ps},
    {"spawn",  "Spawn a demo background multitasking thread", cmd_spawn},
    {"yield",  "Cooperatively yield CPU execution",           cmd_yield},
    {"svc",    "Trigger synchronous system call via svc #0",  cmd_svc},
    {"reboot", "Reset the system via ARM PSCI firmware call", cmd_reboot},
    {NULL, NULL, NULL}
};

static uint64_t parse_hex(const char *str) {
    uint64_t val = 0;
    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        str += 2;
    }
    while (*str) {
        char c = *str++;
        val <<= 4;
        if (c >= '0' && c <= '9') val |= (c - '0');
        else if (c >= 'a' && c <= 'f') val |= (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') val |= (c - 'A' + 10);
        else break;
    }
    return val;
}

static uint64_t parse_dec(const char *str) {
    uint64_t val = 0;
    while (*str >= '0' && *str <= '9') {
        val = val * 10 + (*str++ - '0');
    }
    return val;
}

static void cmd_help(int argc, char **argv) {
    (void)argc;
    (void)argv;
    kprintf("\nAvailable Shell Commands:\n");
    kprintf("--------------------------------------------------------------------\n");
    for (int i = 0; commands[i].name != NULL; i++) {
        kprintf("  %-8s - %s\n", commands[i].name, commands[i].desc);
    }
    kprintf("--------------------------------------------------------------------\n\n");
}

static void cmd_info(int argc, char **argv) {
    (void)argc;
    (void)argv;
    kprintf("\n--- System Hardware Diagnostics ---\n");
    kprintf("  Current EL:    EL%u\n", (unsigned int)read_current_el());
    kprintf("  MIDR_EL1:      0x%016lx\n", read_midr());
    kprintf("  MPIDR_EL1:     0x%016lx (Core: %u)\n", 
            read_mpidr(), (unsigned int)(read_mpidr() & 0xFF));
    kprintf("  CNTFRQ_EL0:    %u Hz (%u MHz)\n", 
            (unsigned int)read_cntfrq(), (unsigned int)(read_cntfrq() / 1000000));
    kprintf("  Current SP:    0x%016lx\n", read_sp());
    kprintf("  System Uptime: %lu ticks (%lu sec)\n", 
            timer_get_ticks(), timer_get_ticks() / 100);

    kprintf("\n--- Memory Subsystem Diagnostics ---\n");
    mm_dump_stats();
    kprintf("\n");
}

static void cmd_clear(int argc, char **argv) {
    (void)argc;
    (void)argv;
    uart_puts("\033[2J\033[H");
}

static void cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        kprintf("%s%s", argv[i], (i + 1 < argc) ? " " : "");
    }
    kprintf("\n");
}

static void cmd_ls(int argc, char **argv) {
    (void)argc;
    (void)argv;
    ramfs_list();
}

static void cmd_cat(int argc, char **argv) {
    if (argc < 2) {
        kprintf("Usage: cat <filename>\n");
        return;
    }
    char buf[512];
    int bytes = ramfs_read(argv[1], buf, sizeof(buf));
    if (bytes < 0) {
        kprintf("cat: '%s': No such file\n", argv[1]);
        return;
    }
    kprintf("%s\n", buf);
}

static void cmd_write(int argc, char **argv) {
    if (argc < 3) {
        kprintf("Usage: write <filename> <text...>\n");
        return;
    }
    char buffer[1024];
    buffer[0] = '\0';
    size_t curr = 0;
    for (int i = 2; i < argc; i++) {
        size_t len = strlen(argv[i]);
        if (curr + len + 2 < sizeof(buffer)) {
            memcpy(&buffer[curr], argv[i], len);
            curr += len;
            if (i + 1 < argc) {
                buffer[curr++] = ' ';
            }
        }
    }
    buffer[curr++] = '\n';
    buffer[curr] = '\0';

    int ret = ramfs_write(argv[1], buffer, curr);
    if (ret >= 0) {
        kprintf("[+] Successfully wrote %d bytes to '%s'\n", ret, argv[1]);
    } else {
        kprintf("[-] Failed to write to file '%s'\n", argv[1]);
    }
}

static void cmd_touch(int argc, char **argv) {
    if (argc < 2) {
        kprintf("Usage: touch <filename>\n");
        return;
    }
    int ret = ramfs_create(argv[1]);
    if (ret == 0) {
        kprintf("[+] Created file '%s'\n", argv[1]);
    } else if (ret == -2) {
        kprintf("[-] RAMFS is full\n");
    } else {
        kprintf("[-] Error creating file '%s'\n", argv[1]);
    }
}

static void cmd_rm(int argc, char **argv) {
    if (argc < 2) {
        kprintf("Usage: rm <filename>\n");
        return;
    }
    int ret = ramfs_delete(argv[1]);
    if (ret == 0) {
        kprintf("[+] Deleted file '%s'\n", argv[1]);
    } else {
        kprintf("[-] File '%s' not found\n", argv[1]);
    }
}

static void cmd_alloc(int argc, char **argv) {
    if (argc < 2) {
        kprintf("Usage: alloc <bytes>\n");
        return;
    }
    size_t bytes = (size_t)parse_dec(argv[1]);
    if (bytes == 0) {
        kprintf("[-] Invalid allocation size\n");
        return;
    }
    void *ptr = kmalloc(bytes);
    if (ptr) {
        kprintf("[+] kmalloc(%lu) succeeded -> pointer: %p\n", bytes, ptr);
    } else {
        kprintf("[-] kmalloc(%lu) failed (Out of memory)\n", bytes);
    }
}

static void cmd_free(int argc, char **argv) {
    if (argc < 2) {
        kprintf("Usage: free <hex_addr>\n");
        return;
    }
    uint64_t addr = parse_hex(argv[1]);
    if (!addr) {
        kprintf("[-] Invalid address: %s\n", argv[1]);
        return;
    }
    kfree((void *)addr);
    kprintf("[+] kfree(%p) completed\n", (void *)addr);
}

static void cmd_ps(int argc, char **argv) {
    (void)argc;
    (void)argv;
    sched_dump_tasks();
}

static void demo_task_worker(void) {
    kprintf("  [Worker Thread] Started executing in background task context!\n");
    for (int i = 1; i <= 3; i++) {
        kprintf("  [Worker Thread] Iteration %d/3 - yielding CPU...\n", i);
        task_yield();
    }
    kprintf("  [Worker Thread] Completed work, calling task_exit()\n");
    task_exit();
}

static void cmd_spawn(int argc, char **argv) {
    (void)argc;
    (void)argv;
    kprintf("[*] Spawning background worker task...\n");
    int pid = task_create("worker_thread", demo_task_worker, 1);
    if (pid >= 0) {
        kprintf("[+] Task spawned with PID %d!\n", pid);
        kprintf("[*] Yielding CPU to worker task...\n");
        task_yield();
        kprintf("[+] Returned to shell after task yield!\n");
    } else {
        kprintf("[-] Failed to spawn task (task table full)\n");
    }
}

static void cmd_yield(int argc, char **argv) {
    (void)argc;
    (void)argv;
    kprintf("[*] Yielding CPU time slice...\n");
    task_yield();
    kprintf("[*] Resumed in shell task.\n");
}

static void cmd_svc(int argc, char **argv) {
    (void)argc;
    (void)argv;
    kprintf("[*] Issuing user-space system call 'syscall0(SYS_INFO)' via svc #0...\n");
    int64_t ret = syscall0(SYS_INFO);
    kprintf("[+] Syscall returned code: %ld\n", ret);
}

static void cmd_reboot(int argc, char **argv) {
    (void)argc;
    (void)argv;
    kprintf("\n[REBOOT] Issuing PSCI 0.2 SYSTEM_RESET (0x84000009)...\n");
    uart_puts("Goodbye!\n\n");

    register uint64_t r0 __asm__("x0") = 0x84000009;
    __asm__ volatile("hvc #0" : "+r"(r0) : : "x1", "x2", "x3");
    __asm__ volatile("smc #0" : "+r"(r0) : : "x1", "x2", "x3");

    while (1) {
        __asm__ volatile("wfe");
    }
}

void shell_execute(char *line) {
    char *argv[SHELL_MAX_ARGS];
    int argc = 0;

    /* Skip leading whitespace */
    while (*line == ' ' || *line == '\t') line++;
    if (*line == '\0') return;

    /* Tokenize command line */
    while (*line != '\0' && argc < SHELL_MAX_ARGS) {
        argv[argc++] = line;
        while (*line != '\0' && *line != ' ' && *line != '\t') {
            line++;
        }
        if (*line == '\0') break;
        *line++ = '\0';
        while (*line == ' ' || *line == '\t') line++;
    }

    /* Search and execute command */
    for (int i = 0; commands[i].name != NULL; i++) {
        if (strcmp(commands[i].name, argv[0]) == 0) {
            commands[i].func(argc, argv);
            return;
        }
    }

    kprintf("myOS: unknown command '%s'. Type 'help' for available commands.\n", argv[0]);
}

void shell_init(void) {
    kprintf("[+] Interactive CLI Shell initialized\n");
}

void shell_run(void) {
    char line[SHELL_MAX_LINE];
    int pos = 0;

    kprintf("\nType 'help' to view all commands, or 'info' for system stats.\n");

    while (1) {
        kprintf("myOS> ");
        pos = 0;
        memset(line, 0, sizeof(line));

        while (1) {
            char c = uart_getc();

            /* Enter / Return */
            if (c == '\r' || c == '\n') {
                uart_putc('\n');
                line[pos] = '\0';
                break;
            }
            /* Backspace / Delete */
            else if (c == '\b' || c == 0x7F) {
                if (pos > 0) {
                    pos--;
                    line[pos] = '\0';
                    uart_puts("\b \b");
                }
            }
            /* Ctrl+C */
            else if (c == 0x03) {
                uart_puts("^C\n");
                pos = 0;
                line[0] = '\0';
                break;
            }
            /* Ctrl+L */
            else if (c == 0x0C) {
                uart_puts("\033[2J\033[H");
                kprintf("myOS> %s", line);
            }
            /* Printable characters */
            else if (c >= 32 && c <= 126) {
                if (pos < SHELL_MAX_LINE - 1) {
                    line[pos++] = c;
                    uart_putc(c);
                }
            }
        }

        if (pos > 0) {
            shell_execute(line);
        }
    }
}
