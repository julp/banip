cmake_minimum_required(VERSION 2.8.3)

project(libvmod-msgsend C)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})
include(VarnishMOD)

add_subdirectory(${PROJECT_SOURCE_DIR}/../.. queues)

declare_vmod(
    INSTALL
    NAME msgsend
    VCC ${PROJECT_SOURCE_DIR}/vmod_msgsend.vcc
    SOURCES ${PROJECT_SOURCE_DIR}/vmod_msgsend.c
    ADDITIONNAL_LIBRARIES banip
    ADDITIONNAL_INCLUDE_DIRECTORIES ${PROJECT_SOURCE_DIR}/../..
)
