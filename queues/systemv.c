#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/param.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "common.h"
#include "queue.h"

/*
struct msgbuf {
    long mtype;
    char mtext[1];
};
*/

typedef struct {
    int qid;
    char *buffer; /* emulate a struct *msgbuf, the real buffer to read or write is the mtext field, ie buffer + sizeof(long) */
    char *filename;
    size_t buffer_size;
} systemv_queue_t;

void *queue_init(void)
{
    systemv_queue_t *q;

    if (NULL == (q = malloc(sizeof(*q)))) {
        return NULL;
    }
    q->qid = -1;
    q->buffer_size = 0;
    q->filename = q->buffer = NULL;

    return q;
}

queue_err_t queue_set_attribute(void *p, queue_attr_t attr, unsigned long value)
{
    systemv_queue_t *q;

    q = (systemv_queue_t *) p;
    if (NULL == q->filename) {
        return QUEUE_ERR_NOT_OWNER;
    } else {
#if 0
        struct msqid_ds buf;

        if (0 != msgctl(q->qid, IPC_STAT, &buf)) {
            return QUEUE_ERR_GENERAL_FAILURE;
        }
        buf.X = Y;
        if (0 != msgctl(q->qid, IPC_SET, &buf)) {
            return QUEUE_ERR_GENERAL_FAILURE;
        }
#endif
        return QUEUE_ERR_NOT_SUPPORTED;
    }

    return QUEUE_ERR_OK;
}

queue_err_t queue_open(void *p, const char *name, int flags)
{
    int id;
    FILE *fp;
    key_t key;
    mode_t oldmask;
    systemv_queue_t *q;
    struct msqid_ds buf;
    char *s, filename[MAXPATHLEN];

    q = (systemv_queue_t *) p;
    if (HAS_FLAG(flags, QUEUE_FL_OWNER)) {
        if (NULL == (s = strchr(name, ':')) || '\0' == *s) {
            id = 'b';
            if (strlcpy(filename, name, STR_SIZE(filename)) >= STR_SIZE(filename)) {
                errno = E2BIG;
                return QUEUE_ERR_GENERAL_FAILURE;
            }
        } else {
            id = s[1];
            if (s - name >= STR_SIZE(filename)) {
                errno = E2BIG;
                return QUEUE_ERR_GENERAL_FAILURE;
            }
            strncpy(filename, name, s - name);
        }
        if (NULL == (fp = fopen(filename, "wx"))) {
            // TODO: error
            return QUEUE_ERR_GENERAL_FAILURE;
        }
        if (1 != fwrite(&id, sizeof(id), 1, fp)) {
            // TODO: error
            return QUEUE_ERR_GENERAL_FAILURE;
        }
        fflush(fp);
    } else {
        if (strlcpy(filename, name, STR_SIZE(filename)) >= STR_SIZE(filename)) {
            errno = E2BIG;
            return QUEUE_ERR_GENERAL_FAILURE;
        }
        if (NULL == (fp = fopen(filename, "r"))) {
            // TODO: error
            return QUEUE_ERR_GENERAL_FAILURE;
        }
        if (1 != fread(&id, sizeof(id), 1, fp) < sizeof(id)) {
            // TODO: error
            return QUEUE_ERR_GENERAL_FAILURE;
        }
    }
    fclose(fp);
    if (-1 == (key = ftok(filename, id))) {
        // NOTE: errno is not set by ftok
        // TODO: error
        return QUEUE_ERR_GENERAL_FAILURE;
    }
    if (HAS_FLAG(flags, QUEUE_FL_OWNER)) {
        if (NULL == (q->filename = strdup(filename))) {
            // TODO: error
            return QUEUE_ERR_GENERAL_FAILURE;
        }
        oldmask = umask(0);
        q->qid = msgget(key, 0660 | IPC_CREAT | IPC_EXCL);
        umask(oldmask);
    } else {
        q->qid = msgget(key, 0660);
    }
    if (-1 == q->qid) {
        // TODO: error
        return QUEUE_ERR_GENERAL_FAILURE;
    }
    if (0 != msgctl(q->qid, IPC_STAT, &buf)) {
        // TODO: error
        return QUEUE_ERR_GENERAL_FAILURE;
    }
    q->buffer_size = buf.msg_qbytes / 8;
    if (NULL == (q->buffer = malloc(sizeof(long) + sizeof(q->buffer) * q->buffer_size))) {
        // TODO: error
        return QUEUE_ERR_GENERAL_FAILURE;
    }
    *(long *) q->buffer = 1; /* mtype is an integer greater than 0 */

    return QUEUE_ERR_OK;
}

queue_err_t queue_get_attribute(void *p, queue_attr_t attr, unsigned long *value)
{
    systemv_queue_t *q;

    q = (systemv_queue_t *) p;
    switch (attr) {
        case QUEUE_ATTR_MAX_QUEUE_SIZE:
        case QUEUE_ATTR_MAX_MESSAGE_SIZE:
        {
            struct msqid_ds buf;

            if (0 != msgctl(q->qid, IPC_STAT, &buf)) {
                return QUEUE_ERR_GENERAL_FAILURE;
            }
            *value = buf.msg_qbytes;
            break;
        }
        default:
            return QUEUE_ERR_NOT_SUPPORTED;
    }

    return QUEUE_ERR_OK;
}

int queue_receive(void *p, char *buffer, size_t buffer_size)
{
    int read;
    systemv_queue_t *q;

    q = (systemv_queue_t *) p;

    if (-1 != (read = msgrcv(q->qid, q->buffer, buffer_size, 0, 0))) { // TODO: min(buffer_size, q->buffer_size) ?
        strcpy(buffer, q->buffer + sizeof(long)); // TODO: better ?
    }

    return read;
}

queue_err_t queue_send(void *p, const char *msg, int msg_len)
{
    systemv_queue_t *q;

    q = (systemv_queue_t *) p;
    if (msg_len < 0) {
        msg_len = strlen(msg);
    }
    strcpy(q->buffer + sizeof(long), msg); // TODO: safer
    if (0 == msgsnd(q->qid, q->buffer, q->buffer_size,  0)) {
        return QUEUE_ERR_OK;
    } else {
        // TODO: error
        return QUEUE_ERR_GENERAL_FAILURE;
    }
}

queue_err_t queue_close(void **p)
{
    if (NULL != *p) {
        systemv_queue_t *q;

        q = (systemv_queue_t *) *p;
        if (NULL != q->filename) { // we are the owner
            if (0 != msgctl(q->qid, IPC_RMID, NULL)) {
                // TODO: error
                return QUEUE_ERR_GENERAL_FAILURE;
            }
            q->qid = -1;
            if (0 != unlink(q->filename)) {
                // TODO: error
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
