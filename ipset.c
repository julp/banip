#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "common.h"
#include "engine.h"

static void *ipset_open(const char *tablename, char **error)
{
    bool ok;

    ok = false;
    do {
        if (!(ok &= (EXIT_SUCCESS != run_command(error, "ipset -! create %s4 hash:net family inet", tablename)))) {
            break;
        }
        if (!(ok &= (EXIT_SUCCESS != run_command(error, "ipset -! create %s6 hash:net family inet6", tablename)))) {
            break;
        }
        if (!(ok &= (EXIT_SUCCESS != run_command(error, "iptables -I INPUT -m set --match-set %s4 src -j DROP", tablename)))) {
            break;
        }
        if (!(ok &= (EXIT_SUCCESS != run_command(error, "iptables -I INPUT -m set --match-set %s4 src -j DROP", tablename)))) {
            break;
        }
        ok = true;
    } while (false);

    return (void *) !ok;
}

static bool ipset_handle(void *UNUSED(ctxt), const char *tablename, addr_t addr, char **error)
{
    return EXIT_SUCCESS == run_command(error, "ipset -! -A %s%c %s", tablename, addr.fa == AF_INET ? '4' : '6', addr.humanrepr);
}

const engine_t ipset_engine = {
    false,
    "ipset",
    ipset_open,
    ipset_handle,
    NULL
};
