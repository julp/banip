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
#include <unistd.h> /* unlink */

#include "config.h"
#include "common.h"
#include "queue.h"
#include "capsicum.h"

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

void *queue_init(char **error)
{
    systemv_queue_t *q;

    if (NULL == (q = malloc(sizeof(*q)))) {
        set_malloc_error(error, sizeof(*q));
    } else {
        q->qid = -1;
        q->buffer_size = 0;
        q->filename = q->buffer = NULL;
    }

    return q;
}

queue_err_t queue_set_attribute(void *p, queue_attr_t UNUSED(attr), unsigned long UNUSED(value))
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

bool queue_open(void *p, const char *name, int flags, char **error)
{
    bool ok;

    ok = false;
    do {
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
                    set_buffer_overflow_error(error, name, filename, STR_SIZE(filename));
                    break;
                }
            } else {
                id = s[1];
                if (s - name >= STR_SIZE(filename)) {
                    set_buffer_overflow_error(error, name, filename, STR_SIZE(filename));
                    break;
                }
                strncpy(filename, name, s - name);
            }
            if (NULL == (fp = fopen(filename, "wx"))) {
                set_system_error(error, "fopen(\"%s\", \"wx\") failed", filename);
                break;
            }
            if (1 != fwrite(&id, sizeof(id), 1, fp)) {
                set_system_error(error, "fwrite to '%s' failed", filename);
                break;
            }
            fflush(fp);
        } else {
            if (strlcpy(filename, name, STR_SIZE(filename)) >= STR_SIZE(filename)) {
                set_buffer_overflow_error(error, name, filename, STR_SIZE(filename));
                break;
            }
            if (NULL == (fp = fopen(filename, "r"))) {
                set_system_error(error, "fopen(\"%s\", \"r\") failed", filename);
                break;
            }
            if (1 != fread(&id, sizeof(id), 1, fp) < sizeof(id)) {
                set_system_error(error, "fread from '%s' failed", filename);
                break;
            }
        }
        fclose(fp);
        if (-1 == (key = ftok(filename, id))) {
            // NOTE: errno is not set by ftok
            set_generic_error(error, "ftok(\"%s\", %d) failed", filename, id);
            break;
        }
        if (HAS_FLAG(flags, QUEUE_FL_OWNER)) {
            if (NULL == (q->filename = strdup(filename))) {
                set_generic_error(error, "strdup failed to copy \"%s\"", filename);
                break;
            }
            oldmask = umask(0);
            q->qid = msgget(key, 0660 | IPC_CREAT | IPC_EXCL);
            umask(oldmask);
        } else {
            q->qid = msgget(key, 0660);
        }
        if (-1 == q->qid) {
            set_system_error(error, "msgget failed");
            break;
        }
        if (!HAS_FLAG(flags, QUEUE_FL_SENDER)) {
#if 0
https://svnweb.freebsd.org/base/head/sys/kern/sysv_msg.c?revision=282213&view=markup

#define IPCID_TO_IX(id)         ((id) & 0xffff)
#define IPCID_TO_SEQ(id)        (((id) >> 16) & 0xffff)
#define IXSEQ_TO_IPCID(ix,perm) (((perm.seq) << 16) | (ix & 0xffff))
#endif
            if (!CAP_RIGHTS_LIMIT(error, q->qid, CAP_READ)) {
                break;
            }
#if 0
        } else {
            if (!CAP_RIGHTS_LIMIT(error, q->qid, CAP_WRITE)) {
                  break;
            }
#endif
        }
        if (0 != msgctl(q->qid, IPC_STAT, &buf)) {
            set_system_error(error, "msgctl(%d, IPC_STAT, %p) failed", q->qid, &buf);
            break;
        }
        q->buffer_size = buf.msg_qbytes / 8;
        if (NULL == (q->buffer = malloc(sizeof(long) + sizeof(q->buffer) * q->buffer_size))) {
            set_malloc_error(error, sizeof(long) + sizeof(q->buffer) * q->buffer_size);
            break;
        }
        *(long *) q->buffer = 1; /* mtype is an integer greater than 0 */
        ok = true;
    } while (false);

    return ok;
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

int queue_receive(void *p, char *buffer, size_t buffer_size, char **error)
{
    int read;
    systemv_queue_t *q;

    q = (systemv_queue_t *) p;

    if (-1 != (read = msgrcv(q->qid, q->buffer, buffer_size, 0, 0))) { // TODO: min(buffer_size, q->buffer_size) ?
        strcpy(buffer, q->buffer + sizeof(long)); // TODO: better ?
    } else {
        set_system_error(error, "msgrcv failed");
    }

    return read;
}

bool queue_send(void *p, const char *msg, int msg_len, char **error)
{
    bool ok;

    ok = false;
    do {
        systemv_queue_t *q;

        q = (systemv_queue_t *) p;
        if (msg_len < 0) {
            msg_len = strlen(msg);
        }
        strcpy(q->buffer + sizeof(long), msg); // TODO: safer
        if (0 != msgsnd(q->qid, q->buffer, q->buffer_size,  0)) {
            set_system_error(error, "msgsnd failed");
            break;
        }
        ok = true;
    } while (false);

    return ok;
}

bool queue_close(void **p, char **error)
{
    bool ok;

    ok = false;
    do {
        if (NULL != *p) {
            systemv_queue_t *q;

            q = (systemv_queue_t *) *p;
            if (NULL != q->filename) { // we are the owner
                if (-1 != q->qid) {
                    if (0 != msgctl(q->qid, IPC_RMID, NULL)) {
                        set_system_error(error, "msgctl(%d, IPC_RMID, NULL) failed", q->qid);
                        break;
                    }
                    q->qid = -1;
                }
                if (0 != unlink(q->filename)) {
                    set_system_error(error, "unlink(\"%s\") failed", q->filename);
                    break;
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
        ok = true;
    } while (false);

    return ok;
}
