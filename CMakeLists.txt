cmake_minimum_required(VERSION 3.2)
set(CMAKE_TOOLCHAIN_FILE $ENV{WUT_ROOT}/share/wut.toolchain.cmake)
project(romfs-wiiu C)
include("${WUT_ROOT}/share/wut.cmake" REQUIRED)

add_library(romfs-wiiu STATIC source/romfs.c)

install(TARGETS romfs-wiiu DESTINATION "${CMAKE_INSTALL_PREFIX}/lib")
install(DIRECTORY "${CMAKE_SOURCE_DIR}/share" DESTINATION "${CMAKE_INSTALL_PREFIX}")
install(DIRECTORY "${CMAKE_SOURCE_DIR}/include" DESTINATION "${CMAKE_INSTALL_PREFIX}")

