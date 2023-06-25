cmake_minimum_required(VERSION 3.25)
project(jmem C)

set(CMAKE_C_STANDARD 99)
list(APPEND JMEM_HEADER_FILES source/include/jmem/jalloc.h source/include/jmem/lin_jalloc.h source/include/jmem/jmem.h)
add_library(jmem SHARED source/jalloc.c source/lin_jalloc.c source/include/jmem/jmem.h)
add_compile_definitions(JALLOC_TRACKING)
add_compile_definitions(JALLOC_TRAP_COUNT=32)

enable_testing()

add_executable(jalloc_full_test source/tests/jalloc_test.c source/jalloc.c source/include/jmem/jalloc.h)
add_test(NAME jalloc_test COMMAND jalloc_full_test)

add_executable(lin_alloc_test source/tests/lin_alloc_test.c source/lin_jalloc.c source/include/jmem/lin_jalloc.h)
add_test(NAME lin_alloc COMMAND lin_alloc_test)

target_sources(jmem PUBLIC FILE_SET jmem_headers TYPE HEADERS BASE_DIRS source/include/jmem FILES ${JMEM_HEADER_FILES})
install(TARGETS jmem
        FILE_SET jmem_headers
            DESTINATION "/usr/include/jmem"
        LIBRARY
            DESTINATION "/usr/lib")

