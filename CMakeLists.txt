cmake_minimum_required(VERSION 2.8.8)

PROJECT(banip C)

option(DEBUG "Enable/disable debugging" ON) # TODO: OFF

set(CMAKE_C_FLAGS_MAINTAINER "-g -DDEBUG -Wall -Wextra -std=gnu99 -Wwrite-strings -Wstrict-prototypes -Wuninitialized -Wunreachable-code -Wno-comment -Wnonnull"
    CACHE STRING "Flags used by the C compiler during maintainer builds." FORCE)
mark_as_advanced(CMAKE_C_FLAGS_MAINTAINER)
set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES} Maintainer")

if(DEBUG)
	add_definitions(-DDEBUG)
endif(DEBUG)
add_subdirectory(lib)
add_subdirectory(server)
add_subdirectory(clients)
