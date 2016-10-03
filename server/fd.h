#pragma once

#include <stdbool.h>
#include <stdio.h>

typedef struct fd_t fd_t;

void fd_close(fd_t *);
FILE *fd_reopen(fd_t *, char **);
fd_t *fd_wrap(const char *, int, bool, char **);

#if 0
const char *fd_dirname(fd_t *);
const char *fd_basename(fd_t *);
#endif
