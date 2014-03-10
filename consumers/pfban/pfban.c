#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>

#include <unistd.h>
#include <limits.h>
#include <grp.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdarg.h>
#include <time.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#ifdef __FreeBSD__
# include <net/if.h>
# include <net/pfvar.h>
#endif
#include <netdb.h>

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
#define STR_LEN(str) (ARRAY_SIZE(str) - 1)
#define STR_SIZE(str) (ARRAY_SIZE(str))

enum {
    PFBAN_EXIT_SUCCESS = 0,
    PFBAN_EXIT_FAILURE,
    PFBAN_EXIT_USAGE
};

extern char *__progname;

static char optstr[] = "b:dg:hl:p:s:v";

static void usage(void) {
    fprintf(
        stderr,
        "usage: %s [-%s] queue_name table_name\n",
        __progname,
        optstr
    );
    exit(PFBAN_EXIT_USAGE);
}

static FILE *err_file = NULL;

#define errx(fmt, ...)  _verr(1, 0, fmt, ## __VA_ARGS__)
#define errc(fmt, ...)  _verr(1, errno, fmt, ## __VA_ARGS__)
#define warnc(fmt, ...) _verr(0, errno, fmt, ## __VA_ARGS__)
#define warn(fmt, ...)  _verr(0, 0, fmt, ## __VA_ARGS__)

static void _verr(int fatal, int errcode, const char *fmt, ...)
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
        exit(PFBAN_EXIT_FAILURE);
    }
}

static int parse_long(const char *str, long *val)
{
    char *endptr;

    *val = strtol(str, &endptr, 10);
    if ((ERANGE == errno && (LONG_MAX == *val || LONG_MIN == *val)) || (0 != errno && 0 == *val)) {
        errx("overflow or underflow for '%s'", str);
        return 0;
    }
    if (endptr == str) {
        errx("number expected, no digit found");
        return 0;
    }
    if ('\0' != *endptr) {
        errx("number expected, non digit found %c in %s", *endptr, str);
        return 0;
    }
    if (*val <= 0) {
        errx("number should be greater than 0, got %ld", *val);
        return 0;
    }

    return 1;
}

static int dev = -1;
static char *buffer = NULL;
static mqd_t mq = (mqd_t) -1;
static const char *queuename = NULL;
static const char *pidfilename = NULL;
static const char *logfilename = NULL;

static void cleanup(void)
{
    if (NULL != buffer) {
        free(buffer);
        buffer = NULL;
    }
    if (-1 != dev) {
        if (0 != close(dev)) {
            warnc("closing /dev/pf failed");
        }
        dev = -1;
    }
    if (((mqd_t) -1) != (mq)) {
        if (0 != mq_close(mq)) {
            warnc("mq_close failed");
        }
        if (0 != mq_unlink(queuename)) {
            warnc("mq_unlink failed");
        }
        mq = (mqd_t) -1;
    }
    if (NULL != pidfilename) {
        if (0 != unlink(pidfilename)) {
            warnc("mq_unlink failed");
        }
    }
    if (NULL != err_file && fileno(err_file) > 2) {
        fclose(err_file);
        err_file = NULL;
    }
}

static void on_signal(int signo)
{
    switch (signo) {
        case SIGINT:
            cleanup();
            break;
        case SIGUSR1:
            if (NULL != err_file && NULL != logfilename && fileno(err_file) > 2) {
                if (NULL == (err_file = freopen(logfilename, "a", err_file))) {
                    err_file = stderr;
                    warn("freopen failed, falling back to stderr");
                }
            }
            return;
        default:
            /* NOP */
            break;
    }
    signal(signo, SIG_DFL);
    kill(getpid(), signo);
}

int main(int argc, char **argv)
{
    gid_t gid;
    mode_t omask;
    struct mq_attr attr;
    struct sigaction sa;
    int c, dFlag, vFlag;
    const char *tablename;

    gid = (gid_t) -1;
    vFlag = dFlag = 0;
    tablename = queuename = NULL;
    /* default hardcoded values in FreeBSD (/usr/src/sys/kern/uipc_mqueue.c) */
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = 1024;
    atexit(cleanup);
    sa.sa_handler = &on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
    while (-1 != (c = getopt(argc, argv, optstr))) {
        switch (c) {
            case 'b':
            {
                long val;

                if (parse_long(optarg, &val)) {
                    attr.mq_msgsize = val;
                }
                break;
            }
            case 'd':
                dFlag = 1;
                break;
            case 'g':
            {
                struct group *grp;

                if (NULL == (grp = getgrnam(optarg))) {
                    errc("getgrnam failed");
                }
                gid = grp->gr_gid;
                break;
            }
            case 'l':
            {
                logfilename = optarg;
                if (NULL == (err_file = fopen(logfilename, "a"))) {
                    err_file = NULL;
                    warnc("fopen '%s' failed, falling back to stderr", logfilename);
                }
                break;
            }
            case 'p':
            {
                pidfilename = optarg;
                break;
            }
            case 's':
            {
                long val;

                if (parse_long(optarg, &val)) {
                    attr.mq_maxmsg = val;
                }
                break;
            }
            case 'v':
                vFlag++;
                break;
            case 'h':
            default:
                usage();
        }
    }
    argc -= optind;
    argv += optind;

    if (argc != 2) {
        usage();
    } else {
        queuename = *argv++;
        tablename = *argv++;
    }

    if (dFlag) {
        if (0 != daemon(0, !vFlag)) {
            errc("daemon failed");
        }
    }
    if (NULL != pidfilename) {
        FILE *fp;

        if (NULL == (fp = fopen(pidfilename, "w"))) {
            warnc("can't create pid file '%s'", pidfilename);
        } else {
            fprintf(fp, "%ld\n", (long) getpid());
            fclose(fp);
        }
    }

#ifdef __FreeBSD__
    if (-1 == (dev = open("/dev/pf", O_WRONLY))) {
        errc("failed opening /dev/pf");
    }
#endif
    if ((gid_t) -1 != gid) {
        if (0 != setgid(gid)) {
            errc("setgid failed");
        }
        if (0 != setgroups(1, &gid)) {
            errc("setgroups failed");
        }
    }
    omask = umask(0);
    if (((mqd_t) -1) == (mq = mq_open(queuename, O_CREAT | O_RDONLY | O_EXCL, 0420, &attr))) {
        errc("mq_open failed");
    }
    umask(omask);
    if (0 != mq_getattr(mq, &attr)) {
        errc("mq_getattr failed");
    }
    if (NULL == (buffer = calloc(attr.mq_msgsize + 1, sizeof(*buffer)))) {
        errx("calloc failed");
    }
    while (1) {
        ssize_t read;

        if (-1 == (read = mq_receive(mq, buffer, /*(size_t)*/ attr.mq_msgsize, NULL))) {
            // buffer[attr.mq_msgsize] = '\0';
            if (EMSGSIZE == errno) {
                warn("message too long (%zi > %ld), skip: %s", read, attr.mq_msgsize, buffer);
            } else {
                errc("mq_receive failed");
            }
        } else {
#ifdef __FreeBSD__
            int ret;
            struct pfr_addr addr;
            struct pfioc_table io;
            struct sockaddr last_src;
            struct addrinfo *res, *resp;
            struct pfioc_src_node_kill psnk;
#endif

            warn("message received: %s\n", buffer);
#ifdef __FreeBSD__
            bzero(&io, sizeof(io));
            strlcpy(io.pfrio_table.pfrt_name, tablename, sizeof(io.pfrio_table.pfrt_name));
            io.pfrio_buffer = &addr;
            io.pfrio_esize = sizeof(addr);
            io.pfrio_size = 1;
            bzero(&addr, sizeof(addr));
            if (1 == inet_pton(AF_INET, buffer, &addr.pfra_ip4addr)) {
                addr.pfra_af = AF_INET;
                addr.pfra_net = 32;
            } else if (1 == inet_pton(AF_INET6, buffer, &addr.pfra_ip6addr)) {
                addr.pfra_af = AF_INET6;
                addr.pfra_net = 128;
            } else {
                warn("Valid address expected, got: %s", buffer);
            }
            if (-1 == ioctl(dev, DIOCRADDADDRS, &io)) {
                errc("ioctl(DIOCRADDADDRS) failed");
            }
#if 0
            /* kill states */
            memset(&psnk, 0, sizeof(psnk));
            memset(&psnk.psnk_src.addr.v.a.mask, 0xff, sizeof(psnk.psnk_src.addr.v.a.mask));
            memset(&last_src, 0xff, sizeof(last_src));
            if (0 != (ret = getaddrinfo(buffer, NULL, NULL, &res))) {
                errc("getaddrinfo failed");
            }
            for (resp = res; resp; resp = resp->ai_next) {
                if (NULL == resp->ai_addr) {
                    continue;
                }
                if (0 == memcmp(&last_src, resp->ai_addr, sizeof(last_src))) {
                    continue;
                }
                last_src = *(struct sockaddr *)resp->ai_addr;
                psnk.psnk_af = resp->ai_family;
                switch (psnk.psnk_af) {
                    case AF_INET:
                        psnk.psnk_src.addr.v.a.addr.v4 = ((struct sockaddr_in *) resp->ai_addr)->sin_addr;
                        break;
                    case AF_INET6:
                        psnk.psnk_src.addr.v.a.addr.v6 = ((struct sockaddr_in6 *) resp->ai_addr)->sin6_addr;
                        break;
                    default:
                        freeaddrinfo(res);
                        errx("Unknown address family %d", psnk.psnk_af);
                }
                if (-1 == ioctl(dev, DIOCKILLSRCNODES, &psnk)) {
                    errc("ioctl(DIOCKILLSRCNODES) failed");
                }
            }
            freeaddrinfo(res);
# endif
#endif
        }
    }

    return PFBAN_EXIT_SUCCESS;
}
