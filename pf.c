#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/pfvar.h>
#undef v4
#undef v6
#include <sys/ioctl.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>

#include "err.h"
#include "engine.h"
#include "capsicum.h"

typedef struct {
    int fd;
} pf_data_t;

static void *pf_open(const char *UNUSED(tablename))
{
    pf_data_t *data;

    data = malloc(sizeof(*data));
    if (-1 == (data->fd = open("/dev/pf", O_RDWR))) {
        errc("failed opening /dev/pf");
    }
    CAP_RIGHTS_LIMIT(data->fd, CAP_READ, CAP_WRITE, CAP_IOCTL);
    CAP_IOCTLS_LIMIT(data->fd, DIOCRADDADDRS, DIOCKILLSTATES);
#if 0
    {
        struct pfioc_table io;
        struct pfr_table table;

        bzero(&io, sizeof(io));
        io.pfrio_buffer = &table;
        io.pfrio_size = 1;
        io.pfrio_esize = sizeof(table);
        io.pfrio_flags = PFR_TFLAG_PERSIST;
        if (strlen(tablename) >= PF_TABLE_NAME_SIZE) {
            // error
        }
        if (strlcpy(table.pfrt_name, tablename, sizeof(table.pfrt_name)) >= sizeof(table.pfrt_name)) {
            // error
        }
        table.pfrt_anchor[0] = '\0';
        if (-1 == ioctl(dev, DIOCRADDTABLES, &io)) {
            // error
        }
    }
#endif

    return data;
}

/**
 * kill states, taken from pfctl
 * Copyright (c) 2001 Daniel Hartmeier
 * Copyright (c) 2002,2003 Henning Brauer
 * https://svnweb.freebsd.org/base/head/sbin/pfctl/pfctl.c?revision=262799&view=markup#l546
 **/
static int pf_handle(void *ctxt, const char *tablename, addr_t parsed_addr)
{
    int ret;
    pf_data_t *data;
    struct pfr_addr addr;
    struct pfioc_table io;
    struct sockaddr last_src;
    struct addrinfo *res, *resp;
    struct pfioc_state_kill psk;

    data = (pf_data_t *) ctxt;
    bzero(&io, sizeof(io));
    strlcpy(io.pfrio_table.pfrt_name, tablename, sizeof(io.pfrio_table.pfrt_name));
    io.pfrio_buffer = &addr;
    io.pfrio_esize = sizeof(addr);
    io.pfrio_size = 1;
    bzero(&addr, sizeof(addr));
    bzero(&psk, sizeof(psk));
    memset(&psk.psk_src.addr.v.a.mask, 0xff, sizeof(psk.psk_src.addr.v.a.mask));
    memset(&last_src, 0xff, sizeof(last_src));
#if 0
    if (NULL != (p = strchr(buffer, '/'))) {
        int q, r;
        unsigned long prefix;
        struct addrinfo hints;

        *p++ = '\0';
        parse_ulong(p, &prefix);
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
                inet_pton(AF_INET, buffer, &addr.pfra_ip4addr); // assert(1 == inet_pton)
                bzero(&psk.psk_src.addr.v.a.mask.v4, sizeof(psk.psk_src.addr.v.a.mask.pfa.v4));
                psk.psk_src.addr.v.a.mask.v4.s_addr = htonl((u_int32_t) (0xffffffffffULL << (32 - prefix)));
                break;
            case AF_INET6:
                inet_pton(AF_INET6, buffer, &addr.pfra_ip6addr); // assert(1 == inet_pton)
                bzero(&psk.psk_src.addr.v.a.mask.v6, sizeof(psk.psk_src.addr.v.a.mask.pfa.v6));
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
#else
    addr.pfra_af = parsed_addr.fa;
    addr.pfra_net = parsed_addr.netmask;
    memcpy(&addr.pfra_ip6addr, &parsed_addr.sa.v6, sizeof(parsed_addr.sa.v6));
    if (AF_INET == addr.pfra_af && addr.pfra_net < 32) {
        bzero(&psk.psk_src.addr.v.a.mask.pfa.v4, sizeof(psk.psk_src.addr.v.a.mask.pfa.v4));
        psk.psk_src.addr.v.a.mask.pfa.v4.s_addr = htonl((u_int32_t) (0xffffffffffULL << (32 - parsed_addr.netmask)));
    } else if (AF_INET6 == addr.pfra_af && addr.pfra_net < 128) {
        int q, r;

        q = addr.pfra_net >> 3;
        r = addr.pfra_net & 7;
        bzero(&psk.psk_src.addr.v.a.mask.pfa.v6, sizeof(psk.psk_src.addr.v.a.mask.pfa.v6));
        if (q > 0) {
            memset((void *) &psk.psk_src.addr.v.a.mask.pfa.v6, 0xff, q);
        }
        if (r > 0) {
            *((u_char *) &psk.psk_src.addr.v.a.mask.pfa.v6 + q) = (0xff00 >> r) & 0xff;
        }
    }
#endif
    if (-1 == ioctl(data->fd, DIOCRADDADDRS, &io)) {
        errc("ioctl(DIOCRADDADDRS) failed");
    }
#if 0
    if (0 != (ret = getaddrinfo(buffer, NULL, NULL, &res))) {
#else
    if (0 != (ret = getaddrinfo(parsed_addr.humanrepr, NULL, NULL, &res))) {
#endif
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
                psk.psk_src.addr.v.a.addr.pfa.v4 = ((struct sockaddr_in *) resp->ai_addr)->sin_addr;
                break;
            case AF_INET6:
                psk.psk_src.addr.v.a.addr.pfa.v6 = ((struct sockaddr_in6 *) resp->ai_addr)->sin6_addr;
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
#if 0
    free(buffer);
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
    true,
    "pf",
    pf_open,
    pf_handle,
    pf_close
};
