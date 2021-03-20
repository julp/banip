#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "error.h"

void _error_set(char **error, const char *format, ...)
{
    if (NULL != error) {
        int len;
        va_list ap;

        if (NULL != *error) {
            fprintf(stderr, "Warning: overwrite attempt of a previous error: %s\n", *error);
            free(error);
        }
        va_start(ap, format);
        len = vsnprintf(NULL, 0, format, ap);
        va_end(ap);
        if (len >= 0) {
            int chk, size;

            size = len + 1;
            va_start(ap, format);
            *error = malloc(size * sizeof(**error));
            chk = vsnprintf(*error, size, format, ap);
            assert(chk >= 0 && chk == len);
            va_end(ap);
        }
    }
}

void error_free(char **error)
{
    assert(NULL != error);

    free(*error);
    *error = NULL;
}
