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
//     key_t key;
    char *buffer;
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

queue_err_t queue_set_attribute(void *q, queue_attr_t attr, unsigned long value)
{
    //
}

queue_err_t queue_open(void *p, const char *name, int flags)
{
    int id;
    FILE *fp;
    key_t key;
    mode_t oldmask;
    systemv_queue_t *q;
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
            if (strlcpy(filename, s - name, STR_SIZE(filename)) >= STR_SIZE(filename)) {
                errno = E2BIG;
                return QUEUE_ERR_GENERAL_FAILURE;
            }
        }
        oldmask = umask(0);
        if (NULL == (fp = fopen(filename, "wx"))) {
            umask(oldmask);
            //
            return QUEUE_ERR_GENERAL_FAILURE;
        }
        umask(oldmask);
        if (fwrite(&id, sizeof(id), 1, fp) < sizeof(id)) {
            //
            return QUEUE_ERR_GENERAL_FAILURE;
        }
        fflush(fp);
    } else {
        if (NULL == (fp = fopen(filename, "r"))) {
            //
            return QUEUE_ERR_GENERAL_FAILURE;
        }
        if (fread(&id, sizeof(id), 1, fp) < sizeof(id)) {
            //
            return QUEUE_ERR_GENERAL_FAILURE;
        }
    }
    fclose(fp);
    if (-1 == (key = ftok(filename, id))) {
        // errno is not set
        return QUEUE_ERR_GENERAL_FAILURE;
    }
    if (HAS_FLAG(flags, QUEUE_FL_OWNER)) {
//         q->filename = strdup(filename);
        oldmask = umask(0);
        q->qid = msgget(key, 0420 | IPC_CREAT | IPC_EXCL);
        umask(oldmask);
    } else {
        q->qid = msgget(key, 0420);
    }
    if (-1 == q->qid) {
        //
        return QUEUE_ERR_GENERAL_FAILURE;
    }
    // TODO: q->X = Y + buffer allocation before (for both)

    return QUEUE_ERR_OK;
}

queue_err_t queue_get_attribute(void *p, queue_attr_t attr, unsigned long *value)
{
    systemv_queue_t *q;

    q = (systemv_queue_t *) p;
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
    systemv_queue_t *q;

    q = (systemv_queue_t *) p;

    return msgrcv(q->qid, q->buffer, q->buffer_size, 0, 0);
}

queue_err_t queue_send(void *p, const char *msg, int msg_len)
{
    systemv_queue_t *q;

    q = (systemv_queue_t *) p;
    if (0 == msgsnd(q->qid, q->buffer, q->buffer_size,  0)) {
        return QUEUE_ERR_OK;
    } else {
        //
        return QUEUE_ERR_GENERAL_FAILURE;
    }
}

queue_err_t queue_close(void **p)
{
    systemv_queue_t *q;

    q = (systemv_queue_t *) *p;
    if (NULL != q->filename) { // we are the owner
        if (0 != msgctl(q->qid, IPC_RMID, NULL)) {
            //
            return QUEUE_ERR_GENERAL_FAILURE;
        }
        q->qid = -1;
        if (0 != unlink(q->filename)) {
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

    return QUEUE_ERR_OK;
}
