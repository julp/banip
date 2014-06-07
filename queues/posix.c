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
#ifdef WITH_BUFFER
    char *buffer;
#endif
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
#ifdef WITH_BUFFER
    q->filename = q->buffer = NULL;
#else
    q->filename = NULL;
#endif
    /* default hardcoded values on FreeBSD (/usr/src/sys/kern/uipc_mqueue.c) */
    q->attr.mq_maxmsg = 10;
    q->attr.mq_msgsize = 1024;

    return q;
}

queue_err_t queue_set_attribute(void *p, queue_attr_t attr, unsigned long value)
{
    posix_queue_t *q;

    q = (posix_queue_t *) p;
    if (NULL == q->filename) {
        return QUEUE_ERR_NOT_OWNER;
    } else {
        switch (attr) {
            case QUEUE_ATTR_MAX_MESSAGE_SIZE:
                q->attr.mq_msgsize = value;
                break;
            case QUEUE_ATTR_MAX_MESSAGE_IN_QUEUE:
                q->attr.mq_maxmsg = value;
                break;
            default:
                return QUEUE_ERR_NOT_SUPPORTED;
        }
    }

    return QUEUE_ERR_OK;
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
            // TODO: error
            return QUEUE_ERR_GENERAL_FAILURE;
        }
    }
    if (NOT_MQD_T == (q->mq = mq_open(filename, omask, 0420, &q->attr))) {
        if (HAS_FLAG(flags, QUEUE_FL_OWNER)) {
            umask(oldmask);
#ifdef __FreeBSD__
            if (ENOSYS == errno) {
                // please load mqueuefs module with kldload or recompile your kernel to include "options P1003_1B_MQUEUE"
            }
#endif
        }
        // TODO: error
        return QUEUE_ERR_GENERAL_FAILURE;
    }
    if (HAS_FLAG(flags, QUEUE_FL_OWNER)) {
        umask(oldmask);
    } else {
        if (0 != mq_getattr(q->mq, &q->attr)) {
            // TODO: error
            return QUEUE_ERR_GENERAL_FAILURE;
        }
    }
#ifdef WITH_BUFFER
    if (!HAS_FLAG(flags, QUEUE_FL_SENDER)) {
        if (NULL == (q->buffer = calloc(q->attr.mq_msgsize + 1, sizeof(*q->buffer)))) {
            // TODO: error
            return QUEUE_ERR_GENERAL_FAILURE;
        }
    }
#endif

    return QUEUE_ERR_OK;
}

queue_err_t queue_get_attribute(void *p, queue_attr_t attr, unsigned long *value)
{
    posix_queue_t *q;

    q = (posix_queue_t *) p;
    switch (attr) {
        case QUEUE_ATTR_MAX_MESSAGE_SIZE:
            *value = q->attr.mq_msgsize;
            break;
        case QUEUE_ATTR_MAX_MESSAGE_IN_QUEUE:
            *value = q->attr.mq_maxmsg;
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

    return mq_receive(q->mq, buffer, buffer_size, NULL);
}

queue_err_t queue_send(void *p, const char *msg, int msg_len)
{
    long msg_size;
    posix_queue_t *q;

    q = (posix_queue_t *) p;
    if (msg_len < 0) {
        msg_size = strlen(msg) + 1;
    } else {
        msg_size = msg_len + 1;
    }
    if (0 == mq_send(q->mq, msg, msg_size, 0)) {
        return QUEUE_ERR_OK;
    } else {
        // TODO: error
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
                // TODO: error
                return QUEUE_ERR_GENERAL_FAILURE;
            }
            q->mq = NOT_MQD_T;
        }
        if (NULL != q->filename) { // we are the owner
            if (0 != mq_unlink(q->filename)) {
                // TODO: error
                return QUEUE_ERR_GENERAL_FAILURE;
            }
            free(q->filename);
            q->filename = NULL;
        }
#ifdef WITH_BUFFER
        if (NULL != q->buffer) {
            free(q->buffer);
            q->buffer = NULL;
        }
#endif
        free(*p);
        *p = NULL;
    }

    return QUEUE_ERR_OK;
}
