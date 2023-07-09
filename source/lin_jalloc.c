//
// Created by jan on 20.4.2023.
//

#include <errno.h>
#include "include/jmem/lin_jalloc.h"
#include <string.h>
#include <assert.h>
#include <unistd.h>
#ifndef _WIN32
#include <sys/mman.h>
#else
#include <windows.h>
#endif

static const char* const LIN_JALLOC_NAME_STRING = "linear jallocator";

typedef struct linear_jallocator_struct linear_jallocator;
struct linear_jallocator_struct
{
    jallocator interface;
    void* max;
    void* base;
    void* current;
    void* peek;
    unsigned char memory[];
};

void lin_jallocator_destroy(jallocator* allocator)
{
    if (allocator->type != LIN_JALLOC_NAME_STRING)
    {
        return;
    }
    linear_jallocator* this = (linear_jallocator*)allocator;
    const uint_fast64_t ret_v = this->peek - this->base;
#ifndef _WIN32
    munmap(this, sizeof(*this) + this->max - this->base);
#else
    WINBOOL res = VirtualFree(this, 0, MEM_RELEASE);
    assert(res != 0);
#endif
//    return ret_v;
}

void* lin_jalloc(jallocator* allocator, uint_fast64_t size)
{
    if (allocator->type != LIN_JALLOC_NAME_STRING)
    {
        //  Mismatch between function and allocator
        return NULL;
    }
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

void lin_jfree(jallocator* allocator, void* ptr)
{
    if (allocator->type != LIN_JALLOC_NAME_STRING)
    {
        //  Mismatch between function and allocator
        return;
    }
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

void* lin_jrealloc(jallocator* allocator, void* ptr, uint_fast64_t new_size)
{
    if (allocator->type != LIN_JALLOC_NAME_STRING)
    {
        //  Mismatch between function and allocator
        return NULL;
    }
    if (new_size & 7)
    {
        new_size += (8 - (new_size & 7));
    }
    if (!ptr) return lin_jalloc(allocator, new_size);
    linear_jallocator* this = (linear_jallocator*)allocator;
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

void* lin_jallocator_save_state(jallocator* allocator)
{
    if (allocator->type != LIN_JALLOC_NAME_STRING)
    {
        //  Mismatch between function and allocator
        return NULL;
    }
    const linear_jallocator* const this = (linear_jallocator*)allocator;
    return this->current;
}

void lin_jallocator_restore_current(jallocator* allocator, void* ptr)
{
    if (allocator->type != LIN_JALLOC_NAME_STRING)
    {
        //  Mismatch between function and allocator
        return;
    }
    linear_jallocator* const this = (linear_jallocator*)allocator;
    assert(ptr >= this->base && ptr < this->max);
    this->current = ptr;
}

static uint_fast64_t lin_jallocator_get_size(const linear_jallocator* jallocator)
{
    return jallocator->max - jallocator->base;
}


jallocator* lin_jallocator_create(uint_fast64_t total_size)
{
    if (!PAGE_SIZE)
    {
#ifndef _WIN32
        PAGE_SIZE = sysconf(_SC_PAGESIZE);
#else
        SYSTEM_INFO sys_info;
        GetSystemInfo(&sys_info);
        uint64_t PAGE_SIZE = sys_info.dwPageSize;
#endif
    }
    total_size = round_to_nearest_page_up(total_size);
    linear_jallocator* this = mmap(NULL, round_to_nearest_page_up(sizeof(linear_jallocator) + total_size), PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
    if (this == MAP_FAILED) return NULL;
    this->base = (void*)((uintptr_t)this + sizeof(*this));
    this->current = (void*)((uintptr_t)this + sizeof(*this));
    this->peek = (void*)((uintptr_t)this + sizeof(*this));
    this->max = (void*)((uintptr_t)this + sizeof(*this) + total_size);

    this->interface.type = LIN_JALLOC_NAME_STRING;

    return &this->interface;
}
