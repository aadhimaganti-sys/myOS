#include "keyboard.h"
#include "input.h"
#include "uart.h"
#include "printf.h"
#include "string.h"

#define KEY_QUEUE_SIZE 256

static struct key_event key_queue[KEY_QUEUE_SIZE];
static int queue_head = 0;
static int queue_tail = 0;

static uint8_t current_modifiers = 0;

/* UART Escape Sequence Parser State */
static int uart_esc_state = 0;
static char uart_esc_buf[8];
static int uart_esc_idx = 0;

static void queue_push(struct key_event *ev) {
    int next = (queue_tail + 1) % KEY_QUEUE_SIZE;
    if (next != queue_head) {
        key_queue[queue_tail] = *ev;
        queue_tail = next;
    }
}

static int queue_pop(struct key_event *ev) {
    if (queue_head == queue_tail) {
        return 0;
    }
    *ev = key_queue[queue_head];
    queue_head = (queue_head + 1) % KEY_QUEUE_SIZE;
    return 1;
}

uint8_t keyboard_get_modifiers(void) {
    return current_modifiers;
}

const char *keyboard_key_name(uint16_t scancode) {
    switch (scancode) {
        case KEY_ESC:       return "Escape";
        case KEY_ENTER:     return "Enter";
        case KEY_BACKSPACE: return "Backspace";
        case KEY_TAB:       return "Tab";
        case KEY_SPACE:     return "Space";
        case KEY_LEFTSHIFT: return "LShift";
        case KEY_RIGHTSHIFT:return "RShift";
        case KEY_LEFTCTRL:  return "LCtrl";
        case KEY_LEFTALT:   return "LAlt";
        case KEY_CAPSLOCK:  return "CapsLock";
        case KEY_UP:        return "Up";
        case KEY_DOWN:      return "Down";
        case KEY_LEFT:      return "Left";
        case KEY_RIGHT:     return "Right";
        case KEY_DELETE:    return "Delete";
        case KEY_F1:        return "F1";
        case KEY_F2:        return "F2";
        case KEY_F3:        return "F3";
        case KEY_F4:        return "F4";
        case KEY_F5:        return "F5";
        case KEY_F6:        return "F6";
        case KEY_F7:        return "F7";
        case KEY_F8:        return "F8";
        case KEY_F9:        return "F9";
        case KEY_F10:       return "F10";
        case KEY_A: return "A"; case KEY_B: return "B"; case KEY_C: return "C";
        case KEY_D: return "D"; case KEY_E: return "E"; case KEY_F: return "F";
        case KEY_G: return "G"; case KEY_H: return "H"; case KEY_I: return "I";
        case KEY_J: return "J"; case KEY_K: return "K"; case KEY_L: return "L";
        case KEY_M: return "M"; case KEY_N: return "N"; case KEY_O: return "O";
        case KEY_P: return "P"; case KEY_Q: return "Q"; case KEY_R: return "R";
        case KEY_S: return "S"; case KEY_T: return "T"; case KEY_U: return "U";
        case KEY_V: return "V"; case KEY_W: return "W"; case KEY_X: return "X";
        case KEY_Y: return "Y"; case KEY_Z: return "Z";
        case KEY_0: return "0"; case KEY_1: return "1"; case KEY_2: return "2";
        case KEY_3: return "3"; case KEY_4: return "4"; case KEY_5: return "5";
        case KEY_6: return "6"; case KEY_7: return "7"; case KEY_8: return "8";
        case KEY_9: return "9";
        default: return "Key";
    }
}

/* Scancode to ASCII tables for VirtIO hardware events */
static const char key_to_ascii_norm[128] = {
    [KEY_ESC] = 27,
    [KEY_1] = '1', [KEY_2] = '2', [KEY_3] = '3', [KEY_4] = '4', [KEY_5] = '5',
    [KEY_6] = '6', [KEY_7] = '7', [KEY_8] = '8', [KEY_9] = '9', [KEY_0] = '0',
    [KEY_MINUS] = '-', [KEY_EQUAL] = '=', [KEY_BACKSPACE] = '\b', [KEY_TAB] = '\t',
    [KEY_Q] = 'q', [KEY_W] = 'w', [KEY_E] = 'e', [KEY_R] = 'r', [KEY_T] = 't',
    [KEY_Y] = 'y', [KEY_U] = 'u', [KEY_I] = 'i', [KEY_O] = 'o', [KEY_P] = 'p',
    [KEY_LEFTBRACE] = '[', [KEY_RIGHTBRACE] = ']', [KEY_ENTER] = '\n',
    [KEY_A] = 'a', [KEY_S] = 's', [KEY_D] = 'd', [KEY_F] = 'f', [KEY_G] = 'g',
    [KEY_H] = 'h', [KEY_J] = 'j', [KEY_K] = 'k', [KEY_L] = 'l',
    [KEY_SEMICOLON] = ';', [KEY_APOSTROPHE] = '\'', [KEY_GRAVE] = '`',
    [KEY_BACKSLASH] = '\\',
    [KEY_Z] = 'z', [KEY_X] = 'x', [KEY_C] = 'c', [KEY_V] = 'v', [KEY_B] = 'b',
    [KEY_N] = 'n', [KEY_M] = 'm', [KEY_COMMA] = ',', [KEY_DOT] = '.', [KEY_SLASH] = '/',
    [KEY_SPACE] = ' '
};

static const char key_to_ascii_shift[128] = {
    [KEY_ESC] = 27,
    [KEY_1] = '!', [KEY_2] = '@', [KEY_3] = '#', [KEY_4] = '$', [KEY_5] = '%',
    [KEY_6] = '^', [KEY_7] = '&', [KEY_8] = '*', [KEY_9] = '(', [KEY_0] = ')',
    [KEY_MINUS] = '_', [KEY_EQUAL] = '+', [KEY_BACKSPACE] = '\b', [KEY_TAB] = '\t',
    [KEY_Q] = 'Q', [KEY_W] = 'W', [KEY_E] = 'E', [KEY_R] = 'R', [KEY_T] = 'T',
    [KEY_Y] = 'Y', [KEY_U] = 'U', [KEY_I] = 'I', [KEY_O] = 'O', [KEY_P] = 'P',
    [KEY_LEFTBRACE] = '{', [KEY_RIGHTBRACE] = '}', [KEY_ENTER] = '\n',
    [KEY_A] = 'A', [KEY_S] = 'S', [KEY_D] = 'D', [KEY_F] = 'F', [KEY_G] = 'G',
    [KEY_H] = 'H', [KEY_J] = 'J', [KEY_K] = 'K', [KEY_L] = 'L',
    [KEY_SEMICOLON] = ':', [KEY_APOSTROPHE] = '"', [KEY_GRAVE] = '~',
    [KEY_BACKSLASH] = '|',
    [KEY_Z] = 'Z', [KEY_X] = 'X', [KEY_C] = 'C', [KEY_V] = 'V', [KEY_B] = 'B',
    [KEY_N] = 'N', [KEY_M] = 'M', [KEY_COMMA] = '<', [KEY_DOT] = '>', [KEY_SLASH] = '?',
    [KEY_SPACE] = ' '
};

void keyboard_feed_char(char c) {
    uint16_t scancode = 0;

    if (c >= 'a' && c <= 'z') {
        scancode = KEY_A + (c - 'a');
    } else if (c >= 'A' && c <= 'Z') {
        scancode = KEY_A + (c - 'A');
        current_modifiers |= MOD_SHIFT;
    } else if (c >= '0' && c <= '9') {
        scancode = (c == '0') ? KEY_0 : (KEY_1 + (c - '1'));
    } else if (c == '\r' || c == '\n') {
        scancode = KEY_ENTER;
        c = '\n';
    } else if (c == '\b' || c == 0x7F) {
        scancode = KEY_BACKSPACE;
        c = '\b';
    } else if (c == '\t') {
        scancode = KEY_TAB;
    } else if (c == ' ') {
        scancode = KEY_SPACE;
    } else if (c == 27) {
        scancode = KEY_ESC;
    }

    struct key_event ev;
    ev.scancode = scancode;
    ev.pressed = 1;
    ev.modifiers = current_modifiers;
    ev.ascii = c;
    queue_push(&ev);

    ev.pressed = 0;
    queue_push(&ev);

    if (c >= 'A' && c <= 'Z') {
        current_modifiers &= ~MOD_SHIFT;
    }
}

void keyboard_feed_event(uint16_t scancode, uint8_t pressed, char ascii) {
    struct key_event ev;
    ev.scancode = scancode;
    ev.pressed = pressed;
    ev.modifiers = current_modifiers;
    ev.ascii = ascii;
    queue_push(&ev);
}

void keyboard_init(void) {
    queue_head = 0;
    queue_tail = 0;
    current_modifiers = 0;
    uart_esc_state = 0;
    uart_esc_idx = 0;

    /* Initialize VirtIO Input device driver */
    input_init();

    kprintf("[+] Unified Keyboard Driver initialized (VirtIO Input + UART Console)\n");
}

/* Parse ANSI escape sequences arriving from UART */
static void process_uart_char(char c) {
    if (uart_esc_state == 0) {
        if (c == 27) { /* ESC */
            uart_esc_state = 1;
            uart_esc_idx = 0;
            return;
        }
        keyboard_feed_char(c);
        return;
    } else if (uart_esc_state == 1) {
        if (c == '[') {
            uart_esc_state = 2; /* CSI sequence */
            return;
        } else if (c == 'O') {
            uart_esc_state = 3; /* SS3 sequence, e.g. F1-F4 */
            return;
        } else {
            /* Bare ESC followed by character */
            keyboard_feed_event(KEY_ESC, 1, 27);
            keyboard_feed_event(KEY_ESC, 0, 0);
            uart_esc_state = 0;
            keyboard_feed_char(c);
            return;
        }
    } else if (uart_esc_state == 2) {
        /* CSI sequence: check terminator */
        if (c == 'A') {
            keyboard_feed_event(KEY_UP, 1, 0);
            keyboard_feed_event(KEY_UP, 0, 0);
            uart_esc_state = 0;
        } else if (c == 'B') {
            keyboard_feed_event(KEY_DOWN, 1, 0);
            keyboard_feed_event(KEY_DOWN, 0, 0);
            uart_esc_state = 0;
        } else if (c == 'C') {
            keyboard_feed_event(KEY_RIGHT, 1, 0);
            keyboard_feed_event(KEY_RIGHT, 0, 0);
            uart_esc_state = 0;
        } else if (c == 'D') {
            keyboard_feed_event(KEY_LEFT, 1, 0);
            keyboard_feed_event(KEY_LEFT, 0, 0);
            uart_esc_state = 0;
        } else if (c >= '0' && c <= '9') {
            if (uart_esc_idx < (int)sizeof(uart_esc_buf) - 1) {
                uart_esc_buf[uart_esc_idx++] = c;
            }
        } else if (c == '~') {
            uart_esc_buf[uart_esc_idx] = '\0';
            if (strcmp(uart_esc_buf, "15") == 0) {
                keyboard_feed_event(KEY_F5, 1, 0);
                keyboard_feed_event(KEY_F5, 0, 0);
            } else if (strcmp(uart_esc_buf, "3") == 0) {
                keyboard_feed_event(KEY_DELETE, 1, 0);
                keyboard_feed_event(KEY_DELETE, 0, 0);
            }
            uart_esc_state = 0;
        } else {
            uart_esc_state = 0;
        }
    } else if (uart_esc_state == 3) {
        /* SS3 sequence: F1-F4 */
        if (c == 'P') {
            keyboard_feed_event(KEY_F1, 1, 0);
            keyboard_feed_event(KEY_F1, 0, 0);
        } else if (c == 'Q') {
            keyboard_feed_event(KEY_F2, 1, 0);
            keyboard_feed_event(KEY_F2, 0, 0);
        } else if (c == 'R') {
            keyboard_feed_event(KEY_F3, 1, 0);
            keyboard_feed_event(KEY_F3, 0, 0);
        } else if (c == 'S') {
            keyboard_feed_event(KEY_F4, 1, 0);
            keyboard_feed_event(KEY_F4, 0, 0);
        }
        uart_esc_state = 0;
    }
}

int keyboard_poll(struct key_event *ev) {
    /* 1. Poll VirtIO Hardware Keyboard events first */
    struct virtio_input_event raw_ev;
    while (input_get_key_event(&raw_ev)) {
        uint16_t scancode = raw_ev.code;
        uint8_t pressed = (raw_ev.value != 0);

        /* Update Modifiers */
        if (scancode == KEY_LEFTSHIFT || scancode == KEY_RIGHTSHIFT) {
            if (pressed) current_modifiers |= MOD_SHIFT;
            else current_modifiers &= ~MOD_SHIFT;
        } else if (scancode == KEY_LEFTCTRL) {
            if (pressed) current_modifiers |= MOD_CTRL;
            else current_modifiers &= ~MOD_CTRL;
        } else if (scancode == KEY_LEFTALT) {
            if (pressed) current_modifiers |= MOD_ALT;
            else current_modifiers &= ~MOD_ALT;
        } else if (scancode == KEY_CAPSLOCK && pressed) {
            current_modifiers ^= MOD_CAPS;
        }

        /* Determine ASCII translation */
        char ascii = 0;
        if (pressed && scancode < 128) {
            int shift = (current_modifiers & MOD_SHIFT) != 0;
            if ((current_modifiers & MOD_CAPS) && (scancode >= KEY_A && scancode <= KEY_Z)) {
                shift = !shift;
            }
            ascii = shift ? key_to_ascii_shift[scancode] : key_to_ascii_norm[scancode];
        }

        struct key_event kev;
        kev.scancode = scancode;
        kev.pressed = pressed;
        kev.modifiers = current_modifiers;
        kev.ascii = ascii;
        queue_push(&kev);
    }

    /* 2. Poll UART Console Keyboard events */
    char c_uart;
    while (uart_getc_nonblock(&c_uart)) {
        process_uart_char(c_uart);
    }

    return queue_pop(ev);
}

int keyboard_has_char(void) {
    struct key_event dummy;
    if (keyboard_poll(&dummy)) {
        queue_push(&dummy);
        return 1;
    }
    return 0;
}

char keyboard_getchar(void) {
    struct key_event ev;
    while (1) {
        if (keyboard_poll(&ev)) {
            if (ev.pressed && ev.ascii != 0) {
                return ev.ascii;
            }
        }
        __asm__ volatile("wfe");
    }
}
