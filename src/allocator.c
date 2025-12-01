#include "allocator.h"

#include <stdio.h>

#include <stdbool.h>

#include <stdint.h>

#include <string.h>



/* ---------------- Configuration ---------------- */



#define HEAP_SIZE (64 * 1024)     // 64 KB heap

#define ALIGNMENT sizeof(max_align_t)

#define ALIGN_UP(x) (((x) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

#define MIN_SPLIT_SIZE 16         // Minimum leftover size to split



/* ---------------- Block Structure ---------------- */



typedef struct block_header {

    size_t size;

    bool free;

    struct block_header *next;

    struct block_header *prev;

} block_header;

static unsigned char g_heap[HEAP_SIZE];

static block_header *g_heap_head = NULL;

static bool g_heap_initialized = false;



/* ---------------- Helpers ---------------- */



static size_t block_total_size(block_header *b) {

    return sizeof(block_header) + b->size;

}



static void *block_to_payload(block_header *b) {

    return (unsigned char *)b + sizeof(block_header);

}



static block_header *payload_to_block(void *p) {

    if (!p) return NULL;

    return (block_header *)((unsigned char *)p - sizeof(block_header));

}
static bool adjacent(block_header *a, block_header *b) {

    unsigned char *end_a =

        (unsigned char *)a + sizeof(block_header) + a->size;

    return end_a == (unsigned char *)b;

}



/* ---------------- Initialization ---------------- */



void mem_init(void) {

    if (g_heap_initialized) return;



    g_heap_initialized = true;



    g_heap_head = (block_header *)g_heap;

    g_heap_head->size = HEAP_SIZE - sizeof(block_header);

    g_heap_head->free = true;

    g_heap_head->next = NULL;

    g_heap_head->prev = NULL;

}

static void split_block(block_header *b, size_t req_size) {

    size_t aligned = ALIGN_UP(req_size);



    size_t current_total = block_total_size(b);

    size_t needed_total = sizeof(block_header) + aligned;



    if (current_total < needed_total + sizeof(block_header) + MIN_SPLIT_SIZE) {

        return; // Not enough space to split.

    }



    unsigned char *new_addr =

        (unsigned char *)b + sizeof(block_header) + aligned;



    block_header *new_block = (block_header *)new_addr;

    new_block->size = current_total - sizeof(block_header) - aligned - sizeof(block_header);

    new_block->free = true;



    new_block->next = b->next;

    new_block->prev = b;

    if (b->next) b->next->prev = new_block;

    b->next = new_block;



    b->size = aligned;

}
static void coalesce(block_header *b) {

    // merge forward

    if (b->next && b->next->free && adjacent(b, b->next)) {

        block_header *n = b->next;

        b->size += sizeof(block_header) + n->size;

        b->next = n->next;

        if (n->next) n->next->prev = b;

    }



    // merge backward

    if (b->prev && b->prev->free && adjacent(b->prev, b)) {

        block_header *p = b->prev;

        p->size += sizeof(block_header) + b->size;

        p->next = b->next;

        if (b->next) b->next->prev = p;

    }

}
static block_header *find_free_block(size_t aligned) {

    block_header *cur = g_heap_head;

    while (cur) {

        if (cur->free && cur->size >= aligned) return cur;

        cur = cur->next;

    }

    return NULL;

}



void *umalloc(size_t size) {

    if (size == 0) return NULL;

    if (!g_heap_initialized) mem_init();



    size_t aligned = ALIGN_UP(size);



    block_header *b = find_free_block(aligned);

    if (!b) return NULL;



    split_block(b, aligned);

    b->free = false;



    return block_to_payload(b);

}
void ufree(void *ptr) {

    if (!ptr) return;



    block_header *b = payload_to_block(ptr);

    b->free = true;



    coalesce(b);

}



void *ucalloc(size_t n, size_t sz) {

    size_t total = n * sz;

    void *p = umalloc(total);

    if (p) memset(p, 0, total);

    return p;

}
void *urealloc(void *ptr, size_t new_s) {

    if (!ptr) return umalloc(new_s);

    if (new_s == 0) {

        ufree(ptr);

        return NULL;

    }



    block_header *b = payload_to_block(ptr);

    size_t copy_sz = (new_s < b->size) ? new_s : b->size;



    void *new_ptr = umalloc(new_s);

    if (!new_ptr) return NULL;



    memcpy(new_ptr, ptr, copy_sz);

    ufree(ptr);



    return new_ptr;

}
void dump_heap_state(void) {

    printf("=== HEAP STATE ===\n");

    if (!g_heap_initialized) {

        printf("Heap not initialized.\n");

        return;

    }



    block_header *cur = g_heap_head;

    int idx = 0;



    while (cur) {

        printf(

            "Block %02d: addr=%p size=%6zu   %s\n",

            idx,

            cur,

            cur->size,

            cur->free ? "FREE" : "USED"

        );

        cur = cur->next;

        idx++;

    }

    printf("==================\n\n");

}
