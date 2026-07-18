project(vkrend C)

find_package(VulkanHeaders REQUIRED)
find_package(VulkanLoader REQUIRED)

find_program(GLSL_COMPILER NAMES glslangValidator glslc)
if(GLSL_COMPILER)
    get_filename_component(GLSL_COMPILER_NAME ${GLSL_COMPILER} NAME)
    message(STATUS "Found GLSL compiler: ${GLSL_COMPILER} (auto-compile .glsl -> .spv)")
endif()

set(SPV_GENERATED)
foreach(spv_file
    brender.vert.spv
    brender.frag.spv
    overlay.vert.spv
    overlay.frag.spv
)
    get_filename_component(spv_base ${spv_file} NAME_WE)
    string(REPLACE ".spv" ".glsl" glsl_file ${spv_file})
    set(h_path "${CMAKE_CURRENT_BINARY_DIR}/${spv_file}.h")

    if(GLSL_COMPILER)
        set(spv_path "${CMAKE_CURRENT_BINARY_DIR}/${spv_file}")
        set(glsl_path "${CMAKE_CURRENT_SOURCE_DIR}/${glsl_file}")
        if(GLSL_COMPILER_NAME STREQUAL "glslangValidator")
            set(compile_cmd ${GLSL_COMPILER} -V -o ${spv_path} ${glsl_path})
        else()
            set(compile_cmd ${GLSL_COMPILER} -o ${spv_path} ${glsl_path})
        endif()
        add_custom_command(
            OUTPUT ${spv_path}
            COMMAND ${compile_cmd}
            DEPENDS ${glsl_path}
            COMMENT "Compiling ${glsl_file} → ${spv_file}"
        )
        set(embed_source_dir "${CMAKE_CURRENT_BINARY_DIR}")
        set(embed_depends ${spv_path})
    else()
        set(embed_source_dir "${CMAKE_CURRENT_SOURCE_DIR}")
        set(embed_depends ${CMAKE_CURRENT_SOURCE_DIR}/${spv_file})
    endif()

    add_custom_command(
        OUTPUT ${h_path}
        COMMAND ${CMAKE_COMMAND} "-DSOURCE_DIR=${embed_source_dir}" "-DFILE=${spv_file}" -P "${BRender_SOURCE_DIR}/cmake/EmbedResource.cmake"
        DEPENDS ${embed_depends}
        COMMENT "Embedding ${spv_file}"
    )

    list(APPEND SPV_GENERATED ${h_path})
endforeach()

set(VKSTATE_FILES
        cache.c
        state.c
        state.h
        state_clip.c
        state_cull.c
        state_hidden.c
        state_light.c
        state_matrix.c
        state_output.c
        state_primitive.c
        state_surface.c
        )
source_group("STATE" FILES ${VKSTATE_FILES})

set(VIDEO_FILES
        vk_shaders.c
        video.c
        video.h
        )
source_group("VIDEO" FILES ${VIDEO_FILES})

set(RENDERER_FILES
        gv1model.c
        gv1model.h

        gstored.c
        gstored.h
        v1model.c

        onscreen.c

        renderer.c
        renderer.h

        sbuffer.h
        sbuffer.c

        sstate.c
        sstate.h
        )
source_group("Renderer" FILES ${RENDERER_FILES})

set(VKREND_FILES
        drv.h
        drv_ip.h

        driver.c

        outfcty.c
        outfcty.h

        ext_procs.c

        rendfcty.c
        rendfcty.h

        device.c
        device.h

        devpixmp.c
        devpixmp.h
        devpmvkf.c

        devclut.c
        devclut.h

        template.h

        vkassert.c
        vkassert.h
        )

set(INC_FILES
        include/brvkrend.h
        include/brvkrend_p.h
        )

add_library(vkrend-headers INTERFACE)
target_sources(vkrend-headers INTERFACE
        FILE_SET include
        TYPE HEADERS
        BASE_DIRS include
        FILES ${INC_FILES}
        )

target_include_directories(vkrend-headers INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/brender/vkrend>
        )

set_target_properties(vkrend-headers PROPERTIES
        EXPORT_NAME "Drivers::VKRend::Headers"
        FOLDER "BRender/Drivers"
        )

add_library(vkrend
        ${VKREND_FILES}
        ${RENDERER_FILES}
        ${VIDEO_FILES}
        ${VKSTATE_FILES}
        ${INC_FILES}
        ${SPV_GENERATED}
        )

get_target_property(target_type vkrend TYPE)
if(target_type STREQUAL SHARED_LIBRARY)
    target_compile_definitions(vkrend PRIVATE -DDEFINE_BR_ENTRY_POINT)
endif()

target_include_directories(vkrend PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(vkrend PRIVATE BRender::DDI Vulkan::Loader Vulkan::Headers)
target_link_libraries(vkrend PUBLIC vkrend-headers)

set_target_properties(vkrend PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED TRUE
        EXPORT_NAME "Drivers::VKRend"
        FOLDER "BRender/Drivers"
        )

if(BRENDER_ASAN_ENABLED)
    set(CMAKE_C_FLAGS
            "${CMAKE_C_FLAGS}  -fsanitize=undefined -fsanitize=address"
    )
    target_compile_options(vkrend PUBLIC -fsanitize=address)
    target_link_options(vkrend
            BEFORE PUBLIC -fsanitize=undefined PUBLIC -fsanitize=address,undefined
    )
endif()

add_library(BRender::Drivers::VKRend::Headers ALIAS vkrend-headers)
add_library(BRender::Drivers::VKRend ALIAS vkrend)
