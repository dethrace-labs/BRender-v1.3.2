project(virtual_fb C)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

add_library(virtual_fb
    device.c
    devclut.c
    driver.c
    object.c
    outfcty.c
    devpixmp.c

    device.h
    devclut.h
    drv.h
    drv_ip.h
    object.h
    outfcty.h
)

get_target_property(target_type virtual_fb TYPE)

# target_include_directories(softrend PRIVATE ddi)
#target_compile_definitions(softrend PRIVATE -D__386__)

target_link_libraries(virtual_fb PRIVATE BRender::DDI)

set_target_properties(virtual_fb PROPERTIES
        EXPORT_NAME "Drivers::VirtualFramebuffer"
        FOLDER "BRender/Drivers"
        )

add_library(BRender::Drivers::VirtualFramebuffer ALIAS virtual_fb)
