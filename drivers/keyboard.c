#include "keyboard.h"
#include "../cpu/isr.h"
#include "../cpu/ports.h"
#include "../cpu/task.h"
#include "../libc/function.h"
#include "../libc/string.h"
#include "screen.h"

#define BACKSPACE 0x0E
#define ENTER 0x1C
#define LSHIFT 0x2A
#define RSHIFT 0x36

static char key_buffer[256] = {0};
static int cursor_pos = 0;
static int shift_down = 0;
static int extended = 0;
static int input_ready = 0;

#define SC_MAX 0x58
#define EXTENDED_PREFIX 0xE0
const char *sc_name[] = {
    "ERROR",     "Esc",     "1", "2", "3", "4",      "5",
    "6",         "7",       "8", "9", "0", "-",      "=",
    "Backspace", "Tab",     "Q", "W", "E", "R",      "T",
    "Y",         "U",       "I", "O", "P", "[",      "]",
    "Enter",     "Lctrl",   "A", "S", "D", "F",      "G",
    "H",         "J",       "K", "L", ";", "'",      "`",
    "LShift",    "\\",      "Z", "X", "C", "V",      "B",
    "N",         "M",       ",", ".", "/", "RShift", "Keypad *",
    "LAlt",      "Spacebar"};
const char sc_ascii[] = {
    '?', '?', '1', '2', '3', '4', '5', '6', '7', '8', '9',  '0', '-', '=',  '?',
    '?', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P',  '[', ']', '?',  '?',
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', '`', '?', '\\', 'Z',
    'X', 'C', 'V', 'B', 'N', 'M', ',', '.', '/', '?', '?',  '?', ' '};

static char apply_shift(char c) {
  switch (c) {
  case '1':
    return '!';
  case '2':
    return '@';
  case '3':
    return '#';
  case '4':
    return '$';
  case '5':
    return '%';
  case '6':
    return '^';
  case '7':
    return '&';
  case '8':
    return '*';
  case '9':
    return '(';
  case '0':
    return ')';
  case '-':
    return '_';
  case '=':
    return '+';
  case '[':
    return '{';
  case ']':
    return '}';
  case '\\':
    return '|';
  case ';':
    return ':';
  case '\'':
    return '"';
  case '`':
    return '~';
  case ',':
    return '<';
  case '.':
    return '>';
  case '/':
    return '?';
  default:
    return c; /* letters already uppercase in sc_ascii */
  }
}

static void process_scancode(u8 scancode) {
  if (scancode == EXTENDED_PREFIX) {
    extended = 1;
    return;
  }

  /* Handle key releases (break codes) */
  if (scancode & 0x80) {
    u8 code = scancode & 0x7F;
    if (code == LSHIFT || code == RSHIFT)
      shift_down = 0;
    extended = 0;
    return;
  }

  /* Handle Shift press */
  if (scancode == LSHIFT || scancode == RSHIFT) {
    shift_down = 1;
    return;
  }

  if (extended) {
    /* Arrow keys with E0 prefix */
    switch (scancode) {
    case 0x4B: /* Left */
      if (cursor_pos > 0) {
        screen_move_cursor(-1, 0);
        cursor_pos--;
      }
      break;
    case 0x4D: /* Right */ {
      int len = strlen(key_buffer);
      if (cursor_pos < len) {
        screen_move_cursor(1, 0);
        cursor_pos++;
      }
      break;
    }
    case 0x48: /* Up */
      screen_move_cursor(0, -1);
      break;
    case 0x50: /* Down */
      screen_move_cursor(0, 1);
      break;
    default:
      break;
    }
    extended = 0;
    return;
  }

  if (scancode > SC_MAX)
    return;
  if (scancode == BACKSPACE) {
    backspace(key_buffer);
    kprint_backspace();
  } else if (scancode == ENTER) {
    /* The line is complete: park it for the shell task instead of
     * running it from IRQ context. key_buffer stays intact; the shell
     * clears it via input_consume() after executing the line. */
    input_ready = 1;
    task_wake(0);  /* only a completed line wakes the shell */
    cursor_pos = 0;
  } else {
    char letter = sc_ascii[(int)scancode];
    if (letter == '?')
      return; /* ignore non-printables */
    if (shift_down)
      letter = apply_shift(letter);
    int len = strlen(key_buffer);
    if (cursor_pos < len) {
      /* INSERT mode: shift tail right and redraw */
      if (len < (int)sizeof(key_buffer) - 1) {
        for (int i = len; i >= cursor_pos; --i)
          key_buffer[i + 1] = key_buffer[i];
        key_buffer[cursor_pos] = letter;
        int start_off = screen_get_cursor_offset();
        kprint(key_buffer + cursor_pos); /* prints inserted char + tail */
        screen_set_cursor_offset(start_off +
                                 2); /* place caret after inserted char */
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
}

static void keyboard_callback(registers_t regs) {
  /* Drain every scancode waiting in the 8042 output buffer:
   * the IRQ line stays high while bytes are pending, so a single IRQ
   * can be our only chance to read back-to-back make/break pairs.
   * The shell is woken by process_scancode only when a line completes. */
  while (port_byte_in(0x64) & 0x01) {
    process_scancode(port_byte_in(0x60));
  }
  UNUSED(regs);
}

void init_keyboard() { register_interrupt_handler(IRQ1, keyboard_callback); }

/* Shell task interface: a full line is waiting / ready to be consumed. */
int input_pending(void) {
    return input_ready;
}

char *input_line(void) {
    return key_buffer;
}

void input_consume(void) {
    input_ready = 0;
    key_buffer[0] = '\0';
    cursor_pos = 0;
}
