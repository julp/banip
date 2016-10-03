#pragma once

#include <sys/param.h> /* __FreeBSD__ */

#if defined(__FreeBSD__) && __FreeBSD__ >= 9

# include <stdio.h> /* perror */
# include <unistd.h>
# include <sys/capsicum.h> // previously sys/capability.h?

# define CAP_IOCTLS_LIMIT(fd, ...) \
	do { \
		unsigned long cmds[] = { __VA_ARGS__ }; \
 \
		if (0 != cap_ioctls_limit(fd, cmds, ARRAY_SIZE(cmds))) { \
			fprintf(stderr, "%s:%s:%d: fd is %d\n", __FILE__, __func__, __LINE__, (int) fd); \
			perror("cap_ioctls_limit"); \
		} \
	} while (0);

# define CAP_RIGHTS_LIMIT(fd, ...) \
	do { \
		cap_rights_t rights; \
 \
		cap_rights_init(&rights, ## __VA_ARGS__); \
		if (0 != cap_rights_limit(fd, &rights) && ENOSYS != errno) { \
			fprintf(stderr, "%s:%s:%d: fd is %d\n", __FILE__, __func__, __LINE__, (int) fd); \
			perror("cap_rights_limit"); \
		} \
	} while (0);

# define CAP_ENTER() \
	do { \
		if (0 != cap_enter() && ENOSYS != errno) { \
			perror("cap_enter"); \
		} \
	} while (0);

#else

static inline int cap_getmode(unsigned int *mode) {
	*mode = 0;      // 0 = not currently sandboxed
	errno = ENOSYS; // capsicum is not available
	return -1;      // -1 = failure
}

typedef uint32_t cap_rights_t;

# if 0
static inline *cap_rights_init(cap_rights_t *rights, ...)
{
	return rights;
}

static inline cap_rights_t *cap_rights_set(cap_rights_t *rights, ...)
{
	return rights;
}

static inline cap_rights_t *cap_rights_clear(cap_rights_t *rights, ...)
{
	return rights;
}

static inline bool cap_rights_is_set(const cap_rights_t *rights, ...)
{
	return true;
}

static inline bool cap_rights_is_valid(const cap_rights_t *rights)
{
	return true;
}

static inline cap_rights_t *cap_rights_merge(cap_rights_t *dst, const cap_rights_t *src)
{
	return dst;
}

static inline cap_rights_t *cap_rights_remove(cap_rights_t *dst, const cap_rights_t *src)
{
	return dst;
}

static inline bool cap_rights_contains(const cap_rights_t *big, const cap_rights_t *little)
{
	return true;
}
# else
#  define cap_rights_init(rights, ...) \
	/* NOP */

#  define cap_rights_set(rights, ...) \
	/* NOP */

#  define cap_rights_clear(rights, ...) \
	/* NOP */

#  define cap_rights_is_set(rights, ...) \
	false

#  define cap_rights_is_valid(rights) \
	true

#  define cap_rights_merge(dst, src) \
	dst

#  define cap_rights_remove(dst, src) \
	dst

#  define cap_rights_contains(big, little) \
	true
# endif

# define CAP_READ 0
# define CAP_WRITE 0
# define CAP_SEEK_TELL 0
# define CAP_SEEK 0
# define CAP_PREAD 0
# define CAP_PWRITE 0
# define CAP_MMAP 0
# define CAP_MMAP_R 0
# define CAP_MMAP_W 0
# define CAP_MMAP_X 0
# define CAP_MMAP_RW 0
# define CAP_MMAP_RX 0
# define CAP_MMAP_WX 0
# define CAP_MMAP_RWX 0
# define CAP_CREATE 0
# define CAP_FEXECVE 0
# define CAP_FSYNC 0
# define CAP_FTRUNCATE 0
# define CAP_LOOKUP 0
# define CAP_FCHDIR 0
# define CAP_FCHFLAGS 0
# define CAP_CHFLAGSAT 0
# define CAP_FCHMOD 0
# define CAP_FCHMODAT 0
# define CAP_FCHOWN 0
# define CAP_FCHOWNAT 0
# define CAP_FCNTL 0
# define CAP_FLOCK 0
# define CAP_FPATHCONF 0
# define CAP_FSCK 0
# define CAP_FSTAT 0
# define CAP_FSTATAT 0
# define CAP_FSTATFS 0
# define CAP_FUTIMES 0
# define CAP_FUTIMESAT 0
# define CAP_LINKAT_TARGET 0
# define CAP_MKDIRAT 0
# define CAP_MKFIFOAT 0
# define CAP_MKNODAT 0
# define CAP_RENAMEAT_SOURCE 0
# define CAP_SYMLINKAT 0
# define CAP_UNLINKAT 0
# define CAP_ACCEPT 0
# define CAP_BIND 0
# define CAP_CONNECT 0
# define CAP_GETPEERNAME 0
# define CAP_GETSOCKNAME 0
# define CAP_GETSOCKOPT 0
# define CAP_LISTEN 0
# define CAP_PEELOFF 0
# define CAP_RECV 0
# define CAP_SEND 0
# define CAP_SETSOCKOPT 0
# define CAP_SHUTDOWN 0
# define CAP_BINDAT 0
# define CAP_CONNECTAT 0
# define CAP_LINKAT_SOURCE 0
# define CAP_RENAMEAT_TARGET 0
# define CAP_SOCK_CLIENT 0
# define CAP_SOCK_SERVER 0
# define CAP_ALL0 0
# define CAP_UNUSED0_44 0
# define CAP_UNUSED0_57 0
# define CAP_MAC_GET 0
# define CAP_MAC_SET 0
# define CAP_SEM_GETVALUE 0
# define CAP_SEM_POST 0
# define CAP_SEM_WAIT 0
# define CAP_EVENT 0
# define CAP_KQUEUE_EVENT 0
# define CAP_IOCTL 0
# define CAP_TTYHOOK 0
# define CAP_PDGETPID 0
# define CAP_PDWAIT 0
# define CAP_PDKILL 0
# define CAP_EXTATTR_DELETE 0
# define CAP_EXTATTR_GET 0
# define CAP_EXTATTR_LIST 0
# define CAP_EXTATTR_SET 0
# define CAP_ACL_CHECK 0
# define CAP_ACL_DELETE 0
# define CAP_ACL_GET 0
# define CAP_ACL_SET 0
# define CAP_KQUEUE_CHANGE 0
# define CAP_KQUEUE 0
# define CAP_ALL1 0
# define CAP_UNUSED1_22 0
# define CAP_UNUSED1_57 0
# define CAP_POLL_EVENT 0
# define CAP_FCNTL_GETFL 0
# define CAP_FCNTL_SETFL 0
# define CAP_FCNTL_GETOWN 0
# define CAP_FCNTL_SETOWN 0
# define CAP_FCNTL_ALL 0
# define CAP_IOCTLS_ALL 0

# define CAP_IOCTLS_LIMIT(fd, ...) \
	/* NOP */

# define CAP_RIGHTS_LIMIT(fd, ...) \
	/* NOP */

# define CAP_ENTER() \
	/* NOP */

#endif /* FreeBSD >= 9.0 */

#if defined(__FreeBSD__) && __FreeBSD__ >= 10

# include <libcasper.h>
# include <casper/cap_pwd.h>
# include <casper/cap_grp.h>

#else

typedef char cap_channel_t;

# if 0
static inline cap_channel_t *cap_close(void) {
	return (void *) 1; // NULL = failure
}

static inline void cap_close(cap_channel_t *UNUSED(channel)) {
	/* NOP */
}

static inline int cap_fcntls_limit(int UNUSED(fd), uint32_t UNUSED(fcntlrights))
{
	return 0; // -1 = failure
}

static inline int cap_pwd_limit_fields(cap_channel_t *UNUSED(channel), const char * const * UNUSED(fields), size_t UNUSED(nfields)) {
	return 0; // -1 = failure
}

static inline cap_channel_t *cap_service_open(const cap_channel_t *UNSUED(channel), const char *UNUSED(name))
{
	return (cap_channel_t *) 1; // NULL = failure
}
# else
#  define cap_fcntls_limit(fd, fcntlrights) \
	/* NOP */

#  define cap_init(channel) \
	channel = (cap_channel_t *) 1

#  define cap_close(channel) \
	/* NOP */

#  define cap_service_open(channelin, name, channelout) \
	((cap_channel_t *) 1)

#  define cap_pwd_limit_fields(channel, fields, nfields) \
	0
# endif /* 0 */

# include <sys/types.h>
# include <pwd.h>
# include <grp.h>

# define cap_getpwuid(channel, uid) \
	getpwuid(uid)

# define cap_getpwnam(channel, name) \
	getpwnam(name)

# define cap_getgrgid(channel, gid) \
	getgrgid(gid)

#endif /* FreeBSD >= 10 */
