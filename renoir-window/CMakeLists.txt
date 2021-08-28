cmake_minimum_required(VERSION 3.16)

# list the header files
set(HEADER_FILES
	include/renoir-window/Window.h
)

# list the source files
set(SOURCE_FILES
)

# add library target
add_library(renoir-window)

# list os specfic files
if(WIN32)
	set(SOURCE_FILES ${SOURCE_FILES}
		src/renoir-window/winos/Window.cpp
	)
elseif(UNIX AND NOT APPLE)
	set(SOURCE_FILES ${SOURCE_FILES}
		src/renoir-window/linux/Window.cpp
	)
elseif(APPLE)
	set(SOURCE_FILES ${SOURCE_FILES}
		src/renoir-window/macos/Window.mm
	)
	find_library(COCOA_LIBRARY Cocoa)
	target_link_libraries(renoir-window PRIVATE ${COCOA_LIBRARY})
	enable_language(OBJCXX)
endif()

target_sources(renoir-window
	PRIVATE ${HEADER_FILES}
			${SOURCE_FILES}
)

add_library(MoustaphaSaad::renoir-window ALIAS renoir-window)

target_link_libraries(renoir-window
	PRIVATE
		"$<$<PLATFORM_ID:Linux>:X11;GL;GLU>"
		"$<$<PLATFORM_ID:Darwin>:-framework Cocoa>"
		MoustaphaSaad::mn
)

# make it reflect the same structure as the one on disk
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${HEADER_FILES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCE_FILES})

# enable C++17
# disable any compiler specifc extensions
# add d suffix in debug mode
target_compile_features(renoir-window PUBLIC cxx_std_17)
set_target_properties(renoir-window PROPERTIES
	CXX_EXTENSIONS OFF
	DEBUG_POSTFIX d
)

# generate exports header file
include(GenerateExportHeader)
generate_export_header(renoir-window
	EXPORT_FILE_NAME ${CMAKE_CURRENT_SOURCE_DIR}/include/renoir-window/Exports.h
)

# list include directories
target_include_directories(renoir-window
	PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/include
)
