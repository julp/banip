#include <stdlib.h>

#include "engine.h"

#ifndef __has_attribute
# define __has_attribute(x) 0
#endif /* !__has_attribute */

#if __GNUC__ || __has_attribute(unused)
# define UNUSED(x) UNUSED_ ## x __attribute__((unused))
#else
# define UNUSED
#endif /* UNUSED */

static void *dummy_open(void)
{
    return NULL;
}

static int dummy_handle(void *UNUSED(ctxt), const char *UNUSED(tablename), const char *UNUSED(buffer))
{
    return 1;
}

static void dummy_close(void *UNUSED(ctxt))
{
}

const engine_t dummy_engine = {
    "dummy",
    dummy_open,
    dummy_handle,
    dummy_close
};
