#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>

#include "config.h"
#ifdef HAVE_LIBBSD_STRLCPY
# include <bsd/string.h>
#endif /* !HAVE_LIBBSD_STRLCPY */
#include "common.h"
#include "parse.h"
#include "err.h"

bool parse_ulong(const char *str, unsigned long *val, char **error)
{
    bool ok;

    ok = false;
    do {
        char *endptr;

        *val = strtoul(str, &endptr, 10);
        if ((ERANGE == errno && ULONG_MAX == *val) || (0 != errno && 0 == *val)) {
            set_generic_error(error, "overflow or underflow for '%s'", str);
            break;
        }
        if (endptr == str) {
            set_generic_error(error, "number expected, no digit found");
            break;
        }
        if ('\0' != *endptr) {
            set_generic_error(error, "number expected, non digit found %c in %s", *endptr, str);
            break;
        }
        if (*val <= 0) {
            set_generic_error(error, "number should be greater than 0, got %ld", *val);
            break;
        }
        ok = true;
    } while (false);

    return ok;
}

#if 0
struct {
    struct in_addr;
    uint8_t netmask;
} static const reserved[] = {
    // TODO: add 127/8 ?
    // { .s_addr = htonl(0x0a000000) }
    // voir /usr/include/netinet/in.h
    { (in_addr_t) (0x0a000000), 8 },  /* 10/8 */
    { (in_addr_t) (0xac100000), 12 }, /* 172.16/12 */
    { (in_addr_t) (0xc0a80000), 16 }, /* 192.168/16 */
};

int ip4_matchnet(const struct in6_addr *ip, const struct in_addr *net, const unsigned char mask)
{
    struct in_addr m;

    /* do this explicitely here so we don't rely on how the compiler handles
     * the shift overflow below. */
    if (mask == 0)
        return 1;

    /* constuct a bit mask out of the net length.
     * remoteip and ip are network byte order, it's easier
     * to convert mask to network byte order than both
     * to host order. It's ugly, isn't it? */
    m.s_addr = htonl(-1 - ((1U << (32 - mask)) - 1));

    return ((ip->s6_addr32[3] & m.s_addr) == (net->s_addr & m.s_addr));
}

int ip6_matchnet(const struct in6_addr *ip, const struct in6_addr *net, const unsigned char mask)
{
    struct in6_addr maskv6;
    int i;

    memset(maskv6.s6_addr, 0, sizeof(maskv6.s6_addr));
    for (i = 0; i < mask / 32; ++i) {
        maskv6.s6_addr32[i] = -1;
    }
    if ((mask % 32) != 0)
        maskv6.s6_addr32[mask / 32] = htonl(-1 - ((1U << (32 - (mask % 32))) - 1));

    for (i = 3; i >= 0; i--) {
        if ((ip->s6_addr32[i] & maskv6.s6_addr32[i]) != (net->s6_addr32[i] & maskv6.s6_addr32[i])) {
            return 0;
        }
    }
    return 1;
}

const struct in_addr priva = { .s_addr = htonl(0x0a000000) };
const struct in_addr privb = { .s_addr = htonl(0xac100000) };
const struct in_addr privc = { .s_addr = htonl(0xc0a80000) };

/* 10.0.0.0/8 */
flagtmp = ip4_matchnet(&(thisip->addr), &priva, 8);
/* 172.16.0.0/12 */
if (!flagtmp)
    flagtmp = ip4_matchnet(&(thisip->addr), &privb, 12);
/* 192.168.0.0/16 */
if (!flagtmp)
    flagtmp = ip4_matchnet(&(thisip->addr), &privc, 16);
#endif

bool parse_addr(const char *string, addr_t *addr, char **error)
{
    bool ok;
    char *buffer;
    struct addrinfo *res;

    ok = false;
    res = NULL;
    buffer = (char *) string;
    do {
        char *p;

        bzero(addr, sizeof(*addr));
        if (NULL != (p = strchr(string, '/'))) {
            int ret;
            unsigned long prefix;
            struct addrinfo hints;

            buffer = strdup(string);
            buffer[p - string] = '\0';
            parse_ulong(++p, &prefix, error);
            bzero(&hints, sizeof(hints));
            hints.ai_flags |= AI_NUMERICHOST;
            if (0 != (ret = getaddrinfo(buffer, NULL, &hints, &res))) {
                set_generic_error(error, "getaddrinfo failed: %s", gai_strerror(ret));
                break;
            }
            if (res->ai_family == AF_INET && prefix > 32) {
                set_generic_error(error, "prefix too long for AF_INET");
                break;
            } else if (res->ai_family == AF_INET6 && prefix > 128) {
                set_generic_error(error, "prefix too long for AF_INET6");
                break;
            }
#if 0
            switch (res->ai_family) {
                case AF_INET:
                    ret = inet_pton(AF_INET, buffer, &addr->sa.v4);
                    assert(1 == ret);
                    addr->sa_size = sizeof(addr->sa.v4);
                    break;
                case AF_INET6:
                    ret = inet_pton(AF_INET6, buffer, &addr->sa.v6);
                    assert(1 == ret);
                    addr->sa_size = sizeof(addr->sa.v6);
                    break;
                default:
                    assert(0);
            }
#else
            if (AF_INET == res->ai_family && 1 == inet_pton(AF_INET, buffer, &addr->sa.v4)) {
                addr->sa_size = sizeof(addr->sa.v4);
            } else if (AF_INET6 == res->ai_family && 1 == inet_pton(AF_INET6, buffer, &addr->sa.v6)) {
                addr->sa_size = sizeof(addr->sa.v6);
            } else {
                set_generic_error(error, "valid address expected, got: %s", buffer);
                break;
            }
#endif
            addr->netmask = prefix;
            addr->fa = res->ai_family;
        } else {
            if (1 == inet_pton(AF_INET, buffer, &addr->sa.v4)) {
                addr->fa = AF_INET;
                addr->netmask = 32;
                addr->sa_size = sizeof(addr->sa.v4);
            } else if (1 == inet_pton(AF_INET6, buffer, &addr->sa.v6)) {
                addr->fa = AF_INET6;
                addr->netmask = 64;
                addr->sa_size = sizeof(addr->sa.v6);
            } else {
                set_generic_error(error, "valid address expected, got: %s", buffer);
                break;
            }
        }
        // set to 0 the unused part of the IP address to align with the netmask else it might be invalid
        {
            int b, i, j;
            uint32_t *m, *a;
            struct in6_addr mask;

            j = 0;
            m = (uint32_t *) &mask;
            a = (uint32_t *) &addr->sa;
            bzero(&mask, sizeof(mask));
            for (b = addr->netmask; b >= 32; b -= 32) {
                m[j++] = 0xFFFFFFFF;
            }
            for (i = 31; i > 31 - b; --i) {
                m[j] |= (1 << i);
            }
            if (0 != b) {
                m[j] = htonl(m[j]);
            }
            for (i = 0; i < 4; i++) {
                a[i] &= m[i];
            }
        }
        if (strlcpy(addr->humanrepr, buffer, STR_SIZE(addr->humanrepr)) >= STR_SIZE(addr->humanrepr)) {
            set_generic_error(error, "buffer overflow");
            break;
        }
        ok = true;
    } while (false);
    if (NULL != res) {
        freeaddrinfo(res);
    }
    if (buffer != string) {
        free(buffer);
    }

    return ok;
}
