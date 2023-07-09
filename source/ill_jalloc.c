//
// Created by jan on 27.4.2023.
//

#include "include/jmem/ill_jalloc.h"
#ifndef _WIN32
#include <sys/mman.h>
#include <unistd.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>

#else
#include <windows.h>
#endif
typedef struct mem_chunk_struct mem_chunk;
struct mem_chunk_struct
{
#ifdef JALLOC_TRACKING
    uint_fast64_t size:48;
    uint_fast64_t idx:13;
#else
    uint_fast64_t size:63;
#endif
    uint_fast64_t used:1;
    mem_chunk* next;
    mem_chunk* prev;
};
#if __STDC_VERSION__ == 201112L
static_assert(offsetof(mem_chunk, next) == 8);
#endif
typedef struct mem_pool_struct mem_pool;
struct mem_pool_struct
{
    uint_fast64_t size;
    uint_fast64_t free;
    uint_fast64_t used;
    mem_chunk* largest;
    mem_chunk* smallest;
    void* base;
};

/**
 * @brief Opaque structure to store the state of allocator. Not thread safe.
 */
typedef struct ill_jallocator_struct ill_jallocator;

struct ill_jallocator_struct
{
    jallocator interface;

    uint_fast64_t pool_size;
    uint_fast64_t capacity;
    uint_fast64_t count;
#ifdef JALLOC_TRACKING
    uint_fast64_t allocator_index;
    uint_fast64_t total_allocated;
    uint_fast64_t biggest_allocation;
    uint_fast64_t max_allocated;
    uint_fast64_t current_allocated;
#endif
#ifdef JALLOC_TRAP_COUNT
    uint32_t trap_counts;
    uint32_t trap_values[JALLOC_TRAP_COUNT];
    void(* trap_callbacks[JALLOC_TRAP_COUNT])(uint32_t idx, void* param);
    void* trap_params[JALLOC_TRAP_COUNT];
#endif
    mem_pool* pools;
    uint_fast64_t pool_buffer_size;
};

static uint_fast64_t PAGE_SIZE = 0;

static const char* const ILL_JALLOCATOR_TYPE_STRING = "Implicit linked list jallocator";

static inline uint_fast64_t round_to_nearest_page_up(uint_fast64_t v)
{
    uint_fast64_t excess = v & (PAGE_SIZE - 1); //  Works BC PAGE_SIZE is a multiple of two
    if (excess)
    {
        v += PAGE_SIZE - excess;
    }
    return v;
}


void ill_jallocator_destroy(jallocator* allocator)
{
    if (allocator->type != ILL_JALLOCATOR_TYPE_STRING)
    {
        //  Mismatched types
        return;
    }
    ill_jallocator* this = (ill_jallocator*)allocator;
    for (uint_fast32_t i = 0; i < this->count; ++i)
    {
#ifndef _WIN32
        munmap(this->pools[i].base, this->pool_size);
#else
        WINBOOL res = VirtualFree(allocator->pools[i].base, 0, MEM_RELEASE);
        assert(res != 0);
#endif
    }
    for (uint_fast64_t i = 0; i < this->count; ++i)
    {
        this->pools[i] = (mem_pool){};
    }
    munmap(this->pools, this->pool_buffer_size);
    *this = (ill_jallocator){};
    munmap(this, round_to_nearest_page_up(sizeof(*this)));
}

static inline uint_fast64_t round_up_size(uint_fast64_t size)
{
    uint_fast64_t remainder = 8 - (size & 7);
    size += remainder;
    size += offsetof(mem_chunk, next);
    if (size < sizeof(mem_chunk))
    {
        return sizeof(mem_chunk);
    }
    return size;
}

static inline mem_pool* find_supporting_pool(ill_jallocator* allocator, uint_fast64_t size)
{
    for (uint_fast32_t i = 0; i < allocator->count; ++i)
    {
        mem_pool* pool = allocator->pools + i;
        if (pool->largest && size <= pool->largest->size)
        {
            return pool;
        }
    }
    return NULL;
}

static inline mem_chunk* find_ge_chunk_from_largest(mem_pool* pool, uint_fast64_t size)
{
    mem_chunk* current;
    for (current = pool->largest; current; current = current->prev)
    {
        if (current->size >= size)
        {
            return current;
        }
    }
    return NULL;
}

static inline mem_chunk* find_ge_chunk_from_smallest(mem_pool* pool, uint_fast64_t size)
{
    mem_chunk* current;
    for (current = pool->smallest; current; current = current->next)
    {
        if (current->size >= size)
        {
            return current;
        }
    }
    return NULL;
}

static inline void remove_chunk_from_pool(mem_pool* pool, mem_chunk* chunk)
{
    if (chunk->next)
    {
        assert(chunk->next->prev == chunk);
        (chunk->next)->prev = chunk->prev;
    }
    else
    {
//        assert(pool->largest == chunk);
        pool->largest = chunk->prev;
    }
    if (chunk->prev)
    {
        assert(chunk->prev->next == chunk);
        (chunk->prev)->next = chunk->next;
    }
    else
    {
        assert(pool->smallest == chunk);
        pool->smallest = chunk->next;
    }
    pool->free -= chunk->size;
    pool->used += chunk->size;
}

static inline void insert_chunk_into_pool(mem_pool* pool, mem_chunk* chunk)
{
beginning_of_fn:
    assert(chunk->used == 0);
    //  Check if pool has other chunks
    if (pool->free == 0)
    {
        assert(!pool->smallest && !pool->largest);
        pool->smallest = chunk;
        pool->largest = chunk;
        chunk->next = NULL;
        chunk->prev = NULL;
    }
    else
    {
        mem_chunk* ge_chunk = NULL;
        for (mem_chunk* current = pool->smallest; current; current = current->next)
        {
            //  Check if the current directly follows chunk
            if (((void*)chunk) + chunk->size == (void*)current)
            {
                //  Pull the current from the pool
                remove_chunk_from_pool(pool, current);
                //  Merge chunk with current
                chunk->size += current->size;
                goto beginning_of_fn;
            }

            //  Check if the current directly precedes chunk
            if (((void*)current) + current->size == (void*)chunk)
            {
                //  Pull the current from the pool
                remove_chunk_from_pool(pool, current);
                //  Merge chunk with current
                current->size += chunk->size;
                chunk = current;
                goto beginning_of_fn;
            }

            //  Check if current->size is greater than or equal chunk->size
            if (!ge_chunk && current->size >= chunk->size)
            {
                ge_chunk = current;
            }
        }

        //  Find first larger or equally sized chunk
//        mem_chunk* ge_chunk = find_ge_chunk_from_smallest(pool, chunk->size);
        if (!ge_chunk)
        {
            //  No others are larger or of equal size, so this is the new largest
            assert(pool->largest->size < chunk->size);
            pool->largest->next = chunk;
            chunk->prev = pool->largest;
            chunk->next = NULL;
            pool->largest = chunk;
        }
        else
        {
            //  Chunk belongs after the ge_chunk in the list
            chunk->prev = ge_chunk->prev;
            chunk->next = ge_chunk;
            assert(!chunk->prev || chunk->prev->size <= chunk->size);
            assert(!chunk->next || chunk->next->size >= chunk->size);
            //  Check if ge_chunk was smallest in pool
            if (ge_chunk->prev)
            {
                (ge_chunk->prev)->next = chunk;
            }
            else
            {
                assert(ge_chunk == pool->smallest);
                pool->smallest = chunk;
            }
            ge_chunk->prev = chunk;
        }
    }
    pool->free += chunk->size;
    pool->used -= chunk->size;
}

static inline mem_pool* find_chunk_pool(ill_jallocator* allocator, void* ptr)
{
    for (uint_fast32_t i = 0; i < allocator->count; ++i)
    {
        mem_pool* pool = allocator->pools + i;
        if (pool->base <= ptr && pool->base + (pool->used + pool->free) > ptr)
        {
            return pool;
        }
    }
    return NULL;
}

void* ill_jalloc(jallocator* allocator, uint_fast64_t size)
{
    if (allocator->type != ILL_JALLOCATOR_TYPE_STRING)
    {
        //  Mismatched types
        return NULL;
    }
    ill_jallocator* this = (ill_jallocator*)allocator;
    const uint_fast64_t original_size = size;
    //  Round up size to 8 bytes
    size = round_up_size(size);

    //  Check there's a pool that can support the allocation
    mem_pool* pool = find_supporting_pool(this, size);
    if (!pool)
    {
        //  Create a new pool
        if (this->count == this->capacity)
        {
            uint_fast64_t new_memory_size = this->pool_buffer_size + PAGE_SIZE;
            mem_pool* new_ptr = mremap(this->pools, this->pool_buffer_size, new_memory_size, MREMAP_MAYMOVE);
            if (new_ptr == MAP_FAILED)
            {
                new_ptr = mmap(NULL, new_memory_size, PROT_WRITE|PROT_READ, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
                if (new_ptr == MAP_FAILED)
                {
                    return NULL;
                }
                for (uint_fast64_t i = 0; i < this->count; ++i)
                {
                    new_ptr[i] = this->pools[i];
                }
                munmap(this->pools, this->pool_buffer_size);
            }
            memset((void*)((uintptr_t)new_ptr + this->pool_buffer_size), 0, PAGE_SIZE);
            this->pool_buffer_size = new_memory_size;
            this->pools = new_ptr;
        }

        const uint_fast64_t pool_size = round_to_nearest_page_up(this->pool_size > size + sizeof(mem_chunk) ? this->pool_size : size + sizeof(mem_chunk));
#ifndef _WIN32
        mem_chunk* base_chunk = mmap(NULL, pool_size, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
        if (base_chunk == MAP_FAILED)
#else
        mem_chunk* base_chunk = VirtualAlloc(NULL, pool_size, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
        if (base_chunk == NULL)
#endif
        {
            if (allocator->bad_alloc_callback)
            {
                allocator->bad_alloc_callback(allocator, allocator->bad_alloc_param);
            }
            return NULL;
        }
        base_chunk->size = pool_size;
        base_chunk->used = 0;
        base_chunk->next = NULL;
        base_chunk->prev = NULL;
        mem_pool new_pool =
                {
                .base = base_chunk,
                .largest = base_chunk,
                .smallest = base_chunk,
                .used = 0,
                .free = pool_size,
                .size = pool_size,
                };
        pool = this->pools + this->count;
        this->pools[this->count++] = new_pool;
    }

    //  Find the smallest block which fits
    mem_chunk* chunk = find_ge_chunk_from_largest(pool, size);
    assert(chunk);
    remove_chunk_from_pool(pool, chunk);

    //  Check if chunk can be split
    uint_fast64_t remaining = chunk->size - size;
    if (remaining >= sizeof(mem_chunk))
    {
        mem_chunk* new_chunk = ((void*)chunk) + size;
        new_chunk->used = 0;
        new_chunk->size = remaining;
        chunk->size = size;
        insert_chunk_into_pool(pool, new_chunk);
    }

    chunk->used = 1;
#ifdef JALLOC_TRACKING
    chunk->idx = ++this->allocator_index;
#ifdef JALLOC_TRAP_COUNT
    for (uint32_t i = 0; i < this->trap_counts; ++i)
    {
        if (chunk->idx == this->trap_values[i])
        {
            void (* callback)(uint32_t idx, void* param) = this->trap_callbacks[i];
            void* param = this->trap_params[i];
            for (uint32_t j = i; j < this->trap_counts - 1; ++j)
            {
                this->trap_values[j] = this->trap_values[j + 1];
            }
            this->trap_counts -= 1;
            assert(callback);
            callback(this->allocator_index, param);
            break;
        }
    }
#endif
    this->total_allocated += chunk->size;
    if (this->max_allocated < chunk->size)
    {
        this->max_allocated = chunk->size;
    }
    this->current_allocated += chunk->size;
    if (this->current_allocated > this->max_allocated)
    {
        this->max_allocated = this->current_allocated;
    }
#endif
    return &chunk->next;
}

void ill_jfree(jallocator* allocator, void* ptr)
{
    if (allocator->type != ILL_JALLOCATOR_TYPE_STRING)
    {
        //  Mismatched types
        return;
    }
    ill_jallocator* this = (ill_jallocator*)allocator;
    //  Check for null
    if (!ptr) return;
    //  Check what pool this is from
    mem_pool* pool = find_chunk_pool(this, ptr);
    mem_chunk* chunk = ptr - offsetof(mem_chunk, next);
#ifdef JALLOC_TRACKING
    this->current_allocated -= chunk->size;
#endif
    if (!pool)
    {
        return;
    }

    if (chunk->used == 0)
    {
        //  Double free
        if (allocator->double_free_callback)
        {
            allocator->double_free_callback(allocator, allocator->double_free_param);
        }
        return;
    }

    //  Mark chunk as no longer used, then return it back to the pool
    chunk->used = 0;
    insert_chunk_into_pool(pool, chunk);
}

void* ill_jrealloc(jallocator* allocator, void* ptr, uint_fast64_t new_size)
{
    if (allocator->type != ILL_JALLOCATOR_TYPE_STRING)
    {
        //  Mismatched types
        return NULL;
    }
    ill_jallocator* this = (ill_jallocator*)allocator;
    if (!ptr)
    {
        return ill_jalloc(allocator, new_size);
    }
    const uint_fast64_t original_size = new_size;
    new_size = round_up_size(new_size);



    mem_chunk* chunk = ptr - offsetof(mem_chunk, next);
    //  Try and find pool it came from
    mem_pool* pool = find_chunk_pool(this, ptr);
    //  Check if it came from a pool
    if (!pool)
    {
        //  It did not come from a pool
        if (allocator->bad_alloc_callback)
        {
            allocator->bad_alloc_callback(allocator, allocator->bad_alloc_param);
        }
        return NULL;
    }

    //  Since this dereferences chunk, this can cause SIGSEGV
    if (new_size == chunk->size)
    {
        return ptr;
    }


    const uint_fast64_t old_size = chunk->size;
    //  Check if increasing or decreasing the chunk's size
    if (new_size > chunk->size)
    {
        //  Increasing
        //  Check if current block can be expanded so that there's no moving it
        //  Location of potential candidate
        mem_chunk* possible_chunk = ((void*)chunk) + chunk->size;
        if (!((void*)possible_chunk >= pool->base                              //  Is the pointer in range?
            && (void*)possible_chunk < pool->base + pool->free + pool->used //  Is the pointer in range?
            && possible_chunk->used == 0                                         //  Is the other chunk in use
            && possible_chunk->size + chunk->size >= new_size))               //  Is the other chunk large enough to accommodate us
        {
            //  Can not make use of any adjacent chunks, so allocate a new block, copy memory to it, free current block, then return the new block
            void* new_ptr = ill_jalloc(allocator, new_size - offsetof(mem_chunk, next));
            memcpy(new_ptr, ptr, chunk->size - offsetof(mem_chunk, next));
            ill_jfree(allocator, ptr);
            return new_ptr;
        }

        //  All requirements met
        //  Pull the chunk from the pool
        remove_chunk_from_pool(pool, possible_chunk);
        //  Join the two chunks together
        chunk->size += possible_chunk->size;
        //  Redo size check
        goto size_check;
    }
    else
    {
    size_check:
        assert(chunk->size >= new_size);
        //  Decreasing
        //  Check if block can be split in two
        const uint_fast64_t remainder = chunk->size - new_size;
        if (remainder < sizeof(mem_chunk))
        {
            //  Can not be split, return the original pointer
            return ptr;
        }
        //  Split the chunk
        mem_chunk* new_chunk = ((void*)chunk) + new_size;
        chunk->size = new_size;
        new_chunk->size = remainder;
        new_chunk->used = 0;
        //  Put the split chunk into the pool
        insert_chunk_into_pool(pool, new_chunk);
    }


#ifdef JALLOC_TRACKING
    this->current_allocated -= old_size;
    this->current_allocated += new_size;
    this->total_allocated += new_size > old_size ? new_size - old_size : 0;
    if (this->current_allocated > this->max_allocated)
    {
        this->max_allocated = this->current_allocated;
    }
    if (new_size > this->biggest_allocation)
    {
        this->biggest_allocation = new_size;
    }
#endif
    return &chunk->next;
}

int ill_jallocator_verify(jallocator* allocator, int_fast32_t* i_pool, int_fast32_t* i_block)
{
    if (allocator->type != ILL_JALLOCATOR_TYPE_STRING)
    {
        //  Mismatched types
        return -2;
    }
    ill_jallocator* this = (ill_jallocator*)allocator;
#ifndef NDEBUG
#define VERIFICATION_CHECK(x) assert(x)
#else
#define VERIFICATION_CHECK(x) if (!(x)) { if (i_pool) *i_pool = i; if (i_block) *i_block = j; return -1;} (void)0
#endif

    for (int_fast32_t i = 0, j = 0; i < this->count; ++i, j = -1)
    {
        const mem_pool* pool = this->pools + i;
        uint_fast32_t accounted_free_space = 0, accounted_used_space = 0;
        //  Loop forward to verify forward links and free space
        j = 0;
        for (const mem_chunk* current = pool->smallest; current; current = current->next, ++j)
        {
            VERIFICATION_CHECK(current->prev || current == pool->smallest);
            VERIFICATION_CHECK(!current->prev || current->size >= current->prev->size);
            VERIFICATION_CHECK(!current->next || current->next->prev == current);
            VERIFICATION_CHECK(current->used == 0);
            VERIFICATION_CHECK(current->size >= sizeof(mem_chunk));
            accounted_free_space += current->size;
        }
        VERIFICATION_CHECK(accounted_free_space == pool->free);

        accounted_free_space = 0;
        //  Loop forward to verify backwards links and free space
        j = 0;
        for (const mem_chunk* current = pool->largest; current; current = current->prev, ++j)
        {
            VERIFICATION_CHECK(current->next || current == pool->largest);
            VERIFICATION_CHECK(!current->next || current->size <= current->next->size);
            VERIFICATION_CHECK(!current->prev || current->prev->next == current);
            VERIFICATION_CHECK(current->used == 0);
            VERIFICATION_CHECK(current->size >= sizeof(mem_chunk));
            accounted_free_space += current->size;
        }
        VERIFICATION_CHECK(accounted_free_space == pool->free);

        accounted_free_space = 0;
        j = 0;
        //  Do a full walk through the whole block
        for (void* current = pool->base; current < pool->base + this->pool_size; current += ((mem_chunk*)current)->size, j -= 1)
        {
            mem_chunk* chunk = current;
            VERIFICATION_CHECK(chunk->size >= sizeof(mem_chunk));
            VERIFICATION_CHECK(current < pool->base + this->pool_size);
            if (chunk->used)
            {
                accounted_used_space += chunk->size;
            }
            else
            {
                accounted_free_space += chunk->size;
            }
            if (chunk->used == 0)
            {
                if (chunk->next)
                {
                    VERIFICATION_CHECK(chunk->next->prev == chunk && chunk->size <= chunk->next->size);
                }
                if (chunk->prev)
                {
                    VERIFICATION_CHECK(chunk->prev->next == chunk && chunk->size >= chunk->prev->size);
                }
            }
        }
    }
    return 0;
}


uint_fast32_t ill_jallocator_count_used_blocks(jallocator* allocator, uint_fast32_t size_out_buffer, uint_fast32_t* out_buffer)
{
    if (allocator->type != ILL_JALLOCATOR_TYPE_STRING)
    {
        //  Mismatched types
        return -1;
    }
    ill_jallocator* this = (ill_jallocator*)allocator;
#ifndef JALLOC_TRACKING
    return 0;
#else
    uint_fast32_t found = 0;
    for (uint_fast64_t i = 0; i < this->count; ++i)
    {
        const mem_pool* pool = this->pools + i;
        const void* pos = pool->base;
        while (pos != pool->base + pool->used + pool->free)
        {
            if (pos > pool->base + pool->used + pool->free)
            {
                return -1;
            }
            const mem_chunk* chunk = pos;
            if (chunk->used)
            {
                if (found < size_out_buffer)
                {
                    out_buffer[found] = chunk->idx;
                }
                found += 1;
            }
            pos += chunk->size;
        }
    }


    return found;
#endif
}

void ill_jallocator_statistics(
        jallocator* allocator, uint_fast64_t* p_max_allocation_size, uint_fast64_t* p_total_allocated,
        uint_fast64_t* p_max_usage, uint_fast64_t* p_allocation_count)
{
    if (allocator->type != ILL_JALLOCATOR_TYPE_STRING)
    {
        //  Mismatched types
        return;
    }
    ill_jallocator* this = (ill_jallocator*)allocator;
#ifdef JALLOC_TRACKING
    *p_max_allocation_size = this->biggest_allocation;
    *p_max_usage = this->max_allocated;
    *p_total_allocated = this->total_allocated;
    *p_allocation_count = this->allocator_index;
#endif
}

int ill_jallocator_set_debug_trap(jallocator* allocator, uint32_t index, void(*callback_function)(uint32_t index, void* param), void* param)
{
    if (allocator->type != ILL_JALLOCATOR_TYPE_STRING)
    {
        //  Mismatched types
        return 0;
    }
    ill_jallocator* this = (ill_jallocator*)allocator;
    if (!callback_function) return 0;
#ifdef JALLOC_TRAP_COUNT
    if (this->trap_counts == JALLOC_TRAP_COUNT)
    {
        return 0;
    }

    for (uint32_t i = 0; i < this->trap_counts; ++i)
    {
        if (this->trap_values[i] == index)
        {
            return 1;
        }
    }
    this->trap_values[this->trap_counts] = index;
    this->trap_callbacks[this->trap_counts] = callback_function;
    this->trap_params[this->trap_counts] = param;
    this->trap_counts += 1;
#endif
    return 1;
}

static inline uint_fast64_t check_for_aligned_size(mem_chunk* chunk, uint_fast64_t alignment, uint_fast64_t size)
{
    void* ptr = (void*)&chunk->next;
    uintptr_t extra = (uintptr_t)ptr & (alignment - 1);
    if (!extra)
    {
        return size;
    }
    return size + (alignment - extra);
}

static const jallocator_unordered_interface i_unordered =
        {
            .alloc = ill_jalloc,
            .free = ill_jfree,
            .realloc = ill_jrealloc,
        };

static const jallocator_stack_interface i_stack =
        {
                .alloc = ill_jalloc,
                .free = ill_jfree,
                .realloc = ill_jrealloc,
        };

jallocator* ill_jallocator_create(uint_fast64_t pool_size, uint_fast64_t initial_pool_count)
{
    if (!PAGE_SIZE)
    {
#ifndef _WIN32
        PAGE_SIZE = sysconf(_SC_PAGESIZE);
#else
        SYSTEM_INFO sys_info;
        GetSystemInfo(&sys_info);
        PG_SIZE = (long) sys_info.dwPageSize;
#endif
        //  Check that we have the page size
        if (!PAGE_SIZE)
        {
            return NULL;
        }
    }
    ill_jallocator* this = mmap(NULL, round_to_nearest_page_up(sizeof(*this)), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (this == MAP_FAILED)
    {
        return NULL;
    }
    *this = (ill_jallocator){};
    this->capacity = initial_pool_count < 32 ? 32 : initial_pool_count;
    this->pool_buffer_size = round_to_nearest_page_up(this->capacity * sizeof(*this->pools));
    this->capacity = this->pool_buffer_size / sizeof(*this->pools);
    this->pools = mmap(0, this->pool_buffer_size, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
    if (!this->pools)
    {
        munmap(this, round_to_nearest_page_up(sizeof(*this)));
        return NULL;
    }

    this->pool_size = round_to_nearest_page_up(pool_size);
    for (uint_fast32_t i = 0; i < initial_pool_count; ++i)
    {
        mem_pool* p = this->pools + i;
#ifndef _WIN32
        p->base = mmap(NULL, pool_size, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
        if (p->base == MAP_FAILED)
#else
            p->base = VirtualAlloc(NULL, pool_size, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
        if (p->base == NULL)
#endif
        {
            for (uint_fast32_t j = 0; j < i; ++j)
            {
#ifndef _WIN32
                munmap(this->pools[i].base, pool_size);
#else
                WINBOOL res = VirtualFree(this->pools[i].base, 0, MEM_RELEASE);
                assert(res != 0);
#endif
            }
            munmap(this->pools, this->pool_buffer_size);
            munmap(this, round_to_nearest_page_up(sizeof(*this)));
            return NULL;
        }
        p->size = this->pool_size;
        p->used = 0;
        p->free = this->pool_size;
        mem_chunk* c = p->base;
        p->smallest = c;
        p->largest = c;
        c->used = 0;
        c->prev = NULL;
        c->next = NULL;
        c->size = this->pool_size;
    }
    this->count = initial_pool_count;
#ifdef JALLOC_TRACKING
    this->biggest_allocation = 0;
    this->max_allocated = 0;
    this->total_allocated = 0;
    this->allocator_index = 0;
    this->current_allocated = 0;
#endif

    this->interface.type = ILL_JALLOCATOR_TYPE_STRING;
    this->interface.i_unordered = &i_unordered;
    this->interface.i_stack = &i_stack;
    this->interface.i_restore = NULL;   //  Not supported
    this->interface.destructor = ill_jallocator_destroy;

    return &this->interface;
}
