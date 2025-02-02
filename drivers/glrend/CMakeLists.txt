project(glrend)

find_package(OpenGL REQUIRED COMPONENTS OpenGL)

# Embed shaders
add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/default.vert.glsl.h"
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}" "-DFILE=default.vert.glsl" -P "${CMAKE_SOURCE_DIR}/cmake/EmbedResource.cmake"
        DEPENDS default.vert.glsl
)
add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/default.frag.glsl.h"
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}" "-DFILE=default.frag.glsl" -P "${CMAKE_SOURCE_DIR}/cmake/EmbedResource.cmake"
        DEPENDS default.frag.glsl
)
add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/brender.vert.glsl.h"
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}" "-DFILE=brender.vert.glsl" -P "${CMAKE_SOURCE_DIR}/cmake/EmbedResource.cmake"
        DEPENDS brender.vert.glsl
)
add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/brender.frag.glsl.h"
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}" "-DFILE=brender.frag.glsl" -P "${CMAKE_SOURCE_DIR}/cmake/EmbedResource.cmake"
        DEPENDS brender.frag.glsl
)


set(GLSTATE_FILES
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
source_group("STATE" FILES ${GLSTATE_FILES})

set(VIDEO_FILES
        checkboard64.c
        shader_brender.c
        shader_default.c
        # shader_text.c
        video.c
        video.h
        )
source_group("VIDEO" FILES ${VIDEO_FILES})

set(RENDERER_FILES
        gv1buckt.c
        gv1buckt.h

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

set(GLREND_FILES
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
        devpmglf.c

        devclut.c
        devclut.h

        font.c
        font.h

        template.h

        quad.c
        util.c

        glad.c
        )

set(INC_FILES
        include/brglrend.h
        include/brglrend_p.h

        include/glad/glad.h
        include/KHR/khrplatform.h
        )

add_library(glrend-headers INTERFACE)
target_sources(glrend-headers INTERFACE
        FILE_SET include
        TYPE HEADERS
        BASE_DIRS include
        FILES ${INC_FILES}
        )

target_include_directories(glrend-headers INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include/brender/glrend>
        )

set_target_properties(glrend-headers PROPERTIES
        EXPORT_NAME "Drivers::GLRend::Headers"
        FOLDER "BRender/Drivers"
        )

add_library(glrend
        ${GLREND_FILES}
        ${RENDERER_FILES}
        ${VIDEO_FILES}
        ${GLSTATE_FILES}
        ${INC_FILES}

        "${CMAKE_CURRENT_BINARY_DIR}/default.vert.glsl.h"
        "${CMAKE_CURRENT_BINARY_DIR}/default.frag.glsl.h"
        "${CMAKE_CURRENT_BINARY_DIR}/brender.vert.glsl.h"
        "${CMAKE_CURRENT_BINARY_DIR}/brender.frag.glsl.h"
        )

get_target_property(target_type glrend TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
    target_compile_definitions(glrend PRIVATE -DDEFINE_BR_ENTRY_POINT)
endif ()

target_include_directories(glrend PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(glrend PRIVATE OpenGL::GL BRender::DDI)
target_link_libraries(glrend PUBLIC glrend-headers)

set_target_properties(glrend PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED TRUE
        EXPORT_NAME "Drivers::GLRend"
        FOLDER "BRender/Drivers"
        )

if(BRENDER_ASAN_ENABLED)
	set(CMAKE_C_FLAGS
		"${CMAKE_C_FLAGS}  -fsanitize=undefined -fsanitize=address"
	)
	target_compile_options(glrend PUBLIC -fsanitize=address)
	target_link_options(glrend
			BEFORE PUBLIC -fsanitize=undefined PUBLIC -fsanitize=address,undefined
	)
endif()

add_library(BRender::Drivers::GLRend::Headers ALIAS glrend-headers)
add_library(BRender::Drivers::GLRend ALIAS glrend)
