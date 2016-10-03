#include <stdlib.h>
#include <stdio.h>

#include "config.h"
#include "common.h"
#include "command.h"
#include "engine.h"

static bool npfctl_handle(void *UNUSED(ctxt), const char *tablename, addr_t addr, char **error)
{
	return run_command(error, "%s table %s add %s", NPFCTL_EXECUTABLE, tablename, addr.humanrepr);
}

const engine_t npfctl_engine = {
	false,
	"npfctl",
	NULL,
	npfctl_handle,
	NULL
};
