#include "printf.h"
#include "uart.h"
#include "string.h"
#include "gui.h"

struct buffer_ctx {
    char *buf;
    size_t size;
    size_t written;
};

static void emit_buf(char c, void *user_data) {
    struct buffer_ctx *ctx = (struct buffer_ctx *)user_data;
    if (ctx->buf && ctx->written + 1 < ctx->size) {
        ctx->buf[ctx->written] = c;
    }
    ctx->written++;
}

static void emit_uart(char c, void *user_data) {
    (void)user_data;
    uart_putc(c);
    gui_terminal_putc(c);
}

typedef void (*emit_fn)(char c, void *user_data);

static void format_number(emit_fn emit, void *user_data, uint64_t num, int base, int is_signed,
                          int is_upper, int width, int zero_pad, int left_align) {
    char digits_lower[] = "0123456789abcdef";
    char digits_upper[] = "0123456789ABCDEF";
    const char *digits = is_upper ? digits_upper : digits_lower;
    char buffer[65];
    int pos = 0;
    int is_negative = 0;

    if (is_signed && (int64_t)num < 0) {
        is_negative = 1;
        num = (uint64_t)(-(int64_t)num);
    }

    if (num == 0) {
        buffer[pos++] = '0';
    } else {
        while (num > 0) {
            buffer[pos++] = digits[num % base];
            num /= base;
        }
    }

    int num_len = pos + (is_negative ? 1 : 0);
    int pad_len = (width > num_len) ? (width - num_len) : 0;

    if (!left_align && !zero_pad) {
        while (pad_len-- > 0) {
            emit(' ', user_data);
        }
    }

    if (is_negative) {
        emit('-', user_data);
    }

    if (!left_align && zero_pad) {
        while (pad_len-- > 0) {
            emit('0', user_data);
        }
    }

    while (pos > 0) {
        emit(buffer[--pos], user_data);
    }

    if (left_align) {
        while (pad_len-- > 0) {
            emit(' ', user_data);
        }
    }
}

static int format_core(emit_fn emit, void *user_data, const char *fmt, va_list va) {
    int count = 0;

    while (*fmt) {
        if (*fmt != '%') {
            emit(*fmt++, user_data);
            count++;
            continue;
        }

        fmt++; /* skip '%' */

        /* Flags */
        int zero_pad = 0;
        int left_align = 0;
        while (*fmt == '0' || *fmt == '-') {
            if (*fmt == '0') zero_pad = 1;
            if (*fmt == '-') left_align = 1;
            fmt++;
        }
        if (left_align) {
            zero_pad = 0;
        }

        /* Width */
        int width = 0;
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        /* Length modifier */
        int is_long = 0;
        if (*fmt == 'l') {
            is_long = 1;
            fmt++;
            if (*fmt == 'l') {
                is_long = 2;
                fmt++;
            }
        } else if (*fmt == 'z') {
            is_long = 1;
            fmt++;
        }

        char spec = *fmt++;
        if (!spec) break;

        switch (spec) {
            case 'c': {
                char c = (char)va_arg(va, int);
                emit(c, user_data);
                count++;
                break;
            }
            case 's': {
                const char *s = va_arg(va, const char *);
                if (!s) s = "(null)";
                int len = 0;
                while (s[len]) len++;
                int pad = (width > len) ? (width - len) : 0;
                if (!left_align) {
                    while (pad-- > 0) {
                        emit(' ', user_data);
                        count++;
                    }
                }
                while (*s) {
                    emit(*s++, user_data);
                    count++;
                }
                if (left_align) {
                    while (pad-- > 0) {
                        emit(' ', user_data);
                        count++;
                    }
                }
                break;
            }
            case 'd':
            case 'i': {
                int64_t val;
                if (is_long >= 1) {
                    val = va_arg(va, int64_t);
                } else {
                    val = (int64_t)va_arg(va, int);
                }
                format_number(emit, user_data, (uint64_t)val, 10, 1, 0, width, zero_pad, left_align);
                break;
            }
            case 'u': {
                uint64_t val;
                if (is_long >= 1) {
                    val = va_arg(va, uint64_t);
                } else {
                    val = (uint64_t)va_arg(va, unsigned int);
                }
                format_number(emit, user_data, val, 10, 0, 0, width, zero_pad, left_align);
                break;
            }
            case 'x': {
                uint64_t val;
                if (is_long >= 1) {
                    val = va_arg(va, uint64_t);
                } else {
                    val = (uint64_t)va_arg(va, unsigned int);
                }
                format_number(emit, user_data, val, 16, 0, 0, width, zero_pad, left_align);
                break;
            }
            case 'X': {
                uint64_t val;
                if (is_long >= 1) {
                    val = va_arg(va, uint64_t);
                } else {
                    val = (uint64_t)va_arg(va, unsigned int);
                }
                format_number(emit, user_data, val, 16, 0, 1, width, zero_pad, left_align);
                break;
            }
            case 'p': {
                uintptr_t val = (uintptr_t)va_arg(va, void *);
                emit('0', user_data);
                emit('x', user_data);
                count += 2;
                format_number(emit, user_data, val, 16, 0, 0, width ? width : 16, 1, left_align);
                break;
            }
            case '%': {
                emit('%', user_data);
                count++;
                break;
            }
            default: {
                emit('%', user_data);
                emit(spec, user_data);
                count += 2;
                break;
            }
        }
    }

    return count;
}

int kvprintf(const char *fmt, va_list va) {
    return format_core(emit_uart, NULL, fmt, va);
}

int kprintf(const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    int ret = kvprintf(fmt, va);
    va_end(va);
    return ret;
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list va) {
    struct buffer_ctx ctx = { .buf = buf, .size = size, .written = 0 };
    format_core(emit_buf, &ctx, fmt, va);
    if (buf && size > 0) {
        if (ctx.written < size) {
            buf[ctx.written] = '\0';
        } else {
            buf[size - 1] = '\0';
        }
    }
    return (int)ctx.written;
}

int snprintf(char *buf, size_t size, const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    int ret = vsnprintf(buf, size, fmt, va);
    va_end(va);
    return ret;
}
