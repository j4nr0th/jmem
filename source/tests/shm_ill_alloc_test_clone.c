//
// Created by jan on 23.4.2023.
//
#include "../include/jmem/shm_ill_alloc.h"
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <linux/sched.h>
#include <sched.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <stdio.h>
#include <errno.h>

typedef uint32_t u32;

static void* test_fn(void* param)
{
    shm_ill_allocator* const allocator = param;
    void* pointer_array[1024] = {0};
    for (u32 i = 0; i < 1023; ++i)
    {
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        pointer_array[i] = shm_ill_alloc(allocator, 16);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        memset(pointer_array[i], 0xCC, 16);
    }
    pointer_array[1023] = shm_ill_alloc(allocator, 6);

    {
        void* tmp = pointer_array[20];
        pointer_array[20] = pointer_array[21];
        pointer_array[21] = tmp;
    }

    for (u32 i = 0; i < 128; ++i)
    {
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        void* ptr = shm_ill_jrealloc(allocator, pointer_array[i + 128], 64 + i);
        assert(ptr);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        pointer_array[i + 128] = ptr;
    }

    for (u32 i = 0; i < 512; ++i)
    {
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        shm_ill_jfree(allocator, pointer_array[2 * i]);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        pointer_array[2 * i] = NULL;
    }

    void* new_ptr_array[512] = { 0 };
    for (u32 i = 0; i < 512; ++i)
    {
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        void* ptr = shm_ill_jrealloc(allocator, pointer_array[2 * i + 1], 96);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        assert(ptr);
        memset(ptr, 0xCC, 96);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        new_ptr_array[i] = ptr;
    }

    for (u32 i = 0; i < 88; ++i)
    {
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        void* ptr = shm_ill_jrealloc(allocator, new_ptr_array[i], 96 - i);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        assert(ptr);
        memset(ptr, 0xCC, 96 - i);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        new_ptr_array[i] = ptr;
    }

    for (u32 i = 0; i < 512; ++i)
    {
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        shm_ill_jfree(allocator, new_ptr_array[i]);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
    }
    return 0;
}

int main()
{
    void* pointer_array[1024] = {0};
    shm_ill_allocator* allocator = shm_ill_allocator_create(64, 1);
    assert(allocator);
    for (u32 i = 0; i < 1024; ++i)
    {
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        pointer_array[i] = shm_ill_alloc(allocator, 32);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        memset(pointer_array[i], 0xCC, 32);
    }

    for (u32 i = 0; i < 1024; ++i)
    {
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        shm_ill_jfree(allocator, pointer_array[i]);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
    }

    shm_ill_allocator_destroy(allocator);
    allocator = NULL;

    allocator = shm_ill_allocator_create(64, 1);
    assert(allocator);
    for (u32 i = 0; i < 1024; ++i)
    {
        pointer_array[i] = shm_ill_alloc(allocator, 8);
        memset(pointer_array[i], 0xCC, 8);
    }

    for (u32 i = 0; i < 1024; ++i)
    {
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        shm_ill_jfree(allocator, pointer_array[i]);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
    }

    shm_ill_allocator_destroy(allocator);
    allocator = NULL;

    allocator = shm_ill_allocator_create(64, 1);
    assert(allocator);
    for (u32 i = 0; i < 1024; ++i)
    {
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        pointer_array[i] = shm_ill_alloc(allocator, 16);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        memset(pointer_array[i], 0xCC, 16);
    }

    for (u32 i = 0; i < 1024; ++i)
    {
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        pointer_array[i] = shm_ill_jrealloc(allocator, pointer_array[i], 128);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
    }

    for (u32 i = 0; i < 1024; ++i)
    {
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        shm_ill_jfree(allocator, pointer_array[i]);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
    }

    shm_ill_allocator_destroy(allocator);
    allocator = NULL;

    allocator = shm_ill_allocator_create(1024, 32);
    assert(allocator);
    int* const p_end = shm_ill_alloc(allocator, sizeof(*p_end));
    assert(p_end);
    *p_end = 0;
    pid_t parent_tid;
    pid_t child;
    struct clone_args cl_args =
            {
            //  Allocate a new stack
            .stack = 0,
            .stack_size = 0,
            .parent_tid = (__u64)&parent_tid,
            .child_tid = (__u64)&child,
            //            .set_tid = (__u64) &tid,
//            .set_tid_size = sizeof(tid),
            .flags = CLONE_FILES|CLONE_FS|CLONE_IO|CLONE_SYSVSEM|CLONE_PARENT_SETTID|CLONE_CHILD_SETTID,
            };
    long child_tid = 0;
    child_tid = syscall(
            SYS_clone3,
            &cl_args,
            sizeof(cl_args));
    if (child_tid == -1 || errno)
    {
        perror("Could not clone!");
    }
    assert(child_tid >= 0);
    printf("Thread is %d - %d, %d, %ld\nAllocator address: %p\n", gettid(), parent_tid, child, child_tid, allocator);
    if (child_tid == 0)
    {
        printf("Child running\n");
        test_fn(allocator);
        printf("Child closed\n");
        *p_end = 1;
        //  Child
        return 0;
    }
    else
    {
        printf("Parent running\n");
        while (*p_end == 0) sched_yield();
        (void) child_tid;
    }
    shm_ill_allocator_destroy(allocator);
    allocator = NULL;

    allocator = shm_ill_allocator_create(1 << 20, 1);
    assert(allocator);

    memset(pointer_array, 0, sizeof(pointer_array));
    assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
    pointer_array[0] = shm_ill_alloc(allocator, 128);
    assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
    pointer_array[1] = shm_ill_alloc(allocator, 128);
    assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
    pointer_array[2] = shm_ill_alloc(allocator, 128);
    assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
    shm_ill_jfree(allocator, pointer_array[1]);
    assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
    pointer_array[3] = shm_ill_jrealloc(allocator, pointer_array[0], 200);
    assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
    assert(pointer_array[3]);
    pointer_array[0] = pointer_array[3];
    pointer_array[3] = NULL;
    assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
    shm_ill_jfree(allocator, pointer_array[0]);
    assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
    shm_ill_jfree(allocator, pointer_array[2]);
    assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);


    {
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        void* p1 = shm_ill_alloc(allocator, 32);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        void* p2 = shm_ill_alloc(allocator, 32);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        void* p3 = shm_ill_alloc(allocator, 32);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        shm_ill_jfree(allocator, p2);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        p1 = shm_ill_jrealloc(allocator, p1, 48);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        assert(p1);
        shm_ill_jfree(allocator, p1);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        shm_ill_jfree(allocator, p3);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
    }

    {
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        void* p1 = shm_ill_alloc(allocator, 128);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        memset(p1, 0xCC, 128);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        void* ptr = shm_ill_jrealloc(allocator, p1, 32);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        assert(ptr);
        memset(ptr, 0xCC, 32);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        ptr = shm_ill_jrealloc(allocator, ptr, 1);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        assert(ptr);
        shm_ill_jfree(allocator, ptr);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
    }


    shm_ill_allocator_destroy(allocator);
    allocator = NULL;

    allocator = shm_ill_allocator_create(1 << 10, 1);
    assert(allocator);
    assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);

    {
        void* mem1,* mem2, *mem3;
        mem1 = shm_ill_alloc(allocator, (1 << 9) - 8);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        mem2 = shm_ill_alloc(allocator, (1 << 9) - 16);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        mem3 = shm_ill_alloc(allocator, (1 << 10) - 16);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);

        shm_ill_jfree(allocator, mem3);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        shm_ill_jfree(allocator, mem2);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        shm_ill_jfree(allocator, mem1);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);

        mem1 = shm_ill_alloc(allocator, (1 << 10) - 16);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);

        mem1 = shm_ill_jrealloc(allocator, mem1, (1 << 10) - 64);
        assert(mem1);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);


        shm_ill_jfree(allocator, mem1);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
    }

    {
        void* mem1, *mem2, *mem3, *mem4;
        mem1 = shm_ill_alloc(allocator, 128);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        mem2 = shm_ill_alloc(allocator, 64);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        mem3 = shm_ill_alloc(allocator, 128);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        mem4 = shm_ill_alloc(allocator, 128);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);



        shm_ill_jfree(allocator, mem2);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        mem2 = shm_ill_jrealloc(allocator, NULL, 16);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        shm_ill_jfree(allocator, mem2);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        shm_ill_jfree(allocator, mem4);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        shm_ill_jfree(allocator, mem3);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
        shm_ill_jfree(allocator, mem1);
        assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
    }

    assert(shm_ill_allocator_verify(allocator, NULL, NULL) == 0);
    shm_ill_allocator_destroy(allocator);
    allocator = NULL;

    return 0;
}