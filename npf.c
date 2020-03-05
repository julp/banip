#include <unistd.h>
#include <fcntl.h>
#include <net/npf.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

#include "err.h"
#include "engine.h"

typedef struct {
    int fd;
} npf_data_t;

static void *npf_open(const char *UNUSED(tablename))
{
    npf_data_t *data;

    data = malloc(sizeof(*data));
    if (-1 == (data->fd = open("/dev/npf", O_WRONLY))) {
        errc("failed opening /dev/npf");
    }

    return data;
}

static int npf_handle(void *ctxt, const char *tablename, addr_t addr)
{
    int ret;
    npf_data_t *data;
    npf_ioctl_table_t nct;
    struct sockaddr_in sin;
    struct sockaddr_in6 sin6;

    data = (npf_data_t *) ctxt;
    bzero(&nct, sizeof(nct));
#ifdef NPF_ALLOW_NAMED_TABLE
    nct.nct_name = tablename;
#else
    nct.nct_tid = atoi(tablename);
#endif /* NPF_ALLOW_NAMED_TABLE */
    nct.nct_cmd = NPF_CMD_TABLE_ADD;
    nct.nct_data.ent.mask = NPF_NO_NETMASK;
#if 0
    // http://nxr.netbsd.org/xref/src/usr.sbin/npf/npfctl/npf_data.c#158
    uint8_t *ap;

    ap = addr.sa.v6 + (addr.netmask / 8) - 1;
    while (ap >= addr.sa.v6) {
        for (int j = 8; j > 0; j--) {
            if (*ap & 1) {
                break;
            }
            *ap >>= 1;
            --addr.netmask;
            if (0 == addr.netmask) {
                break;
            }
        }
        ap--;
    }
    nct.nct_data.ent.mask = addr.netmask;
#endif
    nct.nct_data.ent.alen = addr.sa_size;
    memcpy(&nct.nct_data.ent.addr, &addr.sa, addr.sa_size);
    if (-1 == ioctl(data->fd, IOC_NPF_TABLE, &nct)) {
        errc("ioctl(IOC_NPF_TABLE) failed");
    }

    return 1;
}

static void npf_close(void *ctxt)
{
    npf_data_t *data;

    data = (npf_data_t *) ctxt;
    if (-1 != data->fd) {
        if (0 != close(data->fd)) {
            warnc("closing /dev/npf failed");
        }
        data->fd = -1;
    }
}

const engine_t npf_engine = {
    true,
    "npf",
    npf_open,
    npf_handle,
    npf_close
};
