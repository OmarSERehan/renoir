cmake_minimum_required(VERSION 3.16)

# list the header files
set(HEADER_FILES
	include/renoir-null/Renoir-null.h
)

# list the source files
set(SOURCE_FILES
	src/renoir-null/Renoir-null.cpp
)

# add library target
add_library(renoir-null)

target_sources(renoir-null
	PRIVATE ${HEADER_FILES}
			${SOURCE_FILES}
			${PROJECT_SOURCE_DIR}/include/renoir/Renoir.h
)

set_target_properties(renoir-null PROPERTIES PREFIX "")

if (RENOIR_UNITY_BUILD)
	set_target_properties(renoir-null
		PROPERTIES UNITY_BUILD_BATCH_SIZE 0
				   UNITY_BUILD true)
endif()

add_library(MoustaphaSaad::renoir-null ALIAS renoir-null)

target_link_libraries(renoir-null
	PRIVATE
		mn
)

# make it reflect the same structure as the one on disk
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${HEADER_FILES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCE_FILES})

# enable C++17
# disable any compiler specifc extensions
target_compile_features(renoir-null PUBLIC cxx_std_17)
set_target_properties(renoir-null PROPERTIES
	CXX_EXTENSIONS OFF
)

# generate exports header file
include(GenerateExportHeader)
generate_export_header(renoir-null
	EXPORT_FILE_NAME ${CMAKE_CURRENT_SOURCE_DIR}/include/renoir-null/Exports.h
)

# list include directories
target_include_directories(renoir-null
	PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${PROJECT_SOURCE_DIR}/include
)

if (${RENOIR_DEBUG_LAYER})
	message(STATUS "feature: renoir-null debug layer enabled")
	target_compile_definitions(renoir-null PRIVATE RENOIR_DEBUG_LAYER=1)
else()
	target_compile_definitions(renoir-null PRIVATE RENOIR_DEBUG_LAYER=0)
endif()

if (${RENOIR_LEAK})
	message(STATUS "feature: renoir-null leak detector enabled")
	target_compile_definitions(renoir-null PRIVATE RENOIR_LEAK=1)
else()
	target_compile_definitions(renoir-null PRIVATE RENOIR_LEAK=0)
endif()