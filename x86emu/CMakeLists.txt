project(x86emu)


set(INC_FILES
        include/x86emu.h
        )

set(FILES
        x86emu.c
        )

add_library(x86emu OBJECT
        ${FILES}
        )

target_sources(x86emu INTERFACE
        FILE_SET include
        TYPE HEADERS
        BASE_DIRS include
        FILES ${INC_FILES}
        )

target_include_directories(x86emu PRIVATE .)
