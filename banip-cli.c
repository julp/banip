#include <stdlib.h>
#include <stdio.h>

#include "common.h"
#include "queue.h"

void _verr(bool fatal, int errcode, const char *fmt, ...)
{
    va_list ap;

    if (NULL != fmt) {
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
        if (errcode) {
            fprintf(stderr, ": ");
        }
    }
    if (errcode) {
        fputs(strerror(errcode), stderr);
    }
    fprintf(stderr, "\n");
    if (fatal) {
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char **argv)
{
    int status;
    void *queue;
    char *error;

    error = NULL;
    queue = NULL;
    status = EXIT_FAILURE;
    do {

        if (argc != 3) {
            fprintf(stderr, "expected arguments are: 1) queue path/name ; 2) message to send\n");
            break;
        }
        if (NULL == (queue = queue_init(&error))) {
            break;
        }
        if (!queue_open(queue, argv[1], QUEUE_FL_SENDER, &error)) {
            break;
        }
        if (queue_send(queue, argv[2], -1, &error)) {
            printf("OK\n");
        }
        status = EXIT_SUCCESS;
    } while (false);
    if (NULL != queue) {
        queue_close(&queue, &error);
    }
    if (NULL != error) {
        fprintf(stderr, "%s\n", error);
        error_free(&error);
    }

    return status;
}
