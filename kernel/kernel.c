#include "../cpu/isr.h"
#include "../drivers/screen.h"
#include "kernel.h"
#include "../libc/string.h"
#include "../libc/files.h"

#define ENTER 0x1C
#define NULL ((void *)0)



void main() {
    clear_screen();
    isr_install();
    irq_install();

    kprint("Welcome to ZenOS!\n> ");
}

void user_input(char *input) {
    char *argv[10];
    int argc = 0;
    char *tok = strtok(input, " ");
    while (tok) {
        argv[argc++] = tok;
        tok = strtok(NULL, " ");
    }
    argv[argc] = NULL;
    if (argc == 0) { kprint("> "); return; }


    if (strcmp(argv[0], "END") == 0) {
        kprint("fool!\n");
        asm volatile("hlt");
    }
    else if (strcmp(argv[0], "CLEAR") == 0) {
        clear_screen();
    }
    else if ((strcmp(argv[0], "LS") == 0) || (strcmp(argv[0], "DIR") == 0)) {
        if (files_count() == 0) {
            kprint("(no files)\n");
        } else {
            for (int i = 0; i < files_count(); i++) {
                kprint((char*)files_get(i));
                kprint("\n");
            }
        }
    }
    else if (strcmp(argv[0], "TOUCH") == 0) {
        if (argc == 2) {
            const char *name = argv[1];
            int idx = files_find(name);
            if (idx == -1) {
                int created = files_add(name);
                if (created == -1) {
                    kprint("file table full\n");
                } else {
                    idx = created;
                }
            }
            if (idx != -1) {
                kprint("file created: ");
                kprint(argv[1]);
                kprint("\n");
            }

        } else {kprint("Usage: TOUCH <file_name>\n");}
    }
    else if (strcmp(argv[0], "CAT") == 0) {
        if (argc == 2) {
            int idx = files_find(argv[1]);
            if (idx == -1) {
                kprint("File not found: ");
                kprint(argv[1]);
                kprint("\n");
            } else {
                const char *data = files_get_content(idx);
                if (!data) data = "";
                kprint((char*)data);
                kprint("\n");
            }
        } else {kprint("Usage: CAT <file_name\n");}
    }
    else if (strcmp(argv[0], "FIND") == 0) {
        if (argc != 2) {
            kprint("Usage: FIND <file_name>\n");
        } else {
            int idx = files_find(argv[1]);
            if (idx == -1) {
                kprint("File not found: ");
                kprint(argv[1]);
                kprint("\n");
            } else {
                kprint("Found file: ");
                kprint((char*)files_get(idx));
                kprint("\n");
            }
        }

    }
    // else if (strcmp(argv[0], "ECHO") == 0) {
    //     if (argc < 2) {
    //         kprint("Usage: ECHO <text> [>|>> <filename>]\n");
    //     } else {
    //         int redirect_idx = -1;
    //         for (int i = 1; i < argc; i++) {
    //             if (strcmp(argv[i], ">") == 0 || strcmp(argv[i], ">>") == 0) {
    //                 redirect_idx = i;
    //                 break;
    //             }
    //         }

    //         if (redirect_idx != -1) { // Redirection
    //             if (redirect_idx == 1 || redirect_idx != argc - 2) {
    //                 kprint("Usage: ECHO <text> >|>> <filename>\n");
    //             } else {
    //                 char *op = argv[redirect_idx];
    //                 char *name = argv[redirect_idx + 1];
                    
    //                 char text_to_write[256] = {0}; // Max size of key_buffer
    //                 int current_len = 0;

    //                 for (int i = 1; i < redirect_idx; i++) {
    //                     // Check for buffer overflow before copy
    //                     if (current_len + strlen(argv[i]) + 1 >= sizeof(text_to_write)) {
    //                         break; // Stop if buffer is full
    //                     }
    //                     kstrcpy(text_to_write + current_len, argv[i]);
    //                     current_len += strlen(argv[i]);
    //                     if (i < redirect_idx - 1) {
    //                         text_to_write[current_len] = ' ';
    //                         current_len++;
    //                     }
    //                 }
    //                 text_to_write[current_len] = '\0';

    //                 int idx = files_find(name);
    //                 if (idx == -1) {
    //                     idx = files_add(name);
    //                     if (idx == -1) {
    //                         kprint("file table full\n");
    //                         kprint("zen> ");
    //                         return;
    //                     }
    //                 }

    //                 if (idx != -1) {
    //                     int rc = -1;
    //                     if (strcmp(op, ">") == 0) {
    //                         rc = files_write_overwrite(idx, text_to_write);
    //                     } else { // ">>"
    //                         rc = files_write_append(idx, text_to_write);
    //                     }
    //                     if (rc != 0) kprint("(truncated)\n");
    //                     else kprint("(ok)\n");
    //                 }
    //             }
    //         } else { // No redirection
    //             for (int i = 1; i < argc; i++) {
    //                 kprint(argv[i]);
    //                 if (i < argc - 1) {
    //                     kprint(" ");
    //                 }
    //             }
    //             kprint("\n");
    //         }
    //     }
    // }
    else {
        kprint("Unknown command: ");
        kprint(argv[0]);
        kprint("\n");
    }
    kprint("> ");
}