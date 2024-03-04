project(ddi_inc)

add_library(ddi_inc INTERFACE)

target_include_directories(ddi_inc INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# target_include_directories(pixelmap PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
