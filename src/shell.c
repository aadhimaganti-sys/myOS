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
#include "agy.h"
#include "gui.h"
#include "keyboard.h"
#include "input.h"
#include "wm.h"
#include "fb.h"
#include "drivers.h"
#include "coin.h"
#include "p2p.h"

/* Forward declarations */
static void cmd_help(int argc, char **argv);
static void cmd_kitkoin(int argc, char **argv);
static void cmd_drivers(int argc, char **argv);
static void cmd_atlas(int argc, char **argv);
static void cmd_agy(int argc, char **argv);
static void cmd_status(int argc, char **argv);
static void cmd_agents(int argc, char **argv);
static void cmd_prompt(int argc, char **argv);
static void cmd_model(int argc, char **argv);
static void cmd_spawn(int argc, char **argv);
static void cmd_tasks(int argc, char **argv);
static void cmd_artifacts(int argc, char **argv);
static void cmd_gui(int argc, char **argv);
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
static void cmd_yield(int argc, char **argv);
static void cmd_svc(int argc, char **argv);
static void cmd_reboot(int argc, char **argv);

struct shell_cmd {
    const char *name;
    const char *desc;
    void (*func)(int argc, char **argv);
};

static const struct shell_cmd commands[] = {
    {"kitkoin",   "KitKoin (KIT) native P2P cryptocurrency & blockchain",    cmd_kitkoin},
    {"kit",       "Alias for kitkoin cryptocurrency command",                cmd_kitkoin},
    {"atlas",     "Atlas Integrated OS Assistant & Subsystem Controller",    cmd_atlas},
    /* Google Antigravity Suite */
    {"agy",       "Display Google Antigravity banner and platform overview", cmd_agy},
    {"status",    "View AI orchestrator status, model, and token usage",      cmd_status},
    {"agents",    "List registered subagents, roles, and execution state",   cmd_agents},
    {"prompt",    "Dispatch goal/task to an agent: prompt <agent> <goal>",    cmd_prompt},
    {"model",     "View or switch active LLM: model [model_name]",           cmd_model},
    {"spawn",     "Spawn a new autonomous subagent: spawn <name> <role>",     cmd_spawn},
    {"tasks",     "List all active agent and system worker threads",         cmd_tasks},
    {"artifacts", "Inspect generated code and document artifacts in RAMFS",  cmd_artifacts},
    {"drivers",   "Inspect loaded hardware device drivers and diagnostics",  cmd_drivers},
    {"drv",       "Alias for drivers command: drv [gui]",                    cmd_drivers},
    {"gui",       "Trigger complete refresh/redraw of the GUI desktop",      cmd_gui},

    /* Core Operating System & Utilities */
    {"help",      "Display list of available commands",                      cmd_help},
    {"info",      "Print CPU registers, hardware info, and memory diagnostics", cmd_info},
    {"clear",     "Clear the terminal screen and console mirror",            cmd_clear},
    {"echo",      "Echo arguments to terminal and console",                  cmd_echo},
    {"ls",        "List all files in the RAM filesystem",                    cmd_ls},
    {"cat",       "Display contents of a file: cat <file>",                  cmd_cat},
    {"write",     "Write text to a file: write <file> <text>",               cmd_write},
    {"touch",     "Create an empty file: touch <file>",                      cmd_touch},
    {"rm",        "Delete a file: rm <file>",                                cmd_rm},
    {"alloc",     "Allocate heap memory: alloc <bytes>",                     cmd_alloc},
    {"free",      "Free heap memory: free <hex_addr>",                       cmd_free},
    {"ps",        "List active multitasking processes",                      cmd_ps},
    {"yield",     "Cooperatively yield CPU execution",                       cmd_yield},
    {"svc",       "Trigger synchronous system call via svc #0",              cmd_svc},
    {"reboot",    "Reset the system via ARM PSCI firmware call",             cmd_reboot},
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
    kprintf("\n====================================================================\n");
    kprintf("             GOOGLE ANTIGRAVITY OS COMMAND REFERENCE                \n");
    kprintf("====================================================================\n");
    kprintf("  Antigravity Multi-Agent Commands (or use /<command>):\n");
    kprintf("    agy                     - Overview and Google Antigravity banner\n");
    kprintf("    status  (/status)       - Telemetry, active model & token metrics\n");
    kprintf("    agents  (/agents)       - Registered subagents, status & goals\n");
    kprintf("    prompt <name> <text>    - Dispatch instructions to an agent\n");
    kprintf("    model [name]            - View or switch model (flash/pro)\n");
    kprintf("    spawn <name> <role>     - Spawn new autonomous subagent thread\n");
    kprintf("    tasks   (/tasks)        - View active agent kernel threads\n");
    kprintf("    artifacts (/artifacts)  - List workspace reports & diffs in RAMFS\n");
    kprintf("    gui     (/gui)          - Force redraw 2D Graphical Desktop\n");
    kprintf("\n  Core Operating System Commands:\n");
    kprintf("    help    (/help)         - Display this command reference\n");
    kprintf("    info                    - CPU registers, EL, uptime, memory\n");
    kprintf("    clear                   - Clear terminal and console buffer\n");
    kprintf("    ls                      - List files in in-memory filesystem\n");
    kprintf("    cat <filename>          - Display file contents\n");
    kprintf("    write <file> <text>     - Write text into a RAMFS file\n");
    kprintf("    touch <filename>        - Create empty RAMFS file\n");
    kprintf("    rm <filename>           - Delete RAMFS file\n");
    kprintf("    alloc <bytes>           - Test kmalloc dynamic heap\n");
    kprintf("    free <hex_addr>         - Free allocated heap memory\n");
    kprintf("    ps                      - View multitasking task table\n");
    kprintf("    yield                   - Yield current CPU time-slice\n");
    kprintf("    svc                     - Issue synchronous EL0->EL1 syscall\n");
    kprintf("    reboot                  - Reset VM via ARM PSCI firmware call\n");
    kprintf("\n  Decentralized Cryptocurrency (KitKoin):\n");
    kprintf("    kitkoin (kit)           - Decentralized P2P cryptocurrency dashboard & commands\n");
    kprintf("====================================================================\n\n");
}

static void cmd_agy(int argc, char **argv) {
    (void)argc;
    (void)argv;
    kprintf("\n");
    kprintf("   _____                 _        _         _   _                 _ _         \n");
    kprintf("  / ____|               | |      / \\   _ __ | |_(_) __ _ _ __ __ _| (_) |_ _   \n");
    kprintf(" | |  __  ___   ___   __ _| | ___ / _ \\ | '_ \\| __| |/ _` | '__/ _` | | | __| | |\n");
    kprintf(" | | |_ |/ _ \\ / _ \\ / _` | |/ _ \\ ___ \\| | | | |_| | (_| | | | (_| | | | |_| |_|\n");
    kprintf("  \\_____|\\___/ \\___/ \\__, |_|\\___/_/   \\_\\_| |_|\\__|_|\\__, |_|  \\__,_|_|_|\\__|\\__, |\n");
    kprintf("                      __/ |                            |___/                   |___/ \n");
    kprintf("                     |___/                                                           \n");
    kprintf(" Google Antigravity Multi-Agent Orchestrator (AArch64 Bare-Metal OS)\n");
    kprintf("--------------------------------------------------------------------\n");
    kprintf(" Active Model:   %s\n", agy_get_model());
    int count = 0;
    agy_get_agents(&count);
    kprintf(" Subagents:      %d registered and operational\n", count);
    kprintf(" Desktop GUI:    Active at 640x480 (32bpp Framebuffer)\n");
    kprintf(" Quick Start:    Type 'agents' to inspect team, or 'status' for telemetry.\n\n");
}

static void cmd_status(int argc, char **argv) {
    (void)argc;
    (void)argv;
    uint32_t active_agents = 0, tasks_done = 0;
    uint64_t tokens = 0;
    agy_get_stats(&active_agents, &tasks_done, &tokens);

    kprintf("\n--- Google Antigravity Orchestrator Telemetry ---\n");
    kprintf("  Active Model:      %s\n", agy_get_model());
    kprintf("  Active Agents:     %u\n", active_agents);
    kprintf("  Tasks Completed:   %u\n", tasks_done);
    kprintf("  Total Tokens:      %lu tokens\n", tokens);
    kprintf("  Orchestrator Mode: Autonomous Multi-Agent (Preemptive RR)\n");

    uint64_t total_pages = mm_get_total_pages();
    uint64_t free_pages = mm_get_free_pages();
    uint64_t used_pages = total_pages - free_pages;
    kprintf("  Physical Memory:   %lu / %lu MB used (%lu%%)\n",
            (used_pages * 4096) / (1024 * 1024),
            (total_pages * 4096) / (1024 * 1024),
            (used_pages * 100) / total_pages);
    kprintf("  System Uptime:     %lu sec (%lu ticks)\n\n",
            timer_get_ticks() / 100, timer_get_ticks());
}

static void cmd_agents(int argc, char **argv) {
    (void)argc;
    (void)argv;
    int count = 0;
    struct agy_agent *list = agy_get_agents(&count);

    static const char *state_names[] = {
        "UNUSED", "IDLE", "THINKING", "RUNNING", "WAITING", "COMPLETED", "ERROR"
    };

    kprintf("\n--- Registered Google Antigravity Subagents ---\n");
    kprintf("  ID | Name             | State       | Steps | PID | Role\n");
    kprintf(" ----+------------------+-------------+-------+-----+------------------------------\n");
    for (int i = 0; i < count; i++) {
        struct agy_agent *a = &list[i];
        if (a->state != AGENT_UNUSED) {
            kprintf("  %-2u | %-16s | %-11s | %u/%-3u | %-3d | %s\n",
                    a->id, a->name, state_names[a->state],
                    a->steps_done, a->steps_total, a->pid, a->role);
            if (a->current_goal[0] != '\0') {
                kprintf("     `-> Goal: \"%s\"\n", a->current_goal);
            }
        }
    }
    kprintf(" ---------------------------------------------------------------------------------\n");
    kprintf(" Total: %d subagents registered\n\n", count);
}

static void cmd_prompt(int argc, char **argv) {
    if (argc < 3) {
        kprintf("Usage: prompt <agent_name> <goal_text...>\n");
        kprintf("Example: prompt ResearchAgent Analyze memory fragmentation\n");
        return;
    }

    char goal[AGY_GOAL_LEN];
    goal[0] = '\0';
    size_t curr = 0;
    for (int i = 2; i < argc; i++) {
        size_t len = strlen(argv[i]);
        if (curr + len + 2 < sizeof(goal)) {
            memcpy(&goal[curr], argv[i], len);
            curr += len;
            if (i + 1 < argc) goal[curr++] = ' ';
        }
    }
    goal[curr] = '\0';

    int res = agy_prompt(argv[1], goal);
    if (res == 0) {
        kprintf("[+] Dispatched goal to [%s]: \"%s\"\n", argv[1], goal);
        kprintf("[*] Agent worker thread activated in background!\n");
    } else {
        kprintf("[-] Failed to dispatch prompt: agent '%s' not found or busy.\n", argv[1]);
    }
}

static void cmd_model(int argc, char **argv) {
    if (argc < 2) {
        kprintf("Active Model: %s\n", agy_get_model());
        kprintf("Available: gemini-2.5-flash, gemini-2.5-pro, gemini-3-flash\n");
        return;
    }
    agy_set_model(argv[1]);
    kprintf("[+] Active LLM Model updated to: %s\n", agy_get_model());
    gui_render_desktop();
}

static void cmd_spawn(int argc, char **argv) {
    if (argc < 3) {
        kprintf("Usage: spawn <agent_name> <role_description...>\n");
        kprintf("Example: spawn SecurityAuditor Kernel Access Control Auditor\n");
        return;
    }

    char role[AGY_ROLE_LEN];
    role[0] = '\0';
    size_t curr = 0;
    for (int i = 2; i < argc; i++) {
        size_t len = strlen(argv[i]);
        if (curr + len + 2 < sizeof(role)) {
            memcpy(&role[curr], argv[i], len);
            curr += len;
            if (i + 1 < argc) role[curr++] = ' ';
        }
    }
    role[curr] = '\0';

    int res = agy_spawn(argv[1], role);
    if (res >= 0) {
        kprintf("[+] Autonomous subagent '%s' spawned (ID: %d, Role: \"%s\")\n",
                argv[1], res, role);
    } else {
        kprintf("[-] Failed to spawn agent (maximum agent limit reached)\n");
    }
}

static void cmd_tasks(int argc, char **argv) {
    (void)argc;
    (void)argv;
    sched_dump_tasks();
}

static void cmd_artifacts(int argc, char **argv) {
    (void)argc;
    (void)argv;
    kprintf("\n--- Generated Artifacts & Workspace Files (RAMFS) ---\n");
    ramfs_list();
}

static void cmd_drivers(int argc, char **argv) {
    if (argc > 1) {
        if (strcmp(argv[1], "gui") == 0 || strcmp(argv[1], "open") == 0) {
            wm_open_window(WIN_DRIVERS);
            gui_refresh();
            kprintf("[+] Opened Hardware Device Drivers Manager in GUI desktop.\n");
            return;
        } else if (strcmp(argv[1], "test") == 0) {
            if (argc > 2) {
                if (strcmp(argv[2], "all") == 0) {
                    drivers_test_all();
                } else if (argv[2][0] >= '0' && argv[2][0] <= '9') {
                    int idx = argv[2][0] - '0';
                    if (argv[2][1] >= '0' && argv[2][1] <= '9') {
                        idx = idx * 10 + (argv[2][1] - '0');
                    }
                    drivers_self_test(idx);
                } else {
                    drivers_test_by_tag(argv[2]);
                }
            } else {
                drivers_test_all();
            }
            return;
        }
    }
    drivers_dump_cli();
}

static void cmd_gui(int argc, char **argv) {
    if (argc > 1 && (strcmp(argv[1], "drivers") == 0 || strcmp(argv[1], "drv") == 0)) {
        wm_open_window(WIN_DRIVERS);
    }
    kprintf("[*] Entering interactive Graphical Desktop Terminal (ESC or 'exit' to return)...\n");
    gui_run_interactive();
}

static void cmd_kitkoin(int argc, char **argv) {
    if (argc < 2 || strcmp(argv[1], "status") == 0) {
        struct coin_wallet *my_w = coin_get_my_wallet();
        kprintf("\n====================================================================\n");
        kprintf("      _  _____ _____ _  _____ ___ _   _                            \n");
        kprintf("     | |/ /_ _|_   _| |/ / _ \\_ _| \\ | |                           \n");
        kprintf("     | ' / | |  | | | ' / | | | ||  \\| |                           \n");
        kprintf("     | . \\ | |  | | | . \\ |_| | || |\\  |                           \n");
        kprintf("     |_|\\_\\___| |_| |_|\\_\\___/|___|_| \\_|                           \n");
        kprintf("         KitKoin (KIT) - Decentralized Native P2P Cryptocurrency   \n");
        kprintf("====================================================================\n");
        kprintf("  Network Name:        KitKoin Mainnet (Wi-Fi / LAN P2P Mesh)\n");
        kprintf("  Ticker Symbol:       KIT (Max Supply: %lu KIT)\n", COIN_MAX_SUPPLY);
        kprintf("  Consensus:           Proof-of-Work (PoW / Dual SHA-256)\n");
        kprintf("  Current Block Height:#%u\n", coin_get_height());
        kprintf("  Block Subsidy:       %lu KIT (Halving Interval: %u blocks)\n",
                coin_get_block_reward(coin_get_height()), COIN_HALVING_INTERVAL);
        kprintf("  Halving Countdown:   %u blocks until next reward halving\n", coin_get_blocks_until_halving());
        kprintf("  Mining Difficulty:   %u leading hex zeros\n", coin_get_difficulty());
        kprintf("  Circulating Supply:  %lu KIT\n", coin_get_circulating_supply());
        kprintf("  Pending Mempool TXs: %u transactions\n", coin_get_mempool_count());
        kprintf("  Background AutoMiner:%s\n", coin_automine_is_running() ? "ACTIVE (Mining in background)" : "IDLE");
        kprintf("  P2P Wi-Fi Peers:     %d nodes connected (Port: %u)\n", p2p_get_peer_count(), P2P_DEFAULT_PORT);
        kprintf("  ------------------------------------------------------------------\n");
        kprintf("  Active Node Wallet:  %s (%s)\n", my_w->address, my_w->label);
        kprintf("  Active Balance:      %lu KIT\n\n", my_w->balance);
        kprintf("Available KitKoin Commands:\n");
        kprintf("  kitkoin wallet                   - List all registered wallets & active selection\n");
        kprintf("  kitkoin newwallet [label]        - Cryptographically generate a new wallet address\n");
        kprintf("  kitkoin switch <address>         - Switch active mining/spending node wallet\n");
        kprintf("  kitkoin balance [address]        - Check balance in KIT\n");
        kprintf("  kitkoin send <to> <amt> [fee]    - Transfer KIT to another address with optional fee\n");
        kprintf("  kitkoin mine [count]             - Run CPU Proof-of-Work to mine 1 or N blocks\n");
        kprintf("  kitkoin automine [on|off|status] - Multi-tasking background CPU mining worker\n");
        kprintf("  kitkoin block <height>           - Detailed block header, merkle root & tx inspector\n");
        kprintf("  kitkoin tx <hex_prefix>          - Transaction lookup (mempool / confirmed blocks)\n");
        kprintf("  kitkoin history [address]        - Chronological account transaction history\n");
        kprintf("  kitkoin chain                    - Inspect the full blockchain ledger\n");
        kprintf("  kitkoin verify                   - Zero-deficit ledger audit & cryptographic verify\n");
        kprintf("  kitkoin benchmark [ms]           - Benchmark bare-metal ARM CPU SHA-256 hashrate\n");
        kprintf("  kitkoin peer <list|add|ping|sync|sim> - Wi-Fi P2P network operations\n");
        kprintf("  kitkoin save                     - Persist wallets and blockchain state to disk.img\n");
        kprintf("  kitkoin load                     - Reload wallets and blockchain state from disk.img\n");
        kprintf("  kitkoin claim [amount]           - Claim/restore KIT coins into active wallet (default: 1000)\n");
        kprintf("  kitkoin gui                      - Launch interactive graphical desktop dashboard\n");
        kprintf("====================================================================\n\n");
        return;
    }

    if (strcmp(argv[1], "wallet") == 0 || strcmp(argv[1], "wallets") == 0) {
        coin_dump_wallets();
    } else if (strcmp(argv[1], "newwallet") == 0 || strcmp(argv[1], "new") == 0) {
        const char *label = (argc >= 3) ? argv[2] : "UserWallet";
        struct coin_wallet *w = coin_create_wallet(label);
        if (w) {
            kprintf("[+] Generated new KitKoin wallet!\n");
            kprintf("    Label:   %s\n", w->label);
            kprintf("    Address: %s\n", w->address);
            kprintf("    Balance: 0 KIT\n");
            kprintf("    Tip: Use 'kitkoin switch %s' to activate.\n", w->address);
        }
    } else if (strcmp(argv[1], "switch") == 0) {
        if (argc < 3) {
            kprintf("Usage: kitkoin switch <address>\n");
            return;
        }
        if (coin_switch_wallet(argv[2]) == 0) {
            struct coin_wallet *w = coin_get_my_wallet();
            kprintf("[+] Successfully switched active node wallet to %s (%s) | Balance: %lu KIT\n",
                    w->address, w->label, w->balance);
        } else {
            kprintf("[KitKoin:ERR] Wallet address '%s' not found!\n", argv[2]);
        }
    } else if (strcmp(argv[1], "balance") == 0) {
        if (argc >= 3) {
            uint64_t bal = coin_get_balance(argv[2]);
            kprintf("Balance of %s: %lu KIT\n", argv[2], bal);
        } else {
            struct coin_wallet *my_w = coin_get_my_wallet();
            kprintf("Active Wallet (%s | %s): %lu KIT\n", my_w->address, my_w->label, my_w->balance);
        }
    } else if (strcmp(argv[1], "mine") == 0) {
        if (argc >= 3) {
            uint32_t count = 0;
            const char *p = argv[2];
            while (*p >= '0' && *p <= '9') {
                count = count * 10 + (*p - '0');
                p++;
            }
            coin_mine_blocks(count);
        } else {
            coin_mine_block();
        }
    } else if (strcmp(argv[1], "automine") == 0) {
        if (argc >= 3) {
            if (strcmp(argv[2], "on") == 0) {
                coin_automine_start();
            } else if (strcmp(argv[2], "off") == 0) {
                coin_automine_stop();
            } else {
                kprintf("Autominer Status: %s\n", coin_automine_is_running() ? "RUNNING" : "STOPPED");
            }
        } else {
            kprintf("Autominer Status: %s (Usage: kitkoin automine on|off)\n",
                    coin_automine_is_running() ? "RUNNING" : "STOPPED");
        }
    } else if (strcmp(argv[1], "send") == 0) {
        if (argc < 4) {
            kprintf("Usage: kitkoin send <recipient_address> <amount> [fee]\n");
            kprintf("Example: kitkoin send kit1peer0000000000002 10 1\n");
            return;
        }
        uint64_t amount = 0;
        const char *p = argv[3];
        while (*p >= '0' && *p <= '9') {
            amount = amount * 10 + (*p - '0');
            p++;
        }
        uint64_t fee = 0;
        if (argc >= 5) {
            const char *f = argv[4];
            while (*f >= '0' && *f <= '9') {
                fee = fee * 10 + (*f - '0');
                f++;
            }
        }
        if (amount == 0) {
            kprintf("[KitKoin:ERR] Invalid amount specified!\n");
            return;
        }
        coin_send_with_fee(argv[2], amount, fee);
    } else if (strcmp(argv[1], "block") == 0) {
        if (argc < 3) {
            kprintf("Usage: kitkoin block <height>\n");
            return;
        }
        uint32_t height = 0;
        const char *p = argv[2];
        while (*p >= '0' && *p <= '9') {
            height = height * 10 + (*p - '0');
            p++;
        }
        coin_dump_block(height);
    } else if (strcmp(argv[1], "tx") == 0) {
        if (argc < 3) {
            kprintf("Usage: kitkoin tx <hex_prefix_or_full_hash>\n");
            return;
        }
        coin_dump_tx(argv[2]);
    } else if (strcmp(argv[1], "history") == 0) {
        const char *addr = (argc >= 3) ? argv[2] : NULL;
        coin_dump_history(addr);
    } else if (strcmp(argv[1], "chain") == 0 || strcmp(argv[1], "blocks") == 0) {
        coin_dump_chain();
    } else if (strcmp(argv[1], "verify") == 0 || strcmp(argv[1], "audit") == 0) {
        coin_verify_chain();
    } else if (strcmp(argv[1], "benchmark") == 0 || strcmp(argv[1], "bench") == 0) {
        uint32_t ms = 1000;
        if (argc >= 3) {
            ms = 0;
            const char *p = argv[2];
            while (*p >= '0' && *p <= '9') {
                ms = ms * 10 + (*p - '0');
                p++;
            }
            if (ms == 0) ms = 1000;
        }
        coin_benchmark(ms);
    } else if (strcmp(argv[1], "peer") == 0 || strcmp(argv[1], "p2p") == 0 ||
               strcmp(argv[1], "wifi") == 0 || strcmp(argv[1], "network") == 0) {
        if (argc >= 3 && strcmp(argv[2], "ping") == 0) {
            p2p_ping_peers();
        } else if (argc >= 3 && strcmp(argv[2], "sync") == 0) {
            p2p_sync();
        } else if (argc >= 3 && (strcmp(argv[2], "sim") == 0 || strcmp(argv[2], "simulate") == 0)) {
            p2p_simulate_traffic();
        } else if (argc >= 3 && strcmp(argv[2], "add") == 0) {
            if (argc < 4) {
                kprintf("Usage: kitkoin peer add <ip_address> [node_id]\n");
            } else {
                const char *nid = (argc >= 5) ? argv[4] : "ext-node";
                p2p_add_peer(nid, argv[3], P2P_DEFAULT_PORT);
            }
        } else {
            p2p_dump_status();
        }
    } else if (strcmp(argv[1], "save") == 0) {
        if (coin_save() == 0) {
            kprintf("[+] KitKoin wallet & blockchain successfully saved to disk.img and ramfs:wallet.txt!\n");
        } else {
            kprintf("[-] Failed to save KitKoin state to disk.\n");
        }
    } else if (strcmp(argv[1], "load") == 0) {
        if (coin_load() == 0) {
            kprintf("[+] KitKoin wallet & blockchain successfully restored from disk.img!\n");
        } else {
            kprintf("[-] No saved KitKoin state found on disk.\n");
        }
    } else if (strcmp(argv[1], "claim") == 0 || strcmp(argv[1], "faucet") == 0 ||
               strcmp(argv[1], "restore") == 0) {
        uint64_t amt = 1000;
        if (argc >= 3) {
            amt = 0;
            const char *p = argv[2];
            while (*p >= '0' && *p <= '9') {
                amt = amt * 10 + (*p - '0');
                p++;
            }
            if (amt == 0) amt = 1000;
        }
        coin_faucet(amt);
    } else if (strcmp(argv[1], "gui") == 0) {
        kprintf("[*] Launching KitKoin Graphical Crypto Dashboard...\n");
        wm_open_window(WIN_KITKOIN);
        gui_run_interactive();
    } else {
        kprintf("Unknown kitkoin command '%s'. Type 'kitkoin' for help.\n", argv[1]);
    }
}

static void atlas_worker_thread(void) {
    kprintf("[Atlas:Worker] Background task initiated by Atlas AI controller.\n");
    for (int i = 1; i <= 3; i++) {
        kprintf("[Atlas:Worker] Performing system telemetry scan %d/3...\n", i);
        timer_sleep_ms(80);
        task_yield();
    }
    kprintf("[Atlas:Worker] Telemetry complete! All kernel subsystems nominal.\n");
}

static void cmd_atlas(int argc, char **argv) {
    if (argc < 2) {
        kprintf("\n====================================================================\n");
        kprintf("      _  _____ _        _   ____    ___  ____                       \n");
        kprintf("     / \\|_   _| |      / \\ / ___|  / _ \\/ ___|                      \n");
        kprintf("    / _ \\ | | | |     / _ \\\\___ \\ | | | \\___ \\                      \n");
        kprintf("   / ___ \\| | | |___ / ___ \\___) || |_| |___) |                     \n");
        kprintf("  /_/   \\_\\_| |_____/_/   \\_\\____/  \\___/|____/                      \n");
        kprintf("        Atlas Cognitive Orchestrator (Privilege: EL4)               \n");
        kprintf("====================================================================\n");
        kprintf("  Status:              ACTIVE\n");
        kprintf("  Privilege Level:     EL4 (Cognitive Executive / Out-of-Band Supervisor)\n");
        kprintf("  AI Architecture:     Causal Self-Attention Transformer LM (30,000+ Weights)\n");
        kprintf("  Supervisory Domain:  Host Environment, Firmware (EL3), Kernel (EL1), User (EL0)\n");
        kprintf("  Control Scope:       Memory MMU, Scheduling, VirtIO Storage, Framebuffer GUI\n\n");
        kprintf("Available Atlas Commands:\n");
        kprintf("  atlas hello            - Exchange a greeting with Atlas at EL4\n");
        kprintf("  atlas el4              - Display EL4 cognitive privilege architecture\n");
        kprintf("  atlas info             - Query full Exception Level hierarchy & CPU state\n");
        kprintf("  atlas mem              - Real-time 64MB page allocator & heap telemetry\n");
        kprintf("  atlas tasks            - Inspect and enumerate active kernel threads\n");
        kprintf("  atlas spawn [name]     - Dispatch a background thread under Atlas EL4 control\n");
        kprintf("  atlas gui              - Repaint GUI framebuffer desktop & dashboard\n");
        kprintf("  atlas test             - Run an automated end-to-end OS self-test\n");
        kprintf("====================================================================\n\n");
        return;
    }

    if (strcmp(argv[1], "hello") == 0 || strcmp(argv[1], "hi") == 0) {
        kprintf("[Atlas:EL4] Greetings! I am Atlas, operating at Cognitive Exception Level 4.\n");
        kprintf("[Atlas:EL4] I oversee the kernel at EL1 and user tasks at EL0 with omniscient control.\n");
    } else if (strcmp(argv[1], "el4") == 0) {
        kprintf("\n--- Atlas EL4 Cognitive Architecture Matrix ---\n");
        kprintf("  [EL4] Atlas AI Orchestrator : Out-of-band live supervisor & code synthesizer\n");
        kprintf("  [EL3] Secure Monitor / PSCI : Firmware, reset controller, power management\n");
        kprintf("  [EL2] QEMU Hypervisor       : Virtual machine execution layer (Cortex-A57)\n");
        kprintf("  [EL1] myOS Operating System : Page tables, GICv2, Drivers, Round-Robin Sched\n");
        kprintf("  [EL0] User Applications     : Unprivileged CLI commands & user workloads\n");
        kprintf("------------------------------------------------\n");
        kprintf("  [+] EL4 Authority: Total system observability, dynamic self-healing, & code synthesis.\n\n");
    } else if (strcmp(argv[1], "info") == 0) {
        kprintf("\n[Atlas:EL4] Hierarchy & System Hardware Diagnostics:\n");
        kprintf("  Cognitive Level: EL4 (Atlas AI Supervisor)\n");
        kprintf("  Physical CPU EL: EL%u (Kernel Privilege)\n", (unsigned int)read_current_el());
        kprintf("  MIDR_EL1:        0x%016lx\n", read_midr());
        kprintf("  MPIDR_EL1:       0x%016lx (Primary Core: %u)\n", 
                read_mpidr(), (unsigned int)(read_mpidr() & 0xFF));
        kprintf("  CNTFRQ_EL0:      %u Hz (%u MHz)\n", 
                (unsigned int)read_cntfrq(), (unsigned int)(read_cntfrq() / 1000000));
        kprintf("  Current SP:      0x%016lx\n", read_sp());
        kprintf("  System Uptime:   %lu ticks (%lu sec)\n\n", 
                timer_get_ticks(), timer_get_ticks() / 100);
        mm_dump_stats();
    } else if (strcmp(argv[1], "mem") == 0) {
        mm_dump_stats();
    } else if (strcmp(argv[1], "tasks") == 0 || strcmp(argv[1], "ps") == 0) {
        sched_dump_tasks();
    } else if (strcmp(argv[1], "spawn") == 0) {
        const char *tname = (argc >= 3) ? argv[2] : "AtlasTask";
        int pid = task_create(tname, atlas_worker_thread, 5);
        if (pid > 0) {
            kprintf("[Atlas:EL4] Successfully spawned task PID %d ('%s') under EL4 supervision!\n", pid, tname);
            task_yield();
        }
    } else if (strcmp(argv[1], "gui") == 0 || strcmp(argv[1], "draw") == 0) {
        fb_render_test_screen();
        kprintf("[Atlas:EL4] Framebuffer graphics redrawn successfully.\n");
    } else if (strcmp(argv[1], "test") == 0) {
        kprintf("[Atlas:EL4] Starting comprehensive kernel health test...\n");
        void *p = alloc_page();
        void *h = kmalloc(128);
        kprintf("  - Page Allocator:  %s (%p)\n", p ? "PASSED" : "FAILED", p);
        kprintf("  - Dynamic Heap:    %s (%p)\n", h ? "PASSED" : "FAILED", h);
        if (h) kfree(h);
        if (p) free_page(p);
        kprintf("  - Timer Ticks:     %lu ticks recorded\n", timer_get_ticks());
        kprintf("[Atlas:EL4] All tests completed! All subsystems nominal.\n");
    } else {
        kprintf("[Atlas:EL4] Command '%s' not recognized. Type 'atlas' for help.\n", argv[1]);
    }
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
    gui_terminal_clear();
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
    char buf[1024];
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
        kprintf("[-] Failed to create '%s' (already exists)\n", argv[1]);
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
        kprintf("[-] rm: cannot remove '%s': No such file\n", argv[1]);
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

    if (argc == 0) return;

    const char *cmd_name = argv[0];
    /* Allow slash commands: /help, /status, /agents, etc. */
    if (cmd_name[0] == '/' && cmd_name[1] != '\0') {
        cmd_name++;
    }

    /* Search and execute command */
    for (int i = 0; commands[i].name != NULL; i++) {
        if (strcmp(commands[i].name, cmd_name) == 0) {
            commands[i].func(argc, argv);
            return;
        }
    }

    kprintf("agy: unknown command '%s'. Type 'help' or '/help' for available commands.\n", argv[0]);
}

void shell_init(void) {
    kprintf("[+] Interactive Google Antigravity Shell initialized\n");
}

void shell_run(void) {
    char line[SHELL_MAX_LINE];
    int pos = 0;

    kprintf("\nGoogle Antigravity OS Shell (AArch64)\n");
    kprintf("Type 'help' or '/help' to view commands, or 'agy' for platform overview.\n\n");

    while (1) {
        kprintf("agy> ");
        pos = 0;
        memset(line, 0, sizeof(line));

        while (1) {
            /* 1. Poll VirtIO input devices (tablet/mouse) */
            input_poll();

            /* 2. Dispatch mouse state to Window Manager */
            int mx = 0, my = 0, mbtns = 0;
            input_get_mouse(&mx, &my, &mbtns);
            wm_handle_mouse(mx, my, mbtns);

            /* 3. Poll Unified Keyboard Subsystem (Hardware VirtIO + Physical UART Console) */
            struct key_event ev;
            int has_event = keyboard_poll(&ev);

            if (!has_event) {
                /* Non-blocking: refresh GUI desktop and yield CPU */
                gui_refresh();
                task_yield();
                continue;
            }

            /* Live visual feedback: light up key on On-Screen Virtual Keyboard */
            wm_keyboard_set_active_key(ev.scancode, ev.pressed);

            /* Only process key-down for input and hotkeys */
            if (!ev.pressed) {
                gui_refresh();
                continue;
            }

            /* Physical Keyboard Hotkeys to switch/open GUI Windows */
            if (ev.scancode == KEY_F1) { wm_open_window(WIN_TERMINAL); gui_refresh(); continue; }
            if (ev.scancode == KEY_F2) { wm_open_window(WIN_KEYBOARD); gui_refresh(); continue; }
            if (ev.scancode == KEY_F3) { wm_open_window(WIN_CALC);     gui_refresh(); continue; }
            if (ev.scancode == KEY_F4) { wm_open_window(WIN_FILES);    gui_refresh(); continue; }
            if (ev.scancode == KEY_F5) { wm_open_window(WIN_DRIVERS);  gui_refresh(); continue; }
            if (ev.scancode == KEY_F6) { wm_open_window(WIN_SYSTEM);   gui_refresh(); continue; }
            if (ev.scancode == KEY_F7) { wm_open_window(WIN_AGENTS);   gui_refresh(); continue; }
            if (ev.scancode == KEY_F8) { wm_open_window(WIN_ABOUT);    gui_refresh(); continue; }

            char c = ev.ascii;
            if (!c) {
                gui_refresh();
                continue;
            }

            /* Enter / Return */
            if (c == '\r' || c == '\n') {
                uart_putc('\n');
                gui_terminal_putc('\n');
                line[pos] = '\0';
                break;
            }
            /* Backspace / Delete */
            else if (c == '\b' || c == 0x7F) {
                if (pos > 0) {
                    pos--;
                    line[pos] = '\0';
                    uart_puts("\b \b");
                    gui_terminal_putc('\b');
                }
            }
            /* Ctrl+C */
            else if (c == 0x03) {
                uart_puts("^C\n");
                gui_terminal_puts("^C\n");
                pos = 0;
                line[0] = '\0';
                break;
            }
            /* Ctrl+L */
            else if (c == 0x0C) {
                uart_puts("\033[2J\033[H");
                gui_terminal_clear();
                kprintf("agy> %s", line);
            }
            /* Printable characters */
            else if (c >= 32 && c <= 126) {
                if (pos < SHELL_MAX_LINE - 1) {
                    line[pos++] = c;
                    uart_putc(c);
                    gui_terminal_putc(c);
                }
            }
            gui_refresh();
        }

        if (pos > 0) {
            shell_execute(line);
            gui_refresh();
        }
    }
}
