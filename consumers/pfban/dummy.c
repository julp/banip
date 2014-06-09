#include <stdlib.h>
#include <stdio.h>

#include "common.h"
#include "engine.h"

static void *dummy_open(void)
{
    return NULL;
}

static int dummy_handle(void *UNUSED(ctxt), const char *UNUSED(tablename), const char *buffer)
{
    fprintf(stderr, "Received: '%s'\n", buffer);

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
