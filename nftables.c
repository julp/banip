#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <linux/netfilter.h>
#include <linux/netfilter/nf_tables.h>
#include <libmnl/libmnl.h>
#include <libnftnl/set.h>

#include "engine.h"

/*
nft add table ip filter
nft add chain ip filter input
nft add set filter blacklist { type ipv4_address\;}
nft add rule ip filter input ip saddr @blacklist drop

# add element(s) to blacklist
nft add element filter blacklist { 192.168.3.4 }

# print blacklist elements
nft list set filter blacklist
*/

/*
#define MNL_SOCKET_BUFFER_SIZE (getpagesize() < 8192L ? getpagesize() : 8192L)
*/
#undef MNL_SOCKET_BUFFER_SIZE
#define MNL_SOCKET_BUFFER_SIZE 8192L

typedef struct {
    uint32_t portid;
    struct nft_set *s;
    struct mnl_socket *nl;
    struct nft_set_elem *e;
    char buf[MNL_SOCKET_BUFFER_SIZE];
} nftables_data_t;

static void *nftables_open(void)
{
    nftables_data_t *data;

    data = malloc(sizeof(*data));
    if (NULL == (data->nl = mnl_socket_open(NETLINK_GENERIC))) {
        errc("mnl_socket_open failed");
    }
    if (mnl_socket_bind(data->nl, 0, MNL_SOCKET_AUTOPID) < 0) {
        errc("mnl_socket_bind failed");
    }
    data->portid = mnl_socket_get_portid(data->nl);
    data->s = nft_set_alloc();
    data->e = nft_set_elem_alloc();
    nft_set_elem_add(data->s, data->e);
    nft_set_attr_set(data->s, NFT_SET_ATTR_TABLE, "filter");

    return data;
}

static int nftables_handle(void *ctxt, const char *tablename, const char *buffer)
{
    int ret;
    nftables_data_t *data;
    struct in_addr addr4;
    struct in6_addr addr6;
    struct nlmsghdr *nlh;
    uint32_t seq, family;

    data = (nftables_data_t *) ctxt;
    seq = time(NULL);
    if (1 == inet_pton(AF_INET, buffer, &addr4)) {
        family = NFPROTO_IPV4;
        nft_set_attr_set(data->s, NFT_SET_ATTR_NAME, tablename);
        nft_set_elem_attr_set(data->e, NFT_SET_ELEM_ATTR_KEY, &addr4, sizeof(addr4));
    } else if (1 == inet_pton(AF_INET6, buffer, &addr6)) {
        family = NFPROTO_IPV6;
        nft_set_attr_set(data->s, NFT_SET_ATTR_NAME, tablename); // TODO: le nom doit être différent ip/ip6?
        nft_set_elem_attr_set(data->e, NFT_SET_ELEM_ATTR_KEY, &addr6, sizeof(addr6));
    } else {
        warn("Valid address expected, got: %s", buffer);
    }
    nlh = nft_set_nlmsg_build_hdr(data->buf, NFT_MSG_NEWSETELEM, family, NLM_F_CREATE | NLM_F_EXCL | NLM_F_ACK, seq);
    nft_set_elems_nlmsg_build_payload(nlh, data->s);
//     data->portid = mnl_socket_get_portid(data->nl);
    if (mnl_socket_sendto(data->nl, nlh, nlh->nlmsg_len) < 0) {
        errc("mnl_socket_sendto failed");
    }
    ret = mnl_socket_recvfrom(data->nl, data->buf, MNL_SOCKET_BUFFER_SIZE);
    while (ret > 0) {
        if ((ret = mnl_cb_run(data->buf, ret, seq, data->portid, NULL, NULL)) <= 0) {
            break;
        }
        ret = mnl_socket_recvfrom(data->nl, data->buf, MNL_SOCKET_BUFFER_SIZE);
    }
    if (-1 == ret) {
        errc("mnl_socket_recvfrom failed"); // ENOENT 2 /* No such file or directory */
    }

    return 1;
}

static void nftables_close(void *ctxt)
{
    nftables_data_t *data;

    data = (nftables_data_t *) ctxt;
    if (NULL != data->s) {
        nft_set_free(data->s);
        data->s = NULL;
    }
    if (NULL != data->nl) {
        mnl_socket_close(data->nl);
        data->nl = NULL;
    }
}

const engine_t nftables_engine = {
    "nftables",
    nftables_open,
    nftables_handle,
    nftables_close
};
