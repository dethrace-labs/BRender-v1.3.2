project(softcube)

find_package(SDL2 CONFIG)
if(NOT SDL2_FOUND)
    find_package(SDL2 MODULE REQUIRED)
endif()

add_executable(softcube16 main.c)
target_compile_definitions(softcube16 PUBLIC -DSDL_MAIN_HANDLED -DSOFTCUBE_16BIT)
target_compile_options(softcube16 PUBLIC )
target_link_options(softcube16 PUBLIC )

target_link_libraries(softcube16 BRender::Full BRender::DDI SDL2::SDL2)
set_property(TARGET softcube16 PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../dat")
set_property(TARGET softcube16 PROPERTY FOLDER "BRender/Examples")

# add_executable(softcube24 main.c)
# target_compile_definitions(softcube24 PUBLIC -DSDL_MAIN_HANDLED)
# target_link_libraries(softcube24 BRender::Full BRender::DDI)
# set_property(TARGET softcube24 PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../dat")
# set_property(TARGET softcube24 PROPERTY FOLDER "BRender/Examples")
