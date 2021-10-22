add_executable(example-window example-window.cpp)
target_link_libraries(example-window renoir-window)

if (RENOIR_EXAMPLES_USES_NULL)
	target_link_libraries(example-window renoir-null)
elseif (RENOIR_EXAMPLES_USES_GL450)
	target_link_libraries(example-window renoir-gl450)
elseif (RENOIR_EXAMPLES_USES_MTL)
	target_link_libraries(example-window renoir-mtl)
endif ()
target_compile_definitions(example-window
	PRIVATE
	RENOIR_EXAMPLES_USES_NULL=$<BOOL:${RENOIR_EXAMPLES_USES_NULL}>
	RENOIR_EXAMPLES_USES_GL450=$<BOOL:${RENOIR_EXAMPLES_USES_GL450}>
	RENOIR_EXAMPLES_USES_MTL=$<BOOL:${RENOIR_EXAMPLES_USES_MTL}>
)

add_executable(example-window-only example-window-only.cpp)
target_link_libraries(example-window-only renoir-window mn)