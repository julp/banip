include(CheckLibraryExists)
include(CheckFunctionExists)

set(LIBRARIES )
# check_library_exists("rt" "mq_open" "lib" HAVE_POSIX_QUEUE)
if(HAVE_POSIX_QUEUE)
    list(APPEND LIBRARIES "rt")
endif(HAVE_POSIX_QUEUE)
check_function_exists("strlcpy" HAVE_STRLCPY)
if(NOT HAVE_STRLCPY)
    check_library_exists("bsd" "strlcpy" "lib" HAVE_LIBBSD_STRLCPY)
    list(APPEND LIBRARIES "bsd")
endif(NOT HAVE_STRLCPY)

if(HAVE_POSIX_QUEUE)
    set(SOURCES "posix.c")
else(HAVE_POSIX_QUEUE)
    set(SOURCES "systemv.c")
endif(HAVE_POSIX_QUEUE)

configure_file(
    "config.h.in"
    "config.h"
    @ONLY
)

add_library(queue STATIC EXCLUDE_FROM_ALL "${SOURCES}")
if(LIBRARIES)
    target_link_libraries(queue ${LIBRARIES})
endif(LIBRARIES)
