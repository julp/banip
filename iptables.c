#include <stdlib.h>
#include <stdio.h>

#include "common.h"
#include "command.h"
#include "engine.h"

static int iptables_handle(void *UNUSED(ctxt), const char *tablename, addr_t addr)
{
    return run_command("iptables -I %s 1 -s %s -j DROP", tablename, addr.humanrepr);
}

const engine_t iptables_engine = {
    false,
    "iptables",
    NULL,
    iptables_handle,
    NULL
};
