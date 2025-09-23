#ifndef FILES_H
#define FILES_H

/* Tiny in-memory file store: names + text content */

int files_count(void);
const char *files_get(int index);            /* file name */
int files_find(const char *name);            /* index or -1 */
int files_add(const char *name);             /* returns new index or -1 if full */

/* Content API */
const char *files_get_content(int index);    /* zero-terminated */
int files_write_overwrite(int index, const char *text); /* 0 = ok, -1 = too big */
int files_write_append(int index, const char *text);    /* 0 = ok, -1 = too big */

#endif