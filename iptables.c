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

static int iptables_handle(void *UNUSED(ctxt), const char *tablename, addr_t addr)
{
    return _run_command("iptables -I %s 1 -s %s -j DROP", tablename, addr.humanrepr);
}

const engine_t iptables_engine = {
    "iptables",
    NULL,
    iptables_handle,
    NULL
};
