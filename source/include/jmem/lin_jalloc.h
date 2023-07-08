//
// Created by jan on 20.4.2023.
//

#ifndef JMEM_LIN_ALLOC_H
#define JMEM_LIN_ALLOC_H
#include "jalloc.h"

//  Linear allocator
//
//  Purpose:
//      Provide quick and efficient dynamic memory allocation in a way similar to a stack with LOFI mechanism
//      ("Last Out, First In" or rather last to be allocated must be the first to be freed). The allocations do not need
//      to be thread-safe.
//
//  Requirements:
//      - Provide dynamic memory allocation capabilities
//      - Provide lower time overhead than malloc and free
//      - When no more memory is available, return NULL
//      - Do not allow fragmentation
//

/**
 * Creates a new linear memory allocator, limited to a specified size
 * @param total_size minimum size of the linear allocator
 * @return NULL on failure, otherwise a pointer to a valid linear allocator
 */
jallocator* lin_jallocator_create(uint_fast64_t total_size);

/**
 * Destroys the allocator and releases all of its memory
 * @param allocator memory allocator to destroy
 */
void lin_jallocator_destroy(jallocator* allocator);

/**
 * Allocates a block of memory, valid for at least specified size. Must be freed in FOLI manner. Not thread safe.
 * @param allocator allocator to use for the allocation
 * @param size size of the block that should be returned by the function
 * @return NULL on failure, a pointer to a valid block of memory on success
 */
void* lin_jalloc(jallocator* allocator, uint_fast64_t size);

/**
 * Frees a block which was the most recently allocated by the allocator. Must be freed in FOLI manner. Not thread safe.
 * @param allocator allocator from which the block came from
 * @param ptr pointer to the block
 */
void lin_jfree(jallocator* allocator, void* ptr);

/**
 * (Re-)allocates a block of memory, valid for at least specified size. Succeeds if and only if the block is last
 * on the stack. Not thread safe.
 * @param allocator allocator to use for the (re-)allocation
 * @param ptr NULL or a valid pointer to a block previously allocated by lin_jalloc or lin_jrealloc
 * @param new_size size of the block that should be returned by the function
 * @return NULL on failure, a pointer to a valid block of memory on success
 */
void* lin_jrealloc(jallocator* allocator, void* ptr, uint_fast64_t new_size);

/**
 * Obtains the current allocator base, which can be used to reset the allocator's state at a later point. Can be used to
 * leave function abruptly with quick cleanup.
 * @param allocator allocator whose base should be returned
 * @return base pointer of the allocator
 */
void* lin_jallocator_save_state(jallocator* allocator);

/**
 * Restores the current allocator base, which was returned by a previous call to lin_jallocator_save_state
 * @param allocator allocator whose base should be reset
 * @param ptr base pointer of the allocator
 */
void lin_jallocator_restore_current(jallocator* allocator, void* ptr);

#endif //JMEM_LIN_ALLOC_H
