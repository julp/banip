cmake_minimum_required(VERSION 2.8)

set(CMAKE_C_FLAGS_MAINTAINER "-g -DDEBUG -Wall -Wextra -std=gnu99 -Wwrite-strings -Wstrict-prototypes -Wuninitialized -Wunreachable-code -Wno-comment -Wnonnull"
    CACHE STRING "Flags used by the C compiler during maintainer builds." FORCE)
mark_as_advanced(CMAKE_C_FLAGS_MAINTAINER)
set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES} Maintainer")

set(SOURCES )
set(LIBRARIES )

list(APPEND SOURCES pfban.c)
list(APPEND LIBRARIES "rt")

if(CMAKE_SYSTEM_NAME MATCHES "BSD$")
    add_definitions(-DWITH_PF)
    list(APPEND SOURCES pf.c)

    if(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
        add_definitions(-DWITH_NPF)
        list(APPEND SOURCES npf.c)
    endif(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")

else(CMAKE_SYSTEM_NAME MATCHES "BSD$")

    # Linux
    add_definitions(-DWITH_NFTABLES)
    list(APPEND LIBRARIES "mnl")
    list(APPEND LIBRARIES "nftnl")
    list(APPEND SOURCES nftables.c)

endif(CMAKE_SYSTEM_NAME MATCHES "BSD$")

add_executable(pfban ${SOURCES})
target_link_libraries(pfban ${LIBRARIES})

option(TEST "enable tests" ON)

if(TEST)
    list(REMOVE_ITEM SOURCES pfban.c)
    list(APPEND SOURCES pftest.c)

    add_executable(pftest ${SOURCES})
    target_link_libraries(pftest ${LIBRARIES})

    add_custom_target(test COMMAND find ${CMAKE_SOURCE_DIR} -name '*.sh' -exec bash {} "\;")
endif(TEST)
