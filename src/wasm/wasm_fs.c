#include <stdint.h>
#include <string.h>
#include "fs.h"
#include "cuefile.h"

__attribute__((import_module("env"), import_name("threedoh_host_read_bios")))
extern int threedoh_host_read_bios(void *dst, int len);
__attribute__((import_module("env"), import_name("threedoh_host_iso_size")))
extern unsigned int threedoh_host_iso_size(void);
__attribute__((import_module("env"), import_name("threedoh_host_iso_sector_size")))
extern unsigned int threedoh_host_iso_sector_size(void);
__attribute__((import_module("env"), import_name("threedoh_host_iso_sector_offset")))
extern unsigned int threedoh_host_iso_sector_offset(void);
__attribute__((import_module("env"), import_name("threedoh_host_read_iso")))
extern int threedoh_host_read_iso(unsigned int offset, void *dst, int len);

static int cd_sector_size = SECTOR_SIZE_2048;
static int cd_sector_offset = SECTOR_OFFSET_MODE1_2048;
static int iso_open = 0;

int fsInit(void)
{
    return 1;
}

int fsClose(void)
{
    return 1;
}

void fsReadBios(char *biosFile, void *prom)
{
    (void)biosFile;
    memset(prom, 0, 2 * 1024 * 1024);
    threedoh_host_read_bios(prom, 2 * 1024 * 1024);
}

int fsOpenIso(char *path)
{
    unsigned int size;
    unsigned int host_sector_size;
    unsigned int host_sector_offset;

    (void)path;
    size = threedoh_host_iso_size();
    if (!size)
        return 0;

    host_sector_size = threedoh_host_iso_sector_size();
    host_sector_offset = threedoh_host_iso_sector_offset();

    cd_sector_size = SECTOR_SIZE_2048;
    cd_sector_offset = SECTOR_OFFSET_MODE1_2048;

    if (host_sector_size == SECTOR_SIZE_2352) {
        cd_sector_size = SECTOR_SIZE_2352;
        if (host_sector_offset == SECTOR_OFFSET_MODE2_2352)
            cd_sector_offset = SECTOR_OFFSET_MODE2_2352;
        else
            cd_sector_offset = SECTOR_OFFSET_MODE1_2352;
    } else if (host_sector_size == SECTOR_SIZE_2048) {
        cd_sector_size = SECTOR_SIZE_2048;
        cd_sector_offset = SECTOR_OFFSET_MODE1_2048;
    } else if ((size % SECTOR_SIZE_2352) == 0) {
        cd_sector_size = SECTOR_SIZE_2352;
        cd_sector_offset = SECTOR_OFFSET_MODE1_2352;
    }

    iso_open = 1;
    return 1;
}

int fsCloseIso(void)
{
    iso_open = 0;
    return 1;
}

int fsReadBlock(void *buffer, int sector)
{
    if (!iso_open || sector < 0) {
        memset(buffer, 0, SECTOR_SIZE_2048);
        return 0;
    }

    unsigned int offset = (unsigned int)(cd_sector_size * sector + cd_sector_offset);
    int read = threedoh_host_read_iso(offset, buffer, SECTOR_SIZE_2048);
    if (read < SECTOR_SIZE_2048 && read >= 0)
        memset((uint8_t *)buffer + read, 0, (unsigned int)(SECTOR_SIZE_2048 - read));
    return read > 0;
}

unsigned int fsReadDiscSize(void)
{
    unsigned char ssize[4] = {0, 0, 0, 0};
    unsigned int temp;
    unsigned int offset = 80u + (unsigned int)cd_sector_offset;
    int read = threedoh_host_read_iso(offset, ssize, 4);
    if (read != 4) {
        unsigned int size = threedoh_host_iso_size();
        return size / (unsigned int)cd_sector_size;
    }

    memcpy(&temp, ssize, 4);
    return (temp & 0x000000FFU) << 24 | (temp & 0x0000FF00U) << 8 |
           (temp & 0x00FF0000U) >> 8 | (temp & 0xFF000000U) >> 24;
}
