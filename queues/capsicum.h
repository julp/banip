#pragma once

#if defined(__FreeBSD__) && __FreeBSD__ >= 9

# include <unistd.h>
# include <sys/capsicum.h>

# define CAP_IOCTLS_LIMIT(fd, ...) \
    do { \
        unsigned long cmds[] = { __VA_ARGS__ }; \
        if (0 != cap_ioctls_limit(fd, cmds, ARRAY_SIZE(cmds))) { \
            perror("cap_ioctls_limit"); \
        } \
    } while (0);

# define CAP_RIGHTS_LIMIT(fd, ...) \
    do { \
        cap_rights_t rights; \
 \
        cap_rights_init(&rights, ## __VA_ARGS__); \
        if (0 != cap_rights_limit(fd, &rights) && ENOSYS != errno) { \
            perror("cap_rights_limit"); \
        } \
    } while (0);

# define CAP_ENTER() \
    do { \
        if (0 != cap_enter() && ENOSYS != errno) { \
            perror("cap_enter"); \
        } \
    } while (0);

#else

# define CAP_IOCTLS_LIMIT(fd, ...) \
    /* NOP */

# define CAP_RIGHTS_LIMIT(fd, ...) \
    /* NOP */

# define CAP_ENTER() \
    /* NOP */

#endif /* FreeBSD >= 9.0 */
