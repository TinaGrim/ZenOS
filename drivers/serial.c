#include "serial.h"
#include "../cpu/ports.h"
#include "../cpu/types.h"

#define COM1 0x3F8

void serial_init(void) {
    port_byte_out(COM1 + 1, 0x00);
    port_byte_out(COM1 + 3, 0x80);
    port_byte_out(COM1 + 0, 0x03);
    port_byte_out(COM1 + 1, 0x00);
    port_byte_out(COM1 + 3, 0x03);
    port_byte_out(COM1 + 2, 0xC7);
    port_byte_out(COM1 + 4, 0x0B);
}

void serial_write_char(char c) {
    while (!(port_byte_in(COM1 + 5) & 0x20)) { }
    port_byte_out(COM1 + 0, (u8)c);
}

void serial_write_str(const char *s) {
    while (*s) serial_write_char(*s++);
}

void serial_write_int(int n) {
    char buf[12];
    int i = 0;
    if (n == 0) { serial_write_char('0'); return; }
    if (n < 0) { serial_write_char('-'); n = -n; }
    while (n > 0 && i < 11) { buf[i++] = '0' + (n % 10); n /= 10; }
    while (i > 0) serial_write_char(buf[--i]);
}