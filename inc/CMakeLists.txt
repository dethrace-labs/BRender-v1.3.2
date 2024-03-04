project(inc)

add_library(inc INTERFACE)

target_include_directories(inc INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# target_include_directories(pixelmap PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
