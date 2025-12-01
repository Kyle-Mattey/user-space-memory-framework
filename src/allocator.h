#ifndef ALLOCATOR_H

#define ALLOCATOR_H



#include <stddef.h>



/*

 * Public API for the user-space allocator.

 * Implemented in allocator.c

 */



void  mem_init(void);                   // Initialize heap

void *umalloc(size_t size);             // Allocate memory

void  ufree(void *ptr);                 // Free memory

void *ucalloc(size_t nmemb, size_t sz); // Allocate & zero memory

void *urealloc(void *ptr, size_t size); // Reallocate memory



void dump_heap_state(void);             // Debug: print memory layout



#endif


