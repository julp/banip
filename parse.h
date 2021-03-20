#pragma once

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdint.h>

typedef struct {
    union {
        struct in_addr v4;
        struct in6_addr v6;
    } sa;
    int fa;
    size_t sa_size;
    uint8_t netmask;
    char humanrepr[INET6_ADDRSTRLEN + 1];
} addr_t;

bool parse_addr(const char *, addr_t *, char **);
bool parse_ulong(const char *, unsigned long *, char **);
