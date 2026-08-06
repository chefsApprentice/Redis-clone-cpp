option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

add_library(project_sanitizers INTERFACE)

if(ENABLE_ASAN)
    target_compile_options(project_sanitizers INTERFACE
        -fsanitize=address
        -fno-omit-frame-pointer
    )

    target_link_options(project_sanitizers INTERFACE
        -fsanitize=address
    )
endif()

if(ENABLE_UBSAN)
    target_compile_options(project_sanitizers INTERFACE
        -fsanitize=undefined
    )

    target_link_options(project_sanitizers INTERFACE
        -fsanitize=undefined
    )
endif()
