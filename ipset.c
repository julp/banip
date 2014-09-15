#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "common.h"
#include "engine.h"

static int _run_command(const char *format, ...)
{
    int l;
    va_list ap;
    char buffer[4096];

    va_start(ap, format);
    l = vsnprintf(buffer, STR_SIZE(buffer), format, ap);
    va_end(ap);
    if (l >= STR_SIZE(buffer)) {
        // error: overflow
        return 0;
    }

    return system(buffer);
}

static void *ipset_open(const char *tablename)
{
    _run_command("ipset create %s4 hash:net family inet", tablename);
    _run_command("ipset create %s6 hash:net family inet6", tablename);

    return NULL;
}

static int ipset_handle(void *UNUSED(ctxt), const char *tablename, addr_t addr)
{
    return _run_command("ipset -A %s%c %s", tablename, addr.fa == AF_INET ? '4' : '6', addr.humanrepr);
}

const engine_t ipset_engine = {
    "ipset",
    ipset_open,
    ipset_handle,
    NULL
};
