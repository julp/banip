#pragma once

#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define CONTEXT_MAX_LENGTH 128
#define BANIP_VERSION ((uint8_t *) "\001\000") /* 1.0 */

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
#define STR_LEN(str)      (ARRAY_SIZE(str) - 1)
#define STR_SIZE(str)     (ARRAY_SIZE(str))

#ifdef DEBUG
# define set_system_error(error, format, ...) \
	_set_error(error, "[%s:%d] " format ": %s", __func__, __LINE__, ## __VA_ARGS__, strerror(errno))

# define set_generic_error(error, format, ...) \
	_set_error(error, "[%s:%d] " format, __func__, __LINE__, ## __VA_ARGS__)
#else
# define set_system_error(error, format, ...) \
	_set_error(error, format ": %s", ## __VA_ARGS__, strerror(errno))

# define set_generic_error(error, format, ...) \
	_set_error(error, format, ## __VA_ARGS__)
#endif /* DEBUG */

typedef struct {
	uint8_t netmask;
	socklen_t addr_len;
	union {
		struct sockaddr sa;
		struct sockaddr_in sa4;
		struct sockaddr_in6 sa6;
	} u;
	char humanrepr[INET6_ADDRSTRLEN + STR_SIZE("/xx")]; // + \0 + allow CIDR notation
} addr_t;

typedef struct {
	uint8_t version[2];
	addr_t addr; // TODO: remplacer ça par union { ... } u; de ci-dessus?
	size_t context_len;
	char context[CONTEXT_MAX_LENGTH];
} banip_request_t;

enum {
	FL_NONE = 0,
	FL_FD_SET = 1<<0,
	FL_CRED_SET = 1<<1
};

typedef struct {
	banip_request_t super;
	int fd;
	uid_t uid;
	gid_t gid;
	uint32_t flags;
} banip_authenticated_request_t;

void _set_error(char **, const char *, ...);

int banip_get_fd(void *);

void *banip_client_new(const char *, cap_rights_t *, char **);
void *banip_server_new(const char *, cap_rights_t *, char **);

void banip_close(void *);
bool banip_receive(void *, int, banip_authenticated_request_t *, char **);

bool banip_ban_from_fd(void *, int, const char *, size_t, char **);
bool banip_ban_from_sa(void *, const struct sockaddr * const, size_t, const char *, size_t, char **);
bool banip_ban_from_string(void *, const char *, size_t, const char *, size_t, char **);
