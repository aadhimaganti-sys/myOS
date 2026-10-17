#include "fb.h"
#include "mm.h"
#include "printf.h"
#include "string.h"

static struct framebuffer current_fb;
static int fb_initialized = 0;
static uint32_t *back_buffer = NULL;

/* Embedded 8x8 ASCII Bitmap Font (Characters 32 to 126) */
static const uint8_t font_8x8[95][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* ' ' */
    {0x18,0x3c,0x3c,0x18,0x18,0x00,0x18,0x00}, /* '!' */
    {0x66,0x66,0x24,0x00,0x00,0x00,0x00,0x00}, /* '"' */
    {0x6c,0x6c,0xfe,0x6c,0xfe,0x6c,0x6c,0x00}, /* '#' */
    {0x18,0x3e,0x60,0x3c,0x06,0x7c,0x18,0x00}, /* '$' */
    {0x00,0x63,0x66,0x0c,0x18,0x33,0x63,0x00}, /* '%' */
    {0x38,0x6c,0x38,0x76,0xdc,0xcc,0x76,0x00}, /* '&' */
    {0x18,0x18,0x30,0x00,0x00,0x00,0x00,0x00}, /* ''' */
    {0x0c,0x18,0x30,0x30,0x30,0x18,0x0c,0x00}, /* '(' */
    {0x30,0x18,0x0c,0x0c,0x0c,0x18,0x30,0x00}, /* ')' */
    {0x00,0x66,0x3c,0xff,0x3c,0x66,0x00,0x00}, /* '*' */
    {0x00,0x18,0x18,0x7e,0x18,0x18,0x00,0x00}, /* '+' */
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30}, /* ',' */
    {0x00,0x00,0x00,0x7e,0x00,0x00,0x00,0x00}, /* '-' */
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, /* '.' */
    {0x06,0x0c,0x18,0x30,0x60,0xc0,0x80,0x00}, /* '/' */
    {0x7c,0xc6,0xce,0xd6,0xe6,0xc6,0x7c,0x00}, /* '0' */
    {0x18,0x38,0x18,0x18,0x18,0x18,0x7e,0x00}, /* '1' */
    {0x7c,0xc6,0x06,0x1c,0x30,0x60,0xfe,0x00}, /* '2' */
    {0x7c,0xc6,0x06,0x3c,0x06,0xc6,0x7c,0x00}, /* '3' */
    {0x1c,0x3c,0x6c,0xcc,0xfe,0x0c,0x1e,0x00}, /* '4' */
    {0xfe,0xc0,0xfc,0x06,0x06,0xc6,0x7c,0x00}, /* '5' */
    {0x78,0x0c,0xc0,0xfc,0xc6,0xc6,0x7c,0x00}, /* '6' */
    {0xfe,0xc6,0x0c,0x18,0x30,0x30,0x30,0x00}, /* '7' */
    {0x7c,0xc6,0xc6,0x7c,0xc6,0xc6,0x7c,0x00}, /* '8' */
    {0x7c,0xc6,0xc6,0x7e,0x06,0x0c,0x78,0x00}, /* '9' */
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00}, /* ':' */
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x30}, /* ';' */
    {0x0c,0x18,0x30,0x60,0x30,0x18,0x0c,0x00}, /* '<' */
    {0x00,0x00,0x7e,0x00,0x7e,0x00,0x00,0x00}, /* '=' */
    {0x30,0x18,0x0c,0x06,0x0c,0x18,0x30,0x00}, /* '>' */
    {0x7c,0xc6,0x0c,0x18,0x18,0x00,0x18,0x00}, /* '?' */
    {0x7c,0xc6,0xde,0xde,0xdc,0xc0,0x7c,0x00}, /* '@' */
    {0x38,0x6c,0xc6,0xfe,0xc6,0xc6,0xc6,0x00}, /* 'A' */
    {0xfc,0x66,0x66,0x7c,0x66,0x66,0xfc,0x00}, /* 'B' */
    {0x3c,0x66,0xc0,0xc0,0xc0,0x66,0x3c,0x00}, /* 'C' */
    {0xf8,0x6c,0x66,0x66,0x66,0x6c,0xf8,0x00}, /* 'D' */
    {0xfe,0x62,0x68,0x78,0x68,0x62,0xfe,0x00}, /* 'E' */
    {0xfe,0x62,0x68,0x78,0x68,0x60,0xf0,0x00}, /* 'F' */
    {0x3c,0x66,0xc0,0xc0,0xce,0x66,0x3e,0x00}, /* 'G' */
    {0xc6,0xc6,0xc6,0xfe,0xc6,0xc6,0xc6,0x00}, /* 'H' */
    {0x3c,0x18,0x18,0x18,0x18,0x18,0x3c,0x00}, /* 'I' */
    {0x1e,0x0c,0x0c,0x0c,0xcc,0xcc,0x78,0x00}, /* 'J' */
    {0xe6,0x66,0x6c,0x78,0x6c,0x66,0xe6,0x00}, /* 'K' */
    {0xf0,0x60,0x60,0x60,0x62,0x66,0xfe,0x00}, /* 'L' */
    {0xc6,0xee,0xfe,0xd6,0xc6,0xc6,0xc6,0x00}, /* 'M' */
    {0xc6,0xe6,0xf6,0xde,0xce,0xc6,0xc6,0x00}, /* 'N' */
    {0x7c,0xc6,0xc6,0xc6,0xc6,0xc6,0x7c,0x00}, /* 'O' */
    {0xfc,0x66,0x66,0x7c,0x60,0x60,0xf0,0x00}, /* 'P' */
    {0x7c,0xc6,0xc6,0xc6,0xd6,0x7c,0x0e,0x00}, /* 'Q' */
    {0xfc,0x66,0x66,0x7c,0x6c,0x66,0xe6,0x00}, /* 'R' */
    {0x7c,0xc6,0x60,0x38,0x0c,0xc6,0x7c,0x00}, /* 'S' */
    {0x7e,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, /* 'T' */
    {0xc6,0xc6,0xc6,0xc6,0xc6,0xc6,0x7c,0x00}, /* 'U' */
    {0xc6,0xc6,0xc6,0xc6,0x6c,0x38,0x10,0x00}, /* 'V' */
    {0xc6,0xc6,0xd6,0xfe,0xee,0xc6,0xc6,0x00}, /* 'W' */
    {0xc6,0x6c,0x38,0x38,0x6c,0xc6,0xc6,0x00}, /* 'X' */
    {0x66,0x66,0x66,0x3c,0x18,0x18,0x3c,0x00}, /* 'Y' */
    {0xfe,0x06,0x0c,0x18,0x30,0x60,0xfe,0x00}, /* 'Z' */
    {0x3c,0x30,0x30,0x30,0x30,0x30,0x3c,0x00}, /* '[' */
    {0xc0,0x60,0x30,0x18,0x0c,0x06,0x02,0x00}, /* '\' */
    {0x3c,0x0c,0x0c,0x0c,0x0c,0x0c,0x3c,0x00}, /* ']' */
    {0x10,0x38,0x6c,0xc6,0x00,0x00,0x00,0x00}, /* '^' */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff}, /* '_' */
    {0x30,0x18,0x0c,0x00,0x00,0x00,0x00,0x00}, /* '`' */
    {0x00,0x00,0x78,0x0c,0x7c,0xcc,0x76,0x00}, /* 'a' */
    {0xe0,0x60,0x7c,0x66,0x66,0x66,0xdc,0x00}, /* 'b' */
    {0x00,0x00,0x78,0xcc,0xc0,0xcc,0x78,0x00}, /* 'c' */
    {0x1c,0x0c,0x7c,0xcc,0xcc,0xcc,0x76,0x00}, /* 'd' */
    {0x00,0x00,0x78,0xcc,0xfc,0xc0,0x78,0x00}, /* 'e' */
    {0x38,0x6c,0x60,0xf0,0x60,0x60,0xf0,0x00}, /* 'f' */
    {0x00,0x00,0x76,0xcc,0xcc,0x7c,0x0c,0xf8}, /* 'g' */
    {0xe0,0x60,0x6c,0x76,0x66,0x66,0xe6,0x00}, /* 'h' */
    {0x18,0x00,0x38,0x18,0x18,0x18,0x3c,0x00}, /* 'i' */
    {0x0c,0x00,0x1c,0x0c,0x0c,0xcc,0xcc,0x78}, /* 'j' */
    {0xe0,0x60,0x66,0x6c,0x78,0x6c,0xe6,0x00}, /* 'k' */
    {0x38,0x18,0x18,0x18,0x18,0x18,0x3c,0x00}, /* 'l' */
    {0x00,0x00,0xe6,0xff,0xdb,0xc9,0xc9,0x00}, /* 'm' */
    {0x00,0x00,0xdc,0x66,0x66,0x66,0x66,0x00}, /* 'n' */
    {0x00,0x00,0x7c,0xc6,0xc6,0xc6,0x7c,0x00}, /* 'o' */
    {0x00,0x00,0xdc,0x66,0x66,0x7c,0x60,0xf0}, /* 'p' */
    {0x00,0x00,0x76,0xcc,0xcc,0x7c,0x0c,0x1e}, /* 'q' */
    {0x00,0x00,0xdc,0x76,0x66,0x60,0xf0,0x00}, /* 'r' */
    {0x00,0x00,0x7c,0xc0,0x78,0x0e,0xfc,0x00}, /* 's' */
    {0x10,0x30,0x7c,0x30,0x30,0x34,0x18,0x00}, /* 't' */
    {0x00,0x00,0xcc,0xcc,0xcc,0xcc,0x76,0x00}, /* 'u' */
    {0x00,0x00,0xc6,0xc6,0xc6,0x6c,0x38,0x00}, /* 'v' */
    {0x00,0x00,0xc6,0xd6,0xfe,0xfe,0x6c,0x00}, /* 'w' */
    {0x00,0x00,0xc6,0x6c,0x38,0x6c,0xc6,0x00}, /* 'x' */
    {0x00,0x00,0xc6,0xc6,0xce,0x76,0x06,0x7c}, /* 'y' */
    {0x00,0x00,0xfc,0x98,0x30,0x64,0xfc,0x00}, /* 'z' */
    {0x0e,0x18,0x18,0x70,0x18,0x18,0x0e,0x00}, /* '{' */
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, /* '|' */
    {0x70,0x18,0x18,0x0e,0x18,0x18,0x70,0x00}, /* '}' */
    {0x76,0xdc,0x00,0x00,0x00,0x00,0x00,0x00}  /* '~' */
};

#define FW_CFG_BASE      0x09020000ULL
#define FW_CFG_DATA      (*(volatile uint8_t *)(FW_CFG_BASE + 0x00))
#define FW_CFG_SEL       (*(volatile uint16_t *)(FW_CFG_BASE + 0x08))
#define FW_CFG_DMA       (*(volatile uint64_t *)(FW_CFG_BASE + 0x10))

#define FW_CFG_SIGNATURE 0x0000
#define FW_CFG_FILE_DIR  0x0019

#define FW_CFG_DMA_CTL_ERROR  0x01
#define FW_CFG_DMA_CTL_READ   0x02
#define FW_CFG_DMA_CTL_SKIP   0x04
#define FW_CFG_DMA_CTL_SELECT 0x08
#define FW_CFG_DMA_CTL_WRITE  0x10

struct ramfb_cfg {
    uint64_t addr;
    uint32_t fourcc;
    uint32_t flags;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
} __attribute__((packed));

struct fw_cfg_dma_access {
    uint32_t control;
    uint32_t length;
    uint64_t address;
} __attribute__((packed));

static struct ramfb_cfg s_ramfb_cfg;
static struct fw_cfg_dma_access s_fw_cfg_dma;

int fb_setup_ramfb(void) {
    if (!fb_initialized || !current_fb.buffer) {
        return -1;
    }

    /* Probe fw_cfg signature */
    FW_CFG_SEL = 0x0000;
    char sig[4];
    sig[0] = FW_CFG_DATA;
    sig[1] = FW_CFG_DATA;
    sig[2] = FW_CFG_DATA;
    sig[3] = FW_CFG_DATA;

    if (sig[0] != 'Q' || sig[1] != 'E' || sig[2] != 'M' || sig[3] != 'U') {
        return 0;
    }

    /* Query file directory */
    FW_CFG_SEL = __builtin_bswap16(FW_CFG_FILE_DIR);
    uint32_t count = 0;
    for (int i = 0; i < 4; i++) {
        count = (count << 8) | FW_CFG_DATA;
    }

    if (count == 0 || count > 2000) {
        FW_CFG_SEL = FW_CFG_FILE_DIR;
        count = 0;
        for (int i = 0; i < 4; i++) {
            count = (count << 8) | FW_CFG_DATA;
        }
    }

    if (count == 0 || count > 2000) {
        return 0;
    }

    uint16_t ramfb_select = 0;
    int found = 0;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t size = 0;
        for (int b = 0; b < 4; b++) size = (size << 8) | FW_CFG_DATA;
        (void)size;

        uint16_t sel = 0;
        sel = (uint16_t)(FW_CFG_DATA << 8);
        sel |= FW_CFG_DATA;

        FW_CFG_DATA; /* reserved */
        FW_CFG_DATA;

        char name[56];
        for (int b = 0; b < 56; b++) {
            name[b] = FW_CFG_DATA;
        }
        name[55] = '\0';

        if (strcmp(name, "etc/ramfb") == 0) {
            ramfb_select = sel;
            found = 1;
            break;
        }
    }

    if (!found) {
        kprintf("[*] fw_cfg probed: 'etc/ramfb' device not attached (software framebuffer active)\n");
        return 0;
    }

    /* Configure hardware RAMFB scanout via fw_cfg DMA */
    s_ramfb_cfg.addr = __builtin_bswap64((uint64_t)current_fb.buffer);
    s_ramfb_cfg.fourcc = __builtin_bswap32(0x34325258); /* DRM_FORMAT_XRGB8888 */
    s_ramfb_cfg.flags = 0;
    s_ramfb_cfg.width = __builtin_bswap32(current_fb.width);
    s_ramfb_cfg.height = __builtin_bswap32(current_fb.height);
    s_ramfb_cfg.stride = __builtin_bswap32(current_fb.pitch);

    s_fw_cfg_dma.address = __builtin_bswap64((uint64_t)&s_ramfb_cfg);
    s_fw_cfg_dma.length = __builtin_bswap32(sizeof(s_ramfb_cfg));
    s_fw_cfg_dma.control = __builtin_bswap32(((uint32_t)ramfb_select << 16) | FW_CFG_DMA_CTL_WRITE | FW_CFG_DMA_CTL_SELECT);

    __asm__ volatile("dsb sy" ::: "memory");
    FW_CFG_DMA = __builtin_bswap64((uint64_t)&s_fw_cfg_dma);
    __asm__ volatile("dsb sy" ::: "memory");

    int timeout = 100000;
    while ((__builtin_bswap32(s_fw_cfg_dma.control) & ~FW_CFG_DMA_CTL_ERROR) != 0 && --timeout > 0) {
        __asm__ volatile("nop");
    }

    if ((__builtin_bswap32(s_fw_cfg_dma.control) & FW_CFG_DMA_CTL_ERROR) == 0) {
        current_fb.is_hardware = 1;
        kprintf("[+] Hardware ramfb scanout active: 640x480 32bpp via QEMU fw_cfg\n");
        return 1;
    }

    return 0;
}

void fb_init(uint32_t width, uint32_t height) {
    current_fb.width = width;
    current_fb.height = height;
    current_fb.pitch = width * sizeof(uint32_t);
    current_fb.bpp = 32;
    current_fb.is_hardware = 0;

    size_t total_bytes = width * height * sizeof(uint32_t);
    size_t pages_needed = (total_bytes + PAGE_SIZE - 1) / PAGE_SIZE;

    current_fb.buffer = (uint32_t *)alloc_pages(pages_needed);
    if (!current_fb.buffer) {
        kprintf("[FB:ERROR] Failed to allocate %u KB for Framebuffer!\n",
                (unsigned int)(total_bytes / 1024));
        return;
    }

    memset(current_fb.buffer, 0, total_bytes);

    /* Allocate secondary buffer for flicker-free double buffering */
    back_buffer = (uint32_t *)alloc_pages(pages_needed);
    if (back_buffer) {
        memset(back_buffer, 0, total_bytes);
        kprintf("[+] Double-buffering enabled (Flicker-Free Backbuffer: %u KB)\n",
                (unsigned int)(total_bytes / 1024));
    } else {
        back_buffer = current_fb.buffer;
    }

    fb_initialized = 1;

    kprintf("[+] Framebuffer driver active: %u x %u x 32bpp (%u KB at %p)\n",
            width, height, (unsigned int)(total_bytes / 1024), (void *)current_fb.buffer);

    /* Attempt to configure QEMU fw_cfg hardware scanout if available */
    fb_setup_ramfb();
}

void fb_clear(uint32_t color) {
    if (!fb_initialized) return;
    uint32_t *buf = back_buffer ? back_buffer : current_fb.buffer;
    if (!buf) return;

    size_t total_pixels = current_fb.width * current_fb.height;
    for (size_t i = 0; i < total_pixels; i++) {
        buf[i] = color;
    }
}

void fb_draw_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!fb_initialized) return;
    uint32_t *buf = back_buffer ? back_buffer : current_fb.buffer;
    if (!buf) return;

    if (x < current_fb.width && y < current_fb.height) {
        buf[y * current_fb.width + x] = color;
    }
}

void fb_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t i = 0; i < w; i++) {
        fb_draw_pixel(x + i, y, color);
        fb_draw_pixel(x + i, y + h - 1, color);
    }
    for (uint32_t j = 0; j < h; j++) {
        fb_draw_pixel(x, y + j, color);
        fb_draw_pixel(x + w - 1, y + j, color);
    }
}

void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t j = 0; j < h; j++) {
        for (uint32_t i = 0; i < w; i++) {
            fb_draw_pixel(x + i, y + j, color);
        }
    }
}

void fb_draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (1) {
        fb_draw_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void fb_draw_char(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg) {
    if (c < 32 || c > 126) {
        c = '?';
    }

    const uint8_t *glyph = font_8x8[c - 32];
    for (uint32_t row = 0; row < 8; row++) {
        uint8_t line = glyph[row];
        for (uint32_t col = 0; col < 8; col++) {
            if (line & (0x80 >> col)) {
                fb_draw_pixel(x + col, y + row, fg);
            } else if (bg != COLOR_BLACK || fg == COLOR_BLACK) {
                fb_draw_pixel(x + col, y + row, bg);
            }
        }
    }
}

void fb_draw_string(uint32_t x, uint32_t y, const char *str, uint32_t fg, uint32_t bg) {
    if (!str) return;
    uint32_t cx = x;
    uint32_t cy = y;

    while (*str) {
        if (*str == '\n') {
            cx = x;
            cy += 10;
        } else {
            fb_draw_char(cx, cy, *str, fg, bg);
            cx += 8;
        }
        str++;
    }
}

void fb_draw_progress_bar(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t percent, uint32_t fill_color, uint32_t bg_color, uint32_t border_color) {
    if (w < 4 || h < 4) return;
    if (percent > 100) percent = 100;

    /* Draw border */
    fb_draw_rect(x, y, w, h, border_color);

    /* Fill inner bar and background */
    uint32_t inner_w = w - 2;
    uint32_t inner_h = h - 2;
    uint32_t fill_w = (inner_w * percent) / 100;

    if (fill_w > 0) {
        fb_fill_rect(x + 1, y + 1, fill_w, inner_h, fill_color);
    }
    if (fill_w < inner_w) {
        fb_fill_rect(x + 1 + fill_w, y + 1, inner_w - fill_w, inner_h, bg_color);
    }
}

void fb_draw_badge(uint32_t x, uint32_t y, const char *text, uint32_t fg_color, uint32_t bg_color) {
    if (!text) return;
    size_t len = strlen(text);
    uint32_t pad_x = 4;
    uint32_t pad_y = 2;
    uint32_t badge_w = (uint32_t)len * 8 + pad_x * 2;
    uint32_t badge_h = 8 + pad_y * 2;

    fb_fill_rect(x, y, badge_w, badge_h, bg_color);
    fb_draw_string(x + pad_x, y + pad_y, text, fg_color, bg_color);
}

void fb_render_test_screen(void) {
    if (!fb_initialized) return;

    /* 1. Desktop Background */
    fb_clear(COLOR_DARK_GRAY);

    /* 2. Top Title Bar */
    fb_fill_rect(0, 0, current_fb.width, 24, COLOR_NAVY);
    fb_draw_string(10, 8, "myOS AArch64 Kernel - Phase 3 Graphical Display Engine", COLOR_WHITE, COLOR_NAVY);

    /* 3. Color Palette Test Bar */
    uint32_t colors[] = {
        COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_YELLOW,
        COLOR_CYAN, COLOR_MAGENTA, COLOR_WHITE, COLOR_LIGHT_GRAY
    };
    uint32_t bar_w = current_fb.width / 8;
    for (uint32_t i = 0; i < 8; i++) {
        fb_fill_rect(i * bar_w, 30, bar_w, 16, colors[i]);
    }

    /* 4. Graphical Demo Window */
    fb_fill_rect(30, 60, 320, 200, COLOR_LIGHT_GRAY);
    fb_draw_rect(30, 60, 320, 200, COLOR_WHITE);
    fb_fill_rect(30, 60, 320, 20, COLOR_TEAL);
    fb_draw_string(36, 66, "Subsystem Status Window", COLOR_WHITE, COLOR_TEAL);

    fb_draw_string(40, 90,  "[OK] 4-Level MMU Identity Paged", COLOR_BLACK, COLOR_LIGHT_GRAY);
    fb_draw_string(40, 110, "[OK] Physical Page Allocator (64 MB)", COLOR_BLACK, COLOR_LIGHT_GRAY);
    fb_draw_string(40, 130, "[OK] Dynamic Heap Allocator (kmalloc)", COLOR_BLACK, COLOR_LIGHT_GRAY);
    fb_draw_string(40, 150, "[OK] Preemptive Multitasking Sched", COLOR_BLACK, COLOR_LIGHT_GRAY);
    fb_draw_string(40, 170, "[OK] VirtIO MMIO Transport Probed", COLOR_BLACK, COLOR_LIGHT_GRAY);
    fb_draw_string(40, 190, "[OK] Framebuffer Graphics Ready", COLOR_BLACK, COLOR_LIGHT_GRAY);

    /* 5. Geometric Shapes Demo */
    fb_fill_rect(380, 60, 230, 200, COLOR_BLACK);
    fb_draw_rect(380, 60, 230, 200, COLOR_WHITE);
    fb_draw_string(390, 70, "Geometry Test", COLOR_CYAN, COLOR_BLACK);

    fb_draw_line(390, 90, 590, 90, COLOR_RED);
    fb_draw_line(390, 90, 490, 180, COLOR_GREEN);
    fb_draw_line(590, 90, 490, 180, COLOR_BLUE);
    fb_draw_rect(420, 110, 80, 50, COLOR_YELLOW);
    fb_fill_rect(440, 125, 40, 20, COLOR_MAGENTA);

    /* 6. Bottom Status Bar */
    fb_fill_rect(0, current_fb.height - 20, current_fb.width, 20, COLOR_NAVY);
    fb_draw_string(10, current_fb.height - 14, "System Status: Phase 3 Operational | Ready for Phase 4 (RAMFS & Shell)", COLOR_WHITE, COLOR_NAVY);

    kprintf("[+] Rendered graphical desktop & subsystem dashboard to framebuffer (%u x %u)\n",
            current_fb.width, current_fb.height);
}

struct framebuffer *fb_get_info(void) {
    return &current_fb;
}

/* -------------------------------------------------------------------------
 * Mouse Cursor & Rounded Geometry Subsystem
 * ------------------------------------------------------------------------- */

/* Crisp 12x18 arrow cursor bitmap:
 * 0 = transparent, 1 = black border, 2 = white interior */
static const uint8_t cursor_bitmap[CURSOR_HEIGHT][CURSOR_WIDTH] = {
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* B........... */
    {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* BB.......... */
    {1, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* BWB......... */
    {1, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0}, /* BWWB........ */
    {1, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0}, /* BWWWB....... */
    {1, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0}, /* BWWWWB...... */
    {1, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0}, /* BWWWWWB..... */
    {1, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0}, /* BWWWWWWB.... */
    {1, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0}, /* BWWWWWWWB... */
    {1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0}, /* BWWWWWWWWB.. */
    {1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0}, /* BWWWWWWWWWB. */
    {1, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1}, /* BWWWWWWBBBBBB */
    {1, 2, 2, 1, 2, 2, 1, 0, 0, 0, 0, 0}, /* BWWBWWB..... */
    {1, 2, 1, 0, 1, 2, 2, 1, 0, 0, 0, 0}, /* BWB.BWWB.... */
    {1, 1, 0, 0, 1, 2, 2, 1, 0, 0, 0, 0}, /* BB..BWWB.... */
    {1, 0, 0, 0, 0, 1, 2, 2, 1, 0, 0, 0}, /* B....BWWB... */
    {0, 0, 0, 0, 0, 1, 2, 2, 1, 0, 0, 0}, /* .....BWWB... */
    {0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0}  /* ......BB.... */
};

/* Buffer to save background pixels under cursor to prevent trails */
static uint32_t cursor_saved_bg[CURSOR_HEIGHT][CURSOR_WIDTH];
static int cursor_saved_x = 0;
static int cursor_saved_y = 0;
static int cursor_drawn = 0;

void fb_restore_cursor(void) {
    if (!cursor_drawn || !current_fb.buffer) return;
    uint32_t *buf = back_buffer ? back_buffer : current_fb.buffer;

    for (int r = 0; r < CURSOR_HEIGHT; r++) {
        int py = cursor_saved_y + r;
        if (py < 0 || (uint32_t)py >= current_fb.height) continue;
        for (int c = 0; c < CURSOR_WIDTH; c++) {
            int px = cursor_saved_x + c;
            if (px < 0 || (uint32_t)px >= current_fb.width) continue;
            buf[py * current_fb.width + px] = cursor_saved_bg[r][c];
        }
    }
    cursor_drawn = 0;
}

void fb_draw_cursor(int x, int y) {
    if (!fb_initialized || !current_fb.buffer) return;
    uint32_t *buf = back_buffer ? back_buffer : current_fb.buffer;

    /* Always restore previous background before drawing at new position */
    if (cursor_drawn) {
        fb_restore_cursor();
    }

    cursor_saved_x = x;
    cursor_saved_y = y;

    /* 1. Save background pixels beneath the cursor */
    for (int r = 0; r < CURSOR_HEIGHT; r++) {
        int py = y + r;
        for (int c = 0; c < CURSOR_WIDTH; c++) {
            int px = x + c;
            if (px >= 0 && (uint32_t)px < current_fb.width &&
                py >= 0 && (uint32_t)py < current_fb.height) {
                cursor_saved_bg[r][c] = buf[py * current_fb.width + px];
            } else {
                cursor_saved_bg[r][c] = COLOR_BLACK;
            }
        }
    }

    /* 2. Draw crisp cursor pixels */
    for (int r = 0; r < CURSOR_HEIGHT; r++) {
        int py = y + r;
        if (py < 0 || (uint32_t)py >= current_fb.height) continue;
        for (int c = 0; c < CURSOR_WIDTH; c++) {
            int px = x + c;
            if (px < 0 || (uint32_t)px >= current_fb.width) continue;
            uint8_t pixel = cursor_bitmap[r][c];
            if (pixel == 1) {
                buf[py * current_fb.width + px] = COLOR_BLACK;
            } else if (pixel == 2) {
                buf[py * current_fb.width + px] = COLOR_WHITE;
            }
        }
    }

    cursor_drawn = 1;
}

void fb_draw_rounded_rect(int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    if (w < 6 || h < 6) {
        fb_draw_rect((uint32_t)x, (uint32_t)y, (uint32_t)w, (uint32_t)h, color);
        return;
    }

    /* Top and bottom horizontal lines */
    for (int i = 2; i < w - 2; i++) {
        fb_draw_pixel(x + i, y, color);
        fb_draw_pixel(x + i, y + h - 1, color);
    }
    /* Left and right vertical lines */
    for (int j = 2; j < h - 2; j++) {
        fb_draw_pixel(x, y + j, color);
        fb_draw_pixel(x + w - 1, y + j, color);
    }
    /* Smooth 2px rounded corners */
    fb_draw_pixel(x + 1, y + 1, color);
    fb_draw_pixel(x + w - 2, y + 1, color);
    fb_draw_pixel(x + 1, y + h - 2, color);
    fb_draw_pixel(x + w - 2, y + h - 2, color);
}

void fb_fill_rounded_rect(int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    if (w < 6 || h < 6) {
        fb_fill_rect((uint32_t)x, (uint32_t)y, (uint32_t)w, (uint32_t)h, color);
        return;
    }

    /* Rows 0 and h-1 (width minus 4) */
    for (int i = 2; i < w - 2; i++) {
        fb_draw_pixel(x + i, y, color);
        fb_draw_pixel(x + i, y + h - 1, color);
    }
    /* Rows 1 and h-2 (width minus 2) */
    for (int i = 1; i < w - 1; i++) {
        fb_draw_pixel(x + i, y + 1, color);
        fb_draw_pixel(x + i, y + h - 2, color);
    }
    /* Inner body rows */
    for (int j = 2; j < h - 2; j++) {
        for (int i = 0; i < w; i++) {
            fb_draw_pixel(x + i, y + j, color);
        }
    }
}

void fb_present(void) {
    if (!fb_initialized || !current_fb.buffer || !back_buffer || back_buffer == current_fb.buffer) {
        return;
    }

    /* Fast 64-bit copy from backbuffer to visible frontbuffer */
    uint64_t *dst = (uint64_t *)current_fb.buffer;
    const uint64_t *src = (const uint64_t *)back_buffer;
    size_t count = (current_fb.width * current_fb.height * sizeof(uint32_t)) / sizeof(uint64_t);
    for (size_t i = 0; i < count; i++) {
        dst[i] = src[i];
    }
    __asm__ volatile("dmb sy" ::: "memory");
}

