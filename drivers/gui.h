#ifndef GUI_H
#define GUI_H

/* Minimal desktop environment over the VBE framebuffer: wallpaper,
 * clickable desktop icons, a taskbar (terminal / files / reboot /
 * shutdown), two windows and a PS/2-driven mouse cursor. The kernel
 * shell loop calls gui_update() whenever it wakes so the cursor tracks
 * the mouse between keystrokes. */

/* Boot-time entry: draws the desktop and opens the terminal window. */
void gui_init(void);

int gui_active(void);

/* Pump pending mouse motion + clicks. Call from the shell idle loop. */
void gui_update(void);

/* Redraw dynamic window contents (e.g. the file list after a command). */
void gui_refresh(void);

/* Close every window and return to a bare desktop (LOGOUT). */
void gui_logout(void);

#endif
