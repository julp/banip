#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <mqueue.h>

#include <unistd.h>
#include <grp.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdarg.h>
#include <time.h>

#include "engine.h"

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
#define STR_LEN(str) (ARRAY_SIZE(str) - 1)
#define STR_SIZE(str) (ARRAY_SIZE(str))

enum {
    PFBAN_EXIT_SUCCESS = 0,
    PFBAN_EXIT_FAILURE,
    PFBAN_EXIT_USAGE
};

extern char *__progname;

static char optstr[] = "b:e:g:l:p:q:s:t:dhv";

static struct option long_options[] =
{
    {"msgsize",          required_argument, NULL, 'b'},
    {"daemonize",        no_argument,       NULL, 'd'},
    {"engine",           required_argument, NULL, 'e'},
    {"group",            required_argument, NULL, 'g'},
    {"log",              required_argument, NULL, 'l'},
    {"pid",              required_argument, NULL, 'p'},
    {"queue",            required_argument, NULL, 'q'},
    {"qsize",            required_argument, NULL, 's'},
    {"table",            required_argument, NULL, 't'},
    {"verbose",          no_argument,       NULL, 'v'},
    {NULL,               no_argument,       NULL, 0}
};

static void usage(void) {
    fprintf(
        stderr,
        "usage: %s [-%s] [ -q queue_name ] [ -t table_name ]\n",
        __progname,
        NULL == strrchr(optstr, ':') ? optstr : strrchr(optstr, ':') + 1
    );
    exit(PFBAN_EXIT_USAGE);
}

static FILE *err_file = NULL;

void _verr(int fatal, int errcode, const char *fmt, ...)
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

static void *ctxt = NULL;
static char *buffer = NULL;
static mqd_t mq = (mqd_t) -1;
static const char *queuename = NULL;
static const engine_t *engine = NULL;
static const char *pidfilename = NULL;
static const char *logfilename = NULL;

static void cleanup(void)
{
    if (NULL != buffer) {
        free(buffer);
        buffer = NULL;
    }
    if (NULL != ctxt) {
        engine->close(ctxt);
        free(ctxt);
        ctxt = NULL;
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
        case SIGTERM:
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

    ctxt = NULL;
    gid = (gid_t) -1;
    vFlag = dFlag = 0;
    tablename = queuename = NULL;
    /* default hardcoded values on FreeBSD (/usr/src/sys/kern/uipc_mqueue.c) */
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = 1024;
    atexit(cleanup);
    sa.sa_handler = &on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &sa, NULL);
    if (NULL == (engine = get_default_engine())) {
        errx("no engine available for your system");
    }
    while (-1 != (c = getopt_long(argc, argv, optstr, long_options, NULL))) {
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
            case 'e':
            {
                if (NULL == (engine = get_engine_by_name(optarg))) {
                    errx("unknown engine '%s'", optarg);
                }
                break;
            }
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
                pidfilename = optarg;
                break;
            case 'q':
                queuename = optarg;
                break;
            case 's':
            {
                long val;

                if (parse_long(optarg, &val)) {
                    attr.mq_maxmsg = val;
                }
                break;
            }
            case 't':
                tablename = optarg;
                break;
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

    if (0 != argc || NULL == queuename || NULL == tablename) {
        usage();
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
    ctxt = engine->open();
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
            engine->handle(ctxt, tablename, buffer);
        }
    }

    return PFBAN_EXIT_SUCCESS;
}
