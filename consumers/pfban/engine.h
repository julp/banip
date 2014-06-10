#ifndef X_ENGINE_H

# define X_ENGINE_H

typedef struct {
    const char const *name;
    void *(*open)(void);
//     int (*getopt)(void *, int, const char *);
    int (*handle)(void *, const char *, const char *);
    void (*close)(void *);
} engine_t;

# include <errno.h>
# define errx(fmt, ...)  _verr(1, 0, fmt, ## __VA_ARGS__)
# define errc(fmt, ...)  _verr(1, errno, fmt, ## __VA_ARGS__)
# define warnc(fmt, ...) _verr(0, errno, fmt, ## __VA_ARGS__)
# define warn(fmt, ...)  _verr(0, 0, fmt, ## __VA_ARGS__)

void _verr(int, int, const char *, ...);

// TODO: move it to a separated file
int parse_long(const char *, long *);

const engine_t *get_default_engine(void);
const engine_t *get_engine_by_name(const char *);

#endif /* !X_ENGINE_H */
