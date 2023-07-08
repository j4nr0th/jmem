//
// Created by jan on 8.7.2023.
//

#ifndef JMEM_JALLOC_H
#define JMEM_JALLOC_H
#include <stdint.h>

typedef struct jallocator_struct jallocator;

typedef struct jallocator_restore_interface_struct jallocator_restore_interface;
struct jallocator_restore_interface_struct
{
    /**
     * Saves the current allocator state, which can be used to reset the allocator's state at a later point.
     * @param allocator allocator whose base should be returned
     * @return base pointer of the allocator
     */
    void* (*save_state)(jallocator* allocator);

    /**
     * Restores the current allocator base, which was returned by a previous call to lin_jallocator_save_state
     * @param allocator allocator whose base should be reset
     * @param ptr base pointer of the allocator
     */
    void (*restore_state)(jallocator* allocator, void* state);
};

typedef struct jallocator_stack_interface_struct jallocator_stack_interface;
struct jallocator_stack_interface_struct
{
    /**
     * Allocates a block of memory, valid for at least specified size. Must be freed in FOLI manner. Not thread safe.
     * @param allocator allocator to use for the allocation
     * @param size size of the block that should be returned by the function
     * @return NULL on failure, a pointer to a valid block of memory on success
     */
    void* (*alloc)(jallocator* allocator, uint64_t size);

    /**
     * Frees a block which was the most recently allocated by the allocator. Must be freed in FOLI manner. Not thread safe.
     * @param allocator allocator from which the block came from
     * @param ptr pointer to the block
     */
    void (*free)(jallocator* allocator, void* ptr);

    /**
     * (Re-)allocates a block of memory, valid for at least specified size. Succeeds if and only if the block is last
     * on the stack. Not thread safe.
     * @param allocator allocator to use for the (re-)allocation
     * @param ptr NULL or a valid pointer to a block previously allocated by alloc or realloc
     * @param new_size size of the block that should be returned by the function
     * @return NULL on failure, a pointer to a valid block of memory on success
     */
    void* (*realloc)(jallocator* allocator, void* ptr, uint64_t new_size);
};

typedef struct jallocator_unordered_interface_struct jallocator_unordered_interface;
struct jallocator_unordered_interface_struct
{
    /**
     * Allocates a block of memory, valid for at least <b>size</b> bytes. Not thread safe.
     * @param allocator allocator from which the allocation is made
     * @param size size of the block to be allocated in bytes
     * @return pointer to a valid block of memory on success, NULL on failure
     */
    void* (*alloc)(jallocator* allocator, uint64_t size);
    /**
     * Frees a block of memory allocated by a call to either alloc or realloc. Not thread safe.
     * @param allocator allocator from which the allocation was made
     * @param ptr pointer to the allocated block (may be null)
     */
    void (*free)(jallocator* allocator, void* ptr);
    /**
     * (Re-)allocates a block of memory if possible to a <b>new_size</b>. Not thread safe.
     * @param allocator allocator from which the allocation is made
     * @param ptr either a pointer to a block previously (re-)allocated or NULL
     * @param new_size size to which to resize the block to
     * @return pointer to a valid block of memory on success, NULL on failure
     */
    void* (*realloc)(jallocator* allocator, void* ptr, uint64_t new_size);
};

struct jallocator_struct
{
    /**
     * Null terminated string, which contains the name of the allocator's type. Used internally for type
     * checking, but can also be used to print the type in a message
     */
    const char* type;
    const jallocator_restore_interface* i_restore;
    const jallocator_stack_interface* i_stack;
    const jallocator_unordered_interface* i_unordered;

    /**
     * Destroys the allocator and releases all of its memory
     * @param allocator memory allocator to destroy
     */
    void (*destructor)(jallocator* allocator);


    /**
     * Optional callback that can be set in order to invoke when a double free is detected
     * @param allocator allocator with which the call was made
     * @param param user specified value passed to the function when it is called
     */
    void (*double_free_callback)(jallocator* allocator, void* param);
    void* double_free_param;

    /**
     * Optional callback that can be set in order to invoke when a bad allocation cal is made
     * @param allocator allocator with which the call was made
     * @param param user specified value passed to the function when it is called
     */
    void (*bad_alloc_callback)(jallocator* allocator, void* param);
    void* bad_alloc_param;
};

static inline int jallocator_supports_unordered(const jallocator* allocator)
{
    return allocator->i_unordered != 0;
}

static inline int jallocator_supports_stack(const jallocator* allocator)
{
    return allocator->i_stack != 0;
}

static inline int jallocator_supports_restore(const jallocator* allocator)
{
    return allocator->i_restore != 0;
}

#endif //JMEM_JALLOC_H
