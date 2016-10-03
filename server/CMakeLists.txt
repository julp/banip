cmake_minimum_required(VERSION 2.8.8)

include(CheckIncludeFile)

set(LIBRARIES banip)
set(SOURCES banipd.c conf.c engine.c fd.c)

if(NOT DEFINED DEFAULT_CONFIGURATION_FILE)
	set(DEFAULT_CONFIGURATION_FILE "/usr/local/etc/banipd.conf")
endif(NOT DEFINED DEFAULT_CONFIGURATION_FILE)

if(CMAKE_SYSTEM_NAME MATCHES "BSD$")
	list(APPEND LIBRARIES util)
	# On *BSD add support of pf(ctl)
	set(WITH_PF TRUE)
	list(APPEND SOURCES engines/pf.c)
	find_program(PFCTL_EXECUTABLE pfctl)
	if(PFCTL_EXECUTABLE)
		set(WITH_PFCTL TRUE)
		list(APPEND SOURCES engines/pfctl.c command.c)
	endif(PFCTL_EXECUTABLE)
	# On FreeBSD, harden with capsicum/casper
	if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
# 		check_include_file("sys/capsicum.h" HAVE_CAPSICUM_H)
# 		check_include_file("sys/capability.h" HAVE_CAPABILITY_H)
# 		include(CheckLibraryExists)
		check_include_file("libcasper.h" HAVE_LIBCASPER_H)
		if(HAVE_LIBCASPER_H)
			list(APPEND LIBRARIES casper cap_pwd cap_grp)
		endif(HAVE_LIBCASPER_H)
	endif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
	# On NetBSD add support for npf(ctl)
	if(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
		include(CheckStructHasMember)
		check_struct_has_member(npf_ioctl_table_t nct_name net/npf.h NPF_ALLOW_NAMED_TABLE)
		#check_struct_has_member(npf_ioctl_table_t nct_tid net/npf.h NPF_ALLOW_NAMED_TABLE)
		set(WITH_NPF TRUE)
		list(APPEND SOURCES engines/npf.c)

		find_program(NPFCTL_EXECUTABLE npfctl)
		if(NPFCTL_EXECUTABLE)
			set(WITH_NPFCTL TRUE)
			list(APPEND SOURCES engines/npfctl.c command.c)
		endif(NPFCTL_EXECUTABLE)
	endif(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
else(CMAKE_SYSTEM_NAME MATCHES "BSD$")
	# Linux
	# nftables
	check_include_file("linux/netfilter/nf_tables.h" HAVE_NFTABLES)
	if(HAVE_NFTABLES)
		set(WITH_NFTABLES TRUE)
		list(APPEND LIBRARIES "mnl")
		list(APPEND LIBRARIES "nftnl")
		list(APPEND SOURCES nftables.c)
	endif(HAVE_NFTABLES)
	# XXX
	#add_executable(nft-set-elem-add nft-set-elem-add.c)
	#target_link_libraries(nft-set-elem-add ${LIBRARIES})

	find_program(IPTABLES_EXECUTABLE iptables)
	if(IPTABLES_EXECUTABLE)
		# iptables (alone)
		set(WITH_IPTABLES TRUE)
		list(APPEND SOURCES engines/iptables.c command.c)
		find_program(IPSET_EXECUTABLE ipset)
		# iptables/ipset
		if(IPSET_EXECUTABLE)
			set(WITH_IPSET TRUE)
			list(APPEND SOURCES engines/ipset.c command.c)
		endif(IPSET_EXECUTABLE)
	endif(IPTABLES_EXECUTABLE)
endif(CMAKE_SYSTEM_NAME MATCHES "BSD$")

list(APPEND SOURCES engines/dummy.c) # in debug mode only?

list(REMOVE_DUPLICATES SOURCES)

configure_file(
	"config.h.in"
	"config.h"
	@ONLY
)

include_directories(
	${PROJECT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(banipd ${SOURCES})
target_link_libraries(banipd ${LIBRARIES})

# Programs for testing
# add_executable(pftest $<TARGET_OBJECTS:__common_sources> pftest.c)
# target_link_libraries(pftest ${LIBRARIES})

# add_custom_target(check COMMAND find ${CMAKE_SOURCE_DIR}/tests/ -name '*.sh' -exec bash {} "\;" DEPENDS banipd pftest)

install(TARGETS banipd RUNTIME DESTINATION sbin)
