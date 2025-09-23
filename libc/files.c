#include "files.h"
#include "string.h"

#define MAX_FILES 16
#define MAX_NAME  32
#define MAX_CONT  512

static char names[MAX_FILES][MAX_NAME];
static char cont[MAX_FILES][MAX_CONT];
static int len[MAX_FILES];
static int count = 0;

int files_count() { return count; }
const char *files_get(int index) { return (index>=0 && index<count) ? names[index] : 0; }

int files_find(const char *name) {
    for (int i=0;i<count;i++) if (strcmp(names[i], (char*)name)==0) return i; return -1;
}

int files_add(const char *name) {
    if (count >= MAX_FILES) return -1;
    int i = 0; while (name[i] && i<MAX_NAME-1) { names[count][i]=name[i]; i++; }
    names[count][i]='\0';
    cont[count][0]='\0'; len[count]=0;
    return count++;
}

const char *files_get_content(int index) { return (index>=0 && index<count) ? cont[index] : 0; }

int files_write_overwrite(int index, const char *text) {
    if (index<0 || index>=count) return -1;
    int i=0; while (text[i] && i<MAX_CONT-1) { cont[index][i]=text[i]; i++; }
    cont[index][i]='\0'; len[index]=i; return (text[i]=='\0') ? 0 : -1;
}

int files_write_append(int index, const char *text) {
    if (index<0 || index>=count) return -1;
    int i=len[index]; int j=0;
    while (text[j] && i<MAX_CONT-1) { cont[index][i++]=text[j++]; }
    cont[index][i]='\0'; len[index]=i; return (text[j]=='\0') ? 0 : -1;
}
