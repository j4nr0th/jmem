cmake_minimum_required(VERSION 3.25)
project(jmem C)

set(CMAKE_C_STANDARD 99)
list(APPEND JMEM_HEADER_FILES include/jmem/jalloc.h include/jmem/lin_jalloc.h include/jmem/jmem.h)
add_library(jmem SHARED source/jalloc.c source/lin_jalloc.c include/jmem/jmem.h)
add_compile_definitions(JALLOC_TRACKING)
add_compile_definitions(JALLOC_TRAP_COUNT=32)

enable_testing()

add_executable(jalloc_full_test source/tests/jalloc_test.c source/jalloc.c include/jmem/jalloc.h)
add_test(NAME jalloc_test COMMAND jalloc_full_test)

add_executable(lin_alloc_test source/tests/lin_alloc_test.c source/lin_jalloc.c include/jmem/lin_jalloc.h)
add_test(NAME lin_alloc COMMAND lin_alloc_test)

target_sources(jmem PUBLIC FILE_SET jmem_headers TYPE HEADERS BASE_DIRS include/jmem FILES ${JMEM_HEADER_FILES})
install(TARGETS jmem
        FILE_SET jmem_headers
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/jmem")
