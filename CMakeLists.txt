cmake_minimum_required(VERSION 3.25)
project(jmem C)

set(CMAKE_C_STANDARD 99)
list(APPEND JMEM_HEADER_FILES
        source/include/jmem/ill_alloc.h
        source/include/jmem/lin_alloc.h
        source/include/jmem/jmem.h
        source/include/jmem/shm_ill_alloc.h)
add_library(jmem source/ill_alloc.c source/lin_alloc.c source/include/jmem/jmem.h source/shm_ill_alloc.c)

enable_testing()

add_executable(ill_alloc_full_test source/tests/ill_alloc_test.c source/ill_alloc.c source/include/jmem/ill_alloc.h)
add_test(NAME ill_alloc COMMAND ill_alloc_full_test)

add_executable(lin_alloc_test source/tests/lin_alloc_test.c source/lin_alloc.c source/include/jmem/lin_alloc.h)
add_test(NAME lin_alloc COMMAND lin_alloc_test)

add_executable(shm_ill_alloc_full_test source/tests/shm_ill_alloc_test.c source/shm_ill_alloc.c source/include/jmem/shm_ill_alloc.h)
add_test(NAME shm_ill_alloc COMMAND shm_ill_alloc_full_test)

add_executable(shm_ill_alloc_full_test_thrd source/tests/shm_ill_alloc_test_thrd.c source/shm_ill_alloc.c source/include/jmem/shm_ill_alloc.h)
add_test(NAME shm_ill_alloc_thrd COMMAND shm_ill_alloc_full_test_thrd)

add_executable(shm_ill_alloc_full_test_clone source/tests/shm_ill_alloc_test_clone.c source/shm_ill_alloc.c source/include/jmem/shm_ill_alloc.h)
add_test(NAME shm_ill_alloc_clone COMMAND shm_ill_alloc_full_test_clone)

