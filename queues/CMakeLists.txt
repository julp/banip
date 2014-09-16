include(CheckLibraryExists)
include(CheckFunctionExists)

set(SOURCES )
set(LIBRARIES )
check_library_exists("rt" "mq_open" "lib" HAVE_POSIX_QUEUE)
if(HAVE_POSIX_QUEUE)
    list(APPEND LIBRARIES "rt")
endif(HAVE_POSIX_QUEUE)
check_function_exists("strlcpy" HAVE_STRLCPY)
if(NOT HAVE_STRLCPY)
    check_library_exists("bsd" "strlcpy" "lib" HAVE_LIBBSD_STRLCPY)
    if(HAVE_LIBBSD_STRLCPY)
        list(APPEND LIBRARIES "bsd")
    else(HAVE_LIBBSD_STRLCPY)
        list(APPEND SOURCES "../missing/strlcpy.c")
    endif(HAVE_LIBBSD_STRLCPY)
endif(NOT HAVE_STRLCPY)

if(HAVE_POSIX_QUEUE)
    list(APPEND SOURCES "posix.c")
else(HAVE_POSIX_QUEUE)
    list(APPEND SOURCES "systemv.c")
endif(HAVE_POSIX_QUEUE)

configure_file(
    "config.h.in"
    "config.h"
    @ONLY
)

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
add_library(queue STATIC EXCLUDE_FROM_ALL ${SOURCES})
set_target_properties(queue PROPERTIES COMPILE_FLAGS "-fPIC")
if(LIBRARIES)
    target_link_libraries(queue ${LIBRARIES})
endif(LIBRARIES)
