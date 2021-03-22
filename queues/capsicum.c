#include "capsicum.h"

#if 0 && defined(__FreeBSD__) && __FreeBSD__ >= 9

# include "err.h"
# include "common.h"

# define MAX_IOCTL_COMMANDS 32

bool _cap_ioctls_limit(char **error, int fd, ...)
{
    bool ok;

    ok = false;
    do {
        va_list ap;
        size_t ncmds;
        cmd_t cmd, cmds[MAX_IOCTL_COMMANDS] = { 0UL };

        ncmds = 0;
        va_start(ap, fd);
        while (ncmds < ARRAY_SIZE(cmds) && 0UL != (cmd = va_arg(ap, cmd_t))) {
            cmds[ncmds++] = cmd;
        }
        va_end(ap);
        if (ncmds >= ARRAY_SIZE(cmds)) { // I know it's not "exact" but at least it's safe
            set_generic_error(error, "maximum of %zu allowed commands exceeded for fd %d", ARRAY_SIZE(cmds), fd);
            break;
        }
        if (0 != cap_ioctls_limit(fd, cmds, ncmds)) {
            set_system_error(error, "cap_ioctls_limit failed to set limits on %d", fd);
            break;
        }
        ok = true;
    } while (false);

    return ok;
}

bool _cap_rights_limit(char **error, int fd, ...)
{
    bool ok;

    ok = false;
    do {
        va_list ap;
        capability_t cap;
        cap_rights_t rights;

        cap_rights_init(&rights);
        va_start(ap, fd);
        while (0ULL != (cap = va_arg(ap, capability_t))) {
            cap_rights_set(&rights, cap);
        }
        va_end(ap);
        if (0 != cap_rights_limit(fd, &rights) && ENOSYS != errno) {
            set_system_error(error, "cap_rights_limit failed to set rights on %d", fd);
            break;
        }
        ok = true;
    } while (false);

    return ok;
}

bool CAP_ENTER(char **error)
{
    bool ok;

    ok = false;
    do {
        if (0 != cap_enter()) {
            if (ENOSYS != errno) {
                set_system_error(error, "cap_enter failed");
                break;
            } else {
                warnc("kernel is compiled without \"options CAPABILITY_MODE\"");
            }
        }
        ok = true;
    } while (false);

    return ok;
}

#endif /* FreeBSD >= 9.0 */
