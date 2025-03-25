##
# Add all core targets to the "Core" export, and
# configure their "include" and "ddi" filesets to be copied.
##

include(GNUInstallDirs)

install(TARGETS
    brender brender-ddi brender-inc brender-inc-ddi
    inc fmt fw host math nulldev pixelmap std v1db
    EXPORT BRenderTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    FILE_SET include DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/brender
    FILE_SET ddi DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/brender/ddi
)

if (BRENDER_BUILD_DRIVERS)
    install(TARGETS
        brender-full
        EXPORT BRenderTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )

    if (TARGET virtual_fb)
        install(TARGETS virtual_fb
            EXPORT BRenderTargets
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        )
    endif()

    if (TARGET glrend)
        install(TARGETS glrend-headers
            EXPORT BRenderTargets
            FILE_SET include DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/brender/glrend
        )

        install(TARGETS glrend
            EXPORT BRenderTargets
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        )
    endif()

    if (TARGET sdl2dev)
        install(TARGETS sdl2dev-headers
            EXPORT BRenderTargets
                FILE_SET include DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/brender/sdl2dev
            )

        install(TARGETS sdl2dev
            EXPORT BRenderTargets
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        )
    endif()

    if (TARGET softrend)
        install(TARGETS softrend
            EXPORT BRenderTargets
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        )
    endif()

    if (TARGET pentprim)
        install(TARGETS pentprim
            EXPORT BRenderTargets
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        )
    endif()

    if((TARGET softrend) OR (TARGET pentprim))
        install(TARGETS x86emu
            EXPORT BRenderTargets
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            FILE_SET include DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/brender
        )
    endif()

endif ()

install(EXPORT BRenderTargets
    NAMESPACE BRender::
    DESTINATION lib/cmake/BRender-1.3.2
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/BRenderConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/BRenderConfig.cmake"
        INSTALL_DESTINATION lib/cmake/BRender-1.3.2
        NO_SET_AND_CHECK_MACRO
        NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/BRenderConfigVersion.cmake"
        VERSION "1.3.2"
        COMPATIBILITY AnyNewerVersion
)

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/BRenderConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/BRenderConfigVersion.cmake
        DESTINATION lib/cmake/BRender-1.3.2
        )

# pkg-config
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/brender.pc.in ${CMAKE_CURRENT_BINARY_DIR}/brender-1.3.2.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/brender-1.3.2.pc DESTINATION lib/pkgconfig)
