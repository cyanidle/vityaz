# ACTHUNG !!!
# For development-only. Main distribution is from amalgam file
cmake_minimum_required(VERSION 3.16)
project(vityaz C)

set(CMAKE_C_STANDARD 99)

add_executable(vityaz
    src/main.c
    src/lex.c
    src/parse.c
)
target_include_directories(vityaz PRIVATE include tapki)
