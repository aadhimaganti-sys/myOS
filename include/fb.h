#ifndef _FB_H
#define _FB_H

#include "types.h"
#include "stddef.h"

/* Standard 32-bit ARGB/XRGB Colors */
#define COLOR_BLACK         0x00000000
#define COLOR_WHITE         0x00FFFFFF
#define COLOR_RED           0x00FF0000
#define COLOR_GREEN         0x0000FF00
#define COLOR_BLUE          0x000000FF
#define COLOR_CYAN          0x0000FFFF
#define COLOR_MAGENTA       0x00FF00FF
#define COLOR_YELLOW        0x00FFFF00
#define COLOR_DARK_GRAY     0x00333333
#define COLOR_LIGHT_GRAY    0x00AAAAAA
#define COLOR_NAVY          0x00001F3F
#define COLOR_TEAL          0x0039CCCC

/* Google Antigravity Brand & UI Colors */
#define COLOR_GOOGLE_BLUE   0x004285F4
#define COLOR_GOOGLE_RED    0x00EA4335
#define COLOR_GOOGLE_YELLOW 0x00FBBC05
#define COLOR_GOOGLE_GREEN  0x0034A853

/* Dark Theme GUI Colors */
#define COLOR_DESKTOP_BG    0x0013151D
#define COLOR_TOPBAR_BG     0x00181A24
#define COLOR_STATUSBAR_BG  0x00181A24
#define COLOR_PANEL_BG      0x001C1F2B
#define COLOR_PANEL_BORDER  0x00333A4E
#define COLOR_PANEL_HEADER  0x00242838
#define COLOR_CONSOLE_BG    0x000F1118
#define COLOR_TEXT_WHITE    0x00F0F2F8
#define COLOR_TEXT_MUTED    0x008E95AA
#define COLOR_TEXT_DIM      0x005E6578

struct framebuffer {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    uint32_t *buffer;
    int is_hardware;
};

/* Framebuffer Primitives */
void fb_init(uint32_t width, uint32_t height);
void fb_clear(uint32_t color);
void fb_draw_pixel(uint32_t x, uint32_t y, uint32_t color);
void fb_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_draw_line(int x0, int y0, int x1, int y1, uint32_t color);
void fb_draw_char(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg);
void fb_draw_string(uint32_t x, uint32_t y, const char *str, uint32_t fg, uint32_t bg);
void fb_draw_progress_bar(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t percent, uint32_t fill_color, uint32_t bg_color, uint32_t border_color);
void fb_draw_badge(uint32_t x, uint32_t y, const char *text, uint32_t fg_color, uint32_t bg_color);
int  fb_setup_ramfb(void);
void fb_render_test_screen(void);
struct framebuffer *fb_get_info(void);
void fb_present(void);

/* Cursor and Rounded Geometry Primitives */
#define CURSOR_WIDTH  12
#define CURSOR_HEIGHT 18

void fb_draw_cursor(int x, int y);
void fb_restore_cursor(void);
void fb_draw_rounded_rect(int x, int y, int w, int h, uint32_t color);
void fb_fill_rounded_rect(int x, int y, int w, int h, uint32_t color);

#endif /* _FB_H */

