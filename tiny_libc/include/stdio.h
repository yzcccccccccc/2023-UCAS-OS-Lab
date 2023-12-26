#ifndef INCLUDE_STDIO_H_
#define INCLUDE_STDIO_H_

#include <stdarg.h>

int printf(const char *fmt, ...);
int vprintf(const char *fmt, va_list va);

// fopen type
#define O_RDONLY    0
#define O_WRONLY    1
#define O_RDWR      2

// lseek
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define SEEK_RP 0
#define SEEK_WP 1

#endif
