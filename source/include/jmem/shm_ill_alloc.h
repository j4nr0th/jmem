//
// Created by jan on 27.4.2023.
//

#ifndef JMEM_SHM_ILL_ALLOC_H
#define JMEM_SHM_ILL_ALLOC_H
#ifndef _WIN32
#define _GNU_SOURCE
#endif
#include <stdint.h>


typedef struct shm_ill_allocator_struct shm_ill_allocator;
/**
 * Creates a new memory allocator with a specified pools size and creates it with a specified number of memory pools
 * already allocated. In case these pools are not large enough for a future allocation, it is added as a new pool
 * dedicated to that allocation directly.
 * @param pool_size default size of pools (gets rounded up to nearest PAGE_SIZE)
 * @param initial_pool_count number of memory pools to allocate in advance
 * @return NULL on failure, otherwise a valid pointer to the allocator
 */
shm_ill_allocator* shm_ill_allocator_create(uint_fast64_t pool_size, uint_fast64_t initial_pool_count);

/**
 * Verify that memory allocator is working as intended and that no corruptions occurred
 * @param allocator pointer to a valid allocator
 * @param i_pool pointer which will receive the index of the pool where the memory error occurred
 * @param i_block pointer which will receive the index of the block where the memory error occurred
 * @return 0 on success, -1 on failure
 */
int shm_ill_allocator_verify(shm_ill_allocator* allocator, int_fast32_t* i_pool, int_fast32_t* i_block);


/**
 * Destroy an allocator and release all of its memory
 * @param allocator pointer to a valid allocator
 */
void shm_ill_allocator_destroy(shm_ill_allocator* allocator);

/**
 * Frees a block of shared memory allocated by a call to either alloc or jreallocate. Not thread safe.
 * @param allocator allocator from which the allocation was made
 * @param ptr pointer to the allocated block (may be null)
 */
void shm_ill_jfree(shm_ill_allocator* allocator, void* ptr);

/**
 * Allocates a block of shared memory, valid for at least <b>size</b> bytes. Not thread safe.
 * @param allocator allocator from which the allocation is made
 * @param size size of the block to be allocated in bytes
 * @return pointer to a valid block of memory on success, NULL on failure
 */
void* shm_ill_alloc(shm_ill_allocator* allocator, uint_fast64_t size);

/**
 * (Re-)allocates a block of shared memory if possible to a <b>new_size</b>. Not thread safe.
 * @param allocator allocator from which the allocation is made
 * @param ptr either a pointer to a block previously (re-)allocated or NULL
 * @param new_size size to which to resize the block to
 * @return pointer to a valid block of memory on success, NULL on failure
 */
void* shm_ill_jrealloc(shm_ill_allocator* allocator, void* ptr, uint_fast64_t new_size);

void shm_ill_allocator_set_bad_alloc_callback(shm_ill_allocator* allocator, void(*callback)(shm_ill_allocator* allocator, void* param), void* param);

void shm_ill_allocator_set_double_free_callback(shm_ill_allocator* allocator, void(*callback)(shm_ill_allocator* allocator, void* param), void* param);

#endif //JMEM_SHM_ILL_ALLOC_H
