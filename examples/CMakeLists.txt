add_executable(example-window example-window.cpp)
target_link_libraries(example-window renoir-window renoir-gl450)

add_executable(example-window-only example-window-only.cpp)
target_link_libraries(example-window-only renoir-window mn)