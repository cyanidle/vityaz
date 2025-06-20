# ACTHUNG !!!
# For development-only
cmake_minimum_required(VERSION 3.16)
project(vityaz C)

set(CMAKE_C_STANDARD 99)

add_executable(vityaz
    src/main.c
    src/lex.c
    src/parse.c
    src/graph.c
    src/util.c
)

if (MSVC)
    target_sources(vityaz PRIVATE src/os-win.c)
    target_compile_options(vityaz PRIVATE /W3)
else()
    target_sources(vityaz PRIVATE src/os-posix.c)
    target_compile_options(vityaz PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
endif()

target_include_directories(vityaz PRIVATE include tapki)
