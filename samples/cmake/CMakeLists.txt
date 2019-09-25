cmake_minimum_required(VERSION 3.0)
set(CMAKE_TOOLCHAIN_FILE "$ENV{DEVKITPRO}/wut/share/wut.toolchain.cmake")

project(romfs-helloworld C)
include("${DEVKITPRO}/wut/share/wut.cmake" REQUIRED)
include("${DEVKITPRO}/portlibs/wiiu/share/romfs-wiiu.cmake" REQUIRED)

# add source files
add_executable(romfs-helloworld
	helloworld.c)

# create and add romfs from "romfs" directory
romfs_add(romfs-helloworld "romfs")

# link requireed libraries
target_link_libraries(romfs-helloworld
	whb
	coreinit
	proc_ui
	sysapp)

# build an rpx
wut_create_rpx(romfs-helloworld.rpx romfs-helloworld)
