#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "common.h"
#include "engine.h"

static void *ipset_open(const char *tablename)
{
    run_command("ipset -! create %s4 hash:net family inet", tablename);
    run_command("ipset -! create %s6 hash:net family inet6", tablename);
    run_command("iptables -I INPUT -m set --match-set %s4 src -j DROP", tablename);
    run_command("iptables -I INPUT -m set --match-set %s4 src -j DROP", tablename);

    return NULL;
}

static int ipset_handle(void *UNUSED(ctxt), const char *tablename, addr_t addr)
{
    return run_command("ipset -! -A %s%c %s", tablename, addr.fa == AF_INET ? '4' : '6', addr.humanrepr);
}

const engine_t ipset_engine = {
    false,
    "ipset",
    ipset_open,
    ipset_handle,
    NULL
};
