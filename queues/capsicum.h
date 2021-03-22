#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <sys/param.h> /* __FreeBSD__ */

#if 0 && defined(__FreeBSD__) && __FreeBSD__ >= 9

# include <unistd.h>
# include <sys/capsicum.h>

typedef uint64_t capability_t;
typedef unsigned long cmd_t;

# define CAP_RIGHTS_LIMIT(error, fd, ...) \
    _cap_rights_limit(error, fd, __VA_ARGS__, 0ULL)

# define CAP_IOCTLS_LIMIT(error, fd, ...) \
    _cap_ioctls_limit(error, fd, __VA_ARGS__, 0UL)

bool CAP_ENTER(char **);
bool _cap_ioctls_limit(char **, int, ...);
bool _cap_rights_limit(char **, int, ...);

#else

# define CAP_RIGHTS_LIMIT(error, fd, ...) \
    true

# define CAP_IOCTLS_LIMIT(error, fd, ...) \
    true

# define CAP_ENTER(error) \
    true

#endif /* FreeBSD >= 9 */

