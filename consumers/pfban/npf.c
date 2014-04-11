#include <net/npf/npf.h>
// #include <net/pfvar.h>

#include "engine.h"

typedef struct {
    int device;
} npf_data_t;

static void *npf_open(void)
{
    npf_data_t *data;

    data = malloc(sizeof(*data));
    if (-1 == (data->device = open("/dev/npf", O_WRONLY))) {
        errc("failed opening /dev/npf");
    }

    return data;
}

static int npf_handle(void *ctxt, const char *tablename, const char *buffer)
{
    int ret;
    npf_data_t *data;
    npf_ioctl_table_t nct;
    struct sockaddr_in sin;
    struct sockaddr_in6 sin6;

    data = (npf_data_t *) ctxt;
//     memset(&nct, 0, sizeof(nct));
    bzero(&nct, sizeof(nct));
    nct.nct_name = tablename;
    nct.nct_cmd = NPF_CMD_TABLE_ADD;
    nct.nct_data.ent.mask = NPF_NO_NETMASK;
    if (1 == inet_pton(AF_INET, buffer, &sin.sin_addr)) {
        nct.nct_data.ent.alen = sizeof(struct in_addr);
        memcpy(&nct.nct_data.ent.addr, &sin.sin_addr, sizeof(sin.sin_addr));
    } else if (1 == inet_pton(AF_INET6, buffer, &sin6.sin_addr)) {
        nct.nct_data.ent.alen = sizeof(struct in6_addr);
        memcpy(&nct.nct_data.ent.addr, &sin6.sin6_addr, sizeof(sin6.sin6_addr));
    } else {
        warn("Valid address expected, got: %s", buffer);
    }
    if (-1 == ioctl(data->device, IOC_NPF_TABLE, &nct)) {
        errc("ioctl(IOC_NPF_TABLE) failed");
    }

    return 1;
}

static void npf_close(void *ctxt)
{
    npf_data_t *data;

    data = (npf_data_t *) ctxt;
    if (-1 != data->device) {
        if (0 != close(data->device)) {
            warnc("closing /dev/npf failed");
        }
        data->device = -1;
    }
}

const engine_t npf_engine = {
    "npf",
    npf_open,
    npf_handle,
    npf_close
};
