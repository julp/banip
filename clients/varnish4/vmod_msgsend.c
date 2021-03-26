#include <stdlib.h>

#include <cache/cache.h>
#include <vcl.h>

#ifndef VRT_H_INCLUDED
# include <vrt.h>
#endif /* !VRT_H_INCLUDED */

#ifndef VDEF_H_INCLUDED
# include <vdef.h>
#endif /* !VDEF_H_INCLUDED */

#include <string.h>

#include "vcc_if.h"

#include "queue.h"
#include "error.h"

#ifdef DEBUG
# include <stdarg.h>
# include <stdio.h>
# define debug(fmt, ...) \
    do { \
        FILE *fp;\
        fp = fopen("/tmp/msgsend.log", "a"); \
        fprintf(fp, fmt "\n", ## __VA_ARGS__); \
        fclose(fp); \
    } while (0);
#else
# define debug(fmt, ...)
#endif /* DEBUG */

/*
cd $HOME/libvmod-msgsend
./autogen.sh && ./configure VARNISHSRC=$HOME/Downloads/varnish-4.0.0/ && make ; sudo make install
*/

struct vmod_msgsend_mqueue {
    unsigned magic;
    #define VMOD_MSGSEND_OBJ_MAGIC 0x9966feff
    void *queue;
    const char *queue_name;
};

VCL_VOID vmod_mqueue__init(const struct vrt_ctx *ctx, struct vmod_msgsend_mqueue **qp, const char *vcl_name, VCL_STRING queue_name)
{
    void *queue;
    char *error;
    struct vmod_msgsend_mqueue *q;

    error = NULL;
    CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
    AN(qp);
    AZ(*qp);
    if (NULL == (queue = queue_init(&error))) {
        if (NULL != ctx->vsl) {
            VSLb(ctx->vsl, SLT_Error, "Can't init queue: %s", error);
            error_free(&error);
        }
    }
    if (!queue_open(queue, queue_name, QUEUE_FL_SENDER, &error)) {
        if (NULL != ctx->vsl) {
            VSLb(ctx->vsl, SLT_Error, "Can't open queue '%s': %s", queue_name, error);
            error_free(&error);
        }
        queue_close(&queue, NULL);
    }
    XXXAN(queue);
    ALLOC_OBJ(q, VMOD_MSGSEND_OBJ_MAGIC);
    AN(q);
    *qp = q;
    q->queue = queue;
    q->queue_name = queue_name;
    AN(*qp);
}

VCL_VOID vmod_mqueue__fini(struct vmod_msgsend_mqueue **qp)
{
    AN(qp);
    CHECK_OBJ_NOTNULL(*qp, VMOD_MSGSEND_OBJ_MAGIC);
    queue_close(&(*qp)->queue, NULL);
    FREE_OBJ(*qp);
    *qp = NULL;
}

VCL_VOID vmod_mqueue_sendmsg(const struct vrt_ctx *ctx, struct vmod_msgsend_mqueue *q, VCL_STRING message)
{
    char *error;
    CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
    CHECK_OBJ_NOTNULL(q, VMOD_MSGSEND_OBJ_MAGIC);

    error = NULL;
    if (!queue_send(q->queue, message, -1, &error)) {
        VSLb(ctx->vsl, SLT_Error, "Failed sending message '%s' on '%s': %s", message, q->queue_name, error);
        error_free(&error);
    }
}
