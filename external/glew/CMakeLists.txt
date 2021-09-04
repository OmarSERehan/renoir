cmake_minimum_required(VERSION 3.9)

# list the header files
set(HEADER_FILES
	include/GL/eglew.h
	include/GL/glew.h
	include/GL/glxew.h
	include/GL/wglew.h
)

# list the source files
set(SOURCE_FILES
	src/glew.c
	src/glewinfo.c
	src/visualinfo.c
)


# add library target
add_library(renoir-glew STATIC
	${HEADER_FILES}
	${SOURCE_FILES}
)

# make it reflect the same structure as the one on disk
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${HEADER_FILES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCE_FILES})

target_compile_definitions(renoir-glew
	PUBLIC
		GLEW_STATIC=1
)

# Windows specfic flags to enable W variations of winapi
if(WIN32)
	target_link_libraries(renoir-glew
		PUBLIC
			opengl32
	)
elseif(UNIX AND NOT APPLE)
	target_link_libraries(renoir-glew
		PUBLIC
			X11
			GL
			GLU
	)
endif()


# add d suffix in debug mode
set_target_properties(renoir-glew PROPERTIES DEBUG_POSTFIX d)

# define debug macro
target_compile_definitions(renoir-glew PRIVATE "$<$<CONFIG:DEBUG>:DEBUG>")

# list include directories
target_include_directories(renoir-glew
	PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/include
)
