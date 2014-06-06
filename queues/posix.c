#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <mqueue.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "common.h"
#include "queue.h"

#define NOT_MQD_T ((mqd_t) -1)

typedef struct {
    mqd_t mq;
    char *buffer;
    char *filename;
    struct mq_attr attr;
} posix_queue_t;

void *queue_init(void)
{
    posix_queue_t *q;

    if (NULL == (q = malloc(sizeof(*q)))) {
        return NULL;
    }
    q->mq = NOT_MQD_T;
    q->filename = q->buffer = NULL;
    /* default hardcoded values on FreeBSD (/usr/src/sys/kern/uipc_mqueue.c) */
    q->attr.mq_maxmsg = 10;
    q->attr.mq_msgsize = 1024;

    return q;
}

queue_err_t queue_set_attribute(void *p, queue_attr_t attr, unsigned long value)
{
    //
}

queue_err_t queue_open(void *p, const char *filename, int flags)
{
    int omask;
    mode_t oldmask;
    posix_queue_t *q;

    q = (posix_queue_t *) p;
    if (HAS_FLAG(flags, QUEUE_FL_SENDER)) {
        omask = O_WRONLY;
    } else {
        omask = O_RDONLY;
    }
    if (HAS_FLAG(flags, QUEUE_FL_OWNER)) {
        omask |= O_CREAT | O_EXCL;
        oldmask = umask(0);
        if (NULL == (q->filename = strdup(filename))) {
            //
            return QUEUE_ERR_GENERAL_FAILURE;
        }
    }
    if (NOT_MQD_T == (q->mq = mq_open(filename, omask, 0420, &q->attr))) {
        if (HAS_FLAG(flags, QUEUE_FL_OWNER)) {
            umask(oldmask);
        }
        //
        return QUEUE_ERR_GENERAL_FAILURE;
    }
    if (HAS_FLAG(flags, QUEUE_FL_OWNER)) {
        umask(oldmask);
    }
/*
    if (0 != mq_getattr(mq, &attr)) {
        errc("mq_getattr failed");
    }
    if (NULL == (buffer = calloc(attr.mq_msgsize + 1, sizeof(*buffer)))) {
        errx("calloc failed");
    }
*/
    if (!HAS_FLAG(flags, QUEUE_FL_SENDER)) {
        if (NULL == (q->buffer = malloc(sizeof(*q->buffer)))) {
            //
            return QUEUE_ERR_GENERAL_FAILURE;
        }
    }

    return QUEUE_ERR_OK;
}

queue_err_t queue_get_attribute(void *p, queue_attr_t attr, unsigned long *value)
{
    switch (attr) {
        case QUEUE_ATTR_X:
            break;
        case QUEUE_ATTR_Y:
            break;
        default:
            return QUEUE_ERR_NOT_SUPPORTED;
    }

    return QUEUE_ERR_OK;
}

int queue_receive(void *p, char *buffer, size_t buffer_size)
{
    posix_queue_t *q;

    q = (posix_queue_t *) p;

    return mq_receive(q->mq, q->buffer, /*(size_t)*/ q->attr.mq_msgsize, NULL);
}

queue_err_t queue_send(void *p, const char *msg, int msg_len)
{
    long msg_size;

    if (msg_len < 0) {
        msg_size = strlen(msg) + 1;
    } else {
        msg_size = msg_len + 1;
    }
    if (0 == mq_send(q->mq, msg, msg_size, 0)) {
        return QUEUE_ERR_OK;
    } else {
        //
        return QUEUE_ERR_GENERAL_FAILURE;
    }
}

queue_err_t queue_close(void **p)
{
    if (NULL != *p) {
        posix_queue_t *q;

        q = (posix_queue_t *) *p;
        if (NOT_MQD_T != q->mq) {
            if (0 != mq_close(q->mq)) {
                //
                return QUEUE_ERR_GENERAL_FAILURE;
            }
            q->mq = NOT_MQD_T;
        }
        if (NULL != q->filename) { // we are the owner
            if (0 != mq_unlink(q->filename)) {
                //
                return QUEUE_ERR_GENERAL_FAILURE;
            }
            free(q->filename);
            q->filename = NULL;
        }
        if (NULL != q->buffer) {
            free(q->buffer);
            q->buffer = NULL;
        }
        free(*p);
        *p = NULL;
    }

    return QUEUE_ERR_OK;
}
