#ifndef ENGINE_H

# define ENGINE_H

# include "common.h"
# include "parse.h"

typedef struct {
    bool drop_privileges;
    const char * const name;
    void *(*open)(const char *);
//     int (*getopt)(void *, int, const char *);
    int (*handle)(void *, const char *, addr_t);
    void (*close)(void *);
} engine_t;

const engine_t *get_default_engine(void);
const engine_t *get_engine_by_name(const char *);

#endif /* !ENGINE_H */
