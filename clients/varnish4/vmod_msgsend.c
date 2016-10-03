#include <stdlib.h>

#include "vrt.h"
#include "cache/cache.h"

#include "vcc_if.h"

#include "queue.h"

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
    void *client;
};

VCL_VOID vmod_mqueue__init(VRT_CTX, struct vmod_msgsend_mqueue **qp, const char *vcl_name, VCL_STRING path)
{
    void *client;
    struct vmod_msgsend_mqueue *q;

    CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
    AN(qp);
    AZ(*qp);
    if (banip_connect(&client, false, path)) {
        if (NULL != ctx->vsl) {
            VSLb(ctx->vsl, SLT_Error, "Can't init client");
        }
    }
    XXXAN(client);
    ALLOC_OBJ(q, VMOD_MSGSEND_OBJ_MAGIC);
    AN(q);
    *qp = q;
    q->client = client;
    AN(*qp);
}

VCL_VOID vmod_mqueue__fini(struct vmod_msgsend_mqueue **qp)
{
    AN(qp);
    CHECK_OBJ_NOTNULL(*qp, VMOD_MSGSEND_OBJ_MAGIC);
    banip_close((*qp)->client);
    FREE_OBJ(*qp);
    *qp = NULL;
}

VCL_VOID __match_proto__()
vmod_mqueue_sendmsg(VRT_CTX, struct vmod_msgsend_mqueue *q)
{
    CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
    CHECK_OBJ_NOTNULL(q, VMOD_MSGSEND_OBJ_MAGIC);
	CHECK_OBJ_NOTNULL(ctx->req, REQ_MAGIC);
	CHECK_OBJ_NOTNULL(ctx->req->sp, SESS_MAGIC);

    if (banip_ban_from_fd(q->client, ctx->req->sp->fd, NULL, 0)) {
        VSLb(ctx->vsl, SLT_Debug, "Message '%s' sent on mqueue");
    } else {
        VSLb(ctx->vsl, SLT_Error, "Failed sending message '%s'");
    }
}

#if 0
struct sess . fd
struct http_conn . fd

struct busyobj . htc
struct req . htc

ctx->req->sp->fd ?
ctx->sp->fd ?
ctx->bo->htc->fd ?
ctx->req->htc->fd ?
#endif
