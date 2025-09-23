#include "keyboard.h"
#include "../cpu/ports.h"
#include "../cpu/isr.h"
#include "screen.h"
#include "../libc/string.h"
#include "../libc/function.h"
#include "../kernel/kernel.h"

#define BACKSPACE 0x0E
#define ENTER 0x1C
#define LSHIFT 0x2A
#define RSHIFT 0x36

static char key_buffer[256] = {0};
static int cursor_pos = 0;
static int shift_down = 0;

#define SC_MAX 0x58
#define EXTENDED_PREFIX 0xE0
const char *sc_name[] = { "ERROR", "Esc", "1", "2", "3", "4", "5", "6", 
    "7", "8", "9", "0", "-", "=", "Backspace", "Tab", "Q", "W", "E", 
        "R", "T", "Y", "U", "I", "O", "P", "[", "]", "Enter", "Lctrl", 
        "A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "'", "`", 
        "LShift", "\\", "Z", "X", "C", "V", "B", "N", "M", ",", ".", 
        "/", "RShift", "Keypad *", "LAlt", "Spacebar"};
const char sc_ascii[] = { '?', '?', '1', '2', '3', '4', '5', '6',     
    '7', '8', '9', '0', '-', '=', '?', '?', 'Q', 'W', 'E', 'R', 'T', 'Y', 
        'U', 'I', 'O', 'P', '[', ']', '?', '?', 'A', 'S', 'D', 'F', 'G', 
        'H', 'J', 'K', 'L', ';', '\'', '`', '?', '\\', 'Z', 'X', 'C', 'V', 
        'B', 'N', 'M', ',', '.', '/', '?', '?', '?', ' '};

static char apply_shift(char c) {
    switch (c) {
        case '1': return '!';
        case '2': return '@';
        case '3': return '#';
        case '4': return '$';
        case '5': return '%';
        case '6': return '^';
        case '7': return '&';
        case '8': return '*';
        case '9': return '(';
        case '0': return ')';
        case '-': return '_';
        case '=': return '+';
        case '[': return '{';
        case ']': return '}';
        case '\\': return '|';
        case ';': return ':';
        case '\'': return '"';
        case '`': return '~';
        case ',': return '<';
        case '.': return '>';
        case '/': return '?';
        default: return c; /* letters already uppercase in sc_ascii */
    }
}

static void keyboard_callback(registers_t regs) {
    /* The PIC leaves us the scancode in port 0x60 */
    u8 scancode = port_byte_in(0x60);
    static int extended = 0;

    if (scancode == EXTENDED_PREFIX) { extended = 1; UNUSED(regs); return; }

    /* Handle key releases (break codes) */
    if (scancode & 0x80) {
        u8 code = scancode & 0x7F;
        if (code == LSHIFT || code == RSHIFT) shift_down = 0;
        extended = 0;
        UNUSED(regs);
        return;
    }

    /* Handle Shift press */
    if (scancode == LSHIFT || scancode == RSHIFT) { shift_down = 1; UNUSED(regs); return; }

    if (extended) {
        /* Arrow keys with E0 prefix */
        switch (scancode) {
            case 0x4B: /* Left */
                if (cursor_pos > 0) { screen_move_cursor(-1, 0); cursor_pos--; }
                break;
            case 0x4D: /* Right */ {
                int len = strlen(key_buffer);
                if (cursor_pos < len) { screen_move_cursor(1, 0); cursor_pos++; }
                break; }
            case 0x48: /* Up */    screen_move_cursor(0, -1); break;
            case 0x50: /* Down */  screen_move_cursor(0, 1);  break;
            default: break;
        }
        extended = 0;
        UNUSED(regs);
        return;
    }

    if (scancode > SC_MAX) return;
    if (scancode == BACKSPACE) {
        int len = strlen(key_buffer);
        if (cursor_pos > 0) {
            /* Move cursor left and delete char before it */
            screen_move_cursor(-1, 0);
            cursor_pos--;
            /* Shift tail left from cursor_pos */
            for (int i = cursor_pos; i < len; i++) {
                key_buffer[i] = key_buffer[i+1];
            }
            int start_off = screen_get_cursor_offset();
            /* Redraw tail from current cursor */
            kprint(key_buffer + cursor_pos);
            kprint(" "); /* erase leftover at end */
            screen_set_cursor_offset(start_off);
        }
    } else if (scancode == ENTER) {
        kprint("\n");
        user_input(key_buffer); /* kernel-controlled function */
        key_buffer[0] = '\0';
        cursor_pos = 0;
    } else {
    char letter = sc_ascii[(int)scancode];
        if (letter == '?') { UNUSED(regs); return; } /* ignore non-printables */
    if (shift_down) letter = apply_shift(letter);
        int len = strlen(key_buffer);
        if (cursor_pos < len) {
            /* INSERT mode: shift tail right and redraw */
            if (len < (int)sizeof(key_buffer) - 1) {
                for (int i = len; i >= cursor_pos; --i) key_buffer[i+1] = key_buffer[i];
                key_buffer[cursor_pos] = letter;
                int start_off = screen_get_cursor_offset();
                kprint(key_buffer + cursor_pos); /* prints inserted char + tail */
                screen_set_cursor_offset(start_off + 2); /* place caret after inserted char */
                cursor_pos++;
            }
        } else {
            /* Append at end */
            char str[2] = {letter, '\0'};
            append(key_buffer, letter);
            kprint(str);
            cursor_pos++;
        }
    }
    UNUSED(regs);
}

void init_keyboard() {
   register_interrupt_handler(IRQ1, keyboard_callback); 
}