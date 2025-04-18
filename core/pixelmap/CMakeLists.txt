project(pixelmap)

target_include_directories(brender-inc INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/brender>
        )

target_include_directories(brender-inc-ddi INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ddi>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/brender/ddi>
        )

set(DDI_FILES
        ddi/pm.h
        ddi/pm_ip.h
        )

set(INC_FILES
        )

set(PIXELMAP_FILES
        fontptrs.c
        genclip.c
        gencopy.c
        mipsupt.c
        pixelmap.c
        pmdsptch.c
        pmfile.c
        pmgen.c
        pmmem.c
        pmmem.h
        pmnull.c
        pmfail.c
        pmsetup.c
        quantize.c
        scale.c
        cmemloops.c

        fnt3x5.c
        fntp4x6.c
        fntp7x9.c
        )

add_library(pixelmap OBJECT ${PIXELMAP_FILES})

target_sources(pixelmap INTERFACE
        FILE_SET include
        TYPE HEADERS
        BASE_DIRS include
        FILES ${INC_FILES}
        )

target_sources(pixelmap INTERFACE
        FILE_SET ddi
        TYPE HEADERS
        BASE_DIRS ${CMAKE_CURRENT_BINARY_DIR} ddi
        FILES ${DDI_FILES}
        )

target_include_directories(pixelmap PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(pixelmap PRIVATE __BR_V1DB__=0)
target_link_libraries(pixelmap PRIVATE brender-inc-ddi)
