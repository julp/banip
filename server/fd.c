#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "config.h"
#ifdef HAVE_LIBBSD_STRLCPY
# include <bsd/string.h>
#endif /* !HAVE_LIBBSD_STRLCPY */
#include "common.h"
#include "banip.h"
#include "fd.h"
#include "capsicum.h"

typedef uint64_t cap_right_t;

struct{
	int flag;
	cap_right_t right;
} static const map[] = {
	{ O_CREAT, CAP_CREATE },
	{ O_TRUNC, CAP_FTRUNCATE },
	{ O_RDONLY, CAP_READ },
	{ O_WRONLY, CAP_SEEK },
	{ O_WRONLY, CAP_WRITE },
};

struct fd_t {
	int fd, dirfd, flags;
	const char *dirname, *filename;
	cap_rights_t rights;
};

fd_t *fd_wrap(const char *filename, int flags, bool sandboxed, char **error)
{
	fd_t *fd;
	char *p, dirname[PATH_MAX];

	if (NULL == (fd = malloc(sizeof(*fd)))) {
		set_system_error(error, "malloc failed");
		goto err;
	}
	fd->flags = flags;
	fd->fd = fd->dirfd = -1;
	fd->filename = fd->dirname = NULL;
	if (NULL == (p = strrchr(filename, '/'))) {
		if (strlcpy(dirname, ".", STR_SIZE(dirname)) >= STR_SIZE(dirname)) {
			set_generic_error(error, "buffer overflow: path '%s' too long", ".");
			goto err;
		}
		fd->filename = strdup(filename);
	} else {
		*p = '\0';
		if (strlcpy(dirname, filename, STR_SIZE(dirname)) >= STR_SIZE(dirname)) {
			set_generic_error(error, "buffer overflow: path '%s' too long", p);
			goto err;
		}
		fd->filename = strdup(p + 1);
	}
	if (NULL == (fd->dirname = realpath(dirname, NULL))) {
		set_system_error(error, "realpath(\"%s\")", dirname);
		goto err;
	} else {
		size_t i;
		cap_rights_t rights;

		if (-1 == (fd->dirfd = open(fd->dirname, O_RDONLY | O_DIRECTORY))) {
			set_system_error(error, "can't open directory '%s'", dirname);
			goto err;
		}
		/**
		 * NOTE:
		 * - CAP_LOOKUP (dirfd) is mandatory to openat
		 * - CAP_FCNTL (dirfd + "children") is needed for internal fcntl(fd, F_GETFL, 0) done by fdopen
		 **/
		cap_rights_init(&rights, CAP_LOOKUP);
		cap_rights_init(&fd->rights, CAP_FCNTL);
		for (i = 0; i < ARRAY_SIZE(map); i++) {
			// NOTE: fd->flags == map[i].flag is to handle the specific case of 0 (O_RDONLY) that HAS_FLAG can't
			if (HAS_FLAG(fd->flags, map[i].flag) || (fd->flags == map[i].flag)) {
				cap_rights_set(&fd->rights, map[i].right);
			}
		}
		cap_rights_merge(&rights, &fd->rights);
		if (0 != cap_rights_limit(fd->dirfd, &rights)) {
			set_system_error(error, "cap_rights_limit(\"%s\") failed", fd->dirname);
			goto err;
		}
	}

	if (false) {
err:
		if (NULL != fd) {
			if (NULL != fd->dirname) {
				free((void *) fd->dirname);
			}
			if (-1 != fd->dirfd) {
				close(fd->dirfd);
			}
		}
		fd = NULL;
	}

	return fd;
}

FILE *fd_reopen(fd_t *fd, char **error)
{
	FILE *fp;
	const char *mode;

	fp = NULL;
	if (-1 != fd->fd) {
		close(fd->fd);
		fd->fd = -1;
	}
// debug("dirfd = %d (%s//%s)", fd->dirfd, fd->dirname, fd->filename);
	if (HAS_FLAG(fd->flags, O_CREAT)) {
		fd->fd = openat(fd->dirfd, fd->filename, fd->flags, 0644);
	} else {
		fd->fd = openat(fd->dirfd, fd->filename, fd->flags);
	}
	if (-1 == fd->fd) {
		set_system_error(error, "openat \"%s/%s\" failed", fd->dirname, fd->filename);
		goto err;
	} else {
		// NOTE: at most, fd->fd can only have the same rights than fd->dirfd. Not more! But you can restrict/lower them.
		if (0 != cap_rights_limit(fd->fd, &fd->rights)) {
			set_system_error(error, "cap_rights_limit(\"%s/%s\") failed", fd->dirname, fd->filename);
			goto err;
		}
	}
	// NOTE: for a complete implementation, see lib/libc/stdio/flags.c (which does the opposite)
	if (HAS_FLAG(fd->flags, O_WRONLY)) {
		mode = HAS_FLAG(fd->flags, O_APPEND) ? "a" : "w";
	} else {
		mode = "r";
	}
	if (NULL == (fp = fdopen(fd->fd, mode))) {
		set_system_error(error, "fdopen \"%s/%s\" failed", fd->dirname, fd->filename);
		goto err;
	}

	if (false) {
err:
		if (-1 != fd->fd) {
			close(fd->fd);
		}
	}

	return fp;
}

void fd_close(fd_t *fd)
{
	if (-1 != fd->fd) {
		close(fd->fd);
	}
	if (-1 != fd->dirfd) {
		close(fd->dirfd);
	}
	free((void *) fd->dirname);
	free((void *) fd->filename);
	free(fd);
}

#if 0
const char *fd_dirname(fd_t *fd)
{
	return fd->dirname;
}

const char *fd_basename(fd_t *fd)
{
	return fd->filename;
}
#endif
