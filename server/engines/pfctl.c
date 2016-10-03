#include <stdlib.h>
#include <stdio.h>

#include "config.h"
#include "common.h"
#include "command.h"
#include "engine.h"

static bool pfctl_handle(void *UNUSED(ctxt), const char *tablename, addr_t addr, char **error)
{
	return run_command(error, "%s -t %s -T add %s", PFCTL_EXECUTABLE, tablename, addr.humanrepr) && run_command(error, "%s -k 0.0.0.0/0 -k %s", PFCTL_EXECUTABLE, addr.humanrepr);
}

const engine_t pfctl_engine = {
	false,
	"pfctl",
	NULL,
	pfctl_handle,
	NULL
};
