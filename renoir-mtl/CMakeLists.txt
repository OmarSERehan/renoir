cmake_minimum_required(VERSION 3.16)

# list the header files
set(HEADER_FILES
	include/renoir-mtl/Renoir-mtl.h
)

# list the source files
set(SOURCE_FILES
	src/renoir-mtl/Renoir-mtl.mm
)

# add library target
add_library(renoir-mtl)

enable_language(OBJCXX)
target_sources(renoir-mtl
	PRIVATE ${HEADER_FILES}
			${SOURCE_FILES}
			${PROJECT_SOURCE_DIR}/include/renoir/Renoir.h
)

set_target_properties(renoir-mtl PROPERTIES PREFIX "")

if (RENOIR_UNITY_BUILD)
	set_target_properties(renoir-mtl
		PROPERTIES UNITY_BUILD_BATCH_SIZE 0
				   UNITY_BUILD true)
endif()

add_library(MoustaphaSaad::renoir-mtl ALIAS renoir-mtl)

find_library(METAL_LIBRARY Metal)
find_library(QUARTZ_LIBRARY QuartzCore)
find_library(COCOA_LIBRARY Cocoa)
target_link_libraries(renoir-mtl
	PRIVATE
		mn
		${METAL_LIBRARY}
		${QUARTZ_LIBRARY}
		${COCOA_LIBRARY}
)

# make it reflect the same structure as the one on disk
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${HEADER_FILES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCE_FILES})

# enable C++17
# disable any compiler specifc extensions
target_compile_features(renoir-mtl PUBLIC cxx_std_17)
set_target_properties(renoir-mtl PROPERTIES
	CXX_EXTENSIONS OFF
)

# generate exports header file
include(GenerateExportHeader)
generate_export_header(renoir-mtl
	EXPORT_FILE_NAME ${CMAKE_CURRENT_SOURCE_DIR}/include/renoir-mtl/Exports.h
)

# list include directories
target_include_directories(renoir-mtl
	PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${PROJECT_SOURCE_DIR}/include
)

if (${RENOIR_DEBUG_LAYER})
	message(STATUS "feature: renoir-mtl debug layer enabled")
	target_compile_definitions(renoir-mtl PRIVATE RENOIR_DEBUG_LAYER=1)
else()
	target_compile_definitions(renoir-mtl PRIVATE RENOIR_DEBUG_LAYER=0)
endif()

if (${RENOIR_LEAK})
	message(STATUS "feature: renoir-mtl leak detector enabled")
	target_compile_definitions(renoir-mtl PRIVATE RENOIR_LEAK=1)
else()
	target_compile_definitions(renoir-mtl PRIVATE RENOIR_LEAK=0)
endif()
