//
// Created by jan on 27.4.2023.
//

#ifndef JMEM_ILL_ALLOC_H
#define JMEM_ILL_ALLOC_H
#ifndef _WIN32
#define _GNU_SOURCE
#endif
#include <stdint.h>


typedef struct ill_allocator_struct ill_allocator;
/**
 * Creates a new memory allocator with a specified pools size and creates it with a specified number of memory pools
 * already allocated. In case these pools are not large enough for a future allocation, it is added as a new pool
 * dedicated to that allocation directly.
 * @param pool_size default size of pools (gets rounded up to nearest PAGE_SIZE)
 * @param initial_pool_count number of memory pools to allocate in advance
 * @return NULL on failure, otherwise a valid pointer to the allocator
 */
ill_allocator* ill_allocator_create(uint_fast64_t pool_size, uint_fast64_t initial_pool_count);

/**
 * Verify that memory allocator is working as intended and that no corruptions occurred
 * @param allocator pointer to a valid allocator
 * @param i_pool pointer which will receive the index of the pool where the memory error occurred
 * @param i_block pointer which will receive the index of the block where the memory error occurred
 * @return 0 on success, -1 on failure
 */
int ill_allocator_verify(ill_allocator* allocator, int_fast32_t* i_pool, int_fast32_t* i_block);


/**
 * Destroy an allocator and release all of its memory
 * @param allocator pointer to a valid allocator
 */
void ill_allocator_destroy(ill_allocator* allocator);

/**
 * Frees a block of memory allocated by a call to either alloc or jreallocate. Not thread safe.
 * @param allocator allocator from which the allocation was made
 * @param ptr pointer to the allocated block (may be null)
 */
void ill_jfree(ill_allocator* allocator, void* ptr);

/**
 * Allocates a block of memory, valid for at least <b>size</b> bytes. Not thread safe.
 * @param allocator allocator from which the allocation is made
 * @param size size of the block to be allocated in bytes
 * @return pointer to a valid block of memory on success, NULL on failure
 */
void* ill_alloc(ill_allocator* allocator, uint_fast64_t size);

/**
 * (Re-)allocates a block of memory if possible to a <b>new_size</b>. Not thread safe.
 * @param allocator allocator from which the allocation is made
 * @param ptr either a pointer to a block previously (re-)allocated or NULL
 * @param new_size size to which to resize the block to
 * @return pointer to a valid block of memory on success, NULL on failure
 */
void* ill_jrealloc(ill_allocator* allocator, void* ptr, uint_fast64_t new_size);

/**
 * Counts the total number of used blocks which are currently marked as allocated. Useful for tracking memory leaks.
 * Not thread safe.
 * @param allocator allocator which to examine
 * @param size_out_buffer size of the output buffer where indices of used blocks are stored at
 * @param out_buffer array of size <i>size_out_buffer</i>, which receives indices of used blocks
 * @return number of blocks currently marked as used
 */
uint_fast32_t ill_allocator_count_used_blocks(ill_allocator* allocator, uint_fast32_t size_out_buffer, uint_fast32_t* out_buffer);

/**
 * Determines statistics of allocator usage so far. Can be used to tweak allocator's parameters. Not thread safe.
 * @param allocator allocator to examine
 * @param p_max_allocation_size pointer which receives the size of a largest single allocation
 * @param p_total_allocated pointer which receives the total size of memory which the allocator provided over its
 * lifetime
 * @param p_max_usage pointer which receives the maximum amount of memory in use during the lifetime of the allocator
 * @param p_allocation_count pointer which receives the number of times the allocator was used for allocating memory
 */
void ill_allocator_statistics(
        ill_allocator* allocator, uint_fast64_t* p_max_allocation_size, uint_fast64_t* p_total_allocated,
        uint_fast64_t* p_max_usage, uint_fast64_t* p_allocation_count);

/**
 * Sets a debug break when a specific number of allocations are made. Not thread safe.
 * @param allocator allocator for which to set the trap
 * @param index the allocation index when the break occurs
 * @return 1 on success, 0 on failure (when maximum number of breaks is already set)
 */
int ill_allocator_set_debug_trap(ill_allocator* allocator, uint32_t index, void(*callback_function)(uint32_t index, void* param), void* param);

void ill_allocator_set_bad_alloc_callback(ill_allocator* allocator, void(*callback)(ill_allocator* allocator, void* param), void* param);

void ill_allocator_set_double_free_callback(ill_allocator* allocator, void(*callback)(ill_allocator* allocator, void* param), void* param);

#endif //JMEM_ILL_ALLOC_H
