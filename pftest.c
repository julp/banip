#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "err.h"
#include "engine.h"

#define TABLENAME "blacklist"

void _verr(bool fatal, int errcode, const char *fmt, ...)
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
    int status;
    void *ctxt;
    char *error;
    const engine_t *engine;

    error = NULL;
    ctxt = NULL;
    status = EXIT_FAILURE;
    do {
        addr_t addr;

        if (argc > 1) {
            if (NULL == (engine = get_engine_by_name(argv[1]))) {
                set_generic_error(&error, "unknown engine '%s'", argv[1]);
                break;
            }
        } else {
            if (NULL == (engine = get_default_engine())) {
                set_generic_error(&error, "no engine available for your system");
                break;
            }
        }
        if (NULL != engine->open) {
            if (NULL == (ctxt = engine->open(TABLENAME, &error))) {
                break;
            }
        }
        if (!parse_addr("1.2.3.4", &addr, &error)) {
            break;
        }
        if (!engine->handle(ctxt, TABLENAME, addr, &error)) {
            break;
        }
        status = EXIT_SUCCESS;
    } while (false);
    if (NULL != error) {
        fprintf(stderr, "%s\n", error);
        error_free(&error);
    }
    if (NULL != ctxt) {
        if (NULL != engine->close) {
            engine->close(ctxt);
        }
        if (NULL != ctxt) {
            free(ctxt);
        }
    }

    return status;
}
