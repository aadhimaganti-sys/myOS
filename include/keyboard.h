#ifndef _KEYBOARD_H
#define _KEYBOARD_H

#include "types.h"
#include "stddef.h"
#include "input.h"

/* Additional Keycodes if not in input.h */
#ifndef KEY_UP
#define KEY_UP       103
#define KEY_LEFT     105
#define KEY_RIGHT    106
#define KEY_DOWN     108
#define KEY_DELETE   111
#endif

#ifndef KEY_CAPSLOCK
#define KEY_CAPSLOCK 58
#endif

#ifndef KEY_F1
#define KEY_F1       59
#define KEY_F2       60
#define KEY_F3       61
#define KEY_F4       62
#define KEY_F5       63
#define KEY_F6       64
#define KEY_F7       65
#define KEY_F8       66
#define KEY_F9       67
#define KEY_F10      68
#endif

/* Keyboard Modifier Flags */
#define MOD_SHIFT    (1 << 0)
#define MOD_CTRL     (1 << 1)
#define MOD_ALT      (1 << 2)
#define MOD_CAPS     (1 << 3)

/* Key Event Structure */
struct key_event {
    uint16_t scancode;
    uint8_t  pressed;    /* 1 = Key Down, 0 = Key Up */
    uint8_t  modifiers;  /* Bitmask of active modifiers */
    char     ascii;      /* Mapped ASCII character, or 0 if non-printable */
};

/* Keyboard Driver APIs */
void keyboard_init(void);
int  keyboard_poll(struct key_event *ev);
char keyboard_getchar(void);
int  keyboard_has_char(void);
uint8_t keyboard_get_modifiers(void);
const char *keyboard_key_name(uint16_t scancode);
void keyboard_feed_char(char c);
void keyboard_feed_event(uint16_t scancode, uint8_t pressed, char ascii);

#endif /* _KEYBOARD_H */
