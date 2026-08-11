project(sdl3rend C)

# NOTE: This driver is not yet wired into the top-level build (drivers/CMakeLists.txt).
# This file is the target manifest, mirroring sdl3rend (the Vulkan driver this one
# replaces). Files listed below are created incrementally during the port.

# SDL3 dependency. When this driver is wired into the build, the BRender top-level
# CMake must locate SDL3 (find_package(SDL3 CONFIG), as dethrace does) and expose it
# to this subdirectory.
find_package(SDL3 CONFIG QUIET)

# Also accept the unified `glslang` tool (the modern glslangValidator
# replacement, and the only binary shipped in the Windows SDK zip) — it shares
# glslangValidator's `-V -o` CLI.
find_program(GLSL_COMPILER NAMES glslangValidator glslang glslc)
if(GLSL_COMPILER)
    get_filename_component(GLSL_COMPILER_NAME ${GLSL_COMPILER} NAME_WE)
    message(STATUS "Found GLSL compiler: ${GLSL_COMPILER} (auto-compile .glsl -> .spv)")
endif()

# Shared shaders live in drivers/commonrend with ##ifdef SDL3GPU/GL_ES/GL_CORE
# markers. Filter for SPIR-V (plain GLSL, literal-first #version 450) before
# compiling; the SDL3GPU marker keeps the set-1/2/3 binding layout that the
# SDL3-GPU pipeline layout uses.
set(SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../commonrend")

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
        set(glsl_src "${SHADER_DIR}/${glsl_file}")
        set(glsl_filtered "${CMAKE_CURRENT_BINARY_DIR}/${glsl_file}")
        set(spv_path "${CMAKE_CURRENT_BINARY_DIR}/${spv_file}")
        add_custom_command(
            OUTPUT ${glsl_filtered}
            COMMAND ${CMAKE_COMMAND} "-DINPUT=${glsl_src}" "-DOUTPUT=${glsl_filtered}" "-DKEEP=SDL3GPU" -P "${BRender_SOURCE_DIR}/cmake/FilterShader.cmake"
            DEPENDS ${glsl_src}
            COMMENT "Filtering ${glsl_file} for SPIR-V"
        )
        if(GLSL_COMPILER_NAME STREQUAL "glslangValidator" OR GLSL_COMPILER_NAME STREQUAL "glslang")
            set(compile_cmd ${GLSL_COMPILER} -V -o ${spv_path} ${glsl_filtered})
        else()
            set(compile_cmd ${GLSL_COMPILER} -o ${spv_path} ${glsl_filtered})
        endif()
        add_custom_command(
            OUTPUT ${spv_path}
            COMMAND ${compile_cmd}
            DEPENDS ${glsl_filtered}
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

set(REND_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../commonrend")

set(SDL3REND_STATE_FILES
        ${REND_COMMON_DIR}/cache.c
        ${REND_COMMON_DIR}/modeldata.c
        ${REND_COMMON_DIR}/modelrender.c
        ${REND_COMMON_DIR}/gstored.c
        ${REND_COMMON_DIR}/state.c
        ${REND_COMMON_DIR}/state.h
        ${REND_COMMON_DIR}/state_clip.c
        ${REND_COMMON_DIR}/state_cull.c
        ${REND_COMMON_DIR}/state_hidden.c
        ${REND_COMMON_DIR}/state_light.c
        ${REND_COMMON_DIR}/state_matrix.c
        ${REND_COMMON_DIR}/state_output.c
        ${REND_COMMON_DIR}/state_primitive.c
        ${REND_COMMON_DIR}/state_surface.c
        )
source_group("STATE" FILES ${SDL3REND_STATE_FILES})

set(VIDEO_FILES
        sdl3_shaders.c
        video.c
        video.h
        ${REND_COMMON_DIR}/shader_data.h
        )
source_group("VIDEO" FILES ${VIDEO_FILES})

set(RENDERER_FILES
        ${REND_COMMON_DIR}/gv1model.c
        ${REND_COMMON_DIR}/gv1model.h

        gstored.h
        v1model.c

        ${REND_COMMON_DIR}/onscreen.c

        ${REND_COMMON_DIR}/renderer.c
        ${REND_COMMON_DIR}/renderer.h

        sbuffer.h
        sbuffer.c

        ${REND_COMMON_DIR}/sstate.c
        ${REND_COMMON_DIR}/sstate.h
        )
source_group("Renderer" FILES ${RENDERER_FILES})

set(SDL3REND_FILES
        drv_ip.h

        driver.c

        ${REND_COMMON_DIR}/outfcty.c
        ${REND_COMMON_DIR}/outfcty.h

        ${REND_COMMON_DIR}/ext_procs.c

        ${REND_COMMON_DIR}/rendfcty.c
        ${REND_COMMON_DIR}/rendfcty.h

        ${REND_COMMON_DIR}/device.c
        ${REND_COMMON_DIR}/device.h

        ${REND_COMMON_DIR}/devpixmp.c
        devpixmp.h
        devpmgpu.c

        ${REND_COMMON_DIR}/devclut.c
        ${REND_COMMON_DIR}/devclut.h

        ${REND_COMMON_DIR}/template.h
        )

set(INC_FILES
        brsdl3rend.h
        )

add_library(sdl3rend-headers INTERFACE)
target_sources(sdl3rend-headers INTERFACE
        FILE_SET include
        TYPE HEADERS
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES ${INC_FILES}
        )

target_include_directories(sdl3rend-headers INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/brender/sdl3rend>
        )

set_target_properties(sdl3rend-headers PROPERTIES
        EXPORT_NAME "Drivers::SDL3REND::Headers"
        FOLDER "BRender/Drivers"
        )

add_library(sdl3rend
        ${SDL3REND_FILES}
        ${RENDERER_FILES}
        ${VIDEO_FILES}
        ${SDL3REND_STATE_FILES}
        ${INC_FILES}
        ${SPV_GENERATED}
        )

get_target_property(target_type sdl3rend TYPE)
if(target_type STREQUAL SHARED_LIBRARY)
    target_compile_definitions(sdl3rend PRIVATE -DDEFINE_BR_ENTRY_POINT)
endif()

target_compile_definitions(sdl3rend PRIVATE
        BREND_DRIVER_SDL3REND
        BREND_DRIVER_SUFFIX_UPPER=SDL3REND
        BREND_DRIVER_SUFFIX_LOWER=sdl3rend
        )

target_include_directories(sdl3rend PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${REND_COMMON_DIR}
        )

if(SDL3_FOUND)
    target_link_libraries(sdl3rend PRIVATE BRender::DDI SDL3::SDL3)
else()
    target_link_libraries(sdl3rend PRIVATE BRender::DDI)
endif()
target_link_libraries(sdl3rend PUBLIC sdl3rend-headers)

set_target_properties(sdl3rend PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED TRUE
        EXPORT_NAME "Drivers::SDL3REND"
        FOLDER "BRender/Drivers"
        )

if(BRENDER_ASAN_ENABLED)
    set(CMAKE_C_FLAGS
            "${CMAKE_C_FLAGS}  -fsanitize=undefined -fsanitize=address"
    )
    target_compile_options(sdl3rend PUBLIC -fsanitize=address)
    target_link_options(sdl3rend
            BEFORE PUBLIC -fsanitize=undefined PUBLIC -fsanitize=address,undefined
    )
endif()

add_library(BRender::Drivers::SDL3REND::Headers ALIAS sdl3rend-headers)
add_library(BRender::Drivers::SDL3REND ALIAS sdl3rend)
