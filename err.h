#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <errno.h>

#define errx(fmt, ...) \
    _verr(true, 0, fmt, ## __VA_ARGS__)

#define errc(fmt, ...) \
    _verr(true, errno, fmt, ## __VA_ARGS__)

#define warnc(fmt, ...) \
    _verr(false, errno, fmt, ## __VA_ARGS__)

#define warn(fmt, ...) \
    _verr(false, 0, fmt, ## __VA_ARGS__)

void _verr(bool, int, const char *, ...);
