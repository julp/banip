#include <stdlib.h>
#include <stdio.h>

#include "common.h"
#include "engine.h"

static bool dummy_handle(void *UNUSED(ctxt), const char *UNUSED(tablename), addr_t addr, char **UNUSED(error))
{
    fprintf(stderr, "Received: '%s'\n", addr.humanrepr);

    return true;
}

const engine_t dummy_engine = {
    true,
    "dummy",
    NULL,
    dummy_handle,
    NULL
};
