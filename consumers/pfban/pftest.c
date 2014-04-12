#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "engine.h"

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
    const engine_t *engine;

    if (argc > 1) {
        engine = get_engine_by_name(argv[1]);
    } else {
        engine = get_default_engine();
    }
    if (NULL == engine) {
        errx("no engine found");
    }

    ctxt = engine->open();
    engine->handle(ctxt, "blacklist", "1.2.3.4");
    engine->close(ctxt);
    free(ctxt);

    return EXIT_SUCCESS;
}
