#pragma once

#include <sys/param.h> /* __FreeBSD__ */

#if 0 && defined(__FreeBSD__) && __FreeBSD__ >= 9

# include <stdio.h> /* perror */
# include <unistd.h>
# include <sys/capsicum.h>

# define CAP_IOCTLS_LIMIT(fd, ...) \
    do { \
        unsigned long cmds[] = { __VA_ARGS__ }; \
        if (0 != cap_ioctls_limit(fd, cmds, ARRAY_SIZE(cmds))) { \
            fprintf(stderr, "%s:%s:%d: fd is %d", __FILE__, __func__, __LINE__, (int) fd); \
            perror("cap_ioctls_limit"); \
        } \
    } while (0);

# define CAP_RIGHTS_LIMIT(fd, ...) \
    do { \
        cap_rights_t rights; \
 \
        cap_rights_init(&rights, ## __VA_ARGS__); \
        if (0 != cap_rights_limit(fd, &rights) && ENOSYS != errno) { \
            fprintf(stderr, "%s:%s:%d: fd is %d", __FILE__, __func__, __LINE__, (int) fd); \
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
