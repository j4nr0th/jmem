//
// Created by jan on 20.4.2023.
//

#include <errno.h>
#include "include/jmem/lin_alloc.h"
#include <string.h>
#include <assert.h>
#ifndef _WIN32
#include <unistd.h>
#include <sys/mman.h>
#else
#include <Windows.h>
#endif

static const char* const LIN_ALLOC_NAME_STRING = "linear lin_allocator";

typedef struct lin_allocator_struct lin_allocator;
struct lin_allocator_struct
{
    void* max;
    void* base;
    void* current;
    void* peek;
    unsigned char memory[];
};

void lin_allocator_destroy(lin_allocator* allocator)
{
    lin_allocator* this = (lin_allocator*)allocator;
//    const uint_fast64_t ret_v = this->peek - this->base;
#ifndef _WIN32
    munmap(this, sizeof(*this) + this->max - this->base);
#else
    BOOL res = VirtualFree(this, 0, MEM_RELEASE);
    assert(res != 0);
#endif
//    return ret_v;
}

void* lin_alloc(lin_allocator* allocator, uint_fast64_t size)
{
    if (size & 7)
    {
        size += (8 - (size & 7));
    }
    lin_allocator* this = (lin_allocator*)allocator;
    //  Get current allocator position and find new bottom
    void* ret = this->current;
    void* new_bottom = (void*)((uintptr_t)ret + size);
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

void lin_jfree(lin_allocator* allocator, void* ptr)
{
    if (!ptr) return;
    lin_allocator* this = (lin_allocator*)allocator;
    if (this->base <= ptr && this->max > ptr)
    {
        //  ptr is from the allocator
        if (this->current > ptr)
        {
#ifndef NDEBUG
        memset(ptr, 0xCC, (uintptr_t)this->current - (uintptr_t)ptr);
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

void* lin_jrealloc(lin_allocator* allocator, void* ptr, uint_fast64_t new_size)
{
    if (new_size & 7)
    {
        new_size += (8 - (new_size & 7));
    }
    if (!ptr) return lin_alloc(allocator, new_size);
    lin_allocator* this = (lin_allocator*)allocator;
    //  Is the ptr from this allocator
    if (this->base <= ptr && this->max > ptr)
    {
        //  Check for overflow
        void* new_bottom = (void*)(new_size + (uintptr_t)ptr);
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
          memset(this->current, 0xCC, (uintptr_t)new_bottom - (uintptr_t)this->current);
        }
        else if (new_bottom < this->current)
        {
            memset(new_bottom, 0xCC, (uintptr_t)this->current - (uintptr_t)new_bottom);
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

static uint64_t PAGE_SIZE = 0;

static inline uint_fast64_t round_to_nearest_page_up(uint_fast64_t v)
{
    uint_fast64_t excess = v & (PAGE_SIZE - 1); //  Works BC PAGE_SIZE is a multiple of two
    if (excess)
    {
        v += PAGE_SIZE - excess;
    }
    return v;
}

void* lin_allocator_save_state(lin_allocator* allocator)
{
    const lin_allocator* const this = (lin_allocator*)allocator;
    return this->current;
}

void lin_allocator_restore_current(lin_allocator* allocator, void* ptr)
{
    lin_allocator* const this = (lin_allocator*)allocator;
    assert(ptr >= this->base && ptr < this->max);
    this->current = ptr;
}

static uint_fast64_t lin_allocator_get_size(const lin_allocator* lin_allocator)
{
    return (uint_fast64_t)lin_allocator->max - (uint_fast64_t)lin_allocator->base;
}


lin_allocator* lin_allocator_create(uint_fast64_t total_size)
{
    if (!PAGE_SIZE)
    {
#ifndef _WIN32
        PAGE_SIZE = sysconf(_SC_PAGESIZE);
#else
        SYSTEM_INFO sys_info;
        GetSystemInfo(&sys_info);
        PAGE_SIZE = sys_info.dwPageSize;
#endif
    }
    total_size = round_to_nearest_page_up(total_size);
#ifndef _WIN32
    lin_allocator* this = mmap(NULL, round_to_nearest_page_up(sizeof(lin_allocator) + total_size), PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
    if (this == MAP_FAILED) return NULL;
#else
    lin_allocator* this = VirtualAlloc(0, round_to_nearest_page_up(sizeof(lin_allocator) + total_size), MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    if (this == NULL)
    {
        return NULL;
    }
#endif
    this->base = (void*)((uintptr_t)this + sizeof(*this));
    this->current = (void*)((uintptr_t)this + sizeof(*this));
    this->peek = (void*)((uintptr_t)this + sizeof(*this));
    this->max = (void*)((uintptr_t)this + sizeof(*this) + total_size);

    return this;
}
