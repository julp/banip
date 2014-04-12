cmake_minimum_required(VERSION 2.8.3)

project(libvmod-msgsend C)

# cmake . -DVARNISHSRC:PATH=$HOME/Downloads/varnish-4.0.0/

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})
include(VarnishMOD)

declare_vmod(
    INSTALL
    NAME msgsend
    VCC src/vmod_msgsend.vcc
    SOURCES src/vmod_msgsend.c
    ADDITIONNAL_LIBRARIES "rt"
)