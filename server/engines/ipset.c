#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "config.h"
#include "common.h"
#include "engine.h"

static void *ipset_open(const char *tablename, char **error)
{
	run_command(error, "%s -! create %s4 hash:net family inet", IPSET_EXECUTABLE, tablename)
		&& run_command(error, "%s -! create %s6 hash:net family inet6", IPSET_EXECUTABLE, tablename)
		&& run_command(error, "%s -I INPUT -m set --match-set %s4 src -j DROP", IPTABLES_EXECUTABLE, tablename)
		&& run_command(error, "%s -I INPUT -m set --match-set %s4 src -j DROP", IPTABLES_EXECUTABLE, tablename)
	;

    return NULL;
}

static bool ipset_handle(void *UNUSED(ctxt), const char *tablename, addr_t addr, char **error)
{
	return run_command(error, "%s -! -A %s%c %s", IPSET_EXECUTABLE, tablename, addr.fa == AF_INET ? '4' : '6', addr.humanrepr);
}

const engine_t ipset_engine = {
	false,
	"ipset",
	ipset_open,
	ipset_handle,
	NULL
};
