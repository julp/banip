#include <stdlib.h>
#include <stdio.h>

#include "common.h"
#include "engine.h"

static int dummy_handle(void *UNUSED(ctxt), const char *UNUSED(tablename), addr_t addr)
{
    fprintf(stderr, "Received: '%s'\n", addr.humanrepr);

    return 1;
}

const engine_t dummy_engine = {
    "dummy",
    NULL,
    dummy_handle,
    NULL
};
