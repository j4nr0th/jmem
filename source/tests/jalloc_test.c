//
// Created by jan on 23.4.2023.
//
#include "../../include/jmem/jalloc.h"
#include <unistd.h>
#include <assert.h>
#include <string.h>

typedef uint32_t u32;

int main()
{
    void* pointer_array[1024] = {};
    jallocator* allocator = jallocator_create(64, 1);
    assert(allocator);
    for (u32 i = 0; i < 1024; ++i)
    {
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        pointer_array[i] = jalloc(allocator, 32);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        memset(pointer_array[i], 0xCC, 32);
    }

    for (u32 i = 0; i < 1024; ++i)
    {
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        jfree(allocator, pointer_array[i]);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
    }

    jallocator_destroy(allocator);
    allocator = NULL;

    allocator = jallocator_create(64, 1);
    assert(allocator);
    for (u32 i = 0; i < 1024; ++i)
    {
        pointer_array[i] = jalloc(allocator, 8);
        memset(pointer_array[i], 0xCC, 8);
    }

    for (u32 i = 0; i < 1024; ++i)
    {
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        jfree(allocator, pointer_array[i]);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
    }

    jallocator_destroy(allocator);
    allocator = NULL;

    allocator = jallocator_create(64, 1);
    assert(allocator);
    for (u32 i = 0; i < 1024; ++i)
    {
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        pointer_array[i] = jalloc(allocator, 16);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        memset(pointer_array[i], 0xCC, 16);
    }

    for (u32 i = 0; i < 1024; ++i)
    {
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        pointer_array[i] = jrealloc(allocator, pointer_array[i], 128);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
    }

    for (u32 i = 0; i < 1024; ++i)
    {
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        jfree(allocator, pointer_array[i]);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
    }

    jallocator_destroy(allocator);
    allocator = NULL;

    allocator = jallocator_create(1024, 32);
    assert(allocator);
    for (u32 i = 0; i < 1023; ++i)
    {
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        pointer_array[i] = jalloc(allocator, 16);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        memset(pointer_array[i], 0xCC, 16);
    }
    pointer_array[1023] = jalloc(allocator, 6);

    {
        void* tmp = pointer_array[20];
        pointer_array[20] = pointer_array[21];
        pointer_array[21] = tmp;
    }

    for (u32 i = 0; i < 128; ++i)
    {
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        void* ptr = jrealloc(allocator, pointer_array[i + 128], 64 + i);
        assert(ptr);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        pointer_array[i + 128] = ptr;
    }

    for (u32 i = 0; i < 512; ++i)
    {
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        jfree(allocator, pointer_array[2 * i]);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        pointer_array[2 * i] = NULL;
    }

    void* new_ptr_array[512] = { 0 };
    for (u32 i = 0; i < 512; ++i)
    {
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        void* ptr = jrealloc(allocator, pointer_array[2 * i + 1], 96); //   add breakpoint at i == 64
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        assert(ptr);
        memset(ptr, 0xCC, 96);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        new_ptr_array[i] = ptr;
    }

    for (u32 i = 0; i < 88; ++i)
    {
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        void* ptr = jrealloc(allocator, new_ptr_array[i], 96 - i);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        assert(ptr);
        memset(ptr, 0xCC, 96 - i);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        new_ptr_array[i] = ptr;
    }

    for (u32 i = 0; i < 512; ++i)
    {
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        jfree(allocator, new_ptr_array[i]);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
    }

    jallocator_destroy(allocator);
    allocator = NULL;

    allocator = jallocator_create(1 << 20, 1);
    assert(allocator);

    memset(pointer_array, 0, sizeof(pointer_array));
    assert(jallocator_verify(allocator, NULL, NULL) == 0);
    pointer_array[0] = jalloc(allocator, 128);
    assert(jallocator_verify(allocator, NULL, NULL) == 0);
    pointer_array[1] = jalloc(allocator, 128);
    assert(jallocator_verify(allocator, NULL, NULL) == 0);
    pointer_array[2] = jalloc(allocator, 128);
    assert(jallocator_verify(allocator, NULL, NULL) == 0);
    jfree(allocator, pointer_array[1]);
    assert(jallocator_verify(allocator, NULL, NULL) == 0);
    pointer_array[3] = jrealloc(allocator, pointer_array[0], 200);
    assert(jallocator_verify(allocator, NULL, NULL) == 0);
    assert(pointer_array[3]);
    pointer_array[0] = pointer_array[3];
    pointer_array[3] = NULL;
    assert(jallocator_verify(allocator, NULL, NULL) == 0);
    jfree(allocator, pointer_array[0]);
    assert(jallocator_verify(allocator, NULL, NULL) == 0);
    jfree(allocator, pointer_array[2]);
    assert(jallocator_verify(allocator, NULL, NULL) == 0);


    {
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        void* p1 = jalloc(allocator, 32);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        void* p2 = jalloc(allocator, 32);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        void* p3 = jalloc(allocator, 32);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        jfree(allocator, p2);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        p1 = jrealloc(allocator, p1, 48);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        assert(p1);
        jfree(allocator, p1);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        jfree(allocator, p3);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
    }

    {
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        void* p1 = jalloc(allocator, 128);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        memset(p1, 0xCC, 128);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        void* ptr = jrealloc(allocator, p1, 32);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        assert(ptr);
        memset(ptr, 0xCC, 32);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        ptr = jrealloc(allocator, ptr, 1);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        assert(ptr);
        jfree(allocator, ptr);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
    }


    jallocator_destroy(allocator);
    allocator = NULL;

    allocator = jallocator_create(1 << 10, 1);
    assert(allocator);
    assert(jallocator_verify(allocator, NULL, NULL) == 0);

    {
        void* mem1,* mem2, *mem3;
        mem1 = jalloc(allocator, (1 << 9) - 8);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        mem2 = jalloc(allocator, (1 << 9) - 16);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        mem3 = jalloc(allocator, (1 << 10) - 16);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);

        jfree(allocator, mem3);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        jfree(allocator, mem2);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        jfree(allocator, mem1);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);

        mem1 = jalloc(allocator, (1 << 10) - 16);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);

        mem1 = jrealloc(allocator, mem1, (1 << 10) - 64);
        assert(mem1);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);


        jfree(allocator, mem1);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
    }

    {
        void* mem1, *mem2, *mem3, *mem4;
        mem1 = jalloc(allocator, 128);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        mem2 = jalloc(allocator, 64);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        mem3 = jalloc(allocator, 128);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        mem4 = jalloc(allocator, 128);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);



        jfree(allocator, mem2);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        mem2 = jrealloc(allocator, NULL, 16);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        jfree(allocator, mem2);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        jfree(allocator, mem4);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        jfree(allocator, mem3);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
        jfree(allocator, mem1);
        assert(jallocator_verify(allocator, NULL, NULL) == 0);
    }

    assert(jallocator_verify(allocator, NULL, NULL) == 0);
    jallocator_destroy(allocator);
    allocator = NULL;

    return 0;
}