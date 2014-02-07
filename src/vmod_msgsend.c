#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>

#include <stdlib.h>
#include <stdio.h>

#include "vrt.h"
#include "cache/cache.h"

#include "vcc_if.h"

/*
cd $HOME/libvmod-msgsend
./autogen.sh && ./configure VARNISHSRC=$HOME/Downloads/varnish-4.0.0-tp2/ VMODDIR=/usr/lib/varnish/vmods/ && make ; sudo make install
*/

struct vmod_msgsend_mqueue {
    unsigned magic;
    #define VMOD_MSGSEND_OBJ_MAGIC 0x9966feff
    mqd_t mq;
    const char *queue_name;
};

VCL_VOID vmod_mqueue__init(const struct vrt_ctx *ctx, struct vmod_msgsend_mqueue **qp, const char *vcl_name, VCL_STRING queue_name)
{
    mqd_t mq;
    struct vmod_msgsend_mqueue *q;

    CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
    AN(qp);
    AZ(*qp);
    if ((mqd_t) -1 == (mq = mq_open(queue_name, O_WRONLY))) {
        VSLb(ctx->vsl, SLT_Error, "Can't open mqueue '%s'", queue_name);
        return;
    }
    XXXAN(((mqd_t) -1) == mq);
    ALLOC_OBJ(q, VMOD_MSGSEND_OBJ_MAGIC);
    AN(q);
    *qp = q;
    q->mq = mq;
    q->queue_name = queue_name;
    AN(*qp);
}

VCL_VOID vmod_mqueue__fini(struct vmod_msgsend_mqueue **qp)
{
    AN(qp);
    AN(*qp);
    mq_close((*qp)->mq);
    FREE_OBJ(*qp);
    *qp = NULL;
}

VCL_VOID __match_proto__()
vmod_mqueue_sendmsg(const struct vrt_ctx *ctx, struct vmod_msgsend_mqueue *q, VCL_STRING message)
{
    long msg_size;
    struct mq_attr attr;

    CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
    CHECK_OBJ_NOTNULL(q, VMOD_MSGSEND_OBJ_MAGIC);

    msg_size = strlen(message) + 1;
    if (0 != mq_getattr(q->mq, &attr)) {
        VSLb(ctx->vsl, SLT_Error, "Can't get attribute for mqueue '%s'", q->queue_name);
        return;
    }
    if (msg_size > attr.mq_msgsize) {
        VSLb(ctx->vsl, SLT_Error, "Message '%s' is too long for mqueue '%s' (%ld > %ld)", q->queue_name, message, msg_size, attr.mq_msgsize);
        return;
    }
    if (0 != mq_send(q->mq, message, msg_size, 0)) {
        VSLb(ctx->vsl, SLT_Error, "Failed sending message '%s' on '%s'", message, q->queue_name);
    } else {
        VSLb(ctx->vsl, SLT_Debug, "Message '%s' sent on mqueue '%s'", message, q->queue_name);
    }
}
