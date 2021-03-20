#pragma once

#include <errno.h>
#include <stdarg.h>
#include <string.h> /* strerror */

#ifdef DEBUG
# define set_generic_error(error, format, ...) \
    _error_set(error, "[%s:%d] " format, __func__, __LINE__, ## __VA_ARGS__)

# define set_errno_error(error, errno, format, ...) \
    _error_set(error, "[%s:%d] " format ": %s", __func__, __LINE__, ## __VA_ARGS__, strerror(errno))

// #define set_snprintf_error(error, fmt, dst, dst_cap) \
//     _error_set(error, "[%s:%d] buffer overflow: %s doesn't fit into %s (%zu)", __func__, __LINE__, fmt, #dst, dst_cap)
#else
# define set_generic_error(error, format, ...) \
    _error_set(error, format, ## __VA_ARGS__)

# define set_errno_error(error, errno, format, ...) \
    _error_set(error, format ": %s", ## __VA_ARGS__, strerror(errno))
#endif /* DEBUG */

#define set_system_error(error, format, ...) \
    set_errno_error(error, errno, format, ## __VA_ARGS__)

#define set_malloc_error(error, size) \
    set_generic_error(error, "malloc(3) failed to allocate %zu bytes", size)

#define set_calloc_error(error, number, size) \
    set_generic_error(error, "calloc(3) failed to allocate %zu bytes", number * size)

#define set_buffer_overflow_error(error, src, dst, dst_cap) \
    set_generic_error(error, "buffer overflow: %s (\"%s\") doesn't fit into %s (%zu > %zu)", #src, src, #dst, strlen(src) + 1, dst_cap)

void _error_set(char **, const char *, ...);
void error_free(char **);
