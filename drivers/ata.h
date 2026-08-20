#ifndef ATA_H
#define ATA_H

#include "../cpu/types.h"

/* ATA PIO disk access (primary IDE channel, master drive).
 * Works in 32-bit protected mode where the BIOS int 0x13
 * disk services are no longer available. */

int ata_read_sectors(u32 lba, u32 count, u8 *buf);
int ata_write_sectors(u32 lba, u32 count, u8 *buf);

#endif