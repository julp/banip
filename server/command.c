#include <stdlib.h>
#include <stdio.h>

#include "common.h"
#include "command.h"

// TODO: system is NOT safe and sanitize arguments first? (imply to change run_command("command %s", IP) to run_command({ "command", IP })?)
bool run_command(char **UNUSED(error), const char *format, ...)
{
    int l;
    va_list ap;
    char buffer[4096];

    va_start(ap, format);
    l = vsnprintf(buffer, STR_SIZE(buffer), format, ap);
    va_end(ap);
    if (l >= STR_SIZE(buffer)) {
        // error: overflow
        return false;
    }

    return 0 == system(buffer);
}
