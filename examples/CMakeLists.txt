add_executable(example-window example-window.cpp)
target_link_libraries(example-window renoir-window)

if (RENOIR_EXAMPLES_USES_NULL)
	target_link_libraries(example-window renoir-null)
else ()
	target_link_libraries(example-window renoir-gl450)
endif ()
target_compile_definitions(example-window
	PRIVATE
	RENOIR_EXAMPLES_USES_NULL=$<BOOL:${RENOIR_EXAMPLES_USES_NULL}>
)

add_executable(example-window-only example-window-only.cpp)
target_link_libraries(example-window-only renoir-window mn)