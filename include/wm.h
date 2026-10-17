#ifndef _WM_H
#define _WM_H

#include "types.h"
#include "stdint.h"
#include "stddef.h"

/* Window Identifiers */
#define WIN_TERMINAL    0
#define WIN_KEYBOARD    1
#define WIN_CALC        2
#define WIN_FILES       3
#define WIN_DRIVERS     4
#define WIN_SYSTEM      5
#define WIN_AGENTS      6
#define WIN_ABOUT       7
#define WIN_KITKOIN     8
#define WM_MAX_WINDOWS  9

/* Window Structure Definition */
struct wm_window {
    int id;
    const char *title;
    int x, y, w, h;
    int is_visible;
    int is_focused;
    int is_minimized;
    int is_maximized;
    int orig_x, orig_y, orig_w, orig_h;
    int is_dragging;
    int drag_off_x;
    int drag_off_y;
    void (*render)(struct wm_window *w);
};

/* Core Window Manager Lifecycle & Compositor APIs */
void wm_init(void);
void wm_render(void);
void wm_handle_mouse(int x, int y, int buttons);
void wm_handle_key(char c);
void wm_open_window(int id);
void wm_close_window(int id);
void wm_focus_window(int id);
void wm_minimize_window(int id);
void wm_maximize_window(int id);
void wm_restore_window(int id);
int  wm_is_start_menu_open(void);

/* Window Manager Queries and Control */
struct wm_window *wm_get_window(int id);
int  wm_get_mouse_x(void);
int  wm_get_mouse_y(void);
void wm_toggle_start_menu(void);
int  wm_is_dirty(void);
void wm_set_dirty(void);
void wm_keyboard_set_active_key(uint16_t scancode, int pressed);
void wm_inject_char(char c);

#endif /* _WM_H */
