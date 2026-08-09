add_subdirectory(softrend)
add_subdirectory(pentprim)
add_subdirectory(virtual_fb)
add_subdirectory(glrend)
add_subdirectory(sdl3rend)

target_link_libraries(brender-full INTERFACE softrend pentprim virtual_fb glrend sdl3rend)
