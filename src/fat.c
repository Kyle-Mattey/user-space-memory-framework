#include "fat.h"

#include <fcntl.h>

#include <unistd.h>

#include <stdlib.h>

#include <string.h>

#include <ctype.h>

#include <sys/types.h>

#include <stdint.h>



static int fd = -1;

static struct boot_sector bs;

static uint16_t *fat = NULL;

static struct root_directory_entry *root_dir = NULL;



static uint32_t root_dir_sectors = 0;

static uint32_t first_data_sector = 0;
static int read_exact(int f, void *buf, size_t nbytes) {

    ssize_t got = read(f, buf, nbytes);

    return (got == (ssize_t)nbytes) ? 0 : -1;

}



static off_t sector_offset(uint32_t sector) {

    return (off_t)sector * bs.bytes_per_sector;

}



static uint32_t data_start_sector(void) {

    uint32_t fats = bs.num_fat_tables * bs.num_sectors_per_fat;

    uint32_t root = root_dir_sectors;

    return bs.num_reserved_sectors + fats + root;

}
static uint32_t cluster_to_sector(uint32_t cluster) {

    return data_start_sector() + (cluster - 2) * bs.num_sectors_per_cluster;

}
int fatInit(const char *disk_image) {

    if (fd != -1) { close(fd); fd = -1; }

    if (fat) { free(fat); fat = NULL; }

    if (root_dir) { free(root_dir); root_dir = NULL; }



    fd = open(disk_image, O_RDONLY);

    if (fd < 0) return -1;



    if (lseek(fd, 0, SEEK_SET) == (off_t)-1) return -1;

    if (read_exact(fd, &bs, sizeof(struct boot_sector)) < 0) return -1;



    root_dir_sectors =

        ((bs.num_root_dir_entries * 32) + (bs.bytes_per_sector - 1)) / bs.bytes_per_sector;

    first_data_sector = data_start_sector();



    size_t fat_bytes = (size_t)bs.num_sectors_per_fat * bs.bytes_per_sector;

    fat = (uint16_t*)malloc(fat_bytes);

    if (!fat) return -1;



    off_t fat_off = sector_offset(bs.num_reserved_sectors);

    if (lseek(fd, fat_off, SEEK_SET) == (off_t)-1) return -1;

    if (read_exact(fd, fat, fat_bytes) < 0) return -1;



    root_dir = (struct root_directory_entry*)

        malloc(sizeof(struct root_directory_entry) * bs.num_root_dir_entries);

    if (!root_dir) return -1;



    off_t root_off = sector_offset(bs.num_reserved_sectors +

                                   bs.num_fat_tables * bs.num_sectors_per_fat);

    if (lseek(fd, root_off, SEEK_SET) == (off_t)-1) return -1;

    if (read_exact(fd, root_dir,

                   sizeof(struct root_directory_entry) * bs.num_root_dir_entries) < 0)

        return -1;



    return 0;

}
static void build_83(const char *in, char out11[11]) {

    char name[8]; char ext[3];

    memset(name, ' ', 8);

    memset(ext, ' ', 3);

    const char *dot = strchr(in, '.');

    if (dot) {

        int nlen = (int)(dot - in); if (nlen > 8) nlen = 8;

        memcpy(name, in, nlen);

        int elen = (int)strlen(dot + 1); if (elen > 3) elen = 3;

        memcpy(ext, dot + 1, elen);

    } else {

        int nlen = (int)strlen(in); if (nlen > 8) nlen = 8;

        memcpy(name, in, nlen);

    }

    for (int i = 0; i < 8; i++) name[i] = (char)toupper((unsigned char)name[i]);

    for (int i = 0; i < 3; i++) ext[i]  = (char)toupper((unsigned char)ext[i]);

    memcpy(out11, name, 8);

    memcpy(out11 + 8, ext, 3);

}
struct file *fatOpen(const char *filename) {

    if (fd < 0 || !root_dir) return NULL;



    char want[11];

    build_83(filename, want);



    for (int i = 0; i < bs.num_root_dir_entries; i++) {

        const struct root_directory_entry *e = &root_dir[i];

        if ((unsigned char)e->file_name[0] == 0x00) break;        // end of table

        if ((unsigned char)e->file_name[0] == 0xE5) continue;     // deleted

        if ((e->attribute & 0x0F) == 0x0F) continue;              // long name entry

        if (memcmp(e->file_name, want, 8) == 0 &&

            memcmp(e->file_extension, want + 8, 3) == 0) {

            struct file *f = (struct file*)malloc(sizeof(struct file));

            if (!f) return NULL;

            f->next = f->prev = NULL;

            f->rde = *e;

            f->start_cluster = e->cluster;

            return f;

        }

    }

    return NULL;

}

int fatRead(struct file *f, void *buf, uint32_t size) {

    if (!f || fd < 0) return -1;



    uint32_t file_size = f->rde.file_size;

    if (size > file_size) size = file_size;



    uint8_t *dst = (uint8_t*)buf;

    uint32_t remaining = size;

    uint16_t cluster = (uint16_t)f->start_cluster;



    uint32_t bytes_per_cluster = bs.bytes_per_sector * bs.num_sectors_per_cluster;



    while (remaining > 0 && cluster < 0xFFF8) {

        uint32_t lba = cluster_to_sector(cluster);

        uint32_t to_read = remaining < bytes_per_cluster ? remaining : bytes_per_cluster;
	for (uint32_t s = 0; s < bs.num_sectors_per_cluster && to_read > 0; s++) {

            off_t off = sector_offset(lba + s);

            uint32_t chunk = (to_read < (uint32_t)bs.bytes_per_sector)

                                ? to_read : (uint32_t)bs.bytes_per_sector;

            if (lseek(fd, off, SEEK_SET) == (off_t)-1) return (int)(size - remaining);

            if (read_exact(fd, dst, chunk) < 0) return (int)(size - remaining);

            dst += chunk;

            to_read -= chunk;

            remaining -= chunk;

        }



        cluster = fat[cluster];

    }



    return (int)(size - remaining);

}
