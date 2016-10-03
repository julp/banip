#include <errno.h>
#include <fcntl.h>
#include <unistd.h> /* unlink */
#include <sys/stat.h> /* umask */
#include <sys/un.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h> /* strerror */
#include <assert.h>
#include <inttypes.h>
#include <netdb.h>

#include "config.h"
#ifdef HAVE_LIBBSD_STRLCPY
# include <bsd/string.h>
#endif /* !HAVE_LIBBSD_STRLCPY */
#include "common.h"
#include "banip.h"
#include "capsicum.h"

#ifndef SOCK_NONBLOCK
# define SOCK_NONBLOCK 0
#endif /* SOCK_NONBLOCK */
#ifndef SOCK_CLOEXEC
# define SOCK_CLOEXEC 0
#endif /* SOCK_CLOEXEC */
#ifndef SOCK_NOSIGPIPE
# define SOCK_NOSIGPIPE 0
#endif /* SOCK_NOSIGPIPE */

#if defined(LOCAL_CREDS)
# define CRED_LEVEL   0
# define CRED_NAME    LOCAL_CREDS
# define CRED_SC_UID  sc_euid
# define CRED_SC_GID  sc_egid
# define CRED_MESSAGE SCM_CREDS
# define CRED_SIZE    SOCKCREDSIZE(NGROUPS_MAX)
# define CRED_TYPE    struct sockcred
# define GOT_CRED     2
#elif defined(SO_PASSCRED)
# define CRED_LEVEL   SOL_SOCKET
# define CRED_NAME    SO_PASSCRED
# define CRED_SC_UID  uid
# define CRED_SC_GID  gid
# define CRED_MESSAGE SCM_CREDENTIALS
# define CRED_SIZE    sizeof(struct ucred)
# define CRED_TYPE    struct ucred
# define GOT_CRED     2
#else
# define GOT_CRED     0
# define CRED_SIZE    0
# define CRED_TYPE    void * __unused
#endif

typedef struct {
	int fd;
	struct sockaddr_un sun;
} banip_client_t;

#define DEFAULT_SOCKET_PATH "/tmp/banip.sock"

#define warning(format, ...) \
	do { \
		fprintf(stderr, format "\n", ## __VA_ARGS__); \
	} while(0);

static bool parse_addr(const char *, addr_t *, char **);

void _set_error(char **error, const char *format, ...)
{
	if (NULL != error) {
		int len;
		va_list ap;

		if (NULL != *error) {
			warning("Overwrite attempt of a previous error: %s", *error);
			free(error);
		}
		va_start(ap, format);
		len = vsnprintf(NULL, 0, format, ap);
		va_end(ap);
		if (len >= 0) {
			int chk;

			va_start(ap, format);
			*error = malloc(len * sizeof(**error));
			chk = vsnprintf(*error, len, format, ap);
			assert(chk >= 0 && chk == len);
			va_end(ap);
		}
	}
}

static void *banip_connect(bool srv, const char *path, cap_rights_t *UNUSED(rights), char **error)
{
	int fd, rv;
	banip_client_t *bc;

	bc = NULL;
	if (-1 == (fd = socket(PF_LOCAL, /*SOCK_DGRAM*/ SOCK_SEQPACKET /*SOCK_STREAM*/ | SOCK_CLOEXEC | ((/*1 || */srv) ? SOCK_NONBLOCK : 0) | SOCK_NOSIGPIPE, 0))) {
		set_generic_error(error, "%s is already in use", bc->sun.sun_path);
		goto failed;
	}
	if (srv) {
		CAP_RIGHTS_LIMIT(fd, CAP_EVENT, CAP_GETSOCKOPT, CAP_SETSOCKOPT, CAP_CONNECT, CAP_RECV, CAP_BIND, CAP_LISTEN, CAP_ACCEPT, CAP_GETPEERNAME);
	} else {
		CAP_RIGHTS_LIMIT(fd, CAP_SETSOCKOPT, CAP_CONNECT, CAP_SEND);
	}
	bc = malloc(sizeof(*bc));
	bc->fd = fd;
	bzero(&bc->sun, sizeof(bc->sun));
	bc->sun.sun_family = AF_LOCAL;
	strlcpy(bc->sun.sun_path, NULL == path ? DEFAULT_SOCKET_PATH : path, ARRAY_SIZE(bc->sun.sun_path));
/*
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
	bc->sun.sun_len = sizeof(bc->sun);
#endif
*/
#if !SOCK_CLOEXEC
	fcntl(bc->fd, F_SETFD, FD_CLOEXEC);
#endif /* !SOCK_CLOEXEC */
#if !SOCK_NONBLOCK || defined(CRED_LEVEL)
	if (srv) {
# if !SOCK_NONBLOCK
		cap_fcntls_limit(bc->fd, CAP_FCNTL_SETFL | CAP_FCNTL_GETFL);
		fcntl(bc->fd, F_SETFL, fcntl(bc->fd, F_GETFL) | O_NONBLOCK);
# endif /* !SOCK_NONBLOCK */
# ifdef CRED_LEVEL
		{
			int on = 1;

			if (0 != setsockopt(bc->fd, CRED_LEVEL, CRED_NAME, &on, sizeof(on))) {
				set_system_error(error, "setsockopt(LOCAL_CREDS)"); // TODO: STRINGIFY(CRED_NAME)
				goto failed;
			}
		}
# endif /* CRED_LEVEL */
	}
#endif /* CRED_LEVEL || !SOCK_NONBLOCK */
#if !SOCK_NOSIGPIPE
# ifdef SO_NOSIGPIPE
	{
		int o = 1;

		setsockopt(bc->fd, SOL_SOCKET, SO_NOSIGPIPE, &o, sizeof(o));
#if 0
		{
			socklen_t optlen;

			o = 200;
			setsockopt(bc->fd, SOL_SOCKET, SO_RCVBUF, &o, sizeof(o));
			getsockopt(bc->fd, SOL_SOCKET, SO_RCVBUF, &o, &optlen);
			debug("%d SO_RCVBUF = %d", __LINE__, o); // 200
			getsockopt(bc->fd, SOL_SOCKET, SO_SNDBUF, &o, &optlen);
			debug("%d SO_SNDBUF = %d", __LINE__, o); // 200
// 			getsockopt(bc->fd, SOL_SOCKET, SO_RCVLOWAT, &o, &optlen);
// 			debug("%d SO_RCVLOWAT = %d", __LINE__, o); // 1
		}
#endif
	}
# else
	signal(SIGPIPE, SIG_IGN);
# endif /* SO_NOSIGPIPE */
#endif /* !SOCK_NOSIGPIPE */
	if (0 == (rv = connect(bc->fd, (const void *) &bc->sun, (socklen_t) sizeof(bc->sun)))) { // (socklen_t) sizeof(bc->sun) <=> SUN_LEN(/*&*/bc->sun)
		if (srv) {
			set_generic_error(error, "%s is already in use", bc->sun.sun_path);
			goto failed;
		}
	} else {
		if (!srv) {
			set_system_error(error, "connect failed for '%s'", bc->sun.sun_path);
			goto failed;
		}
	}
	if (srv) {
		int serrno;
		mode_t oldumask;

		unlink(bc->sun.sun_path);
		oldumask = umask(0);
		rv = bind(bc->fd, (const void *) &bc->sun, (socklen_t) sizeof(bc->sun)); // (socklen_t) sizeof(bc->sun) <=> SUN_LEN(/*&*/bc->sun)
		serrno = errno;
		umask(oldumask);
		if (-1 == rv) {
			set_system_error(error, "bind failed for '%s'", bc->sun.sun_path);
			goto failed;
		}
		if (-1 == listen(bc->fd, 4)) {
			set_system_error(error, "listen failed");
			goto failed;
		}
	}

	if (false) {
failed:
		if (-1 != fd) {
			close(fd);
		}
		if (NULL != bc) {
			free(bc);
		}
		bc = NULL;
	}

	return bc;
}

void *banip_client_new(const char *path, cap_rights_t *rights, char **error)
{
	return banip_connect(false, path, rights, error);
}

void *banip_server_new(const char *path, cap_rights_t *rights, char **error)
{
	return banip_connect(true, path, rights, error);
}

int banip_get_fd(void *client)
{
	banip_client_t *bc;

	bc = (banip_client_t *) client;

	return bc->fd;
}

bool banip_receive(void *client, int fd, banip_authenticated_request_t *authreq, char **error)
{
	ssize_t rlen;
	struct iovec iov;
	struct msghdr msg;
	banip_client_t *bc;
	banip_request_t *req;
	union {
		struct cmsghdr hdr;
		unsigned char buf[CMSG_SPACE(sizeof(int)) + CMSG_SPACE(CRED_SIZE)];
	} cmsgbuf;

	bc = (banip_client_t *) client;
	req = (banip_request_t *) authreq; // or req = &authrep->super;
	bzero(authreq, sizeof(*authreq));
	bzero(&msg, sizeof(msg));
	iov.iov_base = req;
	iov.iov_len = sizeof(*req);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_flags = 0;
	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_control = cmsgbuf.buf;
	msg.msg_controllen = sizeof(cmsgbuf.buf);
	authreq->fd = -1;
	authreq->uid = -1;
	authreq->gid = -1;
// 	do {
		if (-1 == (rlen = recvmsg(fd, &msg, 0))) {
			set_system_error(error, "recvmsg failed");
			return false;
		} else if (HAS_FLAG(msg.msg_flags, MSG_TRUNC) || HAS_FLAG(msg.msg_flags, MSG_CTRUNC)) {
			set_generic_error(error, "received message is truncated");
			return false;
		} else {
			struct cmsghdr *cmsg;

			for (cmsg = CMSG_FIRSTHDR(&msg); NULL != cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
				// assert(SOL_SOCKET == cmsg->cmsg_level);
				switch (cmsg->cmsg_type) {
					case SCM_RIGHTS:
						if (CMSG_LEN(sizeof(int)) != cmsg->cmsg_len) {
							set_generic_error(error, "size mismatch on receiving fd");
							return false;
						}
						authreq->fd = *((int *) CMSG_DATA(cmsg));
						authreq->flags |= FL_FD_SET;
						break;
#ifdef CRED_MESSAGE
					case CRED_MESSAGE:
					{
						CRED_TYPE *cred;

						cred = (CRED_TYPE *) CMSG_DATA(cmsg);
						authreq->uid = cred->CRED_SC_UID;
						authreq->gid = cred->CRED_SC_GID;
debug("uid:gid = %d:%d", cred->CRED_SC_UID, cred->CRED_SC_GID);
						authreq->flags |= FL_CRED_SET;
						break;
					}
#endif /* CRED_MESSAGE */
					default:
						warning("unexpected header: level = %d, type = %d", cmsg->cmsg_level, cmsg->cmsg_type);
						break;
				}
			}
			if (0 != memcmp(req->version, BANIP_VERSION, ARRAY_SIZE(req->version) - sizeof(req->version[0]))) {
				set_generic_error(error, "version mismatch on protocol (server = %" PRIx16 " vs client = %" PRIx16 ")", BANIP_VERSION, req->version);
				return false;
			}
		}
		debug("ban of %d (reason: %.*s)", authreq->fd, (int) req->context_len, req->context);
// 	} while (rlen > 0);

	return true;
}

static bool banip_send(void *client, int fd, addr_t *addr, const char *context, size_t context_len, char **error)
{
	ssize_t sent;
	struct iovec iov;
	struct msghdr msg;
	banip_client_t *bc;
	banip_request_t req;
	struct cmsghdr *cmsg;
	union {
		struct cmsghdr hdr;
		unsigned char buf[CMSG_SPACE(sizeof(int))];
	} cmsgbuf;

	bc = (banip_client_t *) client;
	bzero(&req, sizeof(req));
	memcpy(req.version, BANIP_VERSION, ARRAY_SIZE(req.version));
	bzero(&msg, sizeof(msg));
	iov.iov_base = &req;
	iov.iov_len = sizeof(req);
	msg.msg_flags = 0;
	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	if (-1 == fd) {
		if (NULL == addr) {
			set_generic_error(error, "no address to be sent");
			return false;
		}
		memcpy(&req.addr, addr, sizeof(req.addr));
	} else {
		msg.msg_control = cmsgbuf.buf;
		msg.msg_controllen = sizeof(cmsgbuf.buf);
		cmsg = CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		cmsg->cmsg_len = CMSG_LEN(sizeof(fd));
		*((int *) CMSG_DATA(cmsg)) = fd;
	}
	if (NULL != context) {
		memcpy(req.context, context, req.context_len = MIN(context_len, ARRAY_SIZE(req.context)));
	}
	if (-1 == (sent = sendmsg(bc->fd, &msg, 0))) {
// 	if (-1 == (sent = send(bc->fd, S("XXX"), 0))) {
// 	if (-1 == (sent = write(bc->fd, S("XXX")))) {
		set_system_error(error, "sendmsg failed");
	}
	debug("sent: %zd", sent);

	return -1 != sent;
}

bool banip_ban_from_sa(void *client, const struct sockaddr * const sa, size_t sa_len, const char *context, size_t context_len, char **error)
{
	addr_t addr;

	if (sa_len > sizeof(addr.u)) {
		bzero(&addr, sizeof(addr));
		memcpy(&addr.u.sa, sa, sa_len);
		return banip_send(client, -1, &addr, context, context_len, error);
	} else {
		set_generic_error(error, "sa_len is too long (%zu > %zu)", sa_len, sizeof(addr.u));
		return false;
	}
}

bool banip_ban_from_string(void *client, const char *address, size_t UNUSED(address_len), const char *context, size_t context_len, char **error)
{
	addr_t addr;

	if (parse_addr(address, &addr, error)) {
		return banip_send(client, -1, &addr, context, context_len, error);
	}

	return false;
}

bool banip_ban_from_fd(void *client, int fd, const char *context, size_t context_len, char **error)
{
	bool ret;

	if (-1 == fd) {
// 		int saved_errno;

		ret = false;
// 		saved_errno = errno;
// 		errno = EBADF;
		set_generic_error(error, "invalid file descriptor");
// 		errno = saved_errno;
	} else {
		ret = banip_send(client, fd, NULL, context, context_len, error);
	}

	return ret;
}

void banip_close(void *client)
{
	banip_client_t *bc;

	bc = (banip_client_t *) client;
	if (-1 != bc->fd) {
		close(bc->fd);
	}
	free(bc);
}

static bool parse_ulong(const char *str, unsigned long *val, char **error)
{
	char *endptr;

	*val = strtoul(str, &endptr, 10);
	if ((ERANGE == errno && ULONG_MAX == *val) || (0 != errno && 0 == *val)) {
		set_generic_error(error, "overflow or underflow for '%s'", str);
		return false;
	}
	if (endptr == str) {
		set_generic_error(error, "number expected, no digit found");
		return false;
	}
	if ('\0' != *endptr) {
		set_generic_error(error, "number expected, non digit found %c in %s", *endptr, str);
		return false;
	}
	if (*val <= 0) {
		set_generic_error(error, "number should be greater than 0, got %ld", *val);
		return false;
	}

	return true;
}

static bool parse_addr(const char *string, addr_t *addr, char **error)
{
	bool ok;
	char *p, *buffer;

	ok = true;
	buffer = (char *) string;
	bzero(addr, sizeof(*addr));
	if (NULL != (p = strchr(string, '/'))) {
		int ret;
		unsigned long prefix;
		struct addrinfo *res;
		struct addrinfo hints;

		buffer = strdup(string);
		buffer[p - string] = '\0';
		parse_ulong(++p, &prefix, error);
		bzero(&hints, sizeof(hints));
		hints.ai_flags |= AI_NUMERICHOST;
		if (0 != (ret = getaddrinfo(buffer, NULL, &hints, &res))) {
			set_generic_error(error, "getaddrinfo failed: %s", gai_strerror(ret));
		}
		if (res->ai_family == AF_INET && prefix > 32) {
			ok = false;
			set_generic_error(error, "prefix too long for AF_INET");
		} else if (res->ai_family == AF_INET6 && prefix > 128) {
			ok = false;
			set_generic_error(error, "prefix too long for AF_INET6");
		} else {
			switch (res->ai_family) {
				case AF_INET:
					addr->addr_len = sizeof(addr->u.sa4);
					ret = inet_pton(AF_INET, buffer, &addr->u.sa4);
					assert(1 == ret);
					break;
				case AF_INET6:
					addr->addr_len = sizeof(addr->u.sa6);
					ret = inet_pton(AF_INET6, buffer, &addr->u.sa6);
					assert(1 == ret);
					break;
				default:
					assert(false);
					break;
			}
			addr->netmask = prefix;
			addr->u.sa.sa_family = res->ai_family;
		}
		freeaddrinfo(res);
	} else {
		if (1 == inet_pton(AF_INET, buffer, &addr->u.sa4.sin_addr)) {
			addr->addr_len = sizeof(addr->u.sa4);
			addr->u.sa.sa_family = AF_INET;
			addr->netmask = 32;
		} else if (1 == inet_pton(AF_INET6, buffer, &addr->u.sa6.sin6_addr)) {
			addr->addr_len = sizeof(addr->u.sa6);
			addr->u.sa.sa_family = AF_INET6;
			addr->netmask = 128;
		} else {
			set_generic_error(error, "valid address expected, got: %s", buffer);
			return false;
		}
	}
	if (strlcpy(addr->humanrepr, buffer, STR_SIZE(addr->humanrepr)) >= STR_SIZE(addr->humanrepr)) {
		ok = false;
		set_generic_error(error, "buffer overflow");
	}
	if (buffer != string) {
		free(buffer);
	}

	return ok;
}
