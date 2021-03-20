cmake_minimum_required(VERSION 2.8.8)

if(CMAKE_VERSION VERSION_GREATER "3.1.0")
    set(CMAKE_C_STANDARD 99)
    set(CMAKE_C_STANDARD_REQUIRED TRUE)
else(CMAKE_VERSION VERSION_GREATER "3.1.0")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99") # "poor" fallback
endif(CMAKE_VERSION VERSION_GREATER "3.1.0")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wwrite-strings -Wstrict-prototypes -Wuninitialized -Wunreachable-code -Wno-comment -Wnonnull -Wunreachable-code")

set(SOURCES
    queues/error.c
    parse.c
    engine.c
)
set(LIBRARIES queue)

include_directories(queues)
# To be able to include config.h when outsourcing
include_directories(${CMAKE_BINARY_DIR}/queues)
add_subdirectory(queues)

if(CMAKE_SYSTEM_NAME MATCHES "BSD$")
    add_definitions(-DWITH_PF)
    list(APPEND SOURCES pf.c)

    if(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
        include(CheckStructHasMember)
        check_struct_has_member(npf_ioctl_table_t nct_name net/npf.h NPF_ALLOW_NAMED_TABLE)
        #check_struct_has_member(npf_ioctl_table_t nct_tid net/npf.h NPF_ALLOW_NAMED_TABLE)
        if(NPF_ALLOW_NAMED_TABLE)
            add_definitions(-DNPF_ALLOW_NAMED_TABLE)
        endif(NPF_ALLOW_NAMED_TABLE)
        add_definitions(-DWITH_NPF)
        list(APPEND SOURCES npf.c)
    endif(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
else(CMAKE_SYSTEM_NAME MATCHES "BSD$")
    # Linux
    # nftables
    include(CheckIncludeFile)
    check_include_file("linux/netfilter/nf_tables.h" HAVE_NFTABLES)
    if(HAVE_NFTABLES)
        add_definitions(-DWITH_NFTABLES)
        list(APPEND LIBRARIES "mnl")
        list(APPEND LIBRARIES "nftnl")
        list(APPEND SOURCES nftables.c)
    endif(HAVE_NFTABLES)
    # iptables/ipset
    find_program(IPSET_EXECUTABLE "ipset")
    if(IPSET_EXECUTABLE)
        add_definitions(-DWITH_IPSET)
        list(APPEND SOURCES ipset.c)
    endif(IPSET_EXECUTABLE)
    # iptables (alone)
    add_definitions(-DWITH_IPTABLES)
    list(APPEND SOURCES iptables.c command.c)
    # XXX
    #add_executable(nft-set-elem-add nft-set-elem-add.c)
    #target_link_libraries(nft-set-elem-add ${LIBRARIES})
endif(CMAKE_SYSTEM_NAME MATCHES "BSD$")

list(APPEND SOURCES dummy.c)

add_library(__common_sources OBJECT EXCLUDE_FROM_ALL ${SOURCES})

add_executable(banipd $<TARGET_OBJECTS:__common_sources> banipd.c)
target_link_libraries(banipd ${LIBRARIES})

add_executable(banip-cli banip-cli.c)
target_link_libraries(banip-cli queue)

# Programs for testing
add_executable(pftest $<TARGET_OBJECTS:__common_sources> pftest.c)
target_link_libraries(pftest ${LIBRARIES})

add_custom_target(check COMMAND find ${CMAKE_SOURCE_DIR}/tests/ -name '*.sh' -exec bash {} "\;" DEPENDS banipd pftest)

install(TARGETS banipd banip-cli RUNTIME DESTINATION sbin)
