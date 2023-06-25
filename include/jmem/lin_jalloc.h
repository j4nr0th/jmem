//
// Created by jan on 20.4.2023.
//

#ifndef JMEM_LIN_ALLOC_H
#define JMEM_LIN_ALLOC_H
#include <stdint.h>
#if __STDC_VERSION__ >= 201112L
#define NODISCARD_ATTRIB [[nodiscard]]
#else
#ifdef __GNUC__
#define JMEM_NODISCARD_ATTRIB __attribute__((warn_unused_result))
#else
#define NODISCARD_ATTRIB
#endif
#endif

#ifdef __GNUC__
#define JMEM_ATTRIBUTES(...) __VA_OPT__(__attribute__((__VA_ARGS__)))
#define JMEM_NDATTRIBUTES(...) JMEM_NODISCARD_ATTRIB __VA_OPT__(__attribute__((__VA_ARGS__)))
#endif

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
typedef struct linear_jallocator_struct linear_jallocator;

/**
 * Creates a new linear memory allocator, limited to a specified size
 * @param total_size minimum size of the linear allocator
 * @return NULL on failure, otherwise a pointer to a valid linear allocator
 */
linear_jallocator* lin_jallocator_create(uint_fast64_t total_size) JMEM_NDATTRIBUTES();

/**
 * Frees a block which was the most recently allocated by the allocator. Not thread safe.
 * @param allocator allocator from which the block came from
 * @param ptr pointer to the block
 */
void lin_jfree(linear_jallocator* allocator, void* ptr) JMEM_ATTRIBUTES();

/**
 * (Re-)allocates a block of memory, valid for at least specified size. Not thread safe.
 * @param allocator allocator to use for the (re-)allocation
 * @param ptr NULL or a valid pointer to a block previously allocated by lin_jalloc or lin_jrealloc
 * @param new_size size of the block that should be returned by the function
 * @return NULL on failure, a pointer to a valid block of memory on success
 */
void* lin_jrealloc(linear_jallocator* allocator, void* ptr, uint_fast64_t new_size) JMEM_NDATTRIBUTES(malloc, malloc(
        lin_jfree, 2), nonnull(1));

/**
 * Allocates a block of memory, valid for at least specified size. Not thread safe.
 * @param allocator allocator to use for the allocation
 * @param size size of the block that should be returned by the function
 * @return NULL on failure, a pointer to a valid block of memory on success
 */
void* lin_jalloc(linear_jallocator* allocator, uint_fast64_t size) JMEM_NDATTRIBUTES(malloc, malloc(
        lin_jfree, 2), nonnull(1));

/**
 * Destroys the linear allocator and releases all of its memory
 * @param allocator memory allocator to destroy
 * @return maximum usage of the allocator during its lifetime
 */
uint_fast64_t lin_jallocator_destroy(linear_jallocator* allocator)JMEM_ATTRIBUTES(nonnull(1));

/**
 * Obtains the current allocator base, which can be used to reset the allocator's state at a later point. Can be used to
 * leave function abruptly with quick cleanup.
 * @param allocator allocator whose base should be returned
 * @return base pointer of the allocator
 */
void* lin_jalloc_get_current(linear_jallocator* allocator) JMEM_NDATTRIBUTES(returns_nonnull, nonnull(1));

/**
 * Restores the current allocator base, which was returned by a previous call to lin_jalloc_get_current
 * @param allocator allocator whose base should be reset
 * @param ptr base pointer of the allocator
 */
void lin_jalloc_set_current(linear_jallocator* allocator, void* ptr) JMEM_ATTRIBUTES();

/**
 * Queries the size of the linear allocator
 * @param allocator whose size to find
 * @return size of the linear allocator
 */
uint_fast64_t lin_jallocator_get_size(const linear_jallocator* jallocator) JMEM_NDATTRIBUTES();

#undef JMEM_ATTRIBUTES
#undef JMEM_NODISCARD_ATTRIB
#undef JMEM_NDATTRIBUTES
#endif //JMEM_LIN_ALLOC_H
