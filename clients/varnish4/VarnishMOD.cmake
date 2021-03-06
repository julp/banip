cmake_minimum_required(VERSION 2.8.3)

#if(NOT DEFINED VARNISHSRC)
    #message(WARNING "You may need to add -DVARNISHSRC:PATH=/path/to/varnish/sources to your cmake command line or define it through its GUI (ccmake & co)")
#endif(NOT DEFINED VARNISHSRC)

find_package(PkgConfig QUIET)

set(PKG_VARNISHAPI_NAME "varnishapi")
set(PKG_VARNISHAPI_VARNAMES_TO_EXPORT vmoddir vmodincludedir vmodtool pkgincludedir)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(VARNISHAPI ${PKG_VARNISHAPI_NAME} REQUIRED)
#     message("PKG_CONFIG_EXECUTABLE = ${PKG_CONFIG_EXECUTABLE}")
    if(VARNISHAPI_FOUND)
        foreach(PKG_VARNISHAPI_VARNAME ${PKG_VARNISHAPI_VARNAMES_TO_EXPORT})
            execute_process(
                COMMAND ${PKG_CONFIG_EXECUTABLE} ${PKG_VARNISHAPI_NAME} --variable=${PKG_VARNISHAPI_VARNAME}
                OUTPUT_VARIABLE PKG_OUTPUT_VAR OUTPUT_STRIP_TRAILING_WHITESPACE
                RESULT_VARIABLE PKG_RESULT_VAR
            )
            if(PKG_RESULT_VAR)
                message(FATAL_ERROR "pkg-config variable ${PKG_VARNISHAPI_VARNAME} not found for varnishapi")
            endif(PKG_RESULT_VAR)
            string(TOUPPER ${PKG_VARNISHAPI_VARNAME} PKG_VARNISHAPI_UPPER_VARNAME)
            set(VARNISHAPI_${PKG_VARNISHAPI_UPPER_VARNAME} ${PKG_OUTPUT_VAR})
        endforeach(PKG_VARNISHAPI_VARNAME)
    endif(VARNISHAPI_FOUND)
endif(PKG_CONFIG_FOUND)

# message("VARNISHAPI_VMODTOOL = ${VARNISHAPI_VMODTOOL}")
# message("VARNISHAPI_VMODINCLUDEDIR = ${VARNISHAPI_VMODINCLUDEDIR}")
# message("VARNISHAPI_VMODDIR = ${VARNISHAPI_VMODDIR}")
# message("VARNISHAPI_PKGINCLUDEDIR = ${VARNISHAPI_PKGINCLUDEDIR}")

# if(VARNISHAPI_VERSION MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)")
#     set(VARNISHAPI_MAJOR_VERSION ${CMAKE_MATCH_1})
#     set(VARNISHAPI_MINOR_VERSION ${CMAKE_MATCH_2})
#     set(VARNISHAPI_PATH_VERSION ${CMAKE_MATCH_3})
# endif(VARNISHAPI_VERSION MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)")

# if(VARNISHAPI_VERSION VERSION_LESS "6.2.0")
    find_package(PythonInterp REQUIRED)
# else(VARNISHAPI_VERSION VERSION_LESS "6.2.0")
    # Varnish 6.2.0 requires Python >= 3.4.0
    # We use Python3 "package" in order to specificly find a python 3 version
    # as there can be several python executables/versions on a same host
#     find_package(Python3 3.4 REQUIRED)
    # alias Python3_EXECUTABLE as PYTHON_EXECUTABLE for compatibility
#     set(PYTHON_EXECUTABLE "${Python3_EXECUTABLE}")
# endif(VARNISHAPI_VERSION VERSION_LESS "6.2.0")

macro(declare_vmod)
    cmake_parse_arguments(VMOD "INSTALL;UNSTRICT" "NAME;VCC" "ADDITIONNAL_INCLUDE_DIRECTORIES;ADDITIONNAL_LIBRARIES;SOURCES" ${ARGN})

    # TODO: !empty(VMOD_SOURCES)
    # TODO: !empty(VMOD_NAME)
    file(APPEND config.h "")
    if(VARNISHAPI_VERSION VERSION_GREATER "4.0.0" AND NOT VMOD_UNSTRICT)
        set(VARNISHAPI_VMODTOOL_ARGS "--strict")
    else(VARNISHAPI_VERSION VERSION_GREATER "4.0.0" AND NOT VMOD_UNSTRICT)
        set(VARNISHAPI_VMODTOOL_ARGS "")
    endif(VARNISHAPI_VERSION VERSION_GREATER "4.0.0" AND NOT VMOD_UNSTRICT)
    add_custom_command(
        OUTPUT "${PROJECT_BINARY_DIR}/vcc_if.c" "${PROJECT_BINARY_DIR}/vcc_if.h"
        COMMAND ${PYTHON_EXECUTABLE} ${VARNISHAPI_VMODTOOL} ${VARNISHAPI_VMODTOOL_ARGS} ${VMOD_VCC}
        DEPENDS ${VMOD_VCC}
    )
#     add_custom_target(
#         "${VMOD_NAME}_vcc_if" ALL
#         COMMENT "vcc_if.[ch]"
#         DEPENDS vcc_if.h vcc_if.c
#     )
#     add_dependencies(${VMOD_NAME} "${VMOD_NAME}_vcc_if")
    list(APPEND VMOD_SOURCES "${PROJECT_BINARY_DIR}/vcc_if.c")
    list(APPEND VMOD_SOURCES "${PROJECT_BINARY_DIR}/vcc_if.h")
    add_library(${VMOD_NAME} SHARED ${VMOD_SOURCES})
    if(VMOD_ADDITIONNAL_LIBRARIES)
        target_link_libraries(${VMOD_NAME} ${VMOD_ADDITIONNAL_LIBRARIES})
    endif(VMOD_ADDITIONNAL_LIBRARIES)
    set(VMOD_INCLUDE_DIRECTORIES )
    list(APPEND VMOD_INCLUDE_DIRECTORIES ${PROJECT_SOURCE_DIR})
    list(APPEND VMOD_INCLUDE_DIRECTORIES ${PROJECT_BINARY_DIR})
    if(DEFINED VARNISHSRC)
        list(APPEND VMOD_INCLUDE_DIRECTORIES "${VARNISHSRC}/include")
    endif(DEFINED VARNISHSRC)
    list(APPEND VMOD_INCLUDE_DIRECTORIES ${VARNISHAPI_VMODINCLUDEDIR})
    list(APPEND VMOD_INCLUDE_DIRECTORIES ${VARNISHAPI_PKGINCLUDEDIR})
    list(APPEND VMOD_INCLUDE_DIRECTORIES ${VMOD_ADDITIONNAL_INCLUDE_DIRECTORIES})
    set_target_properties(${VMOD_NAME} PROPERTIES INCLUDE_DIRECTORIES "${VMOD_INCLUDE_DIRECTORIES}" PREFIX "libvmod_")
    get_target_property(VAR ${VMOD_NAME} INCLUDE_DIRECTORIES)
    if(VMOD_INSTALL)
        install(TARGETS ${VMOD_NAME} LIBRARY DESTINATION ${VARNISHAPI_VMODDIR})
    endif(VMOD_INSTALL)
endmacro(declare_vmod)
