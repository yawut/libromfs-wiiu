 cmake_minimum_required(VERSION 3.3) #3.3 for IN_LIST

# add romfs to project
macro(romfs_add target romfs_dir)
	# find absolute romfs directory
	set(ROMFS_INPUT_DIR ${romfs_dir})
	if(NOT IS_ABSOLUTE ${romfs_dir})
		set(ROMFS_INPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${romfs_dir}")
	endif()

	# build tar object file from directory
	add_custom_command(
		OUTPUT app.romfs.o
		COMMAND tar -H ustar -cvf romfs.tar -C "${ROMFS_INPUT_DIR}" .
		COMMAND ${CMAKE_LINKER} --relocatable --format binary --output app.romfs.o romfs.tar
		COMMAND rm -f romfs.tar
		DEPENDS ${ROMFS_INPUT_DIR}
	)

	# build a static library from the object
	add_library(
		ROMFS
		STATIC
		app.romfs.o
	)

	set_source_files_properties(
		app.romfs.o
		PROPERTIES
		EXTERNAL_OBJECT true
		GENERATED true
	)

  	get_property(languages GLOBAL PROPERTY ENABLED_LANGUAGES)
	if("CXX" IN_LIST languages)
		set_target_properties(
			ROMFS
			PROPERTIES
			LINKER_LANGUAGE CXX
		)
	else()
		set_target_properties(
			ROMFS
			PROPERTIES
			LINKER_LANGUAGE C
		)
	endif()

	# link the tar object static library to the final target


	# find the libromfs-wiiu library and headers
	find_path(
		ROMFS_INCLUDE_DIR
		romfs-wiiu.h
	)

	find_library(
		ROMFS_LIBRARY
		romfs-wiiu
	)

	# add the libromfs-wiiu library and headers to the final target
	target_include_directories(
		${target}
		PUBLIC
		${ROMFS_INCLUDE_DIR}
	)

	target_link_libraries(
		${target}
		${ROMFS_LIBRARY}
		ROMFS
	)
endmacro()
