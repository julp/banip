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

#include "banip.h"
#include "engine.h"
#include "capsicum.h"

typedef struct {
	int fd;
} pf_data_t;

static void *pf_open(const char *UNUSED(tablename), char **error)
{
	pf_data_t *data;

	data = NULL;
	if (NULL == (data = malloc(sizeof(*data)))) {
		set_system_error(error, "malloc failed");
		goto err;
	}
	if (-1 == (data->fd = open("/dev/pf", O_RDWR))) {
		set_system_error(error, "failed opening /dev/pf");
		goto err;
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

err:
	if (false) {
		if (NULL != data) {
			if (-1 != data->fd) {
				close(data->fd);
			}
			free(data);
		}
		data = NULL;
	}

	return data;
}

/**
 * kill states, taken from pfctl
 * Copyright (c) 2001 Daniel Hartmeier
 * Copyright (c) 2002,2003 Henning Brauer
 * https://svnweb.freebsd.org/base/head/sbin/pfctl/pfctl.c?revision=262799&view=markup#l546
 **/
static bool pf_handle(void *ctxt, const char *tablename, addr_t parsed_addr, char **error)
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
	addr.pfra_af = parsed_addr.u.sa.sa_family;
	addr.pfra_net = parsed_addr.netmask;
	memcpy(&addr.pfra_ip6addr, &parsed_addr.u.sa6, sizeof(parsed_addr.u.sa6));
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
	if (-1 == ioctl(data->fd, DIOCRADDADDRS, &io)) {
		set_system_error(error, "ioctl(DIOCRADDADDRS) failed");
		return false;
	}
	if (0 != (ret = getaddrinfo(parsed_addr.humanrepr, NULL, NULL, &res))) {
		set_system_error(error, "getaddrinfo failed: %s", gai_strerror(ret));
		return false;
	}
	for (resp = res; NULL != resp; resp = resp->ai_next) {
		if (NULL == resp->ai_addr) {
			continue;
		}
		if (0 == memcmp(&last_src, resp->ai_addr, sizeof(last_src))) {
			continue;
		}
		last_src = *((struct sockaddr *) resp->ai_addr);
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
				set_generic_error(error, "Unknown address family %d", psk.psk_af);
		}
		if (-1 == ioctl(data->fd, DIOCKILLSTATES, &psk)) {
			set_system_error(error, "ioctl(DIOCKILLSTATES) failed");
		}
	}
	freeaddrinfo(res);

	return true;
}

static void pf_close(void *ctxt)
{
	pf_data_t *data;

	data = (pf_data_t *) ctxt;
	if (-1 != data->fd) {
		close(data->fd);
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
