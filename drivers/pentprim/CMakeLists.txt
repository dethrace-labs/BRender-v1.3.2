project(pentprim C)

set(INFOGEN ${CMAKE_CURRENT_SOURCE_DIR}/infogen.pl)

set(PRIM_FILES ${CMAKE_CURRENT_SOURCE_DIR}/prim_t8.ifg)

function(add_prim base)
	set(input_file "${CMAKE_CURRENT_SOURCE_DIR}/${base}.ifg")
	set(output_file "${base}.c")

	add_custom_command(
		OUTPUT ${output_file}
		COMMAND ${PERL_EXECUTABLE} ${INFOGEN} < ${input_file} > ${output_file}
		MAIN_DEPENDENCY ${input_file}
		DEPENDS ${INFOGEN}
	)

	set(XXXX ${input_file} ${output_file})
	set(PRIM_FILES ${PRIM_FILES} ${XXXX} PARENT_SCOPE)

endfunction()

function(add_prim_suffixed base with_suffixes)
	set(input_file "${CMAKE_CURRENT_SOURCE_DIR}/${base}.ifg")
	set(output_files "${base}f.c" "${base}x.c")

	set(args_float "")
	set(argx_fixed "")

	if(with_suffixes)
		list(APPEND args_float "image_suffix=f")
		list(APPEND args_fixed "image_suffix=x")
	endif()

	list(APPEND args_float "float_components")

	add_custom_command(
		OUTPUT ${output_files}
		COMMAND ${PERL_EXECUTABLE} ${INFOGEN} ${args_float} < ${input_file} > "${base}f.c"
		COMMAND ${PERL_EXECUTABLE} ${INFOGEN} ${args_fixed} < ${input_file} > "${base}x.c"
		MAIN_DEPENDENCY ${input_file}
		DEPENDS ${PRIM_FILES}
	)

	set(XXXX ${input_file} ${output_files})
	set(PRIM_FILES ${PRIM_FILES} ${XXXX} PARENT_SCOPE)

	#set_property(SOURCE match.c APPEND PROPERTY OBJECT_DEPENDS ${output_files})
endfunction()

# add_prim(prim_l15 "")
# add_prim(prim_l16 "")
# add_prim(prim_l24 "")
add_prim(prim_l8  "")
# add_prim(prim_p15 "")
# add_prim(prim_p16 "")
# add_prim(prim_p24 "")
add_prim(prim_p8  "")
# add_prim(prim_t24 "")

add_prim_suffixed(prim_t8 TRUE)
# add_prim_suffixed(prm_t15 FALSE)
# add_prim_suffixed(prm_t16 FALSE)
# add_prim_suffixed(mmx_t15 FALSE)
# add_prim_suffixed(mmx_t16 FALSE)

set_source_files_properties(${PRIM_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)

add_library(pentprim
        awtm.c
        awtmi.h
        awtmz.c
        device.c
        device.h
        dither.c
        driver.c
        drv.h
        drv_ip.h
        fperspi.h
        fperspzi.h
        frcp.c
        l_pi.c
        l_piz.c
        match.c
        match.h
        mmx_ip.h
        object.c
        object.h
        persp.c
        perspi.h
        perspz.c
        perspzi.h
        plib.c
        plib.h
        p_pi.c
        p_piz.c
        prim_ip.h
        pstate.c
        pstate.h
        rastparm.h
        rcp.c
        sbuffer.c
        sbuffer.h
        template.h
        timestmp.h

		work.c
        work.h
		safediv.c
		sar16.c
		decal.c

		pfpsetup.c
		fpsetup.c

		ti8_pizp.c
		t15_pizp.c
		ti8_pi.c
		ti8_piz.c
		fti8_pip.c
		fti8pizp.c
		zb8awtm.c
		zb8p2unl.c
		zb8p2lit.c
		fti8_piz.c
		zb8.c
		zb8p2ulb.c
		zb8p2ltb.c
		compiler.h

		${PRIM_FILES}
)

target_compile_definitions(pentprim PRIVATE "-DPARTS=0xffff")

get_target_property(target_type pentprim TYPE)
if(NOT target_type STREQUAL SHARED_LIBRARY)
    target_compile_definitions(pentprim PRIVATE -DDEFINE_BR_ENTRY_POINT)
endif()

target_link_libraries(pentprim PRIVATE BRender::DDI x86emu)
target_include_directories(pentprim PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

set_target_properties(pentprim PROPERTIES
		EXPORT_NAME "Drivers::SoftPrim"
		FOLDER "BRender/Drivers"
		OUTPUT_NAME "softprmf"
		)

if(BRENDER_ASAN_ENABLED)
	set(CMAKE_C_FLAGS
		"${CMAKE_C_FLAGS}  -fsanitize=undefined -fsanitize=address"
	)
	target_compile_options(pentprim PUBLIC -fsanitize=address)
	target_link_options(pentprim
			BEFORE PUBLIC -fsanitize=undefined PUBLIC -fsanitize=address,undefined
	)
endif()

if(NOT MSVC)
	target_compile_options(pentprim PRIVATE -fno-strict-aliasing)
endif()

add_library(BRender::Drivers::SoftPrim ALIAS pentprim)
