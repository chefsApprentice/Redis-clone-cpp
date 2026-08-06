add_library(project_warnings INTERFACE)

if(MSVC)

target_compile_options(project_warnings INTERFACE
    /W4
)

else()

target_compile_options(project_warnings INTERFACE
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wconversion
    -Wsign-conversion
    -Wnull-dereference
    -Wdouble-promotion
    -Wformat=2
)

endif()
