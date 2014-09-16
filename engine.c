#include <string.h>

#include "engine.h"

#ifdef WITH_PF
extern engine_t pf_engine;
#endif /* PF */

#ifdef WITH_NPF
extern engine_t npf_engine;
#endif /* NPF */

#ifdef WITH_NFTABLES
extern engine_t nftables_engine;
#endif /* NFTABLES */

#ifdef WITH_NFTABLES
extern engine_t ipset_engine;
#endif /* NFTABLES */

#ifdef WITH_IPTABLES
extern engine_t iptables_engine;
#endif /* WITH_IPTABLES */

extern engine_t dummy_engine;

static const engine_t *available_engines[] = {
#ifdef WITH_PF
    &pf_engine,
#endif /* PF */
#ifdef WITH_NPF
    &npf_engine,
#endif /* NPF */
#ifdef WITH_NFTABLES
    &nftables_engine,
#endif /* NFTABLES */
#ifdef WITH_NFTABLES
    &ipset_engine,
#endif /* NFTABLES */
#ifdef WITH_IPTABLES
    &iptables_engine,
#endif /* WITH_IPTABLES */
    &dummy_engine,
    NULL
};

const engine_t *get_default_engine(void)
{
    /* NULL can be returned, caller is responsible to check this later */
    return available_engines[0];
}

const engine_t *get_engine_by_name(const char *name)
{
    const engine_t **e;

    for (e = available_engines; NULL != *e; e++) {
        if (0 == strcmp((*e)->name, name)) {
            return *e;
        }
    }

    return NULL;
}
