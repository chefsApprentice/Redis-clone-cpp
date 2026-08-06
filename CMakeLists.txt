cmake_minimum_required(VERSION 3.15)

project(redis_clone
  VERSION 1.0
  LANGUAGES CXX
)

# ---- Language settings ----
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------
# Warnings
# ---------------------
if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -fdiagnostics-color=always
  )
endif()

add_compile_options(
  -g
  -O0
  -fno-omit-frame-pointer
)

# ---------------------
# Static analysis
# ---------------------
set(CMAKE_CXX_CLANG_TIDY clang-tidy)

include(cmake/Sanitizers.cmake)
include(cmake/Warnings.cmake)
include(cmake/StaticAnalysis.cmake)

# ---------------------
# Utils
# ---------------------
file(GLOB_RECURSE UTILS_SOURCES
    CONFIGURE_DEPENDS
    src/utils/*.cpp
)

add_library(utils ${UTILS_SOURCES})

target_include_directories(utils
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
)


# Export compile_commands.json for tools like cppcheck
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable cppcheck if available
find_program(CPPCHECK_EXECUTABLE cppcheck)

if(CPPCHECK_EXECUTABLE)
    message(STATUS "cppcheck found: ${CPPCHECK_EXECUTABLE}")

    set(CMAKE_CXX_CPPCHECK
        ${CPPCHECK_EXECUTABLE}
        --enable=warning,style,performance,portability
        --inconclusive
        --std=c++17
        --suppress=missingIncludeSystem
    )
else()
    message(WARNING "cppcheck not found, disabling")
endif()


# ---- Client ----
add_executable(tcp_client
    src/tcp_client.cpp
)

target_link_libraries(tcp_client
    PRIVATE utils
    project_sanitizers
)

# ---- Server ----
add_executable(tcp_server
    src/tcp_server.cpp
)

target_link_libraries(tcp_server
    PRIVATE utils
    project_sanitizers
)


# ---------------------
# Testing dependencies
# ---------------------

include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# Disable analysis for dependencies
foreach(target gtest gtest_main gmock gmock_main)
    if(TARGET ${target})
        set_target_properties(${target} PROPERTIES
            CXX_CLANG_TIDY ""
            CXX_CPPCHECK ""
        )
    endif()
endforeach()

set(ENV{GTEST_COLOR} 1)


# ---------------------
# Tests
# ---------------------

include(CTest)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()


# ============================
# Install rules
# ============================

include(GNUInstallDirs)

install(TARGETS tcp_client tcp_server
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS utils
    EXPORT utilsTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY src/utils/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/utils
    FILES_MATCHING PATTERN "*.h"
)

install(EXPORT utilsTargets
    FILE utilsTargets.cmake
    NAMESPACE utils::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/utils
)
