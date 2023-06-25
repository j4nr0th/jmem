//
// Created by jan on 20.4.2023.
//
#include "../../include/jmem/lin_jalloc.h"
#include <time.h>
#include <string.h>
#include <malloc.h>
#include <assert.h>

typedef uint32_t u32;
typedef uint64_t u64;
typedef uint8_t u8;

int main()
{
    linear_jallocator* allocator = lin_jallocator_create(1 << 20);

    clock_t total_base = 0, total_static = 0, total_comparison = 0;
    clock_t begin_base, begin_comparison, begin_static, end_base, end_comparison, end_static;

    for (u32 k = 0; k < 10; ++k)
    {
        const u64 base_array_size = 1 << k;
        {
            u8 constant_array[1 << 20];
            begin_static = clock();
            for (u32 i = k; i < 20; ++i)
            {
                const u64 size = 1 << (i);
                for (u32 j = 0; j < size / base_array_size; ++j)
                {
                    explicit_bzero(constant_array, base_array_size);
                }
            }
            end_static = clock();
        }

        begin_base = clock();
        for (u32 i = k; i < 20; ++i)
        {
            const u64 size = 1 << i;
            for (u32 j = 0; j < size / base_array_size; ++j)
            {
                void* allocated_array = malloc(base_array_size);
                assert(allocated_array);
                explicit_bzero(allocated_array, base_array_size);
                free(allocated_array);
            }
        }
        end_base = clock();

        begin_comparison = clock();
        for (u32 i = k; i < 20; ++i)
        {
            const u64 size = 1 << i;
            for (u32 j = 0; j < size / base_array_size; ++j)
            {
                void* allocated_array = lin_jalloc(allocator, base_array_size);
                assert(allocated_array);
                explicit_bzero(allocated_array, base_array_size);
                lin_jfree(allocator, allocated_array);
            }
        }
        end_comparison = clock();

        clock_t dt_static = end_static - begin_static;
        clock_t dt_malloc = end_base - begin_base;
        clock_t dt_lin = end_comparison - begin_comparison;

        total_static += dt_static;
        total_base += dt_malloc;
        total_comparison += dt_lin;
    }



//    assert(total_base >= total_comparison);
    printf("%lu %lu %lu\n", total_static, total_base, total_comparison);
    printf("malloc time %lu clock ticks\nlin_alloc time %lu clock ticks\n", total_base - total_static, total_comparison - total_static);

    total_static = 0;
    total_comparison = 0;
    total_base = 0;
    for (u32 i = 0; i < 10; ++i)
    {
        void* ptr_array[1 << 10] = {0};
        const u64 base_size = (1 << (i));
        {
            begin_static = clock();
            u8 constant_array[1 << 20];
            for (u32 j = 0; j < 1024; ++j)
            {
                ptr_array[j] = constant_array + j * base_size;
            }
            for (u32 j = 0; j < 1024; ++j)
            {
                explicit_bzero(ptr_array[j], base_size);
            }
            for (u32 j = 0; j < 1024; ++j)
            {
                ptr_array[j] = NULL;
            }
            end_static = clock();
        }

        begin_base = clock();
        for (u32 j = 0; j < 1024; ++j)
        {
            ptr_array[j] = malloc(base_size);
            assert(ptr_array[j]);
        }
        for (u32 j = 0; j < 1024; ++j)
        {
            explicit_bzero(ptr_array[j], base_size);
        }
        for (u32 j = 0; j < 1024; ++j)
        {
            free(ptr_array[j]);
            ptr_array[j] = NULL;
        }
        end_base = clock();


        begin_comparison = clock();
        for (u32 j = 0; j < 1024; ++j)
        {
            ptr_array[j] = lin_jalloc(allocator, base_size);
            assert(ptr_array[j]);
        }
        for (u32 j = 0; j < 1024; ++j)
        {
            explicit_bzero(ptr_array[j], base_size);
        }
        for (u32 j = 0; j < 1024; ++j)
        {
            lin_jfree(allocator, ptr_array[1024 - 1 - j]);
            ptr_array[j] = NULL;
        }
        end_comparison = clock();

        clock_t dt_static = end_static - begin_static;
        clock_t dt_malloc = end_base - begin_base;
        clock_t dt_lin = end_comparison - begin_comparison;

        total_static += dt_static;
        total_base += dt_malloc;
        total_comparison += dt_lin;
    }


//    assert(total_base >= total_comparison);
    printf("%lu %lu %lu\n", total_static, total_base, total_comparison);
    printf("malloc time %lu clock ticks\nlin_alloc time %lu clock ticks\n", total_base - total_static, total_comparison - total_static);

    lin_jallocator_destroy(allocator);
    return 0;
}
