#include <stdlib.h>
#include <stdio.h>

#include "config.h"
#include "common.h"
#include "command.h"
#include "engine.h"

static bool iptables_handle(void *UNUSED(ctxt), const char *tablename, addr_t addr, char **error)
{
	return run_command(error, "%s -I %s 1 -s %s -j DROP", IPTABLES_EXECUTABLE, tablename, addr.humanrepr);
}

const engine_t iptables_engine = {
	false,
	"iptables",
	NULL,
	iptables_handle,
	NULL
};
