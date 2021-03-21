#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

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

#include "common.h"
#include "err.h"
#include "engine.h"
#include "queue.h"
#include "parse.h"
#include "capsicum.h"

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
    exit(BANIPD_EXIT_USAGE);
}

static FILE *err_file = NULL;

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

static void *ctxt = NULL;
static void *queue = NULL;
static char *buffer = NULL;
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
        if (NULL != engine->close) {
            engine->close(ctxt);
        }
        free(ctxt);
        ctxt = NULL;
    }
    queue_close(&queue, NULL);
    if (NULL != pidfilename) {
        if (0 != unlink(pidfilename)) {
            warnc("unlink failed");
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
    addr_t addr;
    char *error;
    struct sigaction sa;
    int c, dFlag, vFlag;
    unsigned long max_message_size;
    const char *queuename, *tablename;

    ctxt = NULL;
    error = NULL;
    gid = (gid_t) -1;
    vFlag = dFlag = 0;
    tablename = queuename = NULL;
    if (NULL == (queue = queue_init(&error))) {
        errx("queue_init failed"); // TODO: better
    }
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
                unsigned long val;

                if (parse_ulong(optarg, &val, &error)) {
                    queue_set_attribute(queue, QUEUE_ATTR_MAX_MESSAGE_SIZE, val); // TODO: check returned value
                } else {
                    errx("invalid value for option -b/--msgsize: %s", error);
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
                unsigned long val;

                if (parse_ulong(optarg, &val, &error)) {
                    queue_set_attribute(queue, QUEUE_ATTR_MAX_MESSAGE_IN_QUEUE, val); // TODO: check returned value
                } else {
                    errx("invalid value for option -s/--qsize: %s", error);
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

    do {
        if (dFlag) {
            if (0 != daemon(0, !vFlag)) {
                set_system_error(&error, "daemon failed");
                break;
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

        if (((gid_t) -1) != gid) {
            if (0 != setgid(gid)) {
                set_system_error(&error, "setgid(%d) failed", gid);
                break;
            }
            if (0 != setgroups(1, &gid)) {
                set_system_error(&error, "setgroups to {%d} failed", gid);
                break;
            }
        }
        CAP_RIGHTS_LIMIT(STDOUT_FILENO, CAP_WRITE);
        CAP_RIGHTS_LIMIT(STDERR_FILENO, CAP_WRITE);
        if (NULL != err_file/* && fileno(err_file) > 2*/) {
            CAP_RIGHTS_LIMIT(fileno(err_file), CAP_WRITE);
        }
        if (!queue_open(queue, queuename, QUEUE_FL_OWNER, &error)) {
            break;
        }
        if (QUEUE_ERR_OK != queue_get_attribute(queue, QUEUE_ATTR_MAX_MESSAGE_SIZE, &max_message_size)) {
            set_generic_error(&error, "queue_get_attribute failed");
            break;
        }
        if (NULL == (buffer = calloc(++max_message_size, sizeof(*buffer)))) {
            set_malloc_error(&error, max_message_size * sizeof(*buffer));
            break;
        }
        if (NULL != engine->open && NULL == (ctxt = engine->open(tablename, &error))) {
            break;
        }
        if (0 == getuid() && engine->drop_privileges) {
            struct passwd *pwd;

            if (NULL == (pwd = getpwnam("nobody")) && NULL == (pwd = getpwnam("daemon"))) {
                set_generic_error(&error, "no nobody or daemon user accounts found on this system");
                break;
            }
            if (0 != setuid(pwd->pw_uid)) {
                set_system_error(&error, "setuid(%d) failed", pwd->pw_uid);
                break;
            }
        }
        CAP_ENTER();
        while (1) {
            ssize_t read;

            if (
                   -1 == (read = queue_receive(queue, buffer, max_message_size, &error))
                || !parse_addr(buffer, &addr, &error)
                || !engine->handle(ctxt, tablename, addr, &error)
            ) {
                _verr(0, 0, "%s", error); // TODO: transition
                error_free(&error);
            }
        }
        /* not reached */
    } while (false);
    if (NULL != error) {
        _verr(0, 0, "%s", error); // TODO: transition
        error_free(&error);
    }

    return BANIPD_EXIT_FAILURE;
}
