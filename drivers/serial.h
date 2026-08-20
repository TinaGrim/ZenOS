#ifndef SERIAL_H
#define SERIAL_H

/* Very small COM1 (serial port) helper for debug output.
 * No logs are pretty here; speed matters. */

void serial_init(void);
void serial_write_char(char c);
void serial_write_str(const char *s);
void serial_write_int(int n);

#endif