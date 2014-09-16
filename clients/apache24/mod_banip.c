#include "apr.h"
#include "apr_hash.h"
#include "apr_strings.h"
#include "ap_config.h"
#include "ap_provider.h"
#include "httpd.h"
#include "http_core.h"
#include "http_config.h"
#include "http_log.h"
#include "http_protocol.h"
#include "http_request.h"

#include "common.h"
#include "queue.h"

#define BANIP_PREFIX "BanIP"
#define BANIP_HEADER "X-BanIP"
#define BANIP_FILTER "XBANIP"

module AP_MODULE_DECLARE_DATA banip_module;

// http://httpd.apache.org/docs/trunk/developer/modguide.html
#if 0
apr_table_t *GET;
ap_args_to_table(r, &GET);
#endif

typedef struct {
    char *pattern;
    ap_regex_t *regexp;
} banip_rule;

typedef struct {
    char *queue;
    int enabled;
    apr_array_header_t *rules;
} banip_server_conf;

typedef struct {
    apr_array_header_t *rules;
} banip_perdir_conf;

static void *queue;

/* ======================== configuration creation/merging ========================  */

static void *config_server_create(apr_pool_t *p, server_rec *s)
{
    banip_server_conf *ret;

    ret = (banip_server_conf *) apr_pcalloc(p, sizeof(*ret));
    ret->enabled = 0;
    ret->queue = NULL;
    ret->rules = apr_array_make(p, 2, sizeof(banip_rule));

    return (void *) ret;
}

static void *config_server_merge(apr_pool_t *p, void *basev, void *overridesv)
{
    banip_server_conf *ret, *base, *overrides;

    overrides = (banip_server_conf *) overridesv;
    ret = (banip_server_conf *) apr_pcalloc(p, sizeof(*ret));
    ret->rules = apr_array_append(p, base->rules, overrides->rules);

    return (void *) ret;
}

static void *config_perdir_create(apr_pool_t *p, char *path)
{
    banip_perdir_conf *ret;

    ret = (banip_perdir_conf *) apr_pcalloc(p, sizeof(*ret));
    ret->rules = apr_array_make(p, 2, sizeof(banip_rule));

    return (void *) ret;
}

static void *config_perdir_merge(apr_pool_t *p, void *basev, void *overridesv)
{
    banip_perdir_conf *ret, *base, *overrides;

    overrides = (banip_perdir_conf *) overridesv;
    ret = (banip_perdir_conf *) apr_pcalloc(p, sizeof(*ret));
    ret->rules = apr_array_append(p, base->rules, overrides->rules);

    return (void *) ret;
}

/* ======================== ? ========================  */

static int banip_queue_send_message(request_rec *r)
{
    banip_server_conf *sconf;

    sconf = (banip_server_conf *) ap_get_module_config(r->server->module_config, &banip_module);
    if (QUEUE_ERR_OK == queue_send(queue, r->useragent_ip, -1)) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "Message '%s' sent on mqueue '%s'", r->useragent_ip, sconf->queue);
    } else {
        ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, "Failed sending message '%s' on '%s'", r->useragent_ip, sconf->queue);
    }
}

static apr_status_t banip_queue_close(void *UNUSED(data))
{
    if (NULL != queue) {
        queue_close(&queue);
    }

    return OK;
}

static int banip_post_config(apr_pool_t *p, apr_pool_t *plog, apr_pool_t *ptemp, server_rec *s)
{
    banip_server_conf *sconf;

    if (AP_SQ_MS_CREATE_PRE_CONFIG == ap_state_query(AP_SQ_MAIN_STATE)) {
        return OK;
    }
    sconf = (banip_server_conf *) ap_get_module_config(s->module_config, &banip_module);
    if (sconf->enabled) {
        if (NULL == sconf->queue) {
            ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, "A '" BANIP_PREFIX "Queue' directive is missing to set queue name");
            return HTTP_INTERNAL_SERVER_ERROR;
        }
        if (NULL == (queue = queue_init())) {
            ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, "Failed to initialize a queue");
            return HTTP_INTERNAL_SERVER_ERROR;
        }
        if (QUEUE_ERR_OK != queue_open(queue, sconf->queue, QUEUE_FL_SENDER)) {
            ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, "Failed to open queue '%s'", sconf->queue);
            queue_close(&queue);
            return HTTP_INTERNAL_SERVER_ERROR;
        }
        apr_pool_cleanup_register(p, NULL, banip_queue_close, apr_pool_cleanup_null);
    }

    return OK;
}

static int banip_fixup(request_rec *r)
{
    int i;
    banip_rule *rules;
    banip_perdir_conf *dconf;
    banip_server_conf *sconf;

    sconf = (banip_server_conf *) ap_get_module_config(r->server->module_config, &banip_module);
    dconf = (banip_perdir_conf *) ap_get_module_config(r->per_dir_config, &banip_module);

    if (!sconf->enabled) {
        return DECLINED;
    }

    rules = (banip_rule *) sconf->rules->elts;
    for (i = 0; i < sconf->rules->nelts; i++) {
        int match;

        match = 0;
        if (NULL == rules[i].regexp) {
            match = NULL != strstr(rules[i].pattern, r->uri);
        } else {
            match = 0 == ap_regexec(rules[i].regexp, r->uri, 0, NULL, 0);
        }

        ap_log_rerror(APLOG_MARK, APLOG_DEBUG + 1, 0, r, "%s %s %s", r->uri, match ? "matches" : "doesn't match", rules[i].pattern);
        if (match) {
            banip_queue_send_message(r);
#if 0
            return DONE;
#else
            return HTTP_FORBIDDEN;
#endif
        }
    }

    return DECLINED;
}

/* ======================== commands ========================  */

static const char *cmd_banip_enable(cmd_parms *cmd, void *cfg, int flag)
{
    banip_perdir_conf *dconf;
    banip_server_conf *sconf;

    dconf = cfg;
    sconf = (banip_server_conf *) ap_get_module_config(cmd->server->module_config, &banip_module);
    if (NULL == cmd->path) {
        sconf->enabled = flag;
    } else {
        //
    }

    return NULL;
}

static const char *cmd_banip_queue(cmd_parms *cmd, void *cfg, const char *arg)
{
    banip_server_conf *sconf;

    sconf = (banip_server_conf *) ap_get_module_config(cmd->server->module_config, &banip_module);
    sconf->queue = arg;

    return NULL;
}

static const char *cmd_banip_rule(cmd_parms *cmd, void *cfg, const char *arg)
{
    banip_rule *rule;
    banip_perdir_conf *dconf;
    banip_server_conf *sconf;

    dconf = cfg;
    sconf = (banip_server_conf *) ap_get_module_config(cmd->server->module_config, &banip_module);
    if (NULL == cmd->path) {
        rule = apr_array_push(sconf->rules);
    } else {
        rule = apr_array_push(dconf->rules);
    }
    rule->pattern = arg;
    if (1) {
        if (NULL == (rule->regexp = ap_pregcomp(cmd->pool, arg, AP_REG_EXTENDED))) {
            return apr_pstrcat(cmd->pool, BANIP_PREFIX "Rule: cannot compile regular expression '", arg, "'", NULL);
        }
    } else {
        rule->regexp = NULL;
    }

    return NULL;
}

#ifndef WITHOUT_OUTPUT_FILTER
static apr_status_t banip_output_filter(ap_filter_t *f, apr_bucket_brigade *in)
{
    int header_found;
    request_rec *r;

    r = f->r;
    header_found = 0;
    if (NULL != apr_table_get(r->headers_out, BANIP_HEADER) || NULL != apr_table_get(r->err_headers_out, BANIP_HEADER)) {
        header_found = 1;
        banip_queue_send_message(r);
        apr_table_unset(r->headers_out, BANIP_HEADER);
        apr_table_unset(r->err_headers_out, BANIP_HEADER);
    }
    ap_remove_output_filter(f);
    if (header_found) {
        ap_die(HTTP_FORBIDDEN, r);
        return HTTP_FORBIDDEN;
    } else {
        return ap_pass_brigade(f->next, in);
    }
}

static void banip_insert_output_filter(request_rec *r)
{
    banip_server_conf *sconf;

    sconf = (banip_server_conf *) ap_get_module_config(r->server->module_config, &banip_module);
    if (sconf->enabled) {
        ap_add_output_filter(BANIP_FILTER, NULL, r, r->connection);
    }
}
#endif /* !WITHOUT_OUTPUT_FILTER */

static const command_rec command_table[] = {
    AP_INIT_FLAG(BANIP_PREFIX "Enable", cmd_banip_enable, NULL, OR_FILEINFO, "TODO"),
    AP_INIT_TAKE1(BANIP_PREFIX "Queue", cmd_banip_queue, NULL, RSRC_CONF, "TODO"),
    AP_INIT_TAKE1(BANIP_PREFIX "Rule", cmd_banip_rule, NULL, OR_FILEINFO, "TODO"),
#if 0
    AP_INIT_ITERATE(BANIP_PREFIX "Policy", cmd_banip_policy, NULL, RSRC_CONF, "TODO"),
    AP_INIT_TAKE1(BANIP_PREFIX "Behavior", cmd_banip_behavior, NULL, OR_FILEINFO, "TODO"),
#endif
    { NULL }
};

static void register_hooks(apr_pool_t *p)
{
    ap_hook_fixups(banip_fixup, NULL, NULL, APR_HOOK_FIRST);
    ap_hook_post_config(banip_post_config, NULL, NULL, APR_HOOK_FIRST);
#ifndef WITHOUT_OUTPUT_FILTER
    ap_register_output_filter(BANIP_FILTER, banip_output_filter, NULL, AP_FTYPE_CONTENT_SET);
    ap_hook_insert_filter(banip_insert_output_filter, NULL, NULL, APR_HOOK_FIRST);
#endif /* !WITHOUT_OUTPUT_FILTER */
}

AP_DECLARE_MODULE(banip) = {
   STANDARD20_MODULE_STUFF,
   config_perdir_create,        /* create per-dir    config structures */
   config_perdir_merge,         /* merge  per-dir    config structures */
   config_server_create,        /* create per-server config structures */
   config_server_merge,         /* merge  per-server config structures */
   command_table,               /* table of config file commands       */
   register_hooks               /* register hooks                      */
};
