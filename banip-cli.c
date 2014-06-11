#include <stdlib.h>
#include <stdio.h>

#include "queue.h"

int main(int argc, char **argv)
{
    void *queue;

    if (argc != 3) {
        printf("expected arguments are: 1) queue path/name ; 2) message to send\n");
        return EXIT_FAILURE;
    }
    if (NULL == (queue = queue_init())) {
        printf("queue_init failed\n");
        return EXIT_FAILURE;
    }
    if (QUEUE_ERR_OK != queue_open(queue, argv[1], QUEUE_FL_SENDER)) {
        printf("queue_open failed\n");
        queue_close(&queue);
        return EXIT_FAILURE;
    }
    if (QUEUE_ERR_OK == queue_send(queue, argv[2], -1)) {
        printf("OK\n");
    } else {
        printf("queue_send failed\n");
    }
    queue_close(&queue);

    return EXIT_SUCCESS;
}
