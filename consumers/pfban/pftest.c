#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
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
    engine_t *engine;

#ifdef WITH_PF
//     register_engine(&pf_engine);
    engine = &pf_engine;
#endif /* PF */

#ifdef WITH_NPF
//     register_engine(&npf_engine);
    engine = &npf_engine;
#endif /* NPF */

#ifdef WITH_NFTABLES
//     register_engine(&nftables_engine);
    engine = &nftables_engine;
#endif /* NFTABLES */

    ctxt = engine->open();
    engine->handle(ctxt, "blacklist", "1.2.3.4");
    engine->close(ctxt);
    free(ctxt);

    return EXIT_SUCCESS;
}
