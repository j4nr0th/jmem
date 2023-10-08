//
// Created by jan on 27.4.2023.
//

#include "include/jmem/shm_ill_alloc.h"
#include <errno.h>
#include <assert.h>
#include <string.h>
#ifndef _WIN32
#include <sys/mman.h>
#include <unistd.h>
#include <stddef.h>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <stdatomic.h>
#else
#include <windows.h>
#endif
typedef struct mem_chunk_struct mem_chunk;
struct mem_chunk_struct
{
#ifdef JMEM_ALLOC_TRACKING
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
typedef struct shm_ill_allocator_struct shm_ill_allocator;

struct shm_ill_allocator_struct
{
#ifndef NDEBUG
    const char* futex_acquired_in;
#endif
    uint32_t futex_waiter_count;
    uint32_t access_futex_value;
    uint_fast64_t pool_size;
    uint_fast64_t capacity;
    uint_fast64_t count;
#ifdef JMEM_ALLOC_TRACKING
    uint_fast64_t allocator_index;
    uint_fast64_t total_allocated;
    uint_fast64_t biggest_allocation;
    uint_fast64_t max_allocated;
    uint_fast64_t current_allocated;
#endif
#ifdef JMEM_ALLOC_TRAP_COUNT
    uint32_t trap_counts;
    uint32_t trap_values[JMEM_ALLOC_TRAP_COUNT];
    void(* trap_callbacks[JMEM_ALLOC_TRAP_COUNT])(uint32_t idx, void* param);
    void* trap_params[JMEM_ALLOC_TRAP_COUNT];
#endif
    mem_pool* pools;
    uint_fast64_t pool_buffer_size;

    void (* bad_alloc_callback)(shm_ill_allocator* allocator, void* param);
    void* bad_alloc_param;

    void (* double_free_callback)(shm_ill_allocator* allocator, void* param);
    void* double_free_param;
};

static uint_fast64_t PAGE_SIZE = 0;

static const char* const ILL_ALLOCATOR_TYPE_STRING = "Shared implicit linked list allocator";

static inline uint_fast64_t round_to_nearest_page_up(uint_fast64_t v)
{
    uint_fast64_t excess = v & (PAGE_SIZE - 1); //  Works BC PAGE_SIZE is a multiple of two
    if (excess)
    {
        v += PAGE_SIZE - excess;
    }
    return v;
}

enum shm_allocator_futex_values
{
    FUTEX_FREE,
    FUTEX_USED,
    FUTEX_DIE,
};

static inline int acquire_allocator_mutex(shm_ill_allocator* const this, const char* fn)
{
    uint32_t fv = atomic_exchange(&this->access_futex_value, FUTEX_USED);
    atomic_fetch_add(&this->futex_waiter_count, 1);
    while (fv != FUTEX_FREE)
    {
        const long res = syscall(
                SYS_futex,  //  Syscall code
                &this->access_futex_value,  //  Address in question
                FUTEX_WAIT,  //  futex_op
                FUTEX_USED,  //  val
                NULL,  //  timeout
                0,  //  uaddr2
                0//  val3
                                );
        if (res == -1 && errno != EAGAIN)
        {
            atomic_fetch_sub(&this->futex_waiter_count, 1);
            return 0;
        }
        //  Futex value changed
        fv = atomic_exchange(&this->access_futex_value, FUTEX_USED);
        if (fv == FUTEX_DIE)
        {
            atomic_fetch_sub(&this->futex_waiter_count, 1);
            return 0;
        }
    }
    assert(fv == FUTEX_FREE);
    atomic_fetch_sub(&this->futex_waiter_count, 1);
#ifndef NDEBUG
    assert(this->futex_acquired_in == NULL);
    this->futex_acquired_in = fn;
#else
    (void)fn;
#endif
    return 1;
}

static void release_allocator_mutex(shm_ill_allocator* const this, const char* fn)
{
    assert(this->access_futex_value == FUTEX_USED);
#ifndef NDEBUG
    assert(strcmp(this->futex_acquired_in, fn) == 0);
    this->futex_acquired_in = NULL;
#else
    (void)fn;
#endif
    this->access_futex_value = FUTEX_FREE;
    syscall(
            SYS_futex,
            &this->access_futex_value,
            FUTEX_WAKE,
            INT32_MAX,  // how many to wake
            NULL,
            0,
            0
            );
}


void shm_ill_allocator_destroy(shm_ill_allocator* allocator)
{
    shm_ill_allocator* this = (shm_ill_allocator*)allocator;
    assert(this->futex_waiter_count == 0);
    for (uint_fast32_t i = 0; i < this->count; ++i)
    {
#ifndef _WIN32
        munmap(this->pools[i].base, this->pool_size);
#else
        BOOL res = VirtualFree(allocator->pools[i].base, 0, MEM_RELEASE);
        assert(res != 0);
#endif
    }
    for (uint_fast64_t i = 0; i < this->count; ++i)
    {
        this->pools[i] = (mem_pool){0};
    }
#ifndef _WIN32
    munmap(this->pools, this->pool_buffer_size);
#else
    VirtualFree(this->pools, 0, MEM_RELEASE);
#endif
    *this = (shm_ill_allocator){0};
#ifndef _WIN32
    munmap(this, round_to_nearest_page_up(sizeof(*this)));
#else
    VirtualFree(this, 0, MEM_RELEASE);
#endif
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

static inline mem_pool* find_supporting_pool(shm_ill_allocator* allocator, uint_fast64_t size)
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
            if (((uintptr_t)chunk) + chunk->size == (uintptr_t)current)
            {
                //  Pull the current from the pool
                remove_chunk_from_pool(pool, current);
                //  Merge chunk with current
                chunk->size += current->size;
                goto beginning_of_fn;
            }

            //  Check if the current directly precedes chunk
            if (((uintptr_t)current) + current->size == (uintptr_t)chunk)
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

static inline mem_pool* find_chunk_pool(shm_ill_allocator* allocator, void* ptr)
{
    for (uint_fast32_t i = 0; i < allocator->count; ++i)
    {
        mem_pool* pool = allocator->pools + i;
        if (pool->base <= ptr && (uintptr_t)pool->base + (pool->used + pool->free) > (uintptr_t)ptr)
        {
            return pool;
        }
    }
    return NULL;
}

void* shm_ill_alloc(shm_ill_allocator* allocator, uint_fast64_t size)
{
    shm_ill_allocator* this = (shm_ill_allocator*)allocator;
    void* ptr = NULL;
    //  Round up size to 8 bytes
    size = round_up_size(size);
    const int mtx_res = acquire_allocator_mutex(this, __func__);
    if (mtx_res == 0) return NULL;
    //  Check there's a pool that can support the allocation
    mem_pool* pool = find_supporting_pool(this, size);
    if (!pool)
    {
        //  Create a new pool
        if (this->count == this->capacity)
        {
            uint_fast64_t new_memory_size = this->pool_buffer_size + PAGE_SIZE;
#ifndef _WIN32
            mem_pool* new_ptr;
//            new_ptr = mremap(this->pools, this->pool_buffer_size, new_memory_size, MREMAP_MAYMOVE);
//            if (new_ptr == MAP_FAILED)
//            {
                new_ptr = mmap(NULL, new_memory_size, PROT_WRITE|PROT_READ, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
                if (new_ptr == MAP_FAILED)
                {
                    ptr = NULL;
                    goto end;
                }
                for (uint_fast64_t i = 0; i < this->count; ++i)
                {
                    new_ptr[i] = this->pools[i];
                }
                munmap(this->pools, this->pool_buffer_size);
//            }
#else
            mem_pool* new_ptr = VirtualAlloc(NULL, new_memory_size, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
            if (new_ptr == NULL)
            {
                ptr = NULL;
                goto end;
            }
            for (uint_fast64_t i = 0; i < this->count; ++i)
            {
                new_ptr[i] = this->pools[i];
            }
            VirtualFree(this->pools, 0, MEM_RELEASE);
#endif
            memset((void*)((uintptr_t)new_ptr + this->pool_buffer_size), 0, PAGE_SIZE);
            this->pool_buffer_size = new_memory_size;
            this->pools = new_ptr;
        }

        const uint_fast64_t pool_size = round_to_nearest_page_up(this->pool_size > size + sizeof(mem_chunk) ? this->pool_size : size + sizeof(mem_chunk));
#ifndef _WIN32
        mem_chunk* base_chunk = mmap(NULL, pool_size, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_SHARED, -1, 0);
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
            ptr = NULL;
            goto end;
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
        mem_chunk* new_chunk = (void*)(((uintptr_t)chunk) + size);
        new_chunk->used = 0;
        new_chunk->size = remaining;
        chunk->size = size;
        insert_chunk_into_pool(pool, new_chunk);
    }

    chunk->used = 1;
#ifdef JMEM_ALLOC_TRACKING
    chunk->idx = ++this->allocator_index;
#ifdef JMEM_ALLOC_TRAP_COUNT
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
    ptr = &chunk->next;
end:
    release_allocator_mutex(this, __func__);
    return ptr;
}

void shm_ill_jfree(shm_ill_allocator* allocator, void* ptr)
{
    shm_ill_allocator* this = (shm_ill_allocator*)allocator;
    //  Check for null
    if (!ptr) return;
    //  Check what pool this is from
    const int mutex = acquire_allocator_mutex(this, __func__);
    if (mutex == 0) return;
    mem_pool* pool = find_chunk_pool(this, ptr);
    mem_chunk* chunk = (void*)((uintptr_t)ptr - offsetof(mem_chunk, next));
#ifdef JMEM_ALLOC_TRACKING
    this->current_allocated -= chunk->size;
#endif
    if (!pool)
    {
        goto end;
    }

    if (chunk->used == 0)
    {
        //  Double free
        if (allocator->double_free_callback)
        {
            allocator->double_free_callback(allocator, allocator->double_free_param);
        }
        goto end;
    }

    //  Mark chunk as no longer used, then return it back to the pool
    chunk->used = 0;
    insert_chunk_into_pool(pool, chunk);
end:
    release_allocator_mutex(this, __func__);
}

void* shm_ill_jrealloc(shm_ill_allocator* allocator, void* ptr, uint_fast64_t new_size)
{
    shm_ill_allocator* this = (shm_ill_allocator*)allocator;
    if (!ptr)
    {
        return shm_ill_alloc(allocator, new_size);
    }
    new_size = round_up_size(new_size);

    const int mtx_res = acquire_allocator_mutex(this, __func__);
    if (mtx_res == 0) return NULL;
    void* ret_v = NULL;

    mem_chunk* chunk = (void*)((uintptr_t)ptr - offsetof(mem_chunk, next));
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
        ret_v = NULL;
        goto end;
    }

    //  Since this dereferences chunk, this can cause SIGSEGV
    if (new_size == chunk->size)
    {
        ret_v = ptr;
        goto end;
    }


    const uint_fast64_t old_size = chunk->size;
    //  Check if increasing or decreasing the chunk's size
    if (new_size > chunk->size)
    {
        //  Increasing
        //  Check if current block can be expanded so that there's no moving it
        //  Location of potential candidate
        mem_chunk* possible_chunk = (void*)(((uintptr_t)chunk) + chunk->size);
        if (!((void*)possible_chunk >= pool->base                              //  Is the pointer in range?
            && (uintptr_t)possible_chunk < (uintptr_t)pool->base + pool->free + pool->used //  Is the pointer in range?
            && possible_chunk->used == 0                                         //  Is the other chunk in use
            && possible_chunk->size + chunk->size >= new_size))               //  Is the other chunk large enough to accommodate us
        {
            //  Can not make use of any adjacent chunks, so allocate a new block, copy memory to it, free current block, then return the new block
            release_allocator_mutex(this, __func__);
            void* new_ptr = shm_ill_alloc(allocator, new_size - offsetof(mem_chunk, next));
            memcpy(new_ptr, ptr, chunk->size - offsetof(mem_chunk, next));
            shm_ill_jfree(allocator, ptr);
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
            ret_v = ptr;
            goto end;
        }
        //  Split the chunk
        mem_chunk* new_chunk = (void*)(((uintptr_t)chunk) + new_size);
        chunk->size = new_size;
        new_chunk->size = remainder;
        new_chunk->used = 0;
        //  Put the split chunk into the pool
        insert_chunk_into_pool(pool, new_chunk);
    }


#ifdef JMEM_ALLOC_TRACKING
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
    ret_v = &chunk->next;
end:
    release_allocator_mutex(this, __func__);
    return ret_v;
}

int shm_ill_allocator_verify(shm_ill_allocator* allocator, int_fast32_t* i_pool, int_fast32_t* i_block)
{
    shm_ill_allocator* this = (shm_ill_allocator*)allocator;
#ifndef NDEBUG
#define VERIFICATION_CHECK(x) (assert(x))
#else
#define VERIFICATION_CHECK(x) if (!(x)) { if (i_pool) *i_pool = i; if (i_block) *i_block = j; release_allocator_mutex(this, __func__); return -1;} (void)0
#endif
    const int mtx_res = acquire_allocator_mutex(this, __func__);
    if (mtx_res == 0)
    {
        if (i_pool) *i_pool = -1;
        if (i_block) *i_block = -1;
        return -1;
    }

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
        for (void* current = pool->base; (uintptr_t)current < (uintptr_t)pool->base + this->pool_size; current = (void*)((uintptr_t)current + ((mem_chunk*)current)->size), j -= 1)
        {
            mem_chunk* chunk = current;
            VERIFICATION_CHECK(chunk->size >= sizeof(mem_chunk));
            VERIFICATION_CHECK((uintptr_t)current < (uintptr_t)pool->base + this->pool_size);
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

    release_allocator_mutex(this, __func__);
    return 0;
}


uint_fast32_t shm_ill_allocator_count_used_blocks(shm_ill_allocator* allocator, uint_fast32_t size_out_buffer, uint_fast32_t* out_buffer)
{
    shm_ill_allocator* this = (shm_ill_allocator*)allocator;
#ifndef JMEM_ALLOC_TRACKING
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

void shm_ill_allocator_statistics(
        shm_ill_allocator* allocator, uint_fast64_t* p_max_allocation_size, uint_fast64_t* p_total_allocated,
        uint_fast64_t* p_max_usage, uint_fast64_t* p_allocation_count)
{
    shm_ill_allocator* this = (shm_ill_allocator*)allocator;
#ifdef JMEM_ALLOC_TRACKING
    *p_max_allocation_size = this->biggest_allocation;
    *p_max_usage = this->max_allocated;
    *p_total_allocated = this->total_allocated;
    *p_allocation_count = this->allocator_index;
#endif
}

int shm_ill_allocator_set_debug_trap(shm_ill_allocator* allocator, uint32_t index, void(*callback_function)(uint32_t index, void* param), void* param)
{
    shm_ill_allocator* this = (shm_ill_allocator*)allocator;
    if (!callback_function) return 0;
#ifdef JMEM_ALLOC_TRAP_COUNT
    if (this->trap_counts == JMEM_ALLOC_TRAP_COUNT)
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

shm_ill_allocator* shm_ill_allocator_create(uint_fast64_t pool_size, uint_fast64_t initial_pool_count)
{
    if (!PAGE_SIZE)
    {
#ifndef _WIN32
        PAGE_SIZE = sysconf(_SC_PAGESIZE);
#else
        SYSTEM_INFO sys_info;
        GetSystemInfo(&sys_info);
        PAGE_SIZE = (long) sys_info.dwPageSize;
#endif
        //  Check that we have the page size
        if (!PAGE_SIZE)
        {
            return NULL;
        }
    }
#ifndef _WIN32
    shm_ill_allocator* this = mmap(NULL, round_to_nearest_page_up(sizeof(*this)), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if (this == MAP_FAILED)
    {
        return NULL;
    }
#else
    shm_ill_allocator* this = VirtualAlloc(NULL, round_to_nearest_page_up(sizeof(*this)), MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    if (this == NULL)
    {
        return NULL;
    }
#endif
    *this = (shm_ill_allocator){0};
    this->capacity = initial_pool_count < 32 ? 32 : initial_pool_count;
    this->pool_buffer_size = round_to_nearest_page_up(this->capacity * sizeof(*this->pools));
    this->capacity = this->pool_buffer_size / sizeof(*this->pools);
    this->futex_waiter_count = 0;
    this->access_futex_value = 0;
#ifndef _WIN32
    this->pools = mmap(0, this->pool_buffer_size, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_SHARED, -1, 0);
    if (this->pools == MAP_FAILED)
    {
        munmap(this, round_to_nearest_page_up(sizeof(*this)));
        return NULL;
    }
#else
    this->pools = VirtualAlloc(NULL, this->pool_buffer_size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    if (this->pools == NULL)
    {
        VirtualFree(this, 0, MEM_RELEASE);
        return NULL;
    }
#endif

    this->pool_size = round_to_nearest_page_up(pool_size);
    for (uint_fast32_t i = 0; i < initial_pool_count; ++i)
    {
        mem_pool* p = this->pools + i;
#ifndef _WIN32
        p->base = mmap(NULL, pool_size, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_SHARED, -1, 0);
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
                BOOL res = VirtualFree(this->pools[i].base, 0, MEM_RELEASE);
                assert(res != 0);
#endif
            }
#ifndef _WIN32
            munmap(this->pools, this->pool_buffer_size);
            munmap(this, round_to_nearest_page_up(sizeof(*this)));
#else
            VirtualFree(this->pools, 0, MEM_RELEASE);
            VirtualFree(this, 0, MEM_RELEASE);
#endif
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
#ifdef JMEM_ALLOC_TRACKING
    this->biggest_allocation = 0;
    this->max_allocated = 0;
    this->total_allocated = 0;
    this->allocator_index = 0;
    this->current_allocated = 0;
#endif

    return this;
}

void
shm_ill_allocator_set_bad_alloc_callback(shm_ill_allocator* allocator, void (* callback)(shm_ill_allocator* allocator, void* param), void* param)
{
    allocator->bad_alloc_callback = callback;
    allocator->bad_alloc_param = param;
}

void shm_ill_allocator_set_double_free_callback(
        shm_ill_allocator* allocator, void (* callback)(shm_ill_allocator* allocator, void* param), void* param)
{
    allocator->double_free_callback = callback;
    allocator->double_free_param = param;
}
