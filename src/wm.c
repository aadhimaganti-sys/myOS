#include "wm.h"
#include "fb.h"
#include "gui.h"
#include "keyboard.h"
#include "input.h"
#include "agy.h"
#include "ramfs.h"
#include "mm.h"
#include "sched.h"
#include "timer.h"
#include "kernel.h"
#include "irq.h"
#include "drivers.h"
#include "printf.h"
#include "string.h"
#include "shell.h"
#include "coin.h"
#include "p2p.h"
#include "sha256.h"

static struct wm_window windows[WM_MAX_WINDOWS];
static int start_menu_open = 0;
static int mouse_curr_x = 320;
static int mouse_curr_y = 240;

/* Window render helpers */
static void render_terminal_window(struct wm_window *w);
static void render_keyboard_window(struct wm_window *w);
static void render_calc_window(struct wm_window *w);
static void render_files_window(struct wm_window *w);
static void render_drivers_window(struct wm_window *w);
static void render_system_window(struct wm_window *w);
static void render_agents_window(struct wm_window *w);
static void render_about_window(struct wm_window *w);
static void render_kitkoin_window(struct wm_window *w);

/* Driver inspection state */
static int selected_driver_idx = 0;
static int tested_driver_idx = -1;

/* -------------------------------------------------------------------------
 * On-Screen Virtual Keyboard (OSK) Configuration & State
 * ------------------------------------------------------------------------- */
struct osk_key {
    const char *label;
    const char *shift_label;
    uint16_t scancode;
    char normal_char;
    char shift_char;
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
};

static uint16_t osk_active_scancode = 0;
static int osk_key_pressed = 0;
static int osk_caps_lock = 0;
static int osk_shift_down = 0;

static const struct osk_key osk_keys[] = {
    /* Row 0 (y = 0, h = 24) */
    { "Esc",  "Esc", KEY_ESC,        27,   27,   0,   0,  26, 24 },
    { "1",    "!",   KEY_1,          '1',  '!',  29,  0,  25, 24 },
    { "2",    "@",   KEY_2,          '2',  '@',  57,  0,  25, 24 },
    { "3",    "#",   KEY_3,          '3',  '#',  85,  0,  25, 24 },
    { "4",    "$",   KEY_4,          '4',  '$',  113, 0,  25, 24 },
    { "5",    "%",   KEY_5,          '5',  '%',  141, 0,  25, 24 },
    { "6",    "^",   KEY_6,          '6',  '^',  169, 0,  25, 24 },
    { "7",    "&",   KEY_7,          '7',  '&',  197, 0,  25, 24 },
    { "8",    "*",   KEY_8,          '8',  '*',  225, 0,  25, 24 },
    { "9",    "(",   KEY_9,          '9',  '(',  253, 0,  25, 24 },
    { "0",    ")",   KEY_0,          '0',  ')',  281, 0,  25, 24 },
    { "-",    "_",   KEY_MINUS,      '-',  '_',  309, 0,  25, 24 },
    { "=",    "+",   KEY_EQUAL,      '=',  '+',  337, 0,  25, 24 },
    { "Bksp", "Bksp",KEY_BACKSPACE,  '\b', '\b', 365, 0,  68, 24 },

    /* Row 1 (y = 28, h = 24) */
    { "Tab",  "Tab", KEY_TAB,        '\t', '\t', 0,   28, 40, 24 },
    { "q",    "Q",   KEY_Q,          'q',  'Q',  43,  28, 26, 24 },
    { "w",    "W",   KEY_W,          'w',  'W',  72,  28, 26, 24 },
    { "e",    "E",   KEY_E,          'e',  'E',  101, 28, 26, 24 },
    { "r",    "R",   KEY_R,          'r',  'R',  130, 28, 26, 24 },
    { "t",    "T",   KEY_T,          't',  'T',  159, 28, 26, 24 },
    { "y",    "Y",   KEY_Y,          'y',  'Y',  188, 28, 26, 24 },
    { "u",    "U",   KEY_U,          'u',  'U',  217, 28, 26, 24 },
    { "i",    "I",   KEY_I,          'i',  'I',  246, 28, 26, 24 },
    { "o",    "O",   KEY_O,          'o',  'O',  275, 28, 26, 24 },
    { "p",    "P",   KEY_P,          'p',  'P',  304, 28, 26, 24 },
    { "[",    "{",   KEY_LEFTBRACE,  '[',  '{',  333, 28, 26, 24 },
    { "]",    "}",   KEY_RIGHTBRACE, ']',  '}',  362, 28, 26, 24 },
    { "\\",   "|",   KEY_BACKSLASH,  '\\', '|',  391, 28, 42, 24 },

    /* Row 2 (y = 56, h = 24) */
    { "Caps", "Caps",KEY_CAPSLOCK,   0,    0,    0,   56, 50, 24 },
    { "a",    "A",   KEY_A,          'a',  'A',  53,  56, 26, 24 },
    { "s",    "S",   KEY_S,          's',  'S',  82,  56, 26, 24 },
    { "d",    "D",   KEY_D,          'd',  'D',  111, 56, 26, 24 },
    { "f",    "F",   KEY_F,          'f',  'F',  140, 56, 26, 24 },
    { "g",    "G",   KEY_G,          'g',  'G',  169, 56, 26, 24 },
    { "h",    "H",   KEY_H,          'h',  'H',  198, 56, 26, 24 },
    { "j",    "J",   KEY_J,          'j',  'J',  227, 56, 26, 24 },
    { "k",    "K",   KEY_K,          'k',  'K',  256, 56, 26, 24 },
    { "l",    "L",   KEY_L,          'l',  'L',  285, 56, 26, 24 },
    { ";",    ":",   KEY_SEMICOLON,  ';',  ':',  314, 56, 26, 24 },
    { "'",    "\"",  KEY_APOSTROPHE, '\'', '"',  343, 56, 26, 24 },
    { "Enter","Enter",KEY_ENTER,     '\n', '\n', 372, 56, 61, 24 },

    /* Row 3 (y = 84, h = 24) */
    { "Shift","Shift",KEY_LEFTSHIFT, 0,    0,    0,   84, 60, 24 },
    { "z",    "Z",   KEY_Z,          'z',  'Z',  63,  84, 26, 24 },
    { "x",    "X",   KEY_X,          'x',  'X',  92,  84, 26, 24 },
    { "c",    "C",   KEY_C,          'c',  'C',  121, 84, 26, 24 },
    { "v",    "V",   KEY_V,          'v',  'V',  150, 84, 26, 24 },
    { "b",    "B",   KEY_B,          'b',  'B',  179, 84, 26, 24 },
    { "n",    "N",   KEY_N,          'n',  'N',  208, 84, 26, 24 },
    { "m",    "M",   KEY_M,          'm',  'M',  237, 84, 26, 24 },
    { ",",    "<",   KEY_COMMA,      ',',  '<',  266, 84, 26, 24 },
    { ".",    ">",   KEY_DOT,        '.',  '>',  295, 84, 26, 24 },
    { "/",    "?",   KEY_SLASH,      '/',  '?',  324, 84, 26, 24 },
    { "^",    "^",   KEY_UP,         0,    0,    353, 84, 28, 24 },
    { "Shift","Shift",KEY_RIGHTSHIFT,0,    0,    384, 84, 49, 24 },

    /* Row 4 (y = 112, h = 24) */
    { "Ctrl", "Ctrl",KEY_LEFTCTRL,   0,    0,    0,   112, 42, 24 },
    { "Alt",  "Alt", KEY_LEFTALT,    0,    0,    45,  112, 38, 24 },
    { "Space","Space",KEY_SPACE,     ' ',  ' ',  86,  112, 180,24 },
    { "<",    "<",   KEY_LEFT,       0,    0,    269, 112, 28, 24 },
    { "v",    "v",   KEY_DOWN,       0,    0,    300, 112, 28, 24 },
    { ">",    ">",   KEY_RIGHT,      0,    0,    331, 112, 28, 24 },
    { "Del",  "Del", KEY_DELETE,     0x7F, 0x7F, 362, 112, 71, 24 }
};
#define OSK_TOTAL_KEYS (int)(sizeof(osk_keys) / sizeof(osk_keys[0]))

/* -------------------------------------------------------------------------
 * Calculator Application State & Logic
 * ------------------------------------------------------------------------- */
static char calc_display[32] = "0";
static int64_t calc_prev_val = 0;
static char calc_op = 0;
static int calc_new_num = 1;

struct calc_btn {
    const char *label;
    int16_t col;
    int16_t row;
    uint32_t bg_col;
};

static const struct calc_btn calc_buttons[] = {
    { "C",   0, 0, 0x00662222 }, { "(",  1, 0, 0x002B3A5A }, { ")",  2, 0, 0x002B3A5A }, { "/", 3, 0, 0x002B3A5A },
    { "7",   0, 1, 0x00242838 }, { "8",  1, 1, 0x00242838 }, { "9",  2, 1, 0x00242838 }, { "*", 3, 1, 0x002B3A5A },
    { "4",   0, 2, 0x00242838 }, { "5",  1, 2, 0x00242838 }, { "6",  2, 2, 0x00242838 }, { "-", 3, 2, 0x002B3A5A },
    { "1",   0, 3, 0x00242838 }, { "2",  1, 3, 0x00242838 }, { "3",  2, 3, 0x00242838 }, { "+", 3, 3, 0x002B3A5A },
    { "0",   0, 4, 0x00242838 }, { "00", 1, 4, 0x00242838 }, { "DEL",2, 4, 0x004A3A28 }, { "=", 3, 4, COLOR_GOOGLE_BLUE }
};
#define CALC_TOTAL_BTNS (int)(sizeof(calc_buttons) / sizeof(calc_buttons[0]))

static int64_t calc_parse_display(void) {
    int64_t val = 0;
    int sign = 1;
    const char *p = calc_display;
    if (*p == '-') {
        sign = -1;
        p++;
    }
    while (*p >= '0' && *p <= '9') {
        val = val * 10 + (*p - '0');
        p++;
    }
    return val * sign;
}

static void calc_press(const char *label) {
    if (strcmp(label, "C") == 0) {
        strcpy(calc_display, "0");
        calc_prev_val = 0;
        calc_op = 0;
        calc_new_num = 1;
    } else if (strcmp(label, "DEL") == 0) {
        int len = strlen(calc_display);
        if (len > 1) {
            calc_display[len - 1] = '\0';
        } else {
            strcpy(calc_display, "0");
            calc_new_num = 1;
        }
    } else if (label[0] >= '0' && label[0] <= '9') {
        if (calc_new_num || strcmp(calc_display, "0") == 0) {
            strcpy(calc_display, label);
            calc_new_num = 0;
        } else {
            if (strlen(calc_display) + strlen(label) < sizeof(calc_display) - 1) {
                strcat(calc_display, label);
            }
        }
    } else if (label[0] == '+' || label[0] == '-' || label[0] == '*' || label[0] == '/') {
        calc_prev_val = calc_parse_display();
        calc_op = label[0];
        calc_new_num = 1;
    } else if (strcmp(label, "=") == 0) {
        if (calc_op) {
            int64_t cur = calc_parse_display();
            int64_t res = 0;
            if (calc_op == '+') res = calc_prev_val + cur;
            else if (calc_op == '-') res = calc_prev_val - cur;
            else if (calc_op == '*') res = calc_prev_val * cur;
            else if (calc_op == '/') res = (cur != 0) ? (calc_prev_val / cur) : 0;

            snprintf(calc_display, sizeof(calc_display), "%ld", (long)res);
            calc_op = 0;
            calc_new_num = 1;
        }
    }
    wm_set_dirty();
}

/* -------------------------------------------------------------------------
 * RAMFS File Explorer State
 * ------------------------------------------------------------------------- */
static int files_selected_idx = 0;

/* -------------------------------------------------------------------------
 * Desktop Icons Configuration
 * ------------------------------------------------------------------------- */
struct desktop_icon {
    const char *name;
    const char *tag;
    int win_id;
    int x;
    int y;
    uint32_t bg_color;
};

static const struct desktop_icon desktop_icons[] = {
    { "Files",     "DIR",  WIN_FILES,    16, 32,  0x00D35400 },
    { "Terminal",  ">_",   WIN_TERMINAL, 16, 78,  0x002C3E50 },
    { "Calculator","123",  WIN_CALC,     16, 124, 0x0027AE60 },
    { "Keyboard",  "KBD",  WIN_KEYBOARD, 16, 170, 0x008E44AD },
    { "Drivers",   "DRV",  WIN_DRIVERS,  16, 216, 0x002980B9 },
    { "System",    "SYS",  WIN_SYSTEM,   16, 262, 0x00C0392B },
    { "Agents",    "AGY",  WIN_AGENTS,   16, 308, 0x0016A085 },
    { "KitKoin",   "KIT",  WIN_KITKOIN,  16, 354, 0x00D68910 },
    { "About",     "INFO", WIN_ABOUT,    16, 400, 0x007F8C8D }
};
#define DESKTOP_ICON_COUNT (int)(sizeof(desktop_icons)/sizeof(desktop_icons[0]))

void wm_init(void) {
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        windows[i].is_visible = 0;
        windows[i].is_focused = 0;
        windows[i].is_minimized = 0;
        windows[i].is_maximized = 0;
    }

    /* 1. Terminal Window (Centered Floating Window, Open by Default) */
    windows[WIN_TERMINAL].id = WIN_TERMINAL;
    windows[WIN_TERMINAL].title = "Terminal Console [Interactive]";
    windows[WIN_TERMINAL].x = 100;
    windows[WIN_TERMINAL].y = 50;
    windows[WIN_TERMINAL].w = 460;
    windows[WIN_TERMINAL].h = 260;
    windows[WIN_TERMINAL].orig_x = 100;
    windows[WIN_TERMINAL].orig_y = 50;
    windows[WIN_TERMINAL].orig_w = 460;
    windows[WIN_TERMINAL].orig_h = 260;
    windows[WIN_TERMINAL].is_visible = 1;
    windows[WIN_TERMINAL].is_focused = 1;
    windows[WIN_TERMINAL].render = render_terminal_window;

    /* 2. On-Screen Virtual Keyboard (Floating) */
    windows[WIN_KEYBOARD].id = WIN_KEYBOARD;
    windows[WIN_KEYBOARD].title = "On-Screen Virtual Keyboard [Interactive]";
    windows[WIN_KEYBOARD].x = 95;
    windows[WIN_KEYBOARD].y = 240;
    windows[WIN_KEYBOARD].w = 450;
    windows[WIN_KEYBOARD].h = 194;
    windows[WIN_KEYBOARD].orig_x = 95;
    windows[WIN_KEYBOARD].orig_y = 240;
    windows[WIN_KEYBOARD].orig_w = 450;
    windows[WIN_KEYBOARD].orig_h = 194;
    windows[WIN_KEYBOARD].render = render_keyboard_window;

    /* 3. Calculator (Floating App) */
    windows[WIN_CALC].id = WIN_CALC;
    windows[WIN_CALC].title = "Calculator";
    windows[WIN_CALC].x = 230;
    windows[WIN_CALC].y = 100;
    windows[WIN_CALC].w = 176;
    windows[WIN_CALC].h = 224;
    windows[WIN_CALC].orig_x = 230;
    windows[WIN_CALC].orig_y = 100;
    windows[WIN_CALC].orig_w = 176;
    windows[WIN_CALC].orig_h = 224;
    windows[WIN_CALC].render = render_calc_window;

    /* 4. Files Explorer (Floating App) */
    windows[WIN_FILES].id = WIN_FILES;
    windows[WIN_FILES].title = "RAMFS File Explorer";
    windows[WIN_FILES].x = 160;
    windows[WIN_FILES].y = 70;
    windows[WIN_FILES].w = 340;
    windows[WIN_FILES].h = 230;
    windows[WIN_FILES].orig_x = 160;
    windows[WIN_FILES].orig_y = 70;
    windows[WIN_FILES].orig_w = 340;
    windows[WIN_FILES].orig_h = 230;
    windows[WIN_FILES].render = render_files_window;

    /* 5. Hardware Device Drivers Manager */
    windows[WIN_DRIVERS].id = WIN_DRIVERS;
    windows[WIN_DRIVERS].title = "Hardware Device Drivers Manager";
    windows[WIN_DRIVERS].x = 30;
    windows[WIN_DRIVERS].y = 50;
    windows[WIN_DRIVERS].w = 580;
    windows[WIN_DRIVERS].h = 270;
    windows[WIN_DRIVERS].orig_x = 30;
    windows[WIN_DRIVERS].orig_y = 50;
    windows[WIN_DRIVERS].orig_w = 580;
    windows[WIN_DRIVERS].orig_h = 270;
    windows[WIN_DRIVERS].render = render_drivers_window;

    /* 6. System Monitor */
    windows[WIN_SYSTEM].id = WIN_SYSTEM;
    windows[WIN_SYSTEM].title = "System & Hardware Monitor";
    windows[WIN_SYSTEM].x = 140;
    windows[WIN_SYSTEM].y = 90;
    windows[WIN_SYSTEM].w = 360;
    windows[WIN_SYSTEM].h = 200;
    windows[WIN_SYSTEM].orig_x = 140;
    windows[WIN_SYSTEM].orig_y = 90;
    windows[WIN_SYSTEM].orig_w = 360;
    windows[WIN_SYSTEM].orig_h = 200;
    windows[WIN_SYSTEM].render = render_system_window;

    /* 7. Agents Dashboard */
    windows[WIN_AGENTS].id = WIN_AGENTS;
    windows[WIN_AGENTS].title = "Antigravity Agent Orchestrator";
    windows[WIN_AGENTS].x = 140;
    windows[WIN_AGENTS].y = 90;
    windows[WIN_AGENTS].w = 360;
    windows[WIN_AGENTS].h = 200;
    windows[WIN_AGENTS].orig_x = 140;
    windows[WIN_AGENTS].orig_y = 90;
    windows[WIN_AGENTS].orig_w = 360;
    windows[WIN_AGENTS].orig_h = 200;
    windows[WIN_AGENTS].render = render_agents_window;

    /* 8. About Dialog */
    windows[WIN_ABOUT].id = WIN_ABOUT;
    windows[WIN_ABOUT].title = "About myOS";
    windows[WIN_ABOUT].x = 140;
    windows[WIN_ABOUT].y = 120;
    windows[WIN_ABOUT].w = 360;
    windows[WIN_ABOUT].h = 180;
    windows[WIN_ABOUT].orig_x = 140;
    windows[WIN_ABOUT].orig_y = 120;
    windows[WIN_ABOUT].orig_w = 360;
    windows[WIN_ABOUT].orig_h = 180;
    windows[WIN_ABOUT].render = render_about_window;

    /* 9. KitKoin Crypto Dashboard */
    windows[WIN_KITKOIN].id = WIN_KITKOIN;
    windows[WIN_KITKOIN].title = "KitKoin (KIT) Crypto Dashboard";
    windows[WIN_KITKOIN].x = 125;
    windows[WIN_KITKOIN].y = 55;
    windows[WIN_KITKOIN].w = 390;
    windows[WIN_KITKOIN].h = 275;
    windows[WIN_KITKOIN].orig_x = 125;
    windows[WIN_KITKOIN].orig_y = 55;
    windows[WIN_KITKOIN].orig_w = 390;
    windows[WIN_KITKOIN].orig_h = 275;
    windows[WIN_KITKOIN].render = render_kitkoin_window;

    drivers_init();
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
        windows[id].is_minimized = 0;
        wm_focus_window(id);
        wm_set_dirty();
    }
}

void wm_close_window(int id) {
    if (id >= 0 && id < WM_MAX_WINDOWS) {
        windows[id].is_visible = 0;
        windows[id].is_focused = 0;
        windows[id].is_minimized = 0;
        wm_set_dirty();
    }
}

void wm_focus_window(int id) {
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        windows[i].is_focused = (i == id);
    }
    wm_set_dirty();
}

void wm_minimize_window(int id) {
    if (id >= 0 && id < WM_MAX_WINDOWS) {
        windows[id].is_minimized = 1;
        windows[id].is_focused = 0;
        for (int i = 0; i < WM_MAX_WINDOWS; i++) {
            if (windows[i].is_visible && !windows[i].is_minimized) {
                wm_focus_window(i);
                break;
            }
        }
        wm_set_dirty();
    }
}

void wm_maximize_window(int id) {
    if (id >= 0 && id < WM_MAX_WINDOWS) {
        struct wm_window *w = &windows[id];
        if (!w->is_maximized) {
            w->orig_x = w->x;
            w->orig_y = w->y;
            w->orig_w = w->w;
            w->orig_h = w->h;
            w->x = 0;
            w->y = 24;
            w->w = 640;
            w->h = 432;
            w->is_maximized = 1;
        } else {
            w->x = w->orig_x;
            w->y = w->orig_y;
            w->w = w->orig_w;
            w->h = w->orig_h;
            w->is_maximized = 0;
        }
        wm_focus_window(id);
        wm_set_dirty();
    }
}

void wm_restore_window(int id) {
    if (id >= 0 && id < WM_MAX_WINDOWS) {
        windows[id].is_visible = 1;
        windows[id].is_minimized = 0;
        wm_focus_window(id);
        wm_set_dirty();
    }
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

static uint64_t osk_release_tick = 0;

void wm_keyboard_set_active_key(uint16_t scancode, int pressed) {
    if (pressed) {
        if (scancode == KEY_CAPSLOCK) {
            osk_caps_lock = !osk_caps_lock;
        }
        if (scancode == KEY_LEFTSHIFT || scancode == KEY_RIGHTSHIFT) {
            osk_shift_down = 1;
        }
        osk_active_scancode = scancode;
        osk_key_pressed = 1;
        osk_release_tick = timer_get_ticks() + 15; /* Highlight for ~150ms */
    } else {
        if (scancode == KEY_LEFTSHIFT || scancode == KEY_RIGHTSHIFT) {
            osk_shift_down = 0;
        }
        if (osk_active_scancode == scancode && timer_get_ticks() >= osk_release_tick) {
            osk_key_pressed = 0;
        }
    }
    wm_set_dirty();
}

static void psci_reboot(void) {
    register uint64_t r0 __asm__("x0") = 0x84000009;
    __asm__ volatile("hvc #0" : "+r"(r0) : : "x1", "x2", "x3");
    __asm__ volatile("smc #0" : "+r"(r0) : : "x1", "x2", "x3");
    while (1) {
        __asm__ volatile("wfe");
    }
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

    int right_pressed = (buttons & 2);
    int right_just_pressed = (right_pressed && !(last_mouse_buttons & 2));

    int mid_pressed = (buttons & 4);
    int mid_just_pressed = (mid_pressed && !(last_mouse_buttons & 4));

    /* Mouse Wheel Scroll Handling */
    static int last_wheel = 0;
    int current_wheel = input_get_mouse_wheel();
    int wheel_delta = current_wheel - last_wheel;
    last_wheel = current_wheel;

    if (wheel_delta != 0) {
        wm_set_dirty();
        if (windows[WIN_FILES].is_focused && windows[WIN_FILES].is_visible) {
            if (wheel_delta > 0 && files_selected_idx > 0) {
                files_selected_idx--;
            } else if (wheel_delta < 0 && files_selected_idx + 1 < ramfs_get_total_files()) {
                files_selected_idx++;
            }
        }
    }

    /* Right-click anywhere toggles Start Menu */
    if (right_just_pressed) {
        wm_toggle_start_menu();
        last_mouse_buttons = buttons;
        return;
    }

    /* Middle-click on window closes it */
    if (mid_just_pressed) {
        for (int i = WM_MAX_WINDOWS - 1; i >= 0; i--) {
            if (windows[i].is_visible &&
                x >= windows[i].x && x <= windows[i].x + windows[i].w &&
                y >= windows[i].y && y <= windows[i].y + windows[i].h) {
                wm_close_window(i);
                last_mouse_buttons = buttons;
                return;
            }
        }
    }

    if (left_just_pressed) {
        /* A. Check Start Menu if open */
        if (start_menu_open) {
            if (x >= 4 && x <= 186 && y >= 206 && y <= 452) {
                int item_idx = (y - 232) / 18;
                if (item_idx == 0) wm_open_window(WIN_TERMINAL);
                else if (item_idx == 1) wm_open_window(WIN_KEYBOARD);
                else if (item_idx == 2) wm_open_window(WIN_CALC);
                else if (item_idx == 3) wm_open_window(WIN_FILES);
                else if (item_idx == 4) wm_open_window(WIN_DRIVERS);
                else if (item_idx == 5) wm_open_window(WIN_SYSTEM);
                else if (item_idx == 6) wm_open_window(WIN_AGENTS);
                else if (item_idx == 7) wm_open_window(WIN_ABOUT);
                else if (item_idx == 8) wm_open_window(WIN_KITKOIN);
                else if (item_idx == 10) psci_reboot();

                start_menu_open = 0;
                last_mouse_buttons = buttons;
                return;
            } else if (y < 456 || x > 70) {
                start_menu_open = 0;
            }
        }

        /* B. Check Bottom Taskbar Clicks (y >= 456) */
        if (y >= 456) {
            if (x >= 4 && x <= 68) {
                wm_toggle_start_menu();
                last_mouse_buttons = buttons;
                return;
            }

            int tab_x = 74;
            int tab_w = 64;
            for (int i = 0; i < WM_MAX_WINDOWS; i++) {
                if (windows[i].is_visible) {
                    if (x >= tab_x && x < tab_x + tab_w) {
                        if (windows[i].is_minimized) {
                            wm_restore_window(i);
                            wm_focus_window(i);
                        } else if (windows[i].is_focused) {
                            wm_minimize_window(i);
                        } else {
                            wm_focus_window(i);
                        }
                        last_mouse_buttons = buttons;
                        return;
                    }
                    tab_x += tab_w + 4;
                }
            }
            last_mouse_buttons = buttons;
            return;
        }

        /* C. Check Windows */
        int hit_window = -1;
        for (int i = 0; i < WM_MAX_WINDOWS; i++) {
            if (windows[i].is_visible && !windows[i].is_minimized && windows[i].is_focused) {
                if (x >= windows[i].x && x <= windows[i].x + windows[i].w &&
                    y >= windows[i].y && y <= windows[i].y + windows[i].h) {
                    hit_window = i;
                    break;
                }
            }
        }
        if (hit_window < 0) {
            for (int i = WM_MAX_WINDOWS - 1; i >= 0; i--) {
                if (windows[i].is_visible && !windows[i].is_minimized) {
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

            int btn_y = w->y + 4;
            int btn_h = 15;
            int btn_w = 15;
            int min_x = w->x + w->w - 52;
            int max_x = w->x + w->w - 34;
            int close_x = w->x + w->w - 17;

            /* Close [X] */
            if (x >= close_x && x < close_x + btn_w && y >= btn_y && y < btn_y + btn_h) {
                wm_close_window(hit_window);
                last_mouse_buttons = buttons;
                return;
            }
            /* Maximize [+] */
            else if (x >= max_x && x < max_x + btn_w && y >= btn_y && y < btn_y + btn_h) {
                if (w->is_maximized) {
                    wm_restore_window(hit_window);
                } else {
                    wm_maximize_window(hit_window);
                }
                last_mouse_buttons = buttons;
                return;
            }
            /* Minimize [-] */
            else if (x >= min_x && x < min_x + btn_w && y >= btn_y && y < btn_y + btn_h) {
                wm_minimize_window(hit_window);
                last_mouse_buttons = buttons;
                return;
            }
            /* Title bar for dragging: y <= w->y + 24 (only if not maximized) */
            else if (y <= w->y + 24) {
                if (!w->is_maximized) {
                    active_drag_window = hit_window;
                    drag_start_x = x;
                    drag_start_y = y;
                    win_orig_x = w->x;
                    win_orig_y = w->y;
                }
                last_mouse_buttons = buttons;
                return;
            }
            /* Client Area Clicks */
            else if (hit_window == WIN_KEYBOARD) {
                int rel_x = x - (w->x + 8);
                int rel_y = y - (w->y + 28);
                for (int k = 0; k < OSK_TOTAL_KEYS; k++) {
                    const struct osk_key *key = &osk_keys[k];
                    if (rel_x >= key->x && rel_x <= key->x + key->w &&
                        rel_y >= key->y && rel_y <= key->y + key->h) {

                        if (key->scancode == KEY_CAPSLOCK) {
                            osk_caps_lock = !osk_caps_lock;
                        } else if (key->scancode == KEY_LEFTSHIFT || key->scancode == KEY_RIGHTSHIFT) {
                            osk_shift_down = !osk_shift_down;
                        } else {
                            char c = (osk_shift_down || osk_caps_lock) ? key->shift_char : key->normal_char;
                            if (c) {
                                wm_inject_char(c);
                            }
                        }
                        osk_active_scancode = key->scancode;
                        osk_key_pressed = 1;
                        wm_set_dirty();
                        break;
                    }
                }
            } else if (hit_window == WIN_CALC) {
                int cx = w->x + 8;
                int cy = w->y + 28;
                int rel_x = x - cx;
                int rel_y = y - (cy + 34);
                if (rel_y >= 0) {
                    int btn_w = 34;
                    int btn_h = 24;
                    int col = rel_x / (btn_w + 4);
                    int row = rel_y / (btn_h + 4);
                    if (col >= 0 && col < 4 && row >= 0 && row < 5) {
                        for (int b = 0; b < CALC_TOTAL_BTNS; b++) {
                            if (calc_buttons[b].col == col && calc_buttons[b].row == row) {
                                calc_press(calc_buttons[b].label);
                                break;
                            }
                        }
                    }
                }
            } else if (hit_window == WIN_FILES) {
                int cx = w->x + 8;
                int cy = w->y + 44;
                int rel_y = y - cy;
                if (rel_y >= 0 && rel_y < 54) {
                    int clicked_file = rel_y / 18;
                    if (clicked_file < ramfs_get_total_files()) {
                        files_selected_idx = clicked_file;
                        wm_set_dirty();
                    }
                }
                /* Open in Term button: cx + 4, w->y + 160, 110, 20 */
                if (x >= cx + 4 && x <= cx + 130 && y >= w->y + 160 && y <= w->y + 182) {
                    struct ramfs_file *f = ramfs_get_file_by_index(files_selected_idx);
                    if (f && f->is_used) {
                        wm_open_window(WIN_TERMINAL);
                        gui_terminal_puts("\n[RAMFS Explorer] Viewing ");
                        gui_terminal_puts(f->name);
                        gui_terminal_puts(":\n");
                        gui_terminal_puts(f->data);
                        gui_terminal_puts("\n");
                    }
                }
            } else if (hit_window == WIN_DRIVERS) {
                int content_x = w->x + 8;
                int content_y = w->y + 28;
                int list_w = 210;

                /* Check driver list click */
                if (x >= content_x && x <= content_x + list_w) {
                    int driver_row_y = content_y + 16;
                    int dcount = drivers_get_count();
                    for (int d = 0; d < dcount; d++) {
                        int row_y = driver_row_y + (d * 18);
                        if (y >= row_y && y < row_y + 18) {
                            selected_driver_idx = d;
                            wm_set_dirty();
                            break;
                        }
                    }
                }

                /* Check [Test Driver] and [Test All] button clicks */
                int test_btn_x = w->x + 235;
                int test_btn_y = w->y + w->h - 30;
                if (x >= test_btn_x && x <= test_btn_x + 90 && y >= test_btn_y && y <= test_btn_y + 18) {
                    tested_driver_idx = selected_driver_idx;
                    drivers_self_test(selected_driver_idx);
                    wm_set_dirty();
                } else if (x >= test_btn_x + 96 && x <= test_btn_x + 176 && y >= test_btn_y && y <= test_btn_y + 18) {
                    drivers_test_all();
                    wm_set_dirty();
                }
            } else if (hit_window == WIN_KITKOIN) {
                int btn_y = w->y + w->h - 32;
                /* Button 1: [ Mine Block ] */
                if (x >= w->x + 10 && x <= w->x + 120 && y >= btn_y && y <= btn_y + 22) {
                    coin_mine_block();
                    gui_terminal_puts("\n[KitKoin GUI] Mined new block from Desktop Window!\n");
                    wm_set_dirty();
                }
                /* Button 2: [ P2P Tx +10 ] */
                else if (x >= w->x + 130 && x <= w->x + 240 && y >= btn_y && y <= btn_y + 22) {
                    p2p_simulate_traffic();
                    wm_set_dirty();
                }
                /* Button 3: [ Audit Chain ] */
                else if (x >= w->x + 250 && x <= w->x + 365 && y >= btn_y && y <= btn_y + 22) {
                    coin_verify_chain();
                    gui_terminal_puts("\n[KitKoin GUI] Cryptographic verification complete.\n");
                    wm_set_dirty();
                }
            }
        } else {
            /* Check Desktop Icon Clicks */
            for (int i = 0; i < DESKTOP_ICON_COUNT; i++) {
                const struct desktop_icon *ic = &desktop_icons[i];
                if (x >= ic->x && x <= ic->x + 50 && y >= ic->y && y <= ic->y + 42) {
                    wm_open_window(ic->win_id);
                    if (windows[ic->win_id].is_minimized) {
                        wm_restore_window(ic->win_id);
                    }
                    wm_focus_window(ic->win_id);
                    last_mouse_buttons = buttons;
                    return;
                }
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
        osk_key_pressed = 0;
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

    /* Title string */
    fb_draw_string(w->x + 8, w->y + 7, w->title, COLOR_TEXT_WHITE, header_bg);

    /* Window control buttons on the right: [-] Minimize, [+] Maximize, [X] Close */
    int btn_y = w->y + 4;
    int btn_h = 15;
    int btn_w = 15;

    /* Minimize [-] */
    int min_x = w->x + w->w - 52;
    fb_fill_rect(min_x, btn_y, btn_w, btn_h, 0x003A4A5A);
    fb_draw_rect(min_x, btn_y, btn_w, btn_h, 0x00556677);
    fb_draw_string(min_x + 4, btn_y + 3, "-", COLOR_WHITE, 0x003A4A5A);

    /* Maximize [+] */
    int max_x = w->x + w->w - 34;
    fb_fill_rect(max_x, btn_y, btn_w, btn_h, 0x003A4A5A);
    fb_draw_rect(max_x, btn_y, btn_w, btn_h, 0x00556677);
    fb_draw_string(max_x + 4, btn_y + 3, w->is_maximized ? "v" : "+", COLOR_WHITE, 0x003A4A5A);

    /* Close [X] */
    int close_x = w->x + w->w - 17;
    fb_fill_rect(close_x, btn_y, btn_w, btn_h, COLOR_GOOGLE_RED);
    fb_draw_rect(close_x, btn_y, btn_w, btn_h, 0x00FF8888);
    fb_draw_string(close_x + 4, btn_y + 3, "x", COLOR_WHITE, COLOR_GOOGLE_RED);
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

/* -------------------------------------------------------------------------
 * Hardware Device Drivers Manager Window
 * ------------------------------------------------------------------------- */
static void render_drivers_window(struct wm_window *w) {
    draw_window_frame(w);

    int content_x = w->x + 8;
    int content_y = w->y + 28;
    int list_w = 210;
    int detail_x = content_x + list_w + 8;
    int detail_w = w->w - list_w - 24;
    int content_h = w->h - 36;

    /* 1. Left Pane: Loaded Drivers List */
    fb_fill_rect(content_x, content_y, list_w, content_h, COLOR_CONSOLE_BG);
    fb_draw_rect(content_x, content_y, list_w, content_h, COLOR_PANEL_BORDER);

    fb_fill_rect(content_x, content_y, list_w, 16, COLOR_PANEL_HEADER);
    fb_draw_string(content_x + 6, content_y + 4, "LOADED DRIVERS (11)", COLOR_CYAN, COLOR_PANEL_HEADER);

    int dcount = drivers_get_count();
    for (int i = 0; i < dcount; i++) {
        struct driver_info *d = drivers_get_by_index(i);
        if (!d) continue;

        int row_y = content_y + 18 + (i * 18);
        int is_sel = (i == selected_driver_idx);

        uint32_t bg = is_sel ? COLOR_GOOGLE_BLUE : COLOR_CONSOLE_BG;
        uint32_t fg = is_sel ? COLOR_WHITE : COLOR_TEXT_WHITE;

        if (is_sel) {
            fb_fill_rect(content_x + 2, row_y, list_w - 4, 16, bg);
        }

        /* Tag badge e.g. [UART] */
        char tag_str[10];
        snprintf(tag_str, sizeof(tag_str), "[%-4s]", d->tag);
        fb_draw_string(content_x + 4, row_y + 4, tag_str, is_sel ? COLOR_WHITE : COLOR_GOOGLE_YELLOW, bg);

        /* Short driver name */
        char short_name[14];
        strncpy(short_name, d->name, 12);
        short_name[12] = '\0';
        fb_draw_string(content_x + 50, row_y + 4, short_name, fg, bg);

        /* Status mini-pill */
        uint32_t st_col = COLOR_GOOGLE_GREEN;
        if (d->status == DRIVER_STANDBY) st_col = COLOR_TEXT_MUTED;
        fb_draw_badge(content_x + 155, row_y + 2, d->status_str, COLOR_BLACK, st_col);
    }

    /* 2. Right Pane: Detailed Hardware Diagnostics & Register Inspection */
    fb_fill_rect(detail_x, content_y, detail_w, content_h, COLOR_PANEL_BG);
    fb_draw_rect(detail_x, content_y, detail_w, content_h, COLOR_PANEL_BORDER);

    fb_fill_rect(detail_x, content_y, detail_w, 16, COLOR_PANEL_HEADER);
    fb_draw_string(detail_x + 8, content_y + 4, "DRIVER DIAGNOSTICS & HARDWARE", COLOR_GOOGLE_YELLOW, COLOR_PANEL_HEADER);

    struct driver_info *sel = drivers_get_by_index(selected_driver_idx);
    if (sel) {
        int dy = content_y + 24;

        /* Driver Name and Category */
        fb_draw_string(detail_x + 8, dy, sel->name, COLOR_WHITE, COLOR_PANEL_BG);
        dy += 14;
        fb_draw_string(detail_x + 8, dy, sel->category, COLOR_TEXT_MUTED, COLOR_PANEL_BG);
        fb_draw_badge(detail_x + detail_w - 75, dy - 12, sel->status_str, COLOR_BLACK, COLOR_GOOGLE_GREEN);
        dy += 16;
        fb_draw_line(detail_x + 6, dy, detail_x + detail_w - 6, dy, COLOR_PANEL_BORDER);
        dy += 8;

        /* Hardware Properties */
        char buf[80];
        if (sel->base_addr) {
            snprintf(buf, sizeof(buf), "Physical MMIO: 0x%08lx", (unsigned long)sel->base_addr);
        } else {
            snprintf(buf, sizeof(buf), "Physical MMIO: System Internal (Core)");
        }
        fb_draw_string(detail_x + 8, dy, buf, COLOR_TEXT_WHITE, COLOR_PANEL_BG);
        dy += 13;

        if (sel->irq) {
            snprintf(buf, sizeof(buf), "GIC Interrupt: IRQ Line %u", sel->irq);
        } else {
            snprintf(buf, sizeof(buf), "GIC Interrupt: Polled / Synchronous");
        }
        fb_draw_string(detail_x + 8, dy, buf, COLOR_TEXT_WHITE, COLOR_PANEL_BG);
        dy += 13;

        snprintf(buf, sizeof(buf), "Hardware Rev:  %s", sel->version);
        fb_draw_string(detail_x + 8, dy, buf, COLOR_TEXT_WHITE, COLOR_PANEL_BG);
        dy += 16;

        /* Live Driver Metrics Box */
        fb_draw_string(detail_x + 8, dy, "Live Hardware Telemetry:", COLOR_CYAN, COLOR_PANEL_BG);
        dy += 12;

        fb_fill_rect(detail_x + 8, dy, detail_w - 16, 26, COLOR_CONSOLE_BG);
        fb_draw_rect(detail_x + 8, dy, detail_w - 16, 26, COLOR_PANEL_BORDER);

        char stats[128];
        if (sel->get_stats) {
            sel->get_stats(stats, sizeof(stats));
            fb_draw_string(detail_x + 12, dy + 8, stats, COLOR_GOOGLE_GREEN, COLOR_CONSOLE_BG);
        }
        dy += 34;

        /* Description */
        fb_draw_string(detail_x + 8, dy, "Driver Architecture & Function:", COLOR_TEXT_MUTED, COLOR_PANEL_BG);
        dy += 12;
        fb_draw_string(detail_x + 8, dy, sel->description, COLOR_TEXT_WHITE, COLOR_PANEL_BG);
        dy += 24;

        /* Action Buttons */
        int test_btn_x = detail_x + 8;
        int test_btn_y = content_y + content_h - 24;
        fb_fill_rect(test_btn_x, test_btn_y, 90, 18, COLOR_GOOGLE_BLUE);
        fb_draw_rect(test_btn_x, test_btn_y, 90, 18, COLOR_WHITE);
        fb_draw_string(test_btn_x + 6, test_btn_y + 5, "[Test Driver]", COLOR_WHITE, COLOR_GOOGLE_BLUE);

        int test_all_x = test_btn_x + 96;
        fb_fill_rect(test_all_x, test_btn_y, 80, 18, 0x002B3A5A);
        fb_draw_rect(test_all_x, test_btn_y, 80, 18, COLOR_PANEL_BORDER);
        fb_draw_string(test_all_x + 6, test_btn_y + 5, "[Test All]", COLOR_WHITE, 0x002B3A5A);

        if (sel->test_status > 0) {
            fb_draw_badge(test_all_x + 86, test_btn_y, "PASS (100% OK)", COLOR_BLACK, COLOR_GOOGLE_GREEN);
        } else if (sel->test_status < 0) {
            fb_draw_badge(test_all_x + 86, test_btn_y, "FAIL", COLOR_WHITE, COLOR_GOOGLE_RED);
        } else {
            fb_draw_badge(test_all_x + 86, test_btn_y, "READY", COLOR_BLACK, COLOR_CYAN);
        }

        if (sel->test_status != 0) {
            fb_draw_string(test_btn_x, test_btn_y - 14, sel->last_test_result,
                           (sel->test_status > 0) ? COLOR_GOOGLE_GREEN : COLOR_GOOGLE_RED, COLOR_PANEL_BG);
        }
    }
}

static void render_keyboard_window(struct wm_window *w) {
    draw_window_frame(w);

    int base_x = w->x + 8;
    int base_y = w->y + 28;

    /* Status indicators in keyboard header */
    if (osk_caps_lock) {
        fb_draw_badge(w->x + w->w - 110, w->y + 5, "CAPS", COLOR_BLACK, COLOR_GOOGLE_YELLOW);
    }
    if (osk_shift_down) {
        fb_draw_badge(w->x + w->w - 55, w->y + 5, "SHIFT", COLOR_BLACK, COLOR_GOOGLE_GREEN);
    }

    /* Auto-decay visual keypress if timer expired */
    if (osk_key_pressed && timer_get_ticks() >= osk_release_tick) {
        osk_key_pressed = 0;
    }

    /* Render all 5 rows of keyboard keys */
    for (int i = 0; i < OSK_TOTAL_KEYS; i++) {
        const struct osk_key *key = &osk_keys[i];
        int kx = base_x + key->x;
        int ky = base_y + key->y;

        int is_active = (osk_key_pressed && osk_active_scancode == key->scancode);
        uint32_t bg = is_active ? COLOR_GOOGLE_BLUE : 0x00242838;
        uint32_t fg = is_active ? COLOR_WHITE : COLOR_TEXT_WHITE;

        if (key->scancode == KEY_CAPSLOCK && osk_caps_lock) {
            bg = COLOR_GOOGLE_YELLOW;
            fg = COLOR_BLACK;
        } else if ((key->scancode == KEY_LEFTSHIFT || key->scancode == KEY_RIGHTSHIFT) && osk_shift_down) {
            bg = COLOR_GOOGLE_GREEN;
            fg = COLOR_BLACK;
        }

        fb_fill_rect(kx, ky, key->w, key->h, bg);
        fb_draw_rect(kx, ky, key->w, key->h, COLOR_PANEL_BORDER);

        const char *label = (osk_shift_down || osk_caps_lock) ? key->shift_label : key->label;
        int text_off_x = (key->w - (strlen(label) * 8)) / 2;
        if (text_off_x < 2) text_off_x = 2;
        int text_off_y = (key->h - 8) / 2;

        fb_draw_string(kx + text_off_x, ky + text_off_y, label, fg, bg);
    }

    /* Quick Tip Label */
    fb_draw_string(base_x + 4, base_y + 144, "Click any key to type into Terminal Console", COLOR_TEXT_MUTED, COLOR_PANEL_BG);
}

static void render_calc_window(struct wm_window *w) {
    draw_window_frame(w);

    int cx = w->x + 8;
    int cy = w->y + 28;
    int cw = w->w - 16;

    /* LCD Screen Display */
    fb_fill_rect(cx, cy, cw, 26, 0x00101820);
    fb_draw_rect(cx, cy, cw, 26, COLOR_PANEL_BORDER);

    /* Render current value (right aligned) */
    int len = strlen(calc_display);
    int disp_x = cx + cw - 8 - (len * 8);
    if (disp_x < cx + 4) disp_x = cx + 4;
    fb_draw_string(disp_x, cy + 8, calc_display, COLOR_GOOGLE_GREEN, 0x00101820);

    /* Operation indicator */
    if (calc_op) {
        char op_str[2] = { calc_op, '\0' };
        fb_draw_string(cx + 6, cy + 8, op_str, COLOR_GOOGLE_YELLOW, 0x00101820);
    }

    /* Buttons Matrix (5 rows x 4 cols) */
    int start_y = cy + 34;
    int btn_w = 34;
    int btn_h = 24;

    for (int i = 0; i < CALC_TOTAL_BTNS; i++) {
        const struct calc_btn *b = &calc_buttons[i];
        int bx = cx + (b->col * (btn_w + 4));
        int by = start_y + (b->row * (btn_h + 4));

        fb_fill_rect(bx, by, btn_w, btn_h, b->bg_col);
        fb_draw_rect(bx, by, btn_w, btn_h, COLOR_PANEL_BORDER);

        int text_off_x = (btn_w - (strlen(b->label) * 8)) / 2;
        int text_off_y = (btn_h - 8) / 2;
        fb_draw_string(bx + text_off_x, by + text_off_y, b->label, COLOR_TEXT_WHITE, b->bg_col);
    }
}

static void render_files_window(struct wm_window *w) {
    draw_window_frame(w);

    int cx = w->x + 8;
    int cy = w->y + 28;

    fb_draw_string(cx, cy, "RAMFS Storage:", COLOR_CYAN, COLOR_PANEL_BG);
    cy += 16;

    int total_files = ramfs_get_total_files();
    for (int i = 0; i < total_files && i < 3; i++) {
        struct ramfs_file *f = ramfs_get_file_by_index(i);
        if (!f || !f->is_used) continue;

        int row_y = cy + (i * 18);
        uint32_t bg = (i == files_selected_idx) ? 0x002B3A5A : COLOR_PANEL_BG;
        fb_fill_rect(cx, row_y - 2, w->w - 16, 16, bg);

        char buf[32];
        snprintf(buf, sizeof(buf), "[TXT] %-10s", f->name);
        fb_draw_string(cx + 2, row_y + 2, buf, COLOR_TEXT_WHITE, bg);
    }

    /* File Preview Section */
    int prev_y = w->y + 92;
    fb_draw_line(cx, prev_y, cx + w->w - 16, prev_y, COLOR_PANEL_BORDER);
    fb_draw_string(cx, prev_y + 4, "Preview Pane:", COLOR_TEXT_MUTED, COLOR_PANEL_BG);

    struct ramfs_file *sel = ramfs_get_file_by_index(files_selected_idx);
    if (sel && sel->is_used) {
        fb_fill_rect(cx, prev_y + 18, w->w - 16, 44, COLOR_CONSOLE_BG);
        fb_draw_rect(cx, prev_y + 18, w->w - 16, 44, COLOR_PANEL_BORDER);

        char line1[20];
        char line2[20];
        strncpy(line1, sel->data, 18);
        line1[18] = '\0';
        fb_draw_string(cx + 4, prev_y + 22, line1, COLOR_TEXT_WHITE, COLOR_CONSOLE_BG);

        if (strlen(sel->data) > 18) {
            strncpy(line2, sel->data + 18, 18);
            line2[18] = '\0';
            fb_draw_string(cx + 4, prev_y + 34, line2, COLOR_TEXT_WHITE, COLOR_CONSOLE_BG);
        }
    }

    /* Open Button */
    fb_draw_badge(cx + 4, w->y + 162, "[ Open in Term ]", COLOR_WHITE, COLOR_GOOGLE_BLUE);
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
    y += 14;
    fb_draw_string(x, y, "Includes On-Screen Keyboard & Desktop Apps.", COLOR_TEXT_MUTED, COLOR_PANEL_BG);
}

static void render_kitkoin_window(struct wm_window *w) {
    draw_window_frame(w);

    int cx = w->x + 10;
    int cy = w->y + 28;
    int cw = w->w - 20;

    /* 1. Header Banner */
    fb_fill_rect(cx, cy, cw, 22, 0x00D68910);
    fb_draw_rect(cx, cy, cw, 22, COLOR_PANEL_BORDER);
    fb_draw_string(cx + 8, cy + 5, "KitKoin (KIT) - Decentralized Native Crypto", COLOR_WHITE, 0x00D68910);
    cy += 28;

    /* 2. Wallet Card */
    fb_fill_rect(cx, cy, cw, 46, 0x001B2631);
    fb_draw_rect(cx, cy, cw, 46, COLOR_PANEL_BORDER);

    struct coin_wallet *my_w = coin_get_my_wallet();
    char addr_str[48];
    snprintf(addr_str, sizeof(addr_str), "Wallet: %s (%s)", my_w ? my_w->address : "N/A", my_w ? my_w->label : "Node");
    fb_draw_string(cx + 8, cy + 6, addr_str, COLOR_CYAN, 0x001B2631);

    char bal_str[36];
    snprintf(bal_str, sizeof(bal_str), "Balance: %lu KIT", my_w ? my_w->balance : 0);
    fb_draw_string(cx + 8, cy + 24, bal_str, COLOR_GOOGLE_GREEN, 0x001B2631);

    char halving_badge[32];
    snprintf(halving_badge, sizeof(halving_badge), "Halving: in %u blks", coin_get_blocks_until_halving());
    fb_draw_string(cx + cw - 165, cy + 24, halving_badge, COLOR_GOOGLE_YELLOW, 0x001B2631);
    cy += 52;

    /* 3. Blockchain Telemetry Matrix */
    fb_fill_rect(cx, cy, cw, 44, 0x00151E28);
    fb_draw_rect(cx, cy, cw, 44, COLOR_PANEL_BORDER);

    char s1[36], s2[36], s3[36], s4[36];
    snprintf(s1, sizeof(s1), "Height: #%u", coin_get_height());
    snprintf(s2, sizeof(s2), "Reward: %lu KIT", coin_get_block_reward(coin_get_height()));
    snprintf(s3, sizeof(s3), "Mempool: %u TXs", coin_get_mempool_count());
    snprintf(s4, sizeof(s4), "Supply: %lu KIT", coin_get_circulating_supply());

    fb_draw_string(cx + 8,  cy + 6,  s1, COLOR_TEXT_WHITE, 0x00151E28);
    fb_draw_string(cx + 180, cy + 6,  s2, COLOR_TEXT_WHITE, 0x00151E28);
    fb_draw_string(cx + 8,  cy + 24, s3, COLOR_GOOGLE_YELLOW, 0x00151E28);
    fb_draw_string(cx + 180, cy + 24, s4, COLOR_CYAN, 0x00151E28);
    cy += 50;

    /* 4. Ledger Snapshot (Last 2 Mined Blocks) */
    fb_draw_string(cx, cy, "Latest Ledger Blocks:", COLOR_TEXT_MUTED, COLOR_PANEL_BG);
    cy += 14;

    uint32_t h = coin_get_height();
    uint32_t start_b = (h > 2) ? (h - 2) : 0;
    for (uint32_t b_idx = start_b; b_idx < h; b_idx++) {
        struct coin_block *b = coin_get_block(b_idx);
        if (!b) continue;

        char b_h[65];
        sha256_to_hex(b->block_hash, b_h);
        b_h[12] = '\0';

        char row[56];
        snprintf(row, sizeof(row), "#%02u | %s... | %u TXs | Subsidy: %lu KIT",
                 b->index, b_h, b->tx_count, b->reward);

        fb_fill_rect(cx, cy, cw, 16, 0x00212F3D);
        fb_draw_string(cx + 4, cy + 2, row, COLOR_TEXT_WHITE, 0x00212F3D);
        cy += 18;
    }

    /* 5. Interactive Action Buttons */
    int btn_y = w->y + w->h - 30;
    fb_draw_badge(w->x + 10,  btn_y, "[ Mine Block ]", COLOR_WHITE, 0x00D68910);
    fb_draw_badge(w->x + 130, btn_y, "[ P2P Tx +10 ]", COLOR_WHITE, COLOR_GOOGLE_BLUE);
    fb_draw_badge(w->x + 250, btn_y, "[ Audit Chain ]", COLOR_WHITE, COLOR_GOOGLE_GREEN);
}

static void render_start_menu(void) {
    int sx = 4;
    int sy = 206;
    int sw = 186;
    int sh = 246;

    fb_fill_rect(sx, sy, sw, sh, COLOR_PANEL_BG);
    fb_draw_rect(sx, sy, sw, sh, COLOR_GOOGLE_BLUE);

    /* Banner */
    fb_fill_rect(sx + 1, sy + 1, sw - 2, 24, COLOR_GOOGLE_BLUE);
    fb_draw_string(sx + 8, sy + 7, "Google Antigravity OS", COLOR_WHITE, COLOR_GOOGLE_BLUE);

    const char *items[] = {
        "[F1] Terminal Console",
        "[F2] On-Screen Keyboard",
        "[F3] Calculator",
        "[F4] RAMFS Explorer",
        "[F5] Device Drivers",
        "[F6] System Monitor",
        "[F7] Agent Dashboard",
        "[F8] About myOS",
        "[F9] KitKoin Crypto Wallet",
        "---------------------",
        "[!] Shut Down / Reboot"
    };

    int iy = sy + 28;
    for (int i = 0; i < 11; i++) {
        uint32_t col = (i == 10) ? COLOR_GOOGLE_RED : ((i == 8) ? COLOR_GOOGLE_YELLOW : COLOR_TEXT_WHITE);
        fb_draw_string(sx + 8, iy + (i * 18), items[i], col, COLOR_PANEL_BG);
    }
}

void wm_render(void) {
    /* 1. Desktop Background */
    fb_clear(COLOR_DESKTOP_BG);

    /* Desktop Wallpaper Geometric Grid Pattern */
    for (int y = 30; y < 450; y += 24) {
        for (int x = 10; x < 630; x += 24) {
            fb_draw_pixel(x, y, 0x001B1E2B);
        }
    }

    /* Render Desktop Icons */
    for (int i = 0; i < DESKTOP_ICON_COUNT; i++) {
        const struct desktop_icon *ic = &desktop_icons[i];
        fb_fill_rounded_rect(ic->x, ic->y, 36, 36, ic->bg_color);
        fb_draw_string(ic->x + 6, ic->y + 12, ic->tag, COLOR_WHITE, ic->bg_color);
        fb_draw_string(ic->x + 42, ic->y + 14, ic->name, COLOR_TEXT_WHITE, COLOR_DESKTOP_BG);
    }

    /* 2. Top Taskbar */
    fb_fill_rect(0, 0, 640, 24, COLOR_TOPBAR_BG);
    fb_draw_line(0, 24, 640, 24, COLOR_PANEL_BORDER);

    fb_draw_badge(8, 4, "myOS", COLOR_WHITE, COLOR_GOOGLE_BLUE);
    fb_draw_string(60, 8, "Google Antigravity OS - Desktop", COLOR_TEXT_WHITE, COLOR_TOPBAR_BG);

    /* Top bar indicators */
    char top_stat[64];
    snprintf(top_stat, sizeof(top_stat), "DRV: 11 Live | RAM Free: %lu KB | Up: %lu s",
             mm_get_free_pages() * 4, timer_get_ticks() / 100);
    fb_draw_string(320, 8, top_stat, COLOR_TEXT_MUTED, COLOR_TOPBAR_BG);

    /* 3. Render Windows (Non-focused first, then focused on top) */
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (windows[i].is_visible && !windows[i].is_minimized && !windows[i].is_focused && windows[i].render) {
            windows[i].render(&windows[i]);
        }
    }
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (windows[i].is_visible && !windows[i].is_minimized && windows[i].is_focused && windows[i].render) {
            windows[i].render(&windows[i]);
        }
    }

    /* 4. Bottom Taskbar with Quick Launch and Hardware Driver Status */
    fb_fill_rect(0, 456, 640, 24, COLOR_STATUSBAR_BG);
    fb_draw_line(0, 456, 640, 456, COLOR_PANEL_BORDER);

    /* Start Button */
    fb_draw_badge(4, 459, " START ", COLOR_WHITE, start_menu_open ? COLOR_GOOGLE_YELLOW : COLOR_GOOGLE_BLUE);

    /* Dynamic Taskbar App Tabs for Open Windows */
    int tab_x = 74;
    int tab_w = 64;
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (windows[i].is_visible) {
            uint32_t tab_bg;
            if (windows[i].is_minimized) {
                tab_bg = COLOR_DARK_GRAY;
            } else if (windows[i].is_focused) {
                tab_bg = COLOR_GOOGLE_BLUE;
            } else {
                tab_bg = COLOR_PANEL_HEADER;
            }
            fb_fill_rect(tab_x, 459, tab_w, 18, tab_bg);
            fb_draw_rect(tab_x, 459, tab_w, 18, COLOR_PANEL_BORDER);

            const char *slabel = "Window";
            if (i == WIN_TERMINAL) slabel = "Term";
            else if (i == WIN_KEYBOARD) slabel = "Kbd";
            else if (i == WIN_CALC) slabel = "Calc";
            else if (i == WIN_FILES) slabel = "Files";
            else if (i == WIN_DRIVERS) slabel = "Drivers";
            else if (i == WIN_SYSTEM) slabel = "System";
            else if (i == WIN_AGENTS) slabel = "Agents";
            else if (i == WIN_ABOUT) slabel = "About";
            else if (i == WIN_KITKOIN) slabel = "KitKoin";

            char tab_text[12];
            if (windows[i].is_minimized) {
                snprintf(tab_text, sizeof(tab_text), "[%s]", slabel);
            } else {
                snprintf(tab_text, sizeof(tab_text), " %s", slabel);
            }
            fb_draw_string(tab_x + 4, 464, tab_text, COLOR_WHITE, tab_bg);

            tab_x += tab_w + 4;
        }
    }

    /* Real-Time Digital Clock (HH:MM:SS) in System Tray */
    uint64_t total_sec = timer_get_ticks() / 100;
    uint32_t hours = (total_sec / 3600) % 24;
    uint32_t mins = (total_sec / 60) % 60;
    uint32_t secs = total_sec % 60;
    char clock_buf[12];
    snprintf(clock_buf, sizeof(clock_buf), "%02u:%02u:%02u", hours, mins, secs);

    fb_fill_rect(564, 459, 72, 18, 0x001B2430);
    fb_draw_rect(564, 459, 72, 18, COLOR_PANEL_BORDER);
    fb_draw_string(570, 464, clock_buf, COLOR_GOOGLE_GREEN, 0x001B2430);

    /* Real-Time Hardware Driver Status Badge */
    fb_draw_badge(500, 459, "HID LIVE", COLOR_BLACK, COLOR_GOOGLE_GREEN);

    /* 5. Render Popup Start Menu if open */
    if (start_menu_open) {
        render_start_menu();
    }

    /* 6. Render interactive mouse cursor */
    fb_draw_cursor(mouse_curr_x, mouse_curr_y);

    /* 7. Present rendered backbuffer to screen (Flicker-Free Flip) */
    fb_present();
}
