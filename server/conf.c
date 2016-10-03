#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pwd.h>
#include <assert.h>

#include "config.h"
#ifdef HAVE_LIBBSD_STRLCPY
# include <bsd/string.h>
#endif /* !HAVE_LIBBSD_STRLCPY */
#include "common.h"
#include "conf.h"
#include "banip.h"

extern struct passwd *mygetpwnam(cap_channel_t *, const char *);

typedef struct {
	/**
	 * On FreeBSD:
	 * - in /usr/include/sys/limits.h, UID_MAX is defined as UINT_MAX
	 * - in /usr/include/sys/_types.h, __uid_t is typedef as __uint32_t
	 * So we are far from 65536... Use an hashtable.
	 **/
	uint32_t rights[65536];
} conf_t;

#define SKIP_SPACES(s, end) \
	do { \
		while (s < end && (' ' == *s || '\t' == *s)) { \
			++s; \
		} \
	} while(0);

static void conf_add(conf_t *conf, uid_t connect, uid_t send, uint32_t flags)
{
	assert(connect < ARRAY_SIZE(conf->rights));
	assert(send < ARRAY_SIZE(conf->rights));

	conf->rights[connect] |= CONNECT;
	conf->rights[send] |= flags | SEND;
}

static bool conf_parseline(conf_t *conf, size_t lineno, char *line, size_t line_len, cap_channel_t *chpwd, char **error)
{
	uint32_t flags;
	char *s, *p, *end;
	struct passwd *pwd;
	uid_t connect, send;

	flags = SAFE;
	if (NULL != (p = strchr(line, '#'))) {
		*p = '\0';
		line_len = p - line - 1;
	}
	if ('\n' == line[line_len - 1]) {
		--line_len;
	}
	if ('\r' == line[line_len - 1]) {
		--line_len;
	}
	s = line;
	line[line_len] = '\0';
	end = line + line_len;
	SKIP_SPACES(s, end);
	if ('\0' == *s) {
		// blank line
		return true;
	} else if (NULL != (p = strpbrk(s, ": \t"))) {
		char sep;

		sep = *p;
		*p = '\0';
		if (NULL != (pwd = mygetpwnam(chpwd, s))) {
			s = p + 1;
			connect = pwd->pw_uid;
			if (':' == sep) {
				if (NULL != (p = strpbrk(s, " \t"))) {
					*p = '\0';
					if (NULL != (pwd = mygetpwnam(chpwd, s))) {
						s = p + 1;
						send = pwd->pw_uid;
					} else {
						// invalid (not a valid user)
						set_generic_error(error, "%s is not a valid user name on line %zu, offset %zu", s, lineno, s - line);
						goto err;
					}
				} else {
					// invalid (expects a space)
					set_generic_error(error, "space expected on line %zu, offset %zu", lineno, s - line);
					goto err;
				}
			} else {
				send = connect;
			}
			SKIP_SPACES(s, end);
			while (NULL != (p = strchr(s, ','))) {
				*p = '\0';
				if (0 == strcmp("safe", s)) {
					/* NOP */
				} else if (0 == strcmp("unsafe", s)) {
					flags |= UNSAFE;
				} else {
					// invalid (unexpected token s)
					set_generic_error(error, "unexpected token '%s' instead of 'safe' or 'unsafe' on line %zu, offset %zu", p, lineno, s - line);
					goto err;
				}
				SKIP_SPACES(s, end);
				s = p + 1;
			}
			conf_add(conf, connect, send, flags);
			return true;
		} else {
			// invalid (not a valid user)
			set_generic_error(error, "%s is not a valid user name on line %zu, offset %zu", s, lineno, s - line);
			goto err;
		}
	} else {
		// invalid (expects a space or ':')
		set_generic_error(error, "space or ':' expected on line %zu, offset %zu", lineno, s - line);
		goto err;
	}

err:
	return false;
}

void conf_destroy(void *ptr)
{
	free(ptr);
}

bool conf_parse(void *oldconf, void **newconf, FILE *fp, cap_channel_t *chpwd, char **error)
{
	conf_t *conf;
	size_t lineno;
	char line[4096];

	assert(NULL != fp);
	lineno = 0;
	*newconf = conf = NULL;
	if (NULL == (conf = malloc(sizeof(*conf)))) {
		set_system_error(error, "malloc failed");
		goto err;
	}
	bzero(conf, sizeof(*conf));
	while (NULL != fgets(line, ARRAY_SIZE(line), fp)) {
		debug("line = %s", line);
		if (!conf_parseline(conf, ++lineno, line, strlen(line), chpwd, error)) {
			goto err;
		}
	}
	*newconf = conf;
	if (NULL != oldconf) {
		free(oldconf); // conf_destroy?
	}

	if (false) {
err:
		free(conf); // conf_destroy?
	}
	fclose(fp);

	return NULL != *newconf;
}

bool conf_is_authorized(void *rawconf, uid_t uid, uint32_t flags)
{
	conf_t *conf;

	assert(uid < ARRAY_SIZE(conf->rights));

	conf = (conf_t *) rawconf;
	if (HAS_FLAG(flags, CONNECT)) {
		return HAS_FLAG(conf->rights[uid], CONNECT);
	} else {
		return HAS_FLAG(conf->rights[uid], SEND | flags);
	}
}
