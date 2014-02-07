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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#ifdef __FreeBSD__
#include <net/if.h>
#include <net/pfvar.h>
#endif

enum {
    PFBAN_EXIT_SUCCESS = 0,
    PFBAN_EXIT_FAILURE,
    PFBAN_EXIT_USAGE
};

extern char *__progname;

static char optstr[] = "b:dg:hs:v";

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

static void _verrx(int code, const char *fmt, va_list ap)
{
    if (NULL == err_file) {
        err_file = stderr;
    }
    fprintf(err_file, "%s: ", __progname);
    if (NULL != fmt) {
        vfprintf(err_file, fmt, ap);
        if (code) {
            fprintf(err_file, ": ");
        }
    }
    if (code) {
        fputs(strerror(code), err_file);
    }
    fprintf(err_file, "\n");
    exit(PFBAN_EXIT_FAILURE);
}

static void errx(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    _verrx(0, fmt, ap);
    va_end(ap);
}

static void errc(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    _verrx(errno, fmt, ap);
    va_end(ap);
}

static void warn(const char *fmt, ...)
{
    va_list ap;

    fprintf(err_file, "%s: ", __progname);
    va_start(ap, fmt);
    if (NULL != fmt) {
        vfprintf(err_file, fmt, ap);
    }
    va_end(ap);
    fprintf(err_file, "\n");
    exit(PFBAN_EXIT_FAILURE);
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
static const char *queuename;
static mqd_t mq = (mqd_t) -1;

static void cleanup(void)
{
    if (NULL != buffer) {
        free(buffer);
        buffer = NULL;
    }
    if (-1 != dev) {
        if (0 != close(dev)) {
            warn("closing /dev/pf failed");
        }
        dev = -1;
    }
    if (((mqd_t) -1) != (mq)) {
        if (0 != mq_close(mq)) {
            warn("mq_close failed");
        }
        if (0 != mq_unlink(queuename)) {
            warn("mq_unlink failed");
        }
        mq = (mqd_t) -1;
    }
}

static void on_sigint(int signo)
{
    cleanup();
    signal(signo, SIG_DFL);
    kill(getpid(), signo);
}

int main(int argc, char **argv)
{
    gid_t gid;
    mode_t omask;
    struct mq_attr attr;
    int c, dFlag, vFlag;
    const char *tablename;

    gid = (gid_t) -1;
    vFlag = dFlag = 0;
    tablename = queuename = NULL;
    /* default hardcoded value in FreeBSD (/usr/src/sys/kern/uipc_mqueue.c) */
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = 1024;
    atexit(cleanup);
    signal(SIGINT, on_sigint);
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
            struct pfr_addr addr;
            struct pfioc_table io;
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
                errc("ioctl failed");
            }
#endif
        }
    }

    return PFBAN_EXIT_SUCCESS;
}
