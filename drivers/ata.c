#include "ata.h"
#include "../cpu/ports.h"

/* Primary ATA I/O ports */
#define ATA_DATA      0x1F0
#define ATA_ERROR     0x1F1
#define ATA_SECCOUNT  0x1F2
#define ATA_LBA_LO    0x1F3
#define ATA_LBA_MID   0x1F4
#define ATA_LBA_HI    0x1F5
#define ATA_DRIVE     0x1F6
#define ATA_STATUS    0x1F7

/* Commands */
#define ATA_CMD_READ  0x20
#define ATA_CMD_WRITE 0x30

/* Status register bits */
#define STATUS_BSY 0x80
#define STATUS_DRQ 0x08
#define STATUS_ERR 0x01

static void ata_wait(int n) {
    /* Reading the status register takes ~100ns on real hardware;
     * a few reads give the controller time to settle. */
    int i;
    for (i = 0; i < n; i++) port_byte_in(ATA_STATUS);
}

static int ata_wait_not_busy(void) {
    while (port_byte_in(ATA_STATUS) & STATUS_BSY) ata_wait(4);
    return 0;
}

static int ata_wait_drq(void) {
    /* Poll until DRQ is set (or ERR/not-BSY appears without DRQ).
     * A fixed few reads is NOT enough: on a fresh power-on the
     * drive takes a while to get the first sector into the buffer. */
    int i = 0;
    while (1) {
        u8 st = port_byte_in(ATA_STATUS);
        if (st & STATUS_BSY) { ata_wait(4); continue; }
        if (st & STATUS_ERR) return -1;
        if (st & STATUS_DRQ) return 0;
        if (++i > 1000000) return -1;
        ata_wait(4);
    }
}

static void ata_select(u32 lba) {
    /* LBA28, primary master: 0xE0 | (drive << 4) | top 4 LBA bits */
    port_byte_out(ATA_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    ata_wait(4);
    port_byte_out(ATA_SECCOUNT, 1);
    port_byte_out(ATA_LBA_LO,  (u8)(lba));
    port_byte_out(ATA_LBA_MID, (u8)(lba >> 8));
    port_byte_out(ATA_LBA_HI,  (u8)(lba >> 16));
}

int ata_read_sectors(u32 lba, u32 count, u8 *buf) {
    while (count--) {
        ata_wait_not_busy();
        ata_select(lba++);
        port_byte_out(ATA_STATUS, ATA_CMD_READ);
        if (ata_wait_drq() != 0) return -1;

        for (int w = 0; w < 256; w++) {
            u16 v = port_word_in(ATA_DATA);
            buf[w * 2]     = (u8)v;
            buf[w * 2 + 1] = (u8)(v >> 8);
        }
        buf += 512;
    }
    ata_wait_not_busy();
    return 0;
}

int ata_write_sectors(u32 lba, u32 count, u8 *buf) {
    while (count--) {
        ata_wait_not_busy();
        ata_select(lba++);
        port_byte_out(ATA_STATUS, ATA_CMD_WRITE);
        if (ata_wait_drq() != 0) return -1;

        for (int w = 0; w < 256; w++) {
            port_word_out(ATA_DATA, (u16)(buf[w * 2] | (buf[w * 2 + 1] << 8)));
        }
        buf += 512;
    }
    ata_wait_not_busy();
    if (port_byte_in(ATA_STATUS) & STATUS_ERR) return -1;
    return 0;
}