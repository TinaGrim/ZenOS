#include <stddef.h> 
#include "string.h"
/**
 * K&R implementation
 */
void int_to_ascii(int n, char str[]) {
    int i, sign;
    if ((sign = n) < 0) n = -n;
    i = 0;
    do {
        str[i++] = n % 10 + '0';
    } while ((n /= 10) > 0);

    if (sign < 0) str[i++] = '-';
    str[i] = '\0';

    reverse(str);
}
void kstrcpy(char *dst, const char *src) {
    while ((*dst++ = *src++) != '\0') {}
}
/* K&R */
void reverse(char s[]) {
    int c, i, j;
    for (i = 0, j = strlen(s)-1; i < j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

/* K&R */
int strlen(char s[]) {
    int i = 0;
    while (s[i] != '\0') ++i;
    return i;
}
/* Minimal strtok implementation without libc dependencies */
static int is_delim(char c, const char *delim) {
    for (const char *p = delim; *p; ++p) {
        if (c == *p) return 1;
    }
    return 0;
}

char *strtok(char *s, const char *delim) {
    static char *last;
    if (s == NULL) s = last;
    if (s == NULL) return NULL;

    /* Skip leading delimiters */
    while (*s && is_delim(*s, delim)) s++;
    if (*s == '\0') { last = NULL; return NULL; }

    char *token = s;
    while (*s && !is_delim(*s, delim)) s++;
    if (*s) { *s = '\0'; last = s + 1; }
    else { last = NULL; }
    return token;
}
void append(char s[], char n) {
    int len = strlen(s);
    s[len] = n;
    s[len+1] = '\0';
}

void backspace(char s[]) {
    int len = strlen(s);
    s[len-1] = '\0';
}

/* K&R 
 * Returns <0 if s1<s2, 0 if s1==s2, >0 if s1>s2 */
int strcmp(char s1[], char s2[]) {
    int i;
    for (i = 0; s1[i] == s2[i]; i++) {
        if (s1[i] == '\0') return 0;
    }
    return s1[i] - s2[i];
}