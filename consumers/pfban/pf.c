#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/pfvar.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <string.h>

#include "engine.h"

typedef struct {
    int fd;
} pf_data_t;

static void *pf_open(void)
{
    pf_data_t *data;

    data = malloc(sizeof(*data));
    if (-1 == (data->fd = open("/dev/pf", O_WRONLY))) {
        errc("failed opening /dev/pf");
    }

    return data;
}

static int pf_handle(void *ctxt, const char *tablename, const char *buffer)
{
    int ret;
    pf_data_t *data;
    struct pfr_addr addr;
    struct pfioc_table io;
    struct sockaddr last_src;
    struct addrinfo *res, *resp;
    struct pfioc_src_node_kill psnk;

    data = (pf_data_t *) ctxt;
    bzero(&io, sizeof(io));
    strlcpy(io.pfrio_table.pfrt_name, tablename, sizeof(io.pfrio_table.pfrt_name));
    io.pfrio_buffer = &addr;
    io.pfrio_esize = sizeof(addr);
    io.pfrio_size = 1;
    bzero(&addr, sizeof(addr));
    if (1 == inet_pton(AF_INET, buffer, &addr.pfra_ip4addr)) {
        addr.pfra_af = AF_INET;
        addr.pfra_net = 32;
    } else if (1 == inet_pton(AF_INET6, buffer, &addr.pfra_ip6addr)) {
        addr.pfra_af = AF_INET6;
        addr.pfra_net = 128;
    } else {
        warn("Valid address expected, got: %s", buffer);
    }
    if (-1 == ioctl(data->fd, DIOCRADDADDRS, &io)) {
        errc("ioctl(DIOCRADDADDRS) failed");
    }
#if 0
    /* kill states */
    memset(&psnk, 0, sizeof(psnk));
    memset(&psnk.psnk_src.addr.v.a.mask, 0xff, sizeof(psnk.psnk_src.addr.v.a.mask));
    memset(&last_src, 0xff, sizeof(last_src));
    if (0 != (ret = getaddrinfo(buffer, NULL, NULL, &res))) {
        errc("getaddrinfo failed");
    }
    for (resp = res; resp; resp = resp->ai_next) {
        if (NULL == resp->ai_addr) {
            continue;
        }
        if (0 == memcmp(&last_src, resp->ai_addr, sizeof(last_src))) {
            continue;
        }
        last_src = *(struct sockaddr *)resp->ai_addr;
        psnk.psnk_af = resp->ai_family;
        switch (psnk.psnk_af) {
            case AF_INET:
                psnk.psnk_src.addr.v.a.addr.v4 = ((struct sockaddr_in *) resp->ai_addr)->sin_addr;
                break;
            case AF_INET6:
                psnk.psnk_src.addr.v.a.addr.v6 = ((struct sockaddr_in6 *) resp->ai_addr)->sin6_addr;
                break;
            default:
                freeaddrinfo(res);
                errx("Unknown address family %d", psnk.psnk_af);
        }
        if (-1 == ioctl(data->fd, DIOCKILLSRCNODES, &psnk)) {
            errc("ioctl(DIOCKILLSRCNODES) failed");
        }
    }
    freeaddrinfo(res);
#endif

    return 1;
}

static void pf_close(void *ctxt)
{
    pf_data_t *data;

    data = (pf_data_t *) ctxt;
    if (-1 != data->fd) {
        if (0 != close(data->fd)) {
            warnc("closing /dev/pf failed");
        }
        data->fd = -1;
    }
}

const engine_t pf_engine = {
    "pf",
    pf_open,
    pf_handle,
    pf_close
};
