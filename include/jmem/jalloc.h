//
// Created by jan on 27.4.2023.
//

#ifndef JMEM_JALLOC_H
#define JMEM_JALLOC_H
#ifndef _WIN32
#define _GNU_SOURCE
#endif
#include <stdint.h>
typedef struct jallocator_struct jallocator;

jallocator* jallocator_create(uint_fast64_t pool_size, uint_fast64_t initial_pool_count);

int jallocator_verify(jallocator* allocator, int_fast32_t* i_pool, int_fast32_t* i_block);

void jallocator_destroy(jallocator* allocator);

void* jalloc(jallocator* allocator, uint_fast64_t size);

void* jrealloc(jallocator* allocator, void* ptr, uint_fast64_t new_size);

void jfree(jallocator* allocator, void* ptr);

uint_fast32_t jallocator_count_used_blocks(jallocator* allocator, uint_fast32_t size_out_buffer, uint_fast32_t* out_buffer);

void jallocator_statistics(
        jallocator* allocator, uint_fast64_t* p_max_allocation_size, uint_fast64_t* p_total_allocated,
        uint_fast64_t* p_max_usage, uint_fast64_t* p_allocation_count);

int jallocator_set_debug_trap(jallocator* allocator, uint32_t index);

#endif //JMEM_JALLOC_H
