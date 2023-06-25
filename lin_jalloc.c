//
// Created by jan on 20.4.2023.
//

#include <errno.h>
#include "lin_jalloc.h"
#include <string.h>
#include <malloc.h>
#include <assert.h>
#include <sys/unistd.h>
#ifndef _WIN32
#include <sys/mman.h>
#else
#include <windows.h>
#endif


struct linear_jallocator_struct
{
    void* max;
    void* base;
    void* current;
    void* peek;
};

uint_fast64_t lin_jallocator_destroy(linear_jallocator* allocator)
{
    linear_jallocator* this = (linear_jallocator*)allocator;
#ifndef _WIN32
    int res = munmap(this->base, this->max - this->base);
    assert(res == 0);
#else
    WINBOOL res = VirtualFree(this->base, 0, MEM_RELEASE);
    assert(res != 0);
#endif
    const uint_fast64_t ret_v = this->peek - this->base;
    free(this);
    return ret_v;
}

void* lin_jalloc(linear_jallocator* allocator, uint_fast64_t size)
{
    if (size & 7)
    {
        size += (8 - (size & 7));
    }
    linear_jallocator* this = (linear_jallocator*)allocator;
    //  Get current allocator position and find new bottom
    void* ret = this->current;
    void* new_bottom = ret + size;
    //  Check if it overflows
    if (new_bottom > this->max)
    {
//        return malloc(size);
        return NULL;
    }
    this->current = new_bottom;
#ifndef NDEBUG
    memset(ret, 0xCC, size);
#endif
    if (this->current > this->peek)
    {
        this->peek = this->current;
    }
    return ret;
}

void lin_jfree(linear_jallocator* allocator, void* ptr)
{
    if (!ptr) return;
    linear_jallocator* this = (linear_jallocator*)allocator;
    if (this->base <= ptr && this->max > ptr)
    {
        //  ptr is from the allocator
        if (this->current > ptr)
        {
#ifndef NDEBUG
        memset(ptr, 0xCC, this->current - ptr);
#endif
            this->current = ptr;
        }
        else
        {
            assert(0);
        }
    }
    else
    {
        assert(0);
    }
}

void* lin_jrealloc(linear_jallocator* allocator, void* ptr, uint_fast64_t new_size)
{
    if (new_size & 7)
    {
        new_size += (8 - (new_size & 7));
    }
    if (!ptr) return lin_jalloc(allocator, new_size);
    linear_jallocator* this = allocator;
    //  Is the ptr from this allocator
    if (this->base <= ptr && this->max > ptr)
    {
        //  Check for overflow
        void* new_bottom = new_size + ptr;
        if (new_bottom > this->max)
        {
//            //  Overflow would happen, so use malloc
//            void* new_ptr = malloc(new_size);
//            if (!new_ptr) return NULL;
//            memcpy(new_ptr, ptr, this->current - ptr);
//            //  Free memory (reduce the ptr)
//            assert(ptr < this->current);
//            this->current = ptr;
//            return new_ptr;
            return NULL;
        }
        //  return ptr if no overflow, but update bottom of stack position
#ifndef NDEBUG
        if (new_bottom > this->current)
        {
          memset(this->current, 0xCC, new_bottom - this->current);
        }
        else if (new_bottom < this->current)
        {
            memset(new_bottom, 0xCC, this->current - new_bottom);
        }
#endif
        this->current = new_bottom;

        if (this->current > this->peek)
        {
            this->peek = this->current;
        }
        return ptr;
    }
    //  ptr was not from this allocator, so assume it was malloced and just pass it on to realloc
//    return realloc(ptr, new_size);
    return NULL;
}

linear_jallocator* lin_jallocator_create(uint_fast64_t total_size)
{
#ifndef _WIN32
    uint64_t PAGE_SIZE = sysconf(_SC_PAGESIZE);
#else
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    uint64_t PAGE_SIZE = sys_info.dwPageSize;
#endif
    linear_jallocator* this = malloc(sizeof(*this));
    if (!this) return this;
    uint64_t extra = (total_size % PAGE_SIZE);
    if (extra)
    {
        total_size += (PAGE_SIZE - extra);
    }
#ifndef _WIN32
    void* mem = mmap(NULL, total_size, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
    if (mem == MAP_FAILED)
#else
    void* mem = VirtualAlloc(NULL, total_size, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
    if (mem == NULL)
#endif
    {
        free(this);
        return NULL;
    }
    this->base = mem;
    this->current = mem;
    this->peek = mem;
    this->max = mem + total_size;

    return this;
}

void* lin_jalloc_get_current(linear_jallocator* allocator)
{
    return allocator->current;
}

void lin_jalloc_set_current(linear_jallocator* allocator, void* ptr)
{
    assert(ptr >= allocator->base && ptr < allocator->max);
    allocator->current = ptr;
}

uint_fast64_t lin_jallocator_get_size(const linear_jallocator* jallocator)
{
    return jallocator->max - jallocator->base;
}
