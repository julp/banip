cmake_minimum_required(VERSION 2.8.8)

set(CMAKE_C_FLAGS_MAINTAINER "-g -DDEBUG -Wall -Wextra -std=gnu99 -Wwrite-strings -Wstrict-prototypes -Wuninitialized -Wunreachable-code -Wno-comment -Wnonnull"
    CACHE STRING "Flags used by the C compiler during maintainer builds." FORCE)
mark_as_advanced(CMAKE_C_FLAGS_MAINTAINER)
set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES} Maintainer")

set(SOURCES "engine.c;")
set(LIBRARIES "queue;")

include_directories(${PROJECT_SOURCE_DIR}/../../queues)

add_subdirectory(${PROJECT_SOURCE_DIR}/../../queues queues)

if(CMAKE_SYSTEM_NAME MATCHES "BSD$")
    add_definitions(-DWITH_PF)
    list(APPEND SOURCES pf.c)

    if(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
        include(CheckStructHasMember)
        CHECK_STRUCT_HAS_MEMBER(npf_ioctl_table_t nct_name net/npf.h NPF_ALLOW_NAMED_TABLE)
        #CHECK_STRUCT_HAS_MEMBER(npf_ioctl_table_t nct_tid net/npf.h NPF_ALLOW_NAMED_TABLE)
        if(NPF_ALLOW_NAMED_TABLE)
            add_definitions(-DNPF_ALLOW_NAMED_TABLE)
        endif(NPF_ALLOW_NAMED_TABLE)
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

list(APPEND SOURCES dummy.c)

add_library(__common_sources OBJECT EXCLUDE_FROM_ALL ${SOURCES})

add_executable(pfban $<TARGET_OBJECTS:__common_sources> pfban.c)
target_link_libraries(pfban ${LIBRARIES})

add_executable(pftest $<TARGET_OBJECTS:__common_sources> pftest.c)
target_link_libraries(pftest ${LIBRARIES})

add_custom_target(test COMMAND find ${CMAKE_SOURCE_DIR}/tests/ -name '*.sh' -exec bash {} "\;" DEPENDS pfban pftest)

install(TARGETS pfban RUNTIME DESTINATION sbin)
