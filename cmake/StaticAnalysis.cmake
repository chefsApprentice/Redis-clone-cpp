find_program(CLANG_TIDY_EXE clang-tidy)

if(ENABLE_CLANG_TIDY AND CLANG_TIDY_EXE)

set(CMAKE_CXX_CLANG_TIDY
    ${CLANG_TIDY_EXE}
)

endif()

find_program(CPPCHECK_EXE cppcheck)

if(ENABLE_CPPCHECK AND CPPCHECK_EXE)

set(CMAKE_CXX_CPPCHECK
    ${CPPCHECK_EXE}
    --enable=all
    --inconclusive
    --std=c++20
)

endif()
