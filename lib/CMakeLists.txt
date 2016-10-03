cmake_minimum_required(VERSION 2.8.8)

set(SOURCES )
set(LIBRARIES )

include(CheckLibraryExists)
include(CheckFunctionExists)

check_function_exists("strlcpy" HAVE_STRLCPY)
if(NOT HAVE_STRLCPY)
	check_library_exists("bsd" "strlcpy" "lib" HAVE_LIBBSD_STRLCPY)
	if(HAVE_LIBBSD_STRLCPY)
		list(APPEND LIBRARIES "bsd")
	else(HAVE_LIBBSD_STRLCPY)
		list(APPEND SOURCES "${PROJECT_SOURCE_DIR}/missing/strlcpy.c")
	endif(HAVE_LIBBSD_STRLCPY)
endif(NOT HAVE_STRLCPY)

configure_file(
	"config.h.in"
	"config.h"
	@ONLY
)

include_directories(
	${PROJECT_SOURCE_DIR}
)
add_library(banip SHARED banip.c)

set_target_properties(
	banip PROPERTIES
	PUBLIC_HEADER ${PROJECT_SOURCE_DIR}/banip.h
)
install(
	TARGETS banip
	LIBRARY DESTINATION lib
	PUBLIC_HEADER DESTINATION include
)
