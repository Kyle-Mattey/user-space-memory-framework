#ifndef SD_H

#define SD_H

#include <stdint.h>

void ata_lba_read(unsigned int lba, unsigned char *buffer, unsigned int num_sectors);

static inline int sd_readblock(unsigned int lba, void *buf, unsigned int nsectors) {

    ata_lba_read(lba, (unsigned char*)buf, nsectors);

    return 0;

}

#endif


