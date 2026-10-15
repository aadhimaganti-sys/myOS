#ifndef _PRINTF_H
#define _PRINTF_H

#include "types.h"
#include "stdarg.h"

int kprintf(const char *fmt, ...);
int kvprintf(const char *fmt, va_list va);
int snprintf(char *buf, size_t size, const char *fmt, ...);
int vsnprintf(char *buf, size_t size, const char *fmt, va_list va);

#endif /* _PRINTF_H */
