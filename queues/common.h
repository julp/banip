#pragma once

#ifndef __has_attribute
# define __has_attribute(x) 0
#endif /* !__has_attribute */

#if __GNUC__ || __has_attribute(unused)
# define UNUSED(x) UNUSED_ ## x __attribute__((unused))
#else
# define UNUSED
#endif /* UNUSED */

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
#define STR_LEN(str) (ARRAY_SIZE(str) - 1)
#define STR_SIZE(str) (ARRAY_SIZE(str))

#define HAS_FLAG(value, flag) \
    (0 != ((value) & (flag)))

#define SET_FLAG(value, flag) \
    ((value) |= (flag))

#define UNSET_FLAG(value, flag) \
    ((value) &= ~(flag))

enum {
    BANIPD_EXIT_SUCCESS = 0,
    BANIPD_EXIT_FAILURE,
    BANIPD_EXIT_USAGE
};

extern char *__progname;
