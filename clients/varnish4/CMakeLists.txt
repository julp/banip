cmake_minimum_required(VERSION 2.8.3)

project(libvmod-msgsend C)

# cmake . -DVARNISHSRC:PATH=$HOME/Downloads/varnish-4.0.1/

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})
include(VarnishMOD)

add_subdirectory(${PROJECT_SOURCE_DIR}/../../queues queues)

declare_vmod(
    INSTALL
    NAME msgsend
    VCC ${PROJECT_SOURCE_DIR}/vmod_msgsend.vcc
    SOURCES ${PROJECT_SOURCE_DIR}/vmod_msgsend.c ${PROJECT_SOURCE_DIR}/../../queues/error.c
    ADDITIONNAL_LIBRARIES queue
    ADDITIONNAL_INCLUDE_DIRECTORIES ${PROJECT_SOURCE_DIR}/../../queues
)
