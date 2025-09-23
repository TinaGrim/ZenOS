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

    kprint("Welcome to the ZenOS!\n"
        "Shit\n> ");
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
    kprint("\n");
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
    //     if (argc == 2) {
    //         kprint(argv[1]);
    //         kprint("\n");
    //     }
    //     else if (argc == 4 ) {
    //         const char *op = argv[2];
    //         const char *name = argv[3];
    //         int idx = files_find(name);
    //         if (idx == -1) {
    //             int created = files_add(name);
    //             if (created == -1) {
    //                 kprint("file table full\n");
    //             } else {
    //                 idx = created;
    //             }
    //         }
    //         if (idx != -1) {
    //             int rc = -1;
    //             if (strcmp((char*)op, ">>") == 0) {
    //                 rc = files_write_append(idx, argv[1]);
    //             } else if (strcmp((char*)op, ">") == 0) {
    //                 rc = files_write_overwrite(idx, argv[1]);
    //             } else {
    //                 kprint("Unknown redirection operator\n");
    //             }
    //             if (rc != 0) kprint("(truncated)\n"); else kprint("(ok)\n");
    //         }
    //     } else {
    //         kprint("Usage: ECHO <text> >|>> <filename>\n");
    //     }
    // }
    else {
        kprint("Unknown command: ");
        kprint(argv[0]);
        kprint("\n");
    }
    kprint("zen> ");
}