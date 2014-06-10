#include <string.h>

#include "engine.h"

#ifdef WITH_PF
extern engine_t pf_engine;
#endif /* PF */

#ifdef WITH_NPF
extern engine_t npf_engine;
#endif /* NPF */

#ifdef WITH_NFTABLES
extern engine_t nftables_engine;
#endif /* NFTABLES */

extern engine_t dummy_engine;

static const engine_t *available_engines[] = {
#ifdef WITH_PF
    &pf_engine,
#endif /* PF */
#ifdef WITH_NPF
    &npf_engine,
#endif /* NPF */
#ifdef WITH_NFTABLES
    &nftables_engine,
#endif /* NFTABLES */
    &dummy_engine,
    NULL
};

// TODO: move it to a separated file
#include <stdlib.h>
#include <limits.h>
int parse_long(const char *str, long *val)
{
    char *endptr;

    *val = strtol(str, &endptr, 10);
    if ((ERANGE == errno && (LONG_MAX == *val || LONG_MIN == *val)) || (0 != errno && 0 == *val)) {
        errx("overflow or underflow for '%s'", str);
        return 0;
    }
    if (endptr == str) {
        errx("number expected, no digit found");
        return 0;
    }
    if ('\0' != *endptr) {
        errx("number expected, non digit found %c in %s", *endptr, str);
        return 0;
    }
    if (*val <= 0) {
        errx("number should be greater than 0, got %ld", *val);
        return 0;
    }

    return 1;
}

const engine_t *get_default_engine(void)
{
    /* NULL can be returned, caller is responsible to check this later */
    return available_engines[0];
}

const engine_t *get_engine_by_name(const char *name)
{
    const engine_t **e;

    for (e = available_engines; NULL != *e; e++) {
        if (0 == strcmp((*e)->name, name)) {
            return *e;
        }
    }

    return NULL;
}
