#ifndef ERR_H

# define ERR_H

# include <errno.h>
# define errx(fmt, ...)  _verr(1, 0, fmt, ## __VA_ARGS__)
# define errc(fmt, ...)  _verr(1, errno, fmt, ## __VA_ARGS__)
# define warnc(fmt, ...) _verr(0, errno, fmt, ## __VA_ARGS__)
# define warn(fmt, ...)  _verr(0, 0, fmt, ## __VA_ARGS__)

void _verr(int, int, const char *, ...);

#endif /* !ERR_H */
