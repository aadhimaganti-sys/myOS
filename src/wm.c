#include "wm.h"
#include "fb.h"
#include "gui.h"
#include "agy.h"
#include "ramfs.h"
#include "mm.h"
#include "sched.h"
#include "timer.h"
#include "kernel.h"
#include "irq.h"
#include "printf.h"
#include "string.h"

static struct wm_window windows[WM_MAX_WINDOWS];
static int start_menu_open = 0;
static int mouse_curr_x = 320;
static int mouse_curr_y = 240;

/* Window render helpers */
static void render_terminal_window(struct wm_window *w);
static void render_agents_window(struct wm_window *w);
static void render_files_window(struct wm_window *w);
static void render_system_window(struct wm_window *w);
static void render_about_window(struct wm_window *w);

void wm_init(void) {
    /* 1. Terminal Window */
    windows[WIN_TERMINAL].id = WIN_TERMINAL;
    windows[WIN_TERMINAL].title = "Terminal Console [Interactive]";
    windows[WIN_TERMINAL].x = 20;
    windows[WIN_TERMINAL].y = 40;
    windows[WIN_TERMINAL].w = 590;
    windows[WIN_TERMINAL].h = 240;
    windows[WIN_TERMINAL].is_visible = 1;
    windows[WIN_TERMINAL].is_focused = 1;
    windows[WIN_TERMINAL].render = render_terminal_window;

    /* 2. Agents Dashboard */
    windows[WIN_AGENTS].id = WIN_AGENTS;
    windows[WIN_AGENTS].title = "Antigravity Agent Orchestrator";
    windows[WIN_AGENTS].x = 20;
    windows[WIN_AGENTS].y = 290;
    windows[WIN_AGENTS].w = 290;
    windows[WIN_AGENTS].h = 150;
    windows[WIN_AGENTS].is_visible = 1;
    windows[WIN_AGENTS].is_focused = 0;
    windows[WIN_AGENTS].render = render_agents_window;

    /* 3. System Monitor */
    windows[WIN_SYSTEM].id = WIN_SYSTEM;
    windows[WIN_SYSTEM].title = "System & Hardware Monitor";
    windows[WIN_SYSTEM].x = 320;
    windows[WIN_SYSTEM].y = 290;
    windows[WIN_SYSTEM].w = 290;
    windows[WIN_SYSTEM].h = 150;
    windows[WIN_SYSTEM].is_visible = 1;
    windows[WIN_SYSTEM].is_focused = 0;
    windows[WIN_SYSTEM].render = render_system_window;

    /* 4. Files Explorer (Initially Hidden) */
    windows[WIN_FILES].id = WIN_FILES;
    windows[WIN_FILES].title = "RAMFS File Explorer";
    windows[WIN_FILES].x = 60;
    windows[WIN_FILES].y = 80;
    windows[WIN_FILES].w = 340;
    windows[WIN_FILES].h = 200;
    windows[WIN_FILES].is_visible = 0;
    windows[WIN_FILES].is_focused = 0;
    windows[WIN_FILES].render = render_files_window;

    /* 5. About Dialog (Initially Hidden) */
    windows[WIN_ABOUT].id = WIN_ABOUT;
    windows[WIN_ABOUT].title = "About myOS";
    windows[WIN_ABOUT].x = 120;
    windows[WIN_ABOUT].y = 100;
    windows[WIN_ABOUT].w = 360;
    windows[WIN_ABOUT].h = 180;
    windows[WIN_ABOUT].is_visible = 0;
    windows[WIN_ABOUT].is_focused = 0;
    windows[WIN_ABOUT].render = render_about_window;

    kprintf("[+] Window Manager initialized (%d windows configured)\n", WM_MAX_WINDOWS);
}

struct wm_window *wm_get_window(int id) {
    if (id >= 0 && id < WM_MAX_WINDOWS) {
        return &windows[id];
    }
    return NULL;
}

static int wm_dirty = 1;

int wm_is_dirty(void) {
    int d = wm_dirty;
    wm_dirty = 0;
    return d;
}

void wm_set_dirty(void) {
    wm_dirty = 1;
}

void wm_open_window(int id) {
    if (id >= 0 && id < WM_MAX_WINDOWS) {
        windows[id].is_visible = 1;
        wm_focus_window(id);
        wm_set_dirty();
    }
}

void wm_close_window(int id) {
    if (id >= 0 && id < WM_MAX_WINDOWS) {
        windows[id].is_visible = 0;
        windows[id].is_focused = 0;
        wm_set_dirty();
    }
}

void wm_focus_window(int id) {
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        windows[i].is_focused = (i == id);
    }
    wm_set_dirty();
}

int wm_is_start_menu_open(void) {
    return start_menu_open;
}

void wm_toggle_start_menu(void) {
    start_menu_open = !start_menu_open;
    wm_set_dirty();
}

int wm_get_mouse_x(void) {
    return mouse_curr_x;
}

int wm_get_mouse_y(void) {
    return mouse_curr_y;
}

static int last_mouse_buttons = 0;
static int active_drag_window = -1;
static int drag_start_x = 0;
static int drag_start_y = 0;
static int win_orig_x = 0;
static int win_orig_y = 0;

void wm_handle_mouse(int x, int y, int buttons) {
    if (x != mouse_curr_x || y != mouse_curr_y || buttons != last_mouse_buttons || active_drag_window >= 0) {
        wm_set_dirty();
    }
    mouse_curr_x = x;
    mouse_curr_y = y;

    int left_pressed = (buttons & 1);
    int left_just_pressed = (left_pressed && !(last_mouse_buttons & 1));
    int left_just_released = (!left_pressed && (last_mouse_buttons & 1));

    if (left_just_pressed) {
        /* 1. Check Taskbar button clicks (y >= 456) */
        if (y >= 456) {
            if (x >= 10 && x <= 95) {
                wm_open_window(WIN_TERMINAL);
            } else if (x >= 105 && x <= 180) {
                wm_open_window(WIN_AGENTS);
            } else if (x >= 190 && x <= 265) {
                wm_open_window(WIN_SYSTEM);
            } else if (x >= 275 && x <= 350) {
                wm_open_window(WIN_FILES);
            } else if (x >= 360 && x <= 435) {
                wm_open_window(WIN_ABOUT);
            }
            last_mouse_buttons = buttons;
            return;
        }

        /* 2. Check Window clicks (focused window first, then other visible windows) */
        int hit_window = -1;
        for (int i = 0; i < WM_MAX_WINDOWS; i++) {
            if (windows[i].is_visible && windows[i].is_focused) {
                if (x >= windows[i].x && x <= windows[i].x + windows[i].w &&
                    y >= windows[i].y && y <= windows[i].y + windows[i].h) {
                    hit_window = i;
                    break;
                }
            }
        }
        if (hit_window < 0) {
            for (int i = WM_MAX_WINDOWS - 1; i >= 0; i--) {
                if (windows[i].is_visible) {
                    if (x >= windows[i].x && x <= windows[i].x + windows[i].w &&
                        y >= windows[i].y && y <= windows[i].y + windows[i].h) {
                        hit_window = i;
                        break;
                    }
                }
            }
        }

        if (hit_window >= 0) {
            struct wm_window *w = &windows[hit_window];
            wm_focus_window(hit_window);

            /* Check close button dot: x + 8 to x + 16, y + 7 to y + 15 */
            if (x >= w->x + 8 && x <= w->x + 16 && y >= w->y + 7 && y <= w->y + 15) {
                wm_close_window(hit_window);
            }
            /* Check title bar for dragging: y <= w->y + 24 */
            else if (y <= w->y + 24) {
                active_drag_window = hit_window;
                drag_start_x = x;
                drag_start_y = y;
                win_orig_x = w->x;
                win_orig_y = w->y;
            }
        }
    } else if (left_pressed && active_drag_window >= 0) {
        /* Dragging active window */
        struct wm_window *w = &windows[active_drag_window];
        int new_x = win_orig_x + (x - drag_start_x);
        int new_y = win_orig_y + (y - drag_start_y);

        if (new_x < 0) new_x = 0;
        if (new_x + w->w > 640) new_x = 640 - w->w;
        if (new_y < 24) new_y = 24;
        if (new_y + w->h > 456) new_y = 456 - w->h;

        w->x = new_x;
        w->y = new_y;
    }

    if (left_just_released) {
        active_drag_window = -1;
    }

    last_mouse_buttons = buttons;
}

void wm_handle_key(char c) {
    /* Keyboard Shortcuts */
    if (c == 27) { /* ESC */
        start_menu_open = 0;
        return;
    }
    if (c == '\t') { /* Tab: Cycle window focus */
        for (int i = 0; i < WM_MAX_WINDOWS; i++) {
            if (windows[i].is_focused) {
                int next = (i + 1) % WM_MAX_WINDOWS;
                while (!windows[next].is_visible && next != i) {
                    next = (next + 1) % WM_MAX_WINDOWS;
                }
                wm_focus_window(next);
                return;
            }
        }
        wm_focus_window(WIN_TERMINAL);
        return;
    }

    /* Route keystroke to focused window */
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (windows[i].is_focused && windows[i].is_visible) {
            if (i == WIN_TERMINAL) {
                gui_terminal_putc(c);
            }
            return;
        }
    }

    /* Default fallback: feed terminal */
    gui_terminal_putc(c);
}

/* -------------------------------------------------------------------------
 * Window Rendering Routines
 * ------------------------------------------------------------------------- */
static void draw_window_frame(struct wm_window *w) {
    uint32_t header_bg = w->is_focused ? COLOR_PANEL_HEADER : COLOR_DARK_GRAY;
    uint32_t border_col = w->is_focused ? COLOR_GOOGLE_BLUE : COLOR_PANEL_BORDER;

    /* Window border and body */
    fb_fill_rect(w->x, w->y, w->w, w->h, border_col);
    fb_fill_rect(w->x + 1, w->y + 1, w->w - 2, w->h - 2, COLOR_PANEL_BG);

    /* Title bar */
    fb_fill_rect(w->x + 1, w->y + 1, w->w - 2, 22, header_bg);
    fb_draw_line(w->x + 1, w->y + 23, w->x + w->w - 2, w->y + 23, border_col);

    /* Window Control Dots */
    fb_fill_rect(w->x + 8,  w->y + 7, 8, 8, COLOR_GOOGLE_RED);
    fb_fill_rect(w->x + 20, w->y + 7, 8, 8, COLOR_GOOGLE_YELLOW);
    fb_fill_rect(w->x + 32, w->y + 7, 8, 8, COLOR_GOOGLE_GREEN);

    /* Title string */
    fb_draw_string(w->x + 48, w->y + 7, w->title, COLOR_TEXT_WHITE, header_bg);
}

static void render_terminal_window(struct wm_window *w) {
    draw_window_frame(w);

    /* Terminal background */
    int text_x = w->x + 8;
    int text_y = w->y + 28;
    int text_w = w->w - 16;
    int text_h = w->h - 36;
    fb_fill_rect(text_x, text_y, text_w, text_h, COLOR_CONSOLE_BG);

    /* Draw terminal text lines */
    int max_rows = text_h / 10;
    if (max_rows > GUI_TERM_ROWS) max_rows = GUI_TERM_ROWS;

    for (int r = 0; r < max_rows; r++) {
        const char *line = gui_terminal_get_line(r);
        if (line && *line) {
            fb_draw_string(text_x + 4, text_y + 4 + (r * 10), line, COLOR_TEXT_WHITE, COLOR_CONSOLE_BG);
        }
    }

    /* Draw text cursor */
    int cur_row, cur_col;
    gui_terminal_get_cursor(&cur_row, &cur_col);
    if (cur_row < max_rows && w->is_focused) {
        int cx = text_x + 4 + (cur_col * 8);
        int cy = text_y + 4 + (cur_row * 10);
        fb_fill_rect(cx, cy + 7, 8, 2, COLOR_GOOGLE_BLUE);
    }
}

static void render_agents_window(struct wm_window *w) {
    draw_window_frame(w);
    int x = w->x + 10;
    int y = w->y + 30;

    int count = 0;
    struct agy_agent *agents = agy_get_agents(&count);

    fb_draw_string(x, y, "ID  NAME           ROLE       STATE", COLOR_CYAN, COLOR_PANEL_BG);
    fb_draw_line(x, y + 10, w->x + w->w - 10, y + 10, COLOR_PANEL_BORDER);
    y += 14;

    for (int i = 0; i < count && i < 4; i++) {
        char buf[64];
        const char *st_str = "IDLE";
        uint32_t st_col = COLOR_GOOGLE_GREEN;
        if (agents[i].state == AGENT_RUNNING) {
            st_str = "RUN ";
            st_col = COLOR_GOOGLE_YELLOW;
        } else if (agents[i].state == AGENT_THINKING) {
            st_str = "THNK";
            st_col = COLOR_GOOGLE_BLUE;
        }

        snprintf(buf, sizeof(buf), "#%u  %-12s %-10s",
                 agents[i].id, agents[i].name, agents[i].role);
        fb_draw_string(x, y, buf, COLOR_TEXT_WHITE, COLOR_PANEL_BG);
        fb_draw_badge(x + 220, y - 2, st_str, COLOR_BLACK, st_col);
        y += 16;
    }
}

static void render_system_window(struct wm_window *w) {
    draw_window_frame(w);
    int x = w->x + 10;
    int y = w->y + 30;

    char buf[64];
    fb_draw_string(x, y, "Hardware & Memory Subsystem", COLOR_CYAN, COLOR_PANEL_BG);
    y += 14;

    snprintf(buf, sizeof(buf), "CPU: Cortex-A57 (EL%u) @ 62MHz", (unsigned int)read_current_el());
    fb_draw_string(x, y, buf, COLOR_TEXT_WHITE, COLOR_PANEL_BG);
    y += 14;

    snprintf(buf, sizeof(buf), "RAM: 64 MB Total (%lu KB Free)", mm_get_free_pages() * 4);
    fb_draw_string(x, y, buf, COLOR_TEXT_WHITE, COLOR_PANEL_BG);
    y += 14;

    snprintf(buf, sizeof(buf), "Uptime: %lu sec (Ticks: %lu)", timer_get_ticks() / 100, timer_get_ticks());
    fb_draw_string(x, y, buf, COLOR_TEXT_WHITE, COLOR_PANEL_BG);
    y += 14;

    fb_draw_badge(x, y + 2, "GICv2", COLOR_WHITE, COLOR_GOOGLE_RED);
    fb_draw_badge(x + 50, y + 2, "MMU 4-Level", COLOR_WHITE, COLOR_GOOGLE_BLUE);
    fb_draw_badge(x + 140, y + 2, "VirtIO", COLOR_WHITE, COLOR_GOOGLE_GREEN);
    fb_draw_badge(x + 200, y + 2, "KBD Active", COLOR_BLACK, COLOR_GOOGLE_YELLOW);
}

static void render_files_window(struct wm_window *w) {
    draw_window_frame(w);
    int x = w->x + 10;
    int y = w->y + 30;
    fb_draw_string(x, y, "File Explorer - In-Memory RAMFS", COLOR_CYAN, COLOR_PANEL_BG);
    y += 16;
    fb_draw_string(x, y, "[TXT] readme.txt    (413 bytes)", COLOR_TEXT_WHITE, COLOR_PANEL_BG);
    y += 14;
    fb_draw_string(x, y, "[TXT] sysinfo.txt   (311 bytes)", COLOR_TEXT_WHITE, COLOR_PANEL_BG);
}

static void render_about_window(struct wm_window *w) {
    draw_window_frame(w);
    int x = w->x + 10;
    int y = w->y + 30;
    fb_draw_string(x, y, "Google Antigravity Operating System", COLOR_GOOGLE_BLUE, COLOR_PANEL_BG);
    y += 16;
    fb_draw_string(x, y, "Version 1.0 (AArch64 Bare-Metal Edition)", COLOR_TEXT_WHITE, COLOR_PANEL_BG);
    y += 14;
    fb_draw_string(x, y, "Built targeting ARMv8-A QEMU virt machine.", COLOR_TEXT_MUTED, COLOR_PANEL_BG);
    y += 14;
    fb_draw_string(x, y, "Featuring 4-Level MMU, GICv2, VirtIO & GUI.", COLOR_TEXT_MUTED, COLOR_PANEL_BG);
}

void wm_render(void) {
    /* 1. Desktop Background */
    fb_clear(COLOR_DESKTOP_BG);

    /* 2. Top Taskbar */
    fb_fill_rect(0, 0, 640, 24, COLOR_TOPBAR_BG);
    fb_draw_line(0, 24, 640, 24, COLOR_PANEL_BORDER);

    fb_draw_badge(8, 4, "myOS", COLOR_WHITE, COLOR_GOOGLE_BLUE);
    fb_draw_string(60, 8, "Google Antigravity OS - Desktop", COLOR_TEXT_WHITE, COLOR_TOPBAR_BG);

    /* Top bar indicators */
    char top_stat[64];
    snprintf(top_stat, sizeof(top_stat), "RAM Free: %lu KB | Up: %lu s",
             mm_get_free_pages() * 4, timer_get_ticks() / 100);
    fb_draw_string(410, 8, top_stat, COLOR_TEXT_MUTED, COLOR_TOPBAR_BG);

    /* 3. Render Windows (Non-focused first, then focused on top) */
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (windows[i].is_visible && !windows[i].is_focused && windows[i].render) {
            windows[i].render(&windows[i]);
        }
    }
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (windows[i].is_visible && windows[i].is_focused && windows[i].render) {
            windows[i].render(&windows[i]);
        }
    }

    /* 4. Bottom Taskbar with Quick Launch and Keyboard Driver Overlay */
    fb_fill_rect(0, 456, 640, 24, COLOR_STATUSBAR_BG);
    fb_draw_line(0, 456, 640, 456, COLOR_PANEL_BORDER);

    /* Taskbar App Buttons */
    fb_draw_badge(10, 460, "[F1] Term", COLOR_WHITE, COLOR_PANEL_HEADER);
    fb_draw_badge(105, 460, "[F2] Agents", COLOR_WHITE, COLOR_PANEL_HEADER);
    fb_draw_badge(190, 460, "[F3] System", COLOR_WHITE, COLOR_PANEL_HEADER);
    fb_draw_badge(275, 460, "[F4] Files", COLOR_WHITE, COLOR_PANEL_HEADER);
    fb_draw_badge(360, 460, "[F5] About", COLOR_WHITE, COLOR_PANEL_HEADER);

    /* Real-Time Keyboard Driver Status Badge */
    fb_draw_badge(470, 460, "KBD: DRIVER LIVE", COLOR_BLACK, COLOR_GOOGLE_GREEN);

    /* 5. Render interactive mouse cursor */
    fb_draw_cursor(mouse_curr_x, mouse_curr_y);

    /* 6. Present rendered backbuffer to screen (Flicker-Free Flip) */
    fb_present();
}
