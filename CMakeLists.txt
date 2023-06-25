cmake_minimum_required(VERSION 3.25)
project(jmem C)

set(CMAKE_C_STANDARD 99)

add_library(jmem SHARED jalloc.c lin_jalloc.c)
add_compile_definitions(JALLOC_TRACKING)
add_compile_definitions(JALLOC_TRAP_COUNT=32)