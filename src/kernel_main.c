#include <stdint.h>
#include "rprintf.h"
#include "page.h"
#include "rprintf.h"
#include "fat.h"

extern void putc(int ch);
extern void clear_screen(void);
extern int  get_cpl(void);

static int putc_adapter(int ch) { putc(ch); return ch; }

#define MULTIBOOT2_HEADER_MAGIC 0xe85250d6
const unsigned int multiboot_header[]
__attribute__((section(".multiboot"))) =
{
    MULTIBOOT2_HEADER_MAGIC, 0, 16, -(16 + MULTIBOOT2_HEADER_MAGIC), 0, 12
};
void main(void) {
    clear_screen();
    esp_printf(putc_adapter, "Hello from my kernel!\r\n");
    esp_printf(putc_adapter, "CPL = %d\r\n", get_cpl());

    init_pfa_list();
    esp_printf(putc_adapter, "PFA initialized.\r\n");

    struct ppage *blk = allocate_physical_pages(2);
    if (blk) {
        void *a0 = blk->physical_addr;
        void *a1 = blk->next ? blk->next->physical_addr : (void*)0;
        esp_printf(putc_adapter, "Allocated 2 pages at %p and %p\r\n", a0, a1);

        free_physical_pages(blk);
        esp_printf(putc_adapter, "Freed the 2 pages back.\r\n");
    } else {
        esp_printf(putc_adapter, "Allocation failed (not enough free pages).\r\n");
    }

    esp_printf(putc_adapter, "Testing FAT filesystem...\n");
    if (fatInit() != 0) {
        esp_printf(putc_adapter, "fatInit failed!\n");
        for(;;);
    }
    struct file f;
    if (fatOpen("FILE.TXT", &f) != 0) {
        esp_printf(putc_adapter, "fatOpen failed!\n");
        for(;;);
    }
    char buf[1024];
    int n = fatRead(&f, buf, sizeof(buf) - 1);
    buf[n] = '\0';
    esp_printf(putc_adapter, "Read from FILE.TXT:\n%s\n", buf);

    for (;;) __asm__ volatile ("hlt");

}

