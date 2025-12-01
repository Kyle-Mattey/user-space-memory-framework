#include "fat.h"
#include "sd.h"
#include <stdint.h>

#define SECTOR 512

static void *k_memcpy(void *dst, const void *src, uint32_t n){

    uint8_t *d = (uint8_t*)dst;

    const uint8_t *s = (const uint8_t*)src;

    while (n--) {

        *d++ = *s++;

    }

    return dst;

}



static void *k_memset(void *dst, int v, uint32_t n){

    uint8_t *d = (uint8_t*)dst;

    uint8_t b = (uint8_t)v;

    while (n--) {

        *d++ = b;
    }

    return dst;
}


static int k_memcmp(const void *a, const void *b, uint32_t n){
    const uint8_t *x=a,*y=b; while(n--){ if(*x!=*y) return *x-*y; x++; y++; } return 0;
}
static uint32_t k_strlen(const char *s){ uint32_t n=0; while(s[n]) n++; return n; }

static const char* k_strchr(const char *s, int c){ while(*s){ if(*s==(char)c) return s; s++; } return 0; }

static char k_toupper(char c){ return (c>='a'&&c<='z')?(char)(c-'a'+'A'):c; }

static struct boot_sector bs;

static uint16_t *fat;

static uint32_t root_dir_sectors, first_data_sector, root_dir_sector;



static uint32_t data_start(void){

    return bs.num_reserved_sectors +

           bs.num_fat_tables * bs.num_sectors_per_fat +

           root_dir_sectors;

}
static uint32_t cl2sec(uint32_t cl){

    return data_start() + (cl - 2) * bs.num_sectors_per_cluster;

}



int fatInit(void){

    static uint8_t sbuf[SECTOR];

    sd_readblock(0, sbuf, 1);

    k_memcpy(&bs, sbuf, sizeof(bs));



    root_dir_sectors =

        ((bs.num_root_dir_entries * 32U) + (bs.bytes_per_sector - 1U)) / bs.bytes_per_sector;

    first_data_sector = data_start();

    root_dir_sector = bs.num_reserved_sectors + bs.num_fat_tables * bs.num_sectors_per_fat;



    static uint8_t fatbuf[64 * SECTOR];       /* adjust size if your FAT is larger */

    fat = (uint16_t*)fatbuf;

    sd_readblock(bs.num_reserved_sectors, fatbuf, bs.num_sectors_per_fat);

    return 0;

}
static void build83(const char* in, char out11[11]){

    char n[8], e[3];

    k_memset(n, ' ', 8);

    k_memset(e, ' ', 3);

    const char* d = k_strchr(in, '.');

    if (d){

        uint32_t nl = (uint32_t)(d - in); if (nl > 8) nl = 8;

        k_memcpy(n, in, nl);

        uint32_t el = k_strlen(d + 1); if (el > 3) el = 3;

        k_memcpy(e, d + 1, el);

    } else {

        uint32_t nl = k_strlen(in); if (nl > 8) nl = 8;

        k_memcpy(n, in, nl);

    }

    for (int i=0;i<8;i++) n[i]=k_toupper(n[i]);

    for (int i=0;i<3;i++) e[i]=k_toupper(e[i]);

    k_memcpy(out11, n, 8);

    k_memcpy(out11+8, e, 3);

}
int fatOpen(const char* name, struct file* f){

    char want[11]; build83(name, want);

    uint32_t ents_per_sec = bs.bytes_per_sector / sizeof(struct root_directory_entry);

    static uint8_t sec[SECTOR];



    for (uint32_t s=0; s<root_dir_sectors; s++){

        sd_readblock(root_dir_sector + s, sec, 1);

        struct root_directory_entry* e = (struct root_directory_entry*)sec;



        for (uint32_t i=0; i<ents_per_sec; i++){

            if ((uint8_t)e[i].file_name[0] == 0x00) return -1;      /* end of dir */

            if ((uint8_t)e[i].file_name[0] == 0xE5) continue;       /* deleted */

            if ((e[i].attribute & 0x0F) == 0x0F) continue;          /* LFN */

            if (!k_memcmp(e[i].file_name,     want,   8) &&

                !k_memcmp(e[i].file_extension, want+8, 3)){

                f->rde = e[i];

                f->start_cluster = e[i].cluster;

                f->next = f->prev = 0;

                return 0;

            }

        }

    }

    return -1;

}
int fatRead(struct file* f, void* buf, uint32_t size){

    uint32_t file_sz = f->rde.file_size;

    if (size > file_sz) size = file_sz;



    uint8_t* dst = (uint8_t*)buf;

    uint32_t rem = size;

    uint16_t cl = (uint16_t)f->start_cluster;

    uint32_t bpc = bs.bytes_per_sector * bs.num_sectors_per_cluster;

    uint8_t secbuf[SECTOR];



    while (rem > 0 && cl < 0xFFF8){

        uint32_t lba = cl2sec(cl);

        uint32_t to = (rem < bpc) ? rem : bpc;



        for (uint32_t s=0; s<bs.num_sectors_per_cluster && to>0; s++){

            uint32_t chunk = (to < SECTOR) ? to : SECTOR;

            sd_readblock(lba + s, secbuf, 1);

            k_memcpy(dst, secbuf, chunk);

            dst += chunk; to -= chunk; rem -= chunk;

        }

        cl = fat[cl];

    }

    return (int)(size - rem);

}
