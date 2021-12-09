add_executable(example-window example-window.cpp)
target_link_libraries(example-window
	PRIVATE
		renoir-window
		renoir-null
		renoir-gl450
)

if (TARGET renoir-dx11)
	target_link_libraries(example-window PRIVATE renoir-dx11)
endif ()

add_executable(example-window-only example-window-only.cpp)
target_link_libraries(example-window-only renoir-window mn)