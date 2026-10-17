#ifndef _GUI_H
#define _GUI_H

#include "types.h"
#include "stddef.h"

/* Graphical Terminal Dimensions (in characters) */
#define GUI_TERM_COLS 70
#define GUI_TERM_ROWS 14

/* Console Text Mirror APIs */
void gui_init(void);
void gui_render_desktop(void);
void gui_refresh(void);
void gui_terminal_putc(char c);
void gui_terminal_puts(const char *str);
void gui_terminal_clear(void);
const char *gui_terminal_get_line(int row);
void gui_terminal_get_cursor(int *row, int *col);

struct key_event;
void gui_handle_key(struct key_event *ev);
void gui_run_interactive(void);

#endif /* _GUI_H */
