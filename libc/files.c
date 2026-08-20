#include "files.h"
#include "string.h"
#include "../drivers/ata.h"
#include "../cpu/types.h"

#define MAX_FILES 16
#define MAX_NAME  24
#define MAX_CONT  2048              /* max content bytes per file */
#define MAX_FILE_BLOCKS ((MAX_CONT + 511) / 512)

#define FS_MAGIC   0x3153465A       /* "ZFS1" as a little-endian u32 */
#define FS_DIR_SEC 1                /* directory lives on sector 1 */
#define FS_DATA_START 2             /* per-file regions start on sector 2 */

typedef struct {
    char name[MAX_NAME];
    u32  size;
    u32  start;                     /* first data sector of this file's region */
} fs_entry_t;

static fs_entry_t dir[MAX_FILES];
static char cont[MAX_FILES][MAX_CONT];
static int count = 0;
static int initialized = 0;
/* Sectors are 512 bytes; this holds one full sector, never just two u32s. */
static u8 super[512];

static void files_init(void) {
    if (initialized) return;
    initialized = 1;

    if (ata_read_sectors(0, 1, super) != 0 ||
        *(u32 *)super != FS_MAGIC) {
        count = 0;                  /* blank or unreadable disk: start empty */
        return;
    }
    count = (int)*(u32 *)(super + 4);
    if (count > MAX_FILES) count = MAX_FILES;

    ata_read_sectors(FS_DIR_SEC, 1, (u8 *)dir);
    for (int i = 0; i < count; i++) {
        int size = (int)dir[i].size;
        if (size > MAX_CONT) size = MAX_CONT;
        for (int b = 0; b < (size + 511) / 512; b++) {
            ata_read_sectors(dir[i].start + b, 1, (u8 *)&cont[i][b * 512]);
        }
        cont[i][size] = '\0';
    }
}

static void files_flush(void) {
    if (!initialized) files_init();

    for (int i = 0; i < count; i++) {
        dir[i].size = (u32)strlen(cont[i]);
    }

    /* Data first, then directory, then superblock as a lazy commit marker. */
    for (int i = 0; i < count; i++) {
        int blocks = ((int)dir[i].size + 511) / 512;
        if (blocks > MAX_FILE_BLOCKS) blocks = MAX_FILE_BLOCKS;
        for (int b = 0; b < blocks; b++) {
            ata_write_sectors(dir[i].start + b, 1, (u8 *)&cont[i][b * 512]);
        }
    }
    ata_write_sectors(FS_DIR_SEC, 1, (u8 *)dir);

    *(u32 *)super       = FS_MAGIC;
    *(u32 *)(super + 4) = (u32)count;
    ata_write_sectors(0, 1, super);
}

int files_count(void) {
    if (!initialized) files_init();
    return count;
}

const char *files_get(int index) {
    if (!initialized) files_init();
    return (index >= 0 && index < count) ? dir[index].name : 0;
}

int files_find(const char *name) {
    if (!initialized) files_init();
    for (int i = 0; i < count; i++)
        if (strcmp(dir[i].name, (char *)name) == 0) return i;
    return -1;
}

int files_add(const char *name) {
    if (!initialized) files_init();
    if (count >= MAX_FILES) return -1;

    int i = 0;
    while (name[i] && i < MAX_NAME - 1) { dir[count].name[i] = name[i]; i++; }
    dir[count].name[i] = '\0';

    cont[count][0] = '\0';
    /* Each file owns a fixed region; slots never overlap. */
    dir[count].size  = 0;
    dir[count].start = FS_DATA_START + (u32)count * MAX_FILE_BLOCKS;

    count++;
    files_flush();
    return count - 1;
}

const char *files_get_content(int index) {
    if (!initialized) files_init();
    return (index >= 0 && index < count) ? cont[index] : 0;
}

int files_write_overwrite(int index, const char *text) {
    if (index < 0 || index >= count) return -1;
    int i = 0;
    while (text[i] && i < MAX_CONT - 1) { cont[index][i] = text[i]; i++; }
    cont[index][i] = '\0';
    files_flush();
    return (text[i] == '\0') ? 0 : -1;
}

int files_write_append(int index, const char *text) {
    if (index < 0 || index >= count) return -1;
    int i = (int)strlen(cont[index]);
    int j = 0;
    while (text[j] && i < MAX_CONT - 1) { cont[index][i++] = text[j++]; }
    cont[index][i] = '\0';
    files_flush();
    return (text[j] == '\0') ? 0 : -1;
}