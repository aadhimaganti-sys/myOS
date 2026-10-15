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
void fb_render_test_screen(void);
struct framebuffer *fb_get_info(void);

#endif /* _FB_H */
