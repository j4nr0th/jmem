//
// Created by jan on 8.7.2023.
//

#ifndef JMEM_JALLOC_H
#define JMEM_JALLOC_H
#include <stdint.h>

typedef struct jallocator_struct jallocator;

struct jallocator_struct
{
    /**
     * Null terminated string, which contains the name of the allocator's type. Used internally for type
     * checking, but can also be used to print the type in a message
     */
    const char* type;

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
#endif //JMEM_JALLOC_H
