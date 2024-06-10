add_subdirectory(softrend)
add_subdirectory(pentprim)
target_link_libraries(brender-full INTERFACE softrend pentprim)
