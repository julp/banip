#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/pfvar.h>
#include <sys/ioctl.h>
#include <netdb.h>
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
    if (-1 == (data->fd = open("/dev/pf", O_RDWR))) {
        errc("failed opening /dev/pf");
    }

    return data;
}

/**
 * kill states, taken from pfctl
 * Copyright (c) 2001 Daniel Hartmeier
 * Copyright (c) 2002,2003 Henning Brauer
 **/
static int pf_handle(void *ctxt, const char *tablename, const char *addrstr)
{
    int ret;
    pf_data_t *data;
    char *p, *buffer;
    struct pfr_addr addr;
    struct pfioc_table io;
    struct sockaddr last_src;
    struct addrinfo *res, *resp;
    struct pfioc_state_kill psk;

    buffer = strdup(addrstr);
    data = (pf_data_t *) ctxt;
    bzero(&io, sizeof(io));
    strlcpy(io.pfrio_table.pfrt_name, tablename, sizeof(io.pfrio_table.pfrt_name));
    io.pfrio_buffer = &addr;
    io.pfrio_esize = sizeof(addr);
    io.pfrio_size = 1;
    bzero(&addr, sizeof(addr));
    memset(&psk, 0, sizeof(psk));
    memset(&psk.psk_src.addr.v.a.mask, 0xff, sizeof(psk.psk_src.addr.v.a.mask));
    memset(&last_src, 0xff, sizeof(last_src));
    if (NULL != (p = strchr(buffer, '/'))) {
        int q, r;
        long prefix;
        struct addrinfo hints;

        *p++ = '\0';
        parse_long(p, &prefix);
        bzero(&hints, sizeof(hints));
        hints.ai_flags |= AI_NUMERICHOST;
        if (0 != (ret = getaddrinfo(buffer, NULL, &hints, &res))) {
            errc("getaddrinfo failed: %s", gai_strerror(ret));
        }
        if (res->ai_family == AF_INET && prefix > 32) {
            errx("prefix too long for AF_INET");
        } else if (res->ai_family == AF_INET6 && prefix > 128) {
            errx("prefix too long for AF_INET6");
        }
        q = prefix >> 3;
        r = prefix & 7;
        switch (res->ai_family) {
            case AF_INET:
                inet_pton(AF_INET, buffer, &addr.pfra_ip4addr); // asset(1 == inet_pton)
                bzero(&psk.psk_src.addr.v.a.mask.v4, sizeof(psk.psk_src.addr.v.a.mask.v4));
                psk.psk_src.addr.v.a.mask.v4.s_addr = htonl((u_int32_t) (0xffffffffffULL << (32 - prefix)));
                break;
            case AF_INET6:
                inet_pton(AF_INET6, buffer, &addr.pfra_ip6addr); // asset(1 == inet_pton)
                bzero(&psk.psk_src.addr.v.a.mask.v6, sizeof(psk.psk_src.addr.v.a.mask.v6));
                if (q > 0) {
                    memset((void *) &psk.psk_src.addr.v.a.mask.v6, 0xff, q);
                }
                if (r > 0) {
                    *((u_char *) &psk.psk_src.addr.v.a.mask.v6 + q) = (0xff00 >> r) & 0xff;
                }
                break;
        }
        addr.pfra_net = prefix;
        addr.pfra_af = res->ai_family;
        freeaddrinfo(res);
    } else {
        if (1 == inet_pton(AF_INET, buffer, &addr.pfra_ip4addr)) {
            addr.pfra_af = AF_INET;
            addr.pfra_net = 32;
        } else if (1 == inet_pton(AF_INET6, buffer, &addr.pfra_ip6addr)) {
            addr.pfra_af = AF_INET6;
            addr.pfra_net = 128;
        } else {
            warn("Valid address expected, got: %s", buffer);
        }
    }
    if (-1 == ioctl(data->fd, DIOCRADDADDRS, &io)) {
        errc("ioctl(DIOCRADDADDRS) failed");
    }
    if (0 != (ret = getaddrinfo(buffer, NULL, NULL, &res))) {
        errc("getaddrinfo failed: %s", gai_strerror(ret));
    }
    for (resp = res; resp; resp = resp->ai_next) {
        if (NULL == resp->ai_addr) {
            continue;
        }
        if (0 == memcmp(&last_src, resp->ai_addr, sizeof(last_src))) {
            continue;
        }
        last_src = *(struct sockaddr *)resp->ai_addr;
        psk.psk_af = resp->ai_family;
        switch (psk.psk_af) {
            case AF_INET:
                psk.psk_src.addr.v.a.addr.v4 = ((struct sockaddr_in *) resp->ai_addr)->sin_addr;
                break;
            case AF_INET6:
                psk.psk_src.addr.v.a.addr.v6 = ((struct sockaddr_in6 *) resp->ai_addr)->sin6_addr;
                break;
            default:
                freeaddrinfo(res);
                errx("Unknown address family %d", psk.psk_af);
        }
        if (-1 == ioctl(data->fd, DIOCKILLSTATES, &psk)) {
            errc("ioctl(DIOCKILLSTATES) failed");
        }
    }
    freeaddrinfo(res);
    free(buffer);

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
