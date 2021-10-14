cmake_minimum_required(VERSION 3.16)

set(MASTER_PROJECT OFF)
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
	set(MASTER_PROJECT ON)
endif ()

project(renoir LANGUAGES CXX C)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_INSTALL_RPATH "$ORIGIN")
set(CMAKE_BUILD_RPATH "$ORIGIN")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

option(RENOIR_BUILD_EXAMPLES "Build example applications that showcase the renoir libraries." ${MASTER_PROJECT})
option(RENOIR_UNITY_BUILD "Combine all renoir source files into one jumbo build." ON)
option(RENOIR_DEBUG_LAYER "Turn on debug layer in underlying graphics api" OFF)
option(RENOIR_LEAK "Turn on leak detector for graphics resources" OFF)
option(RENOIR_EXAMPLES_USES_NULL "Uses renoir null implementation for examples" OFF)

# external dependencies
include(CPM)
CPMAddPackage(
	NAME             mn
	GIT_REPOSITORY   https://github.com/MoustaphaSaad/mn.git
	GIT_TAG          d617fce8b9223ba6fc5e63508d175d55796e71e6
)

add_subdirectory(renoir-window)

add_library(renoir INTERFACE)
add_library(MoustaphaSaad::renoir ALIAS renoir)
target_include_directories(renoir INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

if (WIN32)
	add_subdirectory(renoir-null)

	add_subdirectory(renoir-dx11)
	add_dependencies(renoir renoir-dx11)

	add_subdirectory(external/glew EXCLUDE_FROM_ALL)
	add_subdirectory(renoir-gl450)
	add_dependencies(renoir renoir-gl450)
elseif (UNIX AND NOT APPLE)
	add_subdirectory(renoir-null)

	add_subdirectory(external/glew EXCLUDE_FROM_ALL)
	add_subdirectory(renoir-gl450)
	add_dependencies(renoir renoir-gl450)
elseif (APPLE)
	add_subdirectory(renoir-null)
endif (WIN32)

if (RENOIR_BUILD_EXAMPLES)
	add_subdirectory(examples)
endif()
