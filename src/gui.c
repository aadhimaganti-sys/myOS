#include "gui.h"
#include "wm.h"
#include "fb.h"
#include "agy.h"
#include "mm.h"
#include "irq.h"
#include "timer.h"
#include "sched.h"
#include "ramfs.h"
#include "printf.h"
#include "string.h"
#include "keyboard.h"
#include "input.h"
#include "shell.h"

/* State Management */
static int gui_initialized = 0;

/* Circular Text Buffer for Interactive Console Mirror */
static char term_lines[GUI_TERM_ROWS][GUI_TERM_COLS + 1];
static int term_cursor_row = 0;
static int term_cursor_col = 0;
static int term_dirty = 0;

/* Cache for periodic refresh */
static uint64_t last_uptime_sec = (uint64_t)-1;
static uint64_t last_free_ram   = (uint64_t)-1;
static uint64_t last_heap_used  = (uint64_t)-1;
static int      last_task_count = -1;

void gui_terminal_putc(char c) {
    if (c == '\r') {
        term_cursor_col = 0;
        term_dirty = 1;
        return;
    }

    if (c == '\n') {
        term_cursor_col = 0;
        if (term_cursor_row < GUI_TERM_ROWS - 1) {
            term_cursor_row++;
        } else {
            /* Scroll lines upward */
            for (int r = 0; r < GUI_TERM_ROWS - 1; r++) {
                memcpy(term_lines[r], term_lines[r + 1], GUI_TERM_COLS + 1);
            }
            memset(term_lines[GUI_TERM_ROWS - 1], 0, GUI_TERM_COLS + 1);
        }
        term_dirty = 1;
        return;
    }

    if (c == '\b' || c == 0x7F) {
        if (term_cursor_col > 0) {
            term_cursor_col--;
            term_lines[term_cursor_row][term_cursor_col] = ' ';
            term_dirty = 1;
        }
        return;
    }

    if (c == '\t') {
        int next_tab = (term_cursor_col + 4) & ~3;
        while (term_cursor_col < next_tab && term_cursor_col < GUI_TERM_COLS) {
            term_lines[term_cursor_row][term_cursor_col++] = ' ';
        }
        term_dirty = 1;
        return;
    }

    /* Printable ASCII character */
    if (c >= 32 && c <= 126) {
        if (term_cursor_col >= GUI_TERM_COLS) {
            /* Wrap to next row */
            term_cursor_col = 0;
            if (term_cursor_row < GUI_TERM_ROWS - 1) {
                term_cursor_row++;
            } else {
                for (int r = 0; r < GUI_TERM_ROWS - 1; r++) {
                    memcpy(term_lines[r], term_lines[r + 1], GUI_TERM_COLS + 1);
                }
                memset(term_lines[GUI_TERM_ROWS - 1], 0, GUI_TERM_COLS + 1);
            }
        }

        term_lines[term_cursor_row][term_cursor_col++] = c;
        term_lines[term_cursor_row][term_cursor_col] = '\0';
        term_dirty = 1;
    }
}

void gui_terminal_puts(const char *s) {
    if (!s) return;
    while (*s) {
        gui_terminal_putc(*s++);
    }
}

void gui_terminal_clear(void) {
    memset(term_lines, 0, sizeof(term_lines));
    term_cursor_row = 0;
    term_cursor_col = 0;
    term_dirty = 1;
}

const char *gui_terminal_get_line(int row) {
    if (row >= 0 && row < GUI_TERM_ROWS) {
        return term_lines[row];
    }
    return "";
}

void gui_terminal_get_cursor(int *row, int *col) {
    if (row) *row = term_cursor_row;
    if (col) *col = term_cursor_col;
}

void gui_init(void) {
    memset(term_lines, 0, sizeof(term_lines));
    term_cursor_row = 0;
    term_cursor_col = 0;
    term_dirty = 0;

    /* Initialize welcome banner into console mirror */
    gui_terminal_puts("Google Antigravity OS v1.0 [AArch64 EL1]\n");
    gui_terminal_puts("Desktop Window Manager & Multi-Agent Orchestrator Online.\n");
    gui_terminal_puts("Type 'help' for commands, or 'agents' for active subagents.\n");
    gui_terminal_puts("agy> ");

    /* Initialize Window Manager & GUI Desktop Environment */
    wm_init();

    gui_initialized = 1;
    kprintf("[+] Google Antigravity Desktop GUI & Terminal Mirror initialized\n");
}

void gui_render_desktop(void) {
    if (!gui_initialized) return;

    /* Delegate complete desktop and window rendering to Window Manager */
    wm_render();
    term_dirty = 0;
}

void gui_refresh(void) {
    if (!gui_initialized) return;

    uint64_t ticks = timer_get_ticks();
    uint64_t uptime_sec = ticks / TIMER_HZ;
    uint64_t free_ram = (mm_get_free_pages() * PAGE_SIZE) / (1024 * 1024);
    uint64_t heap_used = mm_get_heap_used_bytes();
    int tasks = sched_get_active_task_count();

    int needs_redraw = 0;

    /* Terminal console dirty (new characters typed or printed) */
    if (term_dirty) {
        term_dirty = 0;
        needs_redraw = 1;
    }

    /* Window Manager dirty (mouse moved, button clicked, window focused/dragged) */
    if (wm_is_dirty()) {
        needs_redraw = 1;
    }

    /* Periodic 1 Hz topbar hardware stats refresh */
    if (uptime_sec != last_uptime_sec || free_ram != last_free_ram ||
        heap_used != last_heap_used || tasks != last_task_count) {
        last_uptime_sec = uptime_sec;
        last_free_ram = free_ram;
        last_heap_used = heap_used;
        last_task_count = tasks;
        needs_redraw = 1;
    }

    if (needs_redraw) {
        wm_render();
    }
}

static char gui_line_buf[128];
static int gui_line_pos = 0;
static int gui_exit_requested = 0;

void wm_inject_char(char c) {
    keyboard_feed_char(c);
}

void gui_handle_key(struct key_event *ev) {
    if (!ev) return;

    /* Highlight key on Virtual On-Screen Keyboard */
    wm_keyboard_set_active_key(ev->scancode, ev->pressed);

    if (!ev->pressed) return;

    /* Hotkeys for Desktop GUI */
    if (ev->scancode == KEY_F1) {
        wm_open_window(WIN_TERMINAL);
        return;
    }
    if (ev->scancode == KEY_F2) {
        wm_open_window(WIN_KEYBOARD);
        return;
    }
    if (ev->scancode == KEY_F3) {
        wm_open_window(WIN_CALC);
        return;
    }
    if (ev->scancode == KEY_F4) {
        wm_open_window(WIN_FILES);
        return;
    }
    if (ev->scancode == KEY_F5) {
        wm_open_window(WIN_DRIVERS);
        return;
    }
    if (ev->scancode == KEY_F6) {
        wm_open_window(WIN_SYSTEM);
        return;
    }
    if (ev->scancode == KEY_F7) {
        wm_open_window(WIN_AGENTS);
        return;
    }
    if (ev->scancode == KEY_F8) {
        wm_open_window(WIN_ABOUT);
        return;
    }
    if (ev->scancode == KEY_TAB) {
        wm_handle_key('\t');
        return;
    }

/* Terminal Command History State */
static int gui_history_browse_idx = -1;
static char gui_saved_edit_line[SHELL_MAX_LINE];

    /* Terminal Window Input Handling */
    struct wm_window *w_term = wm_get_window(WIN_TERMINAL);
    if (w_term && w_term->is_focused && w_term->is_visible) {
        /* Command History: Up Arrow / Ctrl+P */
        if (ev->scancode == KEY_UP || ev->ascii == 0x10) {
            int hcount = shell_history_count();
            if (hcount > 0) {
                if (gui_history_browse_idx == -1) {
                    strncpy(gui_saved_edit_line, gui_line_buf, sizeof(gui_saved_edit_line) - 1);
                    gui_saved_edit_line[sizeof(gui_saved_edit_line) - 1] = '\0';
                    gui_history_browse_idx = hcount - 1;
                } else if (gui_history_browse_idx > 0) {
                    gui_history_browse_idx--;
                }
                while (gui_line_pos > 0) {
                    gui_line_pos--;
                    gui_terminal_putc('\b');
                }
                const char *h = shell_history_get(gui_history_browse_idx);
                if (h) {
                    gui_line_pos = 0;
                    while (h[gui_line_pos] && gui_line_pos < (int)sizeof(gui_line_buf) - 1) {
                        gui_line_buf[gui_line_pos] = h[gui_line_pos];
                        gui_terminal_putc(h[gui_line_pos]);
                        gui_line_pos++;
                    }
                    gui_line_buf[gui_line_pos] = '\0';
                }
            }
            return;
        }

        /* Command History: Down Arrow / Ctrl+N */
        if (ev->scancode == KEY_DOWN || ev->ascii == 0x0E) {
            int hcount = shell_history_count();
            if (gui_history_browse_idx != -1) {
                if (gui_history_browse_idx < hcount - 1) {
                    gui_history_browse_idx++;
                    while (gui_line_pos > 0) {
                        gui_line_pos--;
                        gui_terminal_putc('\b');
                    }
                    const char *h = shell_history_get(gui_history_browse_idx);
                    if (h) {
                        gui_line_pos = 0;
                        while (h[gui_line_pos] && gui_line_pos < (int)sizeof(gui_line_buf) - 1) {
                            gui_line_buf[gui_line_pos] = h[gui_line_pos];
                            gui_terminal_putc(h[gui_line_pos]);
                            gui_line_pos++;
                        }
                        gui_line_buf[gui_line_pos] = '\0';
                    }
                } else {
                    gui_history_browse_idx = -1;
                    while (gui_line_pos > 0) {
                        gui_line_pos--;
                        gui_terminal_putc('\b');
                    }
                    gui_line_pos = 0;
                    while (gui_saved_edit_line[gui_line_pos] && gui_line_pos < (int)sizeof(gui_line_buf) - 1) {
                        gui_line_buf[gui_line_pos] = gui_saved_edit_line[gui_line_pos];
                        gui_terminal_putc(gui_saved_edit_line[gui_line_pos]);
                        gui_line_pos++;
                    }
                    gui_line_buf[gui_line_pos] = '\0';
                }
            }
            return;
        }

        if (ev->scancode == KEY_ENTER || ev->ascii == '\r' || ev->ascii == '\n') {
            gui_terminal_putc('\n');
            gui_line_buf[gui_line_pos] = '\0';
            gui_history_browse_idx = -1;
            gui_saved_edit_line[0] = '\0';
            if (gui_line_pos > 0) {
                if (strcmp(gui_line_buf, "exit") == 0 || strcmp(gui_line_buf, "quit") == 0) {
                    gui_exit_requested = 1;
                    return;
                }
                int exp = shell_expand_history(gui_line_buf, sizeof(gui_line_buf));
                if (exp >= 0) {
                    shell_history_add(gui_line_buf);
                    shell_execute(gui_line_buf);
                }
            }
            gui_line_pos = 0;
            gui_line_buf[0] = '\0';
            gui_terminal_puts("agy> ");
            return;
        }

        if (ev->scancode == KEY_BACKSPACE || ev->ascii == '\b' || ev->ascii == 0x7F) {
            if (gui_line_pos > 0) {
                gui_line_pos--;
                gui_line_buf[gui_line_pos] = '\0';
                gui_terminal_putc('\b');
            }
            return;
        }

        if (ev->ascii >= 32 && ev->ascii <= 126) {
            if (gui_line_pos < (int)sizeof(gui_line_buf) - 1) {
                gui_line_buf[gui_line_pos++] = ev->ascii;
                gui_line_buf[gui_line_pos] = '\0';
                gui_terminal_putc(ev->ascii);
            }
            return;
        }
    } else {
        /* Other focused window */
        if (ev->ascii) {
            wm_handle_key(ev->ascii);
        }
    }
}

void gui_run_interactive(void) {
    gui_exit_requested = 0;
    gui_line_pos = 0;
    memset(gui_line_buf, 0, sizeof(gui_line_buf));

    gui_render_desktop();
    kprintf("[+] Switched to Graphical Desktop Mode (Press ESC or type 'exit' to return)\n");

    while (!gui_exit_requested) {
        /* 1. Poll VirtIO Mouse & pass to Window Manager */
        int mx = 0, my = 0, mbtns = 0;
        input_get_mouse(&mx, &my, &mbtns);
        wm_handle_mouse(mx, my, mbtns);

        /* 2. Poll Unified Keyboard */
        struct key_event ev;
        if (keyboard_poll(&ev)) {
            if (ev.pressed && ev.scancode == KEY_ESC) {
                kprintf("[+] ESC detected: Returning to CLI shell.\n");
                break;
            }
            gui_handle_key(&ev);
        }

        gui_refresh();
        task_yield();
    }

    kprintf("[+] Exited Graphical Desktop mode, returned to CLI shell.\n");
}
