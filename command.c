#include <stdlib.h>
#include <stdio.h>

#include "common.h"
#include "command.h"

int run_command(char **error, const char *format, ...)
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
