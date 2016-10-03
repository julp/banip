#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdarg.h>
#include <time.h>
#include <libutil.h> /* pidfile_{open,write,remove} */
#include <errno.h>
#include <stdbool.h>
#include <assert.h>

#include "config.h"
#include "common.h"
#include "banip.h"
#include "fd.h"
#include "conf.h"
#include "engine.h"
#include "capsicum.h"

#if defined(__FreeBSD__) && __FreeBSD__ >= 9
# define SANDBOXABLE 1
# define UNSANDBOX_FLAG 'U'
# define UNSANDBOX_FLAG_AS_STRING "U"
# else
# define SANDBOXABLE 0
# define UNSANDBOX_FLAG_AS_STRING ""
#endif /* FreeBSD >= 9 */

typedef struct {
	int fd;
	uid_t uid;
	gid_t gid;
} client_t;

enum {
	BANIPD_EXIT_SUCCESS = 0,
	BANIPD_EXIT_FAILURE,
	BANIPD_EXIT_USAGE
};

extern char *__progname;

static char optstr[] = "c:e:f:l:p:s:t:dhv" UNSANDBOX_FLAG_AS_STRING;

static struct option long_options[] =
{
	{"chroot",           required_argument, NULL, 'c'},
	{"daemonize",        no_argument,       NULL, 'd'},
	{"engine",           required_argument, NULL, 'e'},
	{"file",             required_argument, NULL, 'f'},
	{"log",              required_argument, NULL, 'l'},
	{"pid",              required_argument, NULL, 'p'},
	{"socket",           required_argument, NULL, 's'},
	{"table",            required_argument, NULL, 't'},
	{"verbose",          no_argument,       NULL, 'v'},
#if SANDBOXABLE
	{"unsandbox",        no_argument,       NULL, UNSANDBOX_FLAG },
#endif /* SANDBOXABLE */
	{NULL,               no_argument,       NULL, 0}
};

static void usage(void) {
	fprintf(
		stderr,
		"usage: %s [-%s] [ -s path ] [ -t table_name ]\n",
		__progname,
		NULL == strrchr(optstr, ':') ? optstr : strrchr(optstr, ':') + 1
	);
	exit(BANIPD_EXIT_USAGE);
}

static FILE *err_file = NULL;

#define errx(fmt, ...)  _verr(1, 0, fmt, ## __VA_ARGS__)
#define errc(fmt, ...)  _verr(1, errno, fmt, ## __VA_ARGS__)
#define warnc(fmt, ...) _verr(0, errno, fmt, ## __VA_ARGS__)
#define warn(fmt, ...)  _verr(0, 0, fmt, ## __VA_ARGS__)

void _verr(bool fatal, int errcode, const char *fmt, ...)
{
	time_t t;
	va_list ap;
	char buf[STR_SIZE("yyyy-mm-dd hh:mm:ss")];
	struct tm *tm;

	t = time(NULL);
	tm = localtime(&t);
	if (0 == strftime(buf, ARRAY_SIZE(buf), "%F %T", tm)) {
		buf[0] = '\0';
	}
	if (NULL == err_file) {
		err_file = stderr;
	}
	fprintf(err_file, "[%s] %s: ", buf, __progname);
	if (NULL != fmt) {
		va_start(ap, fmt);
		vfprintf(err_file, fmt, ap);
		va_end(ap);
		if (errcode) {
			fprintf(err_file, ": ");
		}
	}
	if (errcode) {
		fputs(strerror(errcode), err_file);
	}
	fprintf(err_file, "\n");
	if (fatal) {
		exit(BANIPD_EXIT_FAILURE);
	}
}

static void *conf;
static void *ctxt;
static void *serv;
static sig_atomic_t quit;
static const engine_t *engine;
static const char *logfilename;

struct passwd *mygetpwnam(cap_channel_t *chpwd, const char *name)
{
	struct passwd *pwd;

	errno = 0;
	if (NULL != chpwd) {
		pwd = cap_getpwnam(chpwd, name);
		if (0 != errno) {
			errc("cap_getpwnam(\"%s\")", name);
		}
	} else {
		pwd = getpwnam(name);
	}

	return pwd;
}

static void bind_event(int kq, uintptr_t ident, short filter, u_short flags, void *udata)
{
	struct kevent ev;

	EV_SET(&ev, ident, filter, flags, 0, 0, udata);
	if (-1 == kevent(kq, &ev, 1, NULL, 0, NULL)) {
		errc("kevent failed");
	}
}

// better to call it mark_free_event or something like that?
static void clear_event(struct kevent *event)
{
	event->ident = (uintptr_t) -1;
}

// TODO: better name?
// NOTE: only to end up a connection (ev has to exists + be valid + be of type EVFILT_READ)
static void close_event(int kq, struct kevent *event, bool forced)
{
	int evtfd;

	evtfd = (int) event->ident;
debug("%d %s disconnection", evtfd, forced ? "forced" : "normal");
	free(event->udata);
	bind_event(kq, evtfd, EVFILT_READ, EV_DELETE, NULL);
	if (forced) {
		shutdown(evtfd, SHUT_RDWR);
	}
	close(evtfd);
	clear_event(event);
}

static const int signals[] = {
	SIGINT,
	SIGHUP,
	SIGTERM,
	SIGQUIT,
	SIGUSR1,
};

int main(int argc, char **argv)
{
	int kq;
	size_t i;
	char *error;
	int confdirfd;
	pid_t otherpid;
	struct pidfh *pfh;
	int c, dFlag, vFlag;
	fd_t *fdconf, *fdlog;
	size_t clients_count;
	bool sandboxed = SANDBOXABLE;
	cap_channel_t *ch, *chpwd, *chgrp;
	const char *sockpath, *tablename, *pidfilename, *conffilename;
	struct kevent events[12 + ARRAY_SIZE(signals) + 1/*our server*/];

	error = NULL;
	clients_count = 0; // TODO
	vFlag = dFlag = 0;
	tablename = "spammers";
	ch = chpwd = chgrp = NULL;
	sockpath = pidfilename = NULL;
	conffilename = DEFAULT_CONFIGURATION_FILE;
	{
		struct sigaction sa;

		bzero(&sa, sizeof(struct sigaction));
		sa.sa_handler = SIG_IGN;
		for (i = 0; i < ARRAY_SIZE(signals); i++) {
			sigaction(signals[i], &sa, NULL);
		}
	}
	for (i = 0; i < ARRAY_SIZE(events); i++) {
		clear_event(&events[i]);
	}
	if (NULL == (engine = get_default_engine())) {
		errx("no engine available for your system");
	}
	while (-1 != (c = getopt_long(argc, argv, optstr, long_options, NULL))) {
		switch (c) {
			case 'c':
				// TODO
				break;
			case 'd':
				dFlag = 1;
				break;
			case 'e':
			{
				if (NULL == (engine = get_engine_by_name(optarg))) {
					errx("unknown engine '%s'", optarg);
				}
				break;
			}
			case 'f':
				conffilename = optarg;
				break;
			case 'l':
				logfilename = optarg;
				break;
			case 'p':
				pidfilename = optarg;
				break;
			case 's':
				sockpath = optarg;
				break;
			case 't':
				tablename = optarg;
				break;
			case 'v':
				vFlag++;
				break;
#if SANDBOXABLE
			case UNSANDBOX_FLAG:
				sandboxed = false;
				break;
#endif /* SANDBOXABLE */
			case 'h':
			default:
				usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (0 != argc || NULL == tablename) {
		usage();
	}
	if (NULL != logfilename) {
		if (NULL == (fdlog = fd_wrap(logfilename, O_WRONLY | O_APPEND | O_CREAT, sandboxed, &error))) {
			errx("wrapping %s failed: %s", logfilename, error);
		}
		if (NULL == (err_file = fd_reopen(fdlog, &error))) {
			errx("can't open %s: %s", logfilename, error);
		}
	}
	if (sandboxed) {
		unsigned int state;

		if (-1 == cap_getmode(&state) && ENOSYS == errno) {
			sandboxed = false;
			warn("kernel is compiled without: options CAPABILITY_MODE. Sandboxing turned off.");
		}
	}
	if (NULL == (pfh = pidfile_open(pidfilename, 0600, &otherpid))) {
		if (EEXIST == errno) {
			errx("Daemon is already running as pid: %jd", (intmax_t) otherpid);
		}
		warn("Cannot open or create pidfile");
	}
	if (dFlag) {
		if (0 != daemon(0, !vFlag)) {
			errc("daemon failed");
			pidfile_remove(pfh);
		}
	}
	if (NULL != pfh) {
		pidfile_write(pfh);
// 		CAP_RIGHTS_LIMIT(pidfile_fileno(pfh), CAP_FSTAT); /* pidfile_remove = fstat + close + unlink */
	}
	if (sandboxed) {
		const char *pwdfields[] = { "pw_name", "pw_uid", "pw_gid" };

		if (NULL == (ch = cap_init())) {
			errc("cap_init");
		}
		if (NULL == (chpwd = cap_service_open(ch, "system.pwd"))) {
			errc("cap_service_open(\"system.grp\")");
		}
		if (-1 == cap_pwd_limit_fields(chpwd, pwdfields, ARRAY_SIZE(pwdfields))) {
			errc("cap_pwd_limit_fields");
		}
		if (NULL == (chgrp = cap_service_open(ch, "system.grp"))) {
			errc("cap_service_open(\"system.grp\")");
		}
		CAP_RIGHTS_LIMIT(STDOUT_FILENO, CAP_WRITE);
		CAP_RIGHTS_LIMIT(STDERR_FILENO, CAP_WRITE);
	}
	if (NULL == (fdconf = fd_wrap(conffilename, O_RDONLY, sandboxed, &error))) {
		errx("wrapping %s failed: %s", conffilename, error);
	}
	if (NULL == (serv = banip_server_new(sockpath, NULL, NULL))) {
		errx("banip_server_new failed");
	}
	if (NULL != engine->open) {
		if (NULL == (ctxt = engine->open(tablename, &error))) {
			errx("failed opening engine '%s': %s", engine->name, error);
		}
	}
	if (0 == getuid() && engine->drop_privileges) {
		struct passwd *pwd;

		if (NULL == (pwd = mygetpwnam(chpwd, "nobody"))) {
			if (NULL == (pwd = mygetpwnam(chpwd, "daemon"))) {
				errx("no nobody or daemon user accounts found on this system");
			}
		}
		if (0 != setgid(pwd->pw_gid)) {
			errc("setgid to %d failed", pwd->pw_gid);
		}
		if (0 != setgroups(1, &pwd->pw_gid)) {
			errc("setgroups to %d failed", pwd->pw_gid);
		}
		if (0 != setuid(pwd->pw_uid)) {
			errc("setuid to %d failed", pwd->pw_uid);
		}
	}
	kq = kqueue();
	if (sandboxed) {
		CAP_RIGHTS_LIMIT(kq, CAP_KQUEUE);
	}
	bind_event(kq, banip_get_fd(serv), EVFILT_READ, EV_ADD | EV_ENABLE, NULL);
#if 1
	for (i = 0; i < ARRAY_SIZE(signals); i++) {
		bind_event(kq, signals[i], EVFILT_SIGNAL, EV_ADD | EV_ENABLE, NULL);
	}
#endif
	if (sandboxed) {
		cap_close(ch);
		CAP_ENTER();
	}
	{
		FILE *fp;

		if (NULL == (fp = fd_reopen(fdconf, &error))) {
			errx("can't open %s: %s", conffilename, error);
		}
		if (!conf_parse(NULL, &conf, fp, chpwd, &error)) {
			errx("parsing of %s failed: %s", conffilename, error);
		}
	}
	while (!quit) {
		int nev;
// 		struct timespec timeout;
		banip_authenticated_request_t req;

// 		bzero(&timeout, sizeof(timeout));
		if (-1 == (nev = kevent(kq, NULL, 0, events, ARRAY_SIZE(events), NULL/*&timeout*/))) {
			if (EINTR == errno) {
				++quit;
				debug("EINTR");
			} else {
				errc("kevent failed");
			}
		} else {
			int i;

			for (i = 0; i < nev; i++) {
				struct kevent *event;

				event = &events[i];
#if 1
				if (EVFILT_SIGNAL == event->filter) {
					switch (event->ident) {
						case SIGINT:
						case SIGQUIT:
						case SIGTERM:
							++quit;
							debug("INT, QUIT or TERM");
							break;
						case SIGHUP:
						{
							FILE *fp;

							debug("HUP");
							if (NULL == (fp = fd_reopen(fdconf, &error))) {
								warn("can't reopen %s", conffilename, error);
								free(error);
								error = NULL;
							} else {
								void *newconf;

								if (!conf_parse(conf, &newconf, fp, chpwd, &error)) {
									warn("parsing of %s failed: %s", conffilename, error);
									free(error);
									error = NULL;
								} else {
									conf = newconf;
								}
							}
							break;
						}
						case SIGUSR1:
							debug("USR1");
							if (NULL != err_file && NULL != logfilename && fileno(err_file) > 2) {
								fflush(err_file);
								fclose(err_file);
								if (NULL == (err_file = fd_reopen(fdlog, &error))) {
									err_file = stderr;
									warnc("freopen %s failed, falling back to stderr: %s", logfilename, error);
									free(error);
									error = NULL;
								}
							}
							break;
						default:
							break;
					}
				} else
#endif
				{
					int evtfd;

					evtfd = (int) event->ident;
					if (HAS_FLAG(event->flags, EV_EOF)) {
						close_event(kq, event, false);
					} else if (banip_get_fd(serv) == (evtfd)) {
						int fd;
						client_t *client;

						debug("new connection");
						if (-1 == (fd = accept(banip_get_fd(serv), NULL, NULL))) {
							errc("accept failed");
						}
						client = malloc(sizeof(*client));
						client->fd = fd;
// 						client->uid = -1;
// 						client->gid = -1;
						if (-1 == getpeereid(fd, &client->uid, &client->gid)) {
							errc("getpeereid failed");
						}
						if (/*-1 == client->uid || */!conf_is_authorized(conf, client->uid, CONNECT)) {
							warn("connection from %d:%d rejected", client->uid, client->gid);
							shutdown(fd, SHUT_RDWR);
							close(fd);
						} else {
							debug("from %d:%d", client->uid, client->gid);
// 							CAP_RIGHTS_LIMIT(fd, CAP_EVENT);
							bind_event(kq, fd, EVFILT_READ, EV_ADD | EV_ENABLE | EV_CLEAR, client);
						}
					} else if (HAS_FLAG(event->flags, EVFILT_READ)) {
						debug("something to read on %d (%ld bytes)", evtfd, event->data);
						if (!banip_receive(serv, evtfd, &req, &error)) {
							errc("banip_receive failed: %s", error);
						} else {
							addr_t addr;
							uint32_t flags;
							client_t *client;
#if 0
							struct group *grp;
							struct passwd *pwd;
							char pw_name[MAXLOGNAME + 1], gr_name[MAXLOGNAME + 1];
#endif

							flags = SEND;
							client = (client_t *) event->udata;
// 							debug("something to read on %d", evtfd);
#if 0
							if (NULL == (pwd = cap_getpwuid(chpwd, client->uid))) {
								warnc("getpwuid");
								snprintf(pw_name, STR_SIZE(pw_name), "%d", client->uid);
							} else {
								strlcpy(pw_name, pwd->pw_name, STR_SIZE(pw_name));
							}
							if (NULL == (grp = cap_getgrgid(chgrp, client->gid))) {
								warnc("getgrgid");
								snprintf(gr_name, STR_SIZE(gr_name), "%d", client->gid);
							} else {
								strlcpy(gr_name, grp->gr_name, STR_SIZE(gr_name));
							}
#endif
							addr.addr_len = sizeof(addr.u.sa6);
							if (HAS_FLAG(req.flags, FL_FD_SET)) {
								flags |= SAFE;
								if (0 != getpeername(req.fd, &addr.u.sa, &addr.addr_len)) {
									errc("getpeername failed");
								}
							} else {
								flags |= UNSAFE;
								memcpy(&addr, &req.super.addr, sizeof(addr));
							}
							if (!HAS_FLAG(req.flags, FL_CRED_SET)) {
								warn("unauthenticated request received (connected as %d:%d)", client->uid, client->gid);
								close_event(kq, event, true);
							} else if (!conf_is_authorized(conf, req.uid, flags)) {
								warn("%s request from %d:%d rejected (connected as %d:%d)", HAS_FLAG(flags, UNSAFE) ? "unsafe" : "safe", req.uid, req.gid, client->uid, client->gid);
								close_event(kq, event, true);
							} else {
								int rc;
								socklen_t len;

								switch (addr.u.sa.sa_family) {
									case AF_INET:
										assert(addr.u.sa.sa_family == addr.u.sa4.sin_family);
										len = sizeof(addr.u.sa4);
										addr.netmask = 32;
										break;
									case AF_INET6:
										assert(addr.u.sa.sa_family == addr.u.sa6.sin6_family);
										len = sizeof(addr.u.sa6);
										addr.netmask = 128;
										break;
									default:
										warn("unexpected sa_family (%d) (AF_INET = %d, AF_INET6 = %d)", addr.u.sa.sa_family, AF_INET, AF_INET6);
										goto unparsable;
										break;
								}
								if (0 != (rc = getnameinfo(&addr.u.sa, len, addr.humanrepr, STR_LEN(addr.humanrepr), NULL, 0, NI_NUMERICHOST))) {
									warn("getnameinfo failed: %s", gai_strerror(rc));
									goto unparsable;
								} else {
									warn("%d:%d requested a ban of %s (%d) (reason: %.*s)", client->uid, client->gid, addr.humanrepr, req.fd, (int) req.super.context_len, req.super.context);
								}
								engine->handle(ctxt, tablename, addr, &error);
unparsable:
								if (HAS_FLAG(req.flags, FL_FD_SET)) {
									close(req.fd);
								}
							}
						}
// 					} else {
					} else if (0 != event->flags) {
						debug("unknown event: %d", event->flags);
					}
				}
			}
		}
	}
	if (NULL != ctxt) {
		if (NULL != engine->close) {
			engine->close(ctxt);
		}
		free(ctxt);
		ctxt = NULL;
	}
	{
		size_t i;
		struct kevent *event;

		for (i = 0; i < ARRAY_SIZE(events); i++) {
			event = &events[i];
			if (EVFILT_SIGNAL == event->filter) {
				bind_event(kq, event->ident, EVFILT_SIGNAL, EV_DELETE, NULL);
			} else {
				int fd;

				fd = (int) event->ident;
				if (-1 != fd) {
					bind_event(kq, fd, EVFILT_READ, EV_DELETE, NULL);
					if (banip_get_fd(serv) != fd) {
						shutdown(fd, SHUT_RDWR);
						close(fd);
						free(event->udata);
					}
				}
			}
		}
	}
	if (NULL != serv) {
		banip_close(serv);
	}
	// TODO: won't work when sandboxed
	if (0 != pidfile_remove(pfh)) {
		warnc("pidfile_remove failed");
	}
	if (NULL != err_file && fileno(err_file) > 2) {
		fclose(err_file);
		err_file = NULL;
	}
	fd_close(fdlog);
	fd_close(fdconf);

	return BANIPD_EXIT_SUCCESS;
}
