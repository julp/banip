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

int parse_ulong(const char *str, unsigned long *val)
{
    char *endptr;

    *val = strtoul(str, &endptr, 10);
    if ((ERANGE == errno && ULONG_MAX == *val) || (0 != errno && 0 == *val)) {
        errx("overflow or underflow for '%s'", str);
        return 0;
    }
    if (endptr == str) {
        errx("number expected, no digit found");
        return 0;
    }
    if ('\0' != *endptr) {
        errx("number expected, non digit found %c in %s", *endptr, str);
        return 0;
    }
    if (*val <= 0) {
        errx("number should be greater than 0, got %ld", *val);
        return 0;
    }

    return 1;
}

int parse_addr(const char *string, addr_t *addr)
{
    char *p, *buffer;

    buffer = (char *) string;
    bzero(addr, sizeof(*addr));
    if (NULL != (p = strchr(string, '/'))) {
        int ret;
        unsigned long prefix;
        struct addrinfo *res;
        struct addrinfo hints;

        buffer = strdup(string);
        buffer[p - string] = '\0';
        parse_ulong(++p, &prefix);
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
        switch (res->ai_family) {
            case AF_INET:
                assert(1 == inet_pton(AF_INET, buffer, &addr->sa.v4));
                addr->sa_size = sizeof(addr->sa.v4);
                break;
            case AF_INET6:
                assert(1 == inet_pton(AF_INET6, buffer, &addr->sa.v6));
                addr->sa_size = sizeof(addr->sa.v6);
                break;
        }
        addr->netmask = prefix;
        addr->fa = res->ai_family;
        freeaddrinfo(res);
    } else {
        if (1 == inet_pton(AF_INET, buffer, &addr->sa.v4)) {
            addr->fa = AF_INET;
            addr->netmask = 32;
            addr->sa_size = sizeof(addr->sa.v4);
        } else if (1 == inet_pton(AF_INET6, buffer, &addr->sa.v6)) {
            addr->fa = AF_INET6;
            addr->netmask = 128;
            addr->sa_size = sizeof(addr->sa.v6);
        } else {
            warn("Valid address expected, got: %s", buffer);
        }
    }
    if (strlcpy(addr->humanrepr, buffer, STR_SIZE(addr->humanrepr)) >= STR_SIZE(addr->humanrepr)) {
        errx("buffer overflow");
    }
    if (buffer != string) {
        free(buffer);
    }

    return 1;
}
