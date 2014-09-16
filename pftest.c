#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "err.h"
#include "engine.h"

#define TABLENAME "blacklist"

void _verr(int fatal, int errcode, const char *fmt, ...)
{
    va_list ap;

    if (NULL != fmt) {
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
        if (errcode) {
            fprintf(stderr, ": ");
        }
    }
    if (errcode) {
        fputs(strerror(errcode), stderr);
    }
    fprintf(stderr, "\n");
    if (fatal) {
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char **argv)
{
    void *ctxt;
    addr_t addr;
    const engine_t *engine;

    ctxt = NULL;
    if (argc > 1) {
        if (NULL == (engine = get_engine_by_name(argv[1]))) {
            errx("unknown engine '%s'", argv[1]);
        }
    } else {
        if (NULL == (engine = get_default_engine())) {
            errx("no engine available for your system");
        }
    }
    if (NULL != engine->open) {
        ctxt = engine->open(TABLENAME);
    }
    parse_addr("1.2.3.4", &addr);
    engine->handle(ctxt, TABLENAME, addr);
    if (NULL != engine->close) {
        engine->close(ctxt);
    }
    if (NULL != ctxt) {
        free(ctxt);
    }

    return EXIT_SUCCESS;
}
