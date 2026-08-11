project(glrend C)

# Embed shared shaders (drivers/commonrend)
set(SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../commonrend")

add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/default.vert.glsl.h"
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${SHADER_DIR}" "-DFILE=default.vert.glsl" -P "${BRender_SOURCE_DIR}/cmake/EmbedResource.cmake"
        DEPENDS "${SHADER_DIR}/default.vert.glsl"
)
add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/default.frag.glsl.h"
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${SHADER_DIR}" "-DFILE=default.frag.glsl" -P "${BRender_SOURCE_DIR}/cmake/EmbedResource.cmake"
        DEPENDS "${SHADER_DIR}/default.frag.glsl"
)
add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/brender.vert.glsl.h"
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${SHADER_DIR}" "-DFILE=brender.vert.glsl" -P "${BRender_SOURCE_DIR}/cmake/EmbedResource.cmake"
        DEPENDS "${SHADER_DIR}/brender.vert.glsl"
)
add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/brender.frag.glsl.h"
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${SHADER_DIR}" "-DFILE=brender.frag.glsl" -P "${BRender_SOURCE_DIR}/cmake/EmbedResource.cmake"
        DEPENDS "${SHADER_DIR}/brender.frag.glsl"
)


set(REND_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../commonrend")

set(GLSTATE_FILES
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
source_group("STATE" FILES ${GLSTATE_FILES})

set(VIDEO_FILES
        checkboard64.c
        shader_brender.c
        shader_default.c
        # shader_text.c
        video.c
        video.h
        ${REND_COMMON_DIR}/shader_data.h
        )
source_group("VIDEO" FILES ${VIDEO_FILES})

set(RENDERER_FILES
        gv1buckt.c
        gv1buckt.h

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

set(GLREND_FILES
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
        devpmglf.c

        ${REND_COMMON_DIR}/devclut.c
        ${REND_COMMON_DIR}/devclut.h

        ${REND_COMMON_DIR}/template.h

        quad.c
        util.c

        glassert.c
        glassert.h

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
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/brender/glrend>
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
if(target_type STREQUAL SHARED_LIBRARY)
    target_compile_definitions(glrend PRIVATE -DDEFINE_BR_ENTRY_POINT)
endif()

target_compile_definitions(glrend PRIVATE
        BREND_DRIVER_GL
        BREND_DRIVER_SUFFIX_UPPER=GL
        BREND_DRIVER_SUFFIX_LOWER=gl
        )

target_include_directories(glrend PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${REND_COMMON_DIR}
        )

target_link_libraries(glrend PRIVATE BRender::DDI)
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
