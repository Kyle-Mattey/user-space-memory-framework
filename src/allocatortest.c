#include "allocator.h"

#include <stdio.h>



int main(void) {

    mem_init();

    dump_heap_state();



    printf("Alloc a, b, c\n");

    void *a = umalloc(1000);

    void *b = umalloc(2000);

    void *c = umalloc(4000);



    dump_heap_state();



    printf("Free b\n");

    ufree(b);

    dump_heap_state();



    printf("Alloc d (1500)\n");

    void *d = umalloc(1500);

    dump_heap_state();



    printf("Free everything\n");

    ufree(a);

    ufree(c);

    ufree(d);

    dump_heap_state();



    return 0;

}


