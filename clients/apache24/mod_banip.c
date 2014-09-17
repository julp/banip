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
#include "util_script.h"

#include "common.h"
#include "queue.h"

#define BANIP_PREFIX "BanIP"
#define BANIP_HEADER "X-BanIP"
#define BANIP_FILTER "XBANIP"

module AP_MODULE_DECLARE_DATA banip_module;

// http://httpd.apache.org/docs/trunk/developer/modguide.html

typedef struct _pattern_compiler pattern_compiler;

typedef struct {
    char *what;
    char *pattern;
    ap_regex_t *regexp;
    pattern_compiler *compiler;
} banip_rule;

struct _pattern_compiler {
    char indicator;
    char *(*compile)(banip_rule *, apr_pool_t *, char *); // returns: NULL for success/error message for failure
    int (*compare)(const banip_rule const *, const char const *); // returns: 0 if equals, < 0 if inferior, > 0 if superior
};

typedef struct {
    char *queue;
    int enabled;
    apr_array_header_t *rules;
} banip_server_conf;

typedef struct {
    apr_table_t *get;
    apr_array_header_t *rules;
} banip_perdir_conf;

static void *queue = NULL;

/* ======================== ? ========================  */

static char *regex_compile(banip_rule *rule, apr_pool_t *pool, char *pattern)
{
    char *ret;

    ret = NULL;
    rule->pattern = pattern;
    if (NULL == (rule->regexp = ap_pregcomp(pool, pattern, AP_REG_EXTENDED))) {
        ret = apr_pstrcat(pool, BANIP_PREFIX "Rule: cannot compile regular expression '", pattern, "'", NULL);
    }

    return ret;
}

static int regex_compare(const banip_rule const *rule, const char const *string)
{
    return ap_regexec(rule->regexp, string, 0, NULL, 0);
}

static char *string_compile(banip_rule *rule, apr_pool_t *pool, char *pattern)
{
    rule->pattern = ++pattern; /* skip initial '=' */

    return NULL;
}

static int string_compare(const banip_rule const *rule, const char const *string)
{
    return strcmp(rule->pattern, string);
}

static const pattern_compiler pattern_compilers[] = {
    { '\0', regex_compile, regex_compare }, /* 0 is default */
    { '=', string_compile, string_compare },
    // !, !=, <, >, >=, <=
};

// static unsigned char pattern_compilers_map[256];

// hide details and do initialization of banip_rule
static char *compile_rule(banip_rule *rule, apr_pool_t *pool, char *pattern)
{
    rule->regexp = NULL;
    if ('=' == pattern[0]) {
        rule->compiler = &pattern_compilers[1];
    } else {
        rule->compiler = &pattern_compilers[0];
    }
//     rule->compiler = &pattern_compilers[pattern_compilers_map[(unsigned char) pattern[0]]];

    return rule->compiler->compile(rule, pool, pattern);
}

/* ======================== ? ========================  */

const char *lookup_default(const char *name, request_rec *r)
{
    return r->uri;
}

const char *lookup_env(const char *name, request_rec *r)
{
    const char *ret;

    ret = NULL;
    if (NULL == (ret = apr_table_get(r->subprocess_env, name))) {
        if (NULL == (ret = apr_table_get(r->notes, name))) {
            ret = getenv(name);
        }
    }

    return ret;
}

const char *lookup_http(const char *name, request_rec *r)
{
    return apr_table_get(r->headers_in, name);
}

const char *lookup_get(const char *name, request_rec *r)
{
    banip_perdir_conf *dconf;

    dconf = (banip_perdir_conf *) ap_get_module_config(r->per_dir_config, &banip_module);
    if (NULL == dconf->get) {
        ap_args_to_table(r, &dconf->get);
    }

    return apr_table_get(dconf->get, name);
}

const char *lookup_post(const char *name, request_rec *r)
{
    return NULL;
}

struct {
    const char *prefix;
    size_t prefix_len;
    const char *(*lookup)(const char *, request_rec *);
} static const variables[] = {
    { NULL, 0, lookup_default },
    { "ENV:", 4, lookup_env },
    { "GET:", 4, lookup_get },
    { "HTTP:", 5, lookup_http },
    { "POST:", 5, lookup_post },
};

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
    ret->get = NULL;
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

    // TODO: we haven't yet drop root privileges here?
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
    banip_rule *rules, *rule;
    banip_perdir_conf *dconf;
    banip_server_conf *sconf;

    sconf = (banip_server_conf *) ap_get_module_config(r->server->module_config, &banip_module);
    dconf = (banip_perdir_conf *) ap_get_module_config(r->per_dir_config, &banip_module);

    if (!sconf->enabled) {
        return DECLINED;
    }

    rules = (banip_rule *) sconf->rules->elts;
    for (i = 0; i < sconf->rules->nelts; i++) {
        size_t j;
        int match;
        const char *string;

        rule = &rules[i];
        string = NULL;
        for (j = 0; j < ARRAY_SIZE(variables); j++) {
            if (variables[j].prefix == rule->what || (NULL != variables[j].prefix && 0 == strncmp(variables[j].prefix, rule->what, variables[j].prefix_len))) {
                string = variables[j].lookup(rule->what + variables[j].prefix_len, r);
                break;
            }
        }
        if (NULL == string) {
            continue;
        }
        match = rule->compiler->compare(&rules[i], string);
        ap_log_rerror(APLOG_MARK, APLOG_ERR/*APLOG_DEBUG + 1*/, 0, r, "%s %s %s", string, 0 == match ? "matches" : "doesn't match", rule->pattern);
        if (0 == match) {
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

static const char *cmd_banip_rule(cmd_parms *cmd, void *cfg, int argc, char *const argv[])
{
    const char *ret;
    banip_rule *rule;
    char *what, *pattern;
    banip_perdir_conf *dconf;
    banip_server_conf *sconf;

    dconf = cfg;
    ret = what = NULL;
    sconf = (banip_server_conf *) ap_get_module_config(cmd->server->module_config, &banip_module);
    if (1 == argc) {
        pattern = argv[0];
    } else if (2 == argc) {
        what = argv[0];
        pattern = argv[1];
    } else {
        return apr_pstrcat(cmd->pool, BANIP_PREFIX "Rule: too many arguments", NULL);
    }
    if (NULL == cmd->path) {
        rule = apr_array_push(sconf->rules);
    } else {
        rule = apr_array_push(dconf->rules);
    }
    if (NULL != (ret = compile_rule(rule, cmd->pool, pattern))) {
        if (NULL != what) {
//             ret = compile_what();
        }
    }
    rule->what = what; /* TODO */

    return ret;
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
    AP_INIT_TAKE_ARGV(BANIP_PREFIX "Rule", cmd_banip_rule, NULL, OR_FILEINFO, "TODO"),
#if 0
    AP_INIT_ITERATE(BANIP_PREFIX "Policy", cmd_banip_policy, NULL, RSRC_CONF, "TODO"),
    AP_INIT_TAKE1(BANIP_PREFIX "Behavior", cmd_banip_behavior, NULL, OR_FILEINFO, "TODO"),
#endif
    { NULL }
};

static void register_hooks(apr_pool_t *p)
{
/*
    size_t i;

    bzero(&pattern_compilers_map, ARRAY_SIZE(pattern_compilers_map));
    for (i = 1; i < ARRAY_SIZE(pattern_compilers); i++) {
        pattern_compilers_map[(unsigned char) pattern_compilers[i].indicator] = i;
    }
*/

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
