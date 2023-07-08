//
// Created by jan on 23.4.2023.
//
#include "../include/jmem/ill_jalloc.h"
#include <unistd.h>
#include <assert.h>
#include <string.h>

typedef uint32_t u32;

int main()
{
    void* pointer_array[1024] = {};
    jallocator* allocator = ill_jallocator_create(64, 1);
    assert(allocator);
    for (u32 i = 0; i < 1024; ++i)
    {
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        pointer_array[i] = ill_jalloc(allocator, 32);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        memset(pointer_array[i], 0xCC, 32);
    }

    for (u32 i = 0; i < 1024; ++i)
    {
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        ill_jfree(allocator, pointer_array[i]);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
    }

    ill_jallocator_destroy(allocator);
    allocator = NULL;

    allocator = ill_jallocator_create(64, 1);
    assert(allocator);
    for (u32 i = 0; i < 1024; ++i)
    {
        pointer_array[i] = ill_jalloc(allocator, 8);
        memset(pointer_array[i], 0xCC, 8);
    }

    for (u32 i = 0; i < 1024; ++i)
    {
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        ill_jfree(allocator, pointer_array[i]);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
    }

    ill_jallocator_destroy(allocator);
    allocator = NULL;

    allocator = ill_jallocator_create(64, 1);
    assert(allocator);
    for (u32 i = 0; i < 1024; ++i)
    {
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        pointer_array[i] = ill_jalloc(allocator, 16);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        memset(pointer_array[i], 0xCC, 16);
    }

    for (u32 i = 0; i < 1024; ++i)
    {
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        pointer_array[i] = ill_jrealloc(allocator, pointer_array[i], 128);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
    }

    for (u32 i = 0; i < 1024; ++i)
    {
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        ill_jfree(allocator, pointer_array[i]);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
    }

    ill_jallocator_destroy(allocator);
    allocator = NULL;

    allocator = ill_jallocator_create(1024, 32);
    assert(allocator);
    for (u32 i = 0; i < 1023; ++i)
    {
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        pointer_array[i] = ill_jalloc(allocator, 16);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        memset(pointer_array[i], 0xCC, 16);
    }
    pointer_array[1023] = ill_jalloc(allocator, 6);

    {
        void* tmp = pointer_array[20];
        pointer_array[20] = pointer_array[21];
        pointer_array[21] = tmp;
    }

    for (u32 i = 0; i < 128; ++i)
    {
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        void* ptr = ill_jrealloc(allocator, pointer_array[i + 128], 64 + i);
        assert(ptr);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        pointer_array[i + 128] = ptr;
    }

    for (u32 i = 0; i < 512; ++i)
    {
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        ill_jfree(allocator, pointer_array[2 * i]);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        pointer_array[2 * i] = NULL;
    }

    void* new_ptr_array[512] = { 0 };
    for (u32 i = 0; i < 512; ++i)
    {
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        void* ptr = ill_jrealloc(allocator, pointer_array[2 * i + 1], 96); //   add breakpoint at i == 64
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        assert(ptr);
        memset(ptr, 0xCC, 96);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        new_ptr_array[i] = ptr;
    }

    for (u32 i = 0; i < 88; ++i)
    {
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        void* ptr = ill_jrealloc(allocator, new_ptr_array[i], 96 - i);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        assert(ptr);
        memset(ptr, 0xCC, 96 - i);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        new_ptr_array[i] = ptr;
    }

    for (u32 i = 0; i < 512; ++i)
    {
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        ill_jfree(allocator, new_ptr_array[i]);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
    }

    ill_jallocator_destroy(allocator);
    allocator = NULL;

    allocator = ill_jallocator_create(1 << 20, 1);
    assert(allocator);

    memset(pointer_array, 0, sizeof(pointer_array));
    assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
    pointer_array[0] = ill_jalloc(allocator, 128);
    assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
    pointer_array[1] = ill_jalloc(allocator, 128);
    assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
    pointer_array[2] = ill_jalloc(allocator, 128);
    assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
    ill_jfree(allocator, pointer_array[1]);
    assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
    pointer_array[3] = ill_jrealloc(allocator, pointer_array[0], 200);
    assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
    assert(pointer_array[3]);
    pointer_array[0] = pointer_array[3];
    pointer_array[3] = NULL;
    assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
    ill_jfree(allocator, pointer_array[0]);
    assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
    ill_jfree(allocator, pointer_array[2]);
    assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);


    {
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        void* p1 = ill_jalloc(allocator, 32);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        void* p2 = ill_jalloc(allocator, 32);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        void* p3 = ill_jalloc(allocator, 32);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        ill_jfree(allocator, p2);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        p1 = ill_jrealloc(allocator, p1, 48);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        assert(p1);
        ill_jfree(allocator, p1);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        ill_jfree(allocator, p3);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
    }

    {
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        void* p1 = ill_jalloc(allocator, 128);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        memset(p1, 0xCC, 128);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        void* ptr = ill_jrealloc(allocator, p1, 32);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        assert(ptr);
        memset(ptr, 0xCC, 32);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        ptr = ill_jrealloc(allocator, ptr, 1);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        assert(ptr);
        ill_jfree(allocator, ptr);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
    }


    ill_jallocator_destroy(allocator);
    allocator = NULL;

    allocator = ill_jallocator_create(1 << 10, 1);
    assert(allocator);
    assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);

    {
        void* mem1,* mem2, *mem3;
        mem1 = ill_jalloc(allocator, (1 << 9) - 8);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        mem2 = ill_jalloc(allocator, (1 << 9) - 16);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        mem3 = ill_jalloc(allocator, (1 << 10) - 16);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);

        ill_jfree(allocator, mem3);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        ill_jfree(allocator, mem2);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        ill_jfree(allocator, mem1);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);

        mem1 = ill_jalloc(allocator, (1 << 10) - 16);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);

        mem1 = ill_jrealloc(allocator, mem1, (1 << 10) - 64);
        assert(mem1);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);


        ill_jfree(allocator, mem1);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
    }

    {
        void* mem1, *mem2, *mem3, *mem4;
        mem1 = ill_jalloc(allocator, 128);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        mem2 = ill_jalloc(allocator, 64);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        mem3 = ill_jalloc(allocator, 128);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        mem4 = ill_jalloc(allocator, 128);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);



        ill_jfree(allocator, mem2);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        mem2 = ill_jrealloc(allocator, NULL, 16);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        ill_jfree(allocator, mem2);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        ill_jfree(allocator, mem4);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        ill_jfree(allocator, mem3);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
        ill_jfree(allocator, mem1);
        assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
    }

    assert(ill_jallocator_verify(allocator, NULL, NULL) == 0);
    ill_jallocator_destroy(allocator);
    allocator = NULL;

    return 0;
}