#include "input.h"
#include "virtio.h"
#include "printf.h"
#include "string.h"
#include "mm.h"

#define VIRTIO_REG(base, offset) (*(volatile uint32_t *)((base) + (offset)))
#define VIRTIO_REG64(base, offset) (*(volatile uint64_t *)((base) + (offset)))

#define MAX_INPUT_DEVICES 4
#define INPUT_QUEUE_SIZE  16
#define KBD_BUFFER_SIZE   256

struct virtio_input_dev {
    int active;
    uintptr_t base;
    char name[128];
    uint32_t qnum;
    struct vring_desc *desc_table;
    struct vring_avail *avail_ring;
    struct vring_used *used_ring;
    struct virtio_input_event *events;
    uint16_t last_used_idx;
};

static struct virtio_input_dev input_devices[MAX_INPUT_DEVICES];
static int num_input_devices = 0;
static int input_subsystem_initialized = 0;

/* Mouse state */
static int mouse_x = 320;
static int mouse_y = 240;
static int mouse_buttons = 0;

/* Keyboard state */
static int shift_state = 0;
static char kbd_buffer[KBD_BUFFER_SIZE];
static volatile uint32_t kbd_head = 0;
static volatile uint32_t kbd_tail = 0;

/* Raw VirtIO key event buffer */
#define RAW_EVENT_BUFFER_SIZE 128
static struct virtio_input_event raw_event_buffer[RAW_EVENT_BUFFER_SIZE];
static volatile uint32_t raw_head = 0;
static volatile uint32_t raw_tail = 0;

static void raw_event_push(struct virtio_input_event *ev) {
    uint32_t next = (raw_head + 1) % RAW_EVENT_BUFFER_SIZE;
    if (next != raw_tail) {
        raw_event_buffer[raw_head] = *ev;
        __asm__ volatile("dmb sy" ::: "memory");
        raw_head = next;
    }
}

/* Scancode to ASCII conversion tables */
static const char keymap_ascii[128] = {
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
    [KEY_SPACE] = ' ',
    [KEY_KPASTERISK] = '*',
    [71] = '7', [72] = '8', [73] = '9', [74] = '-',
    [75] = '4', [76] = '5', [77] = '6', [78] = '+',
    [79] = '1', [80] = '2', [81] = '3', [82] = '0', [83] = '.',
    [96] = '\n', [98] = '/'
};

static const char keymap_ascii_shift[128] = {
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
    [KEY_SPACE] = ' ',
    [KEY_KPASTERISK] = '*',
    [71] = '7', [72] = '8', [73] = '9', [74] = '-',
    [75] = '4', [76] = '5', [77] = '6', [78] = '+',
    [79] = '1', [80] = '2', [81] = '3', [82] = '0', [83] = '.',
    [96] = '\n', [98] = '/'
};

static char scancode_to_ascii(uint16_t code, int shift) {
    if (code >= 128) {
        return 0;
    }
    return shift ? keymap_ascii_shift[code] : keymap_ascii[code];
}

static void kbd_push(char c) {
    uint32_t next = (kbd_head + 1) % KBD_BUFFER_SIZE;
    if (next != kbd_tail) {
        kbd_buffer[kbd_head] = c;
        __asm__ volatile("dmb sy" ::: "memory");
        kbd_head = next;
    }
}

void input_init(void) {
    if (input_subsystem_initialized) {
        return;
    }
    input_subsystem_initialized = 1;
    num_input_devices = 0;
    mouse_x = 320;
    mouse_y = 240;
    mouse_buttons = 0;
    shift_state = 0;
    kbd_head = 0;
    kbd_tail = 0;
    raw_head = 0;
    raw_tail = 0;
    memset(input_devices, 0, sizeof(input_devices));
    memset(kbd_buffer, 0, sizeof(kbd_buffer));
    memset(raw_event_buffer, 0, sizeof(raw_event_buffer));
}

void input_register_device(uintptr_t base) {
    if (!input_subsystem_initialized) {
        input_init();
    }

    if (num_input_devices >= MAX_INPUT_DEVICES) {
        kprintf("[VIRTIO:INPUT] Max input devices (%d) reached! Cannot register 0x%08lx\n",
                MAX_INPUT_DEVICES, base);
        return;
    }

    /* 1. Reset device */
    VIRTIO_REG(base, VIRTIO_MMIO_STATUS) = 0;
    __asm__ volatile("dmb sy" ::: "memory");

    /* 2. Set ACKNOWLEDGE bit */
    VIRTIO_REG(base, VIRTIO_MMIO_STATUS) |= VIRTIO_STATUS_ACKNOWLEDGE;

    /* 3. Set DRIVER bit */
    VIRTIO_REG(base, VIRTIO_MMIO_STATUS) |= VIRTIO_STATUS_DRIVER;

    /* 4. Feature negotiation (accept default baseline) */
    VIRTIO_REG(base, VIRTIO_MMIO_DRIVER_FEATURES_SEL) = 0;
    VIRTIO_REG(base, VIRTIO_MMIO_DRIVER_FEATURES) = 0;
    VIRTIO_REG(base, VIRTIO_MMIO_STATUS) |= VIRTIO_STATUS_FEATURES_OK;

    if (!(VIRTIO_REG(base, VIRTIO_MMIO_STATUS) & VIRTIO_STATUS_FEATURES_OK)) {
        kprintf("[VIRTIO:INPUT] Features negotiation failed on device at 0x%08lx!\n", base);
        VIRTIO_REG(base, VIRTIO_MMIO_STATUS) |= VIRTIO_STATUS_FAILED;
        return;
    }

    /* 5. Query name from config space at base + 0x100 (select = 1, read name string at base + 0x108) */
    *(volatile uint8_t *)(base + VIRTIO_MMIO_CONFIG) = VIRTIO_INPUT_CFG_ID_NAME;
    *(volatile uint8_t *)(base + VIRTIO_MMIO_CONFIG + 1) = 0;
    __asm__ volatile("dmb sy" ::: "memory");

    uint8_t size = *(volatile uint8_t *)(base + VIRTIO_MMIO_CONFIG + 2);
    char name_buf[128];
    memset(name_buf, 0, sizeof(name_buf));
    volatile char *name_src = (volatile char *)(base + VIRTIO_MMIO_CONFIG + 8);
    int read_len = (size > 0 && size < 128) ? size : 127;
    for (int i = 0; i < read_len; i++) {
        char ch = name_src[i];
        if (ch == '\0') break;
        name_buf[i] = ch;
    }
    name_buf[127] = '\0';
    if (name_buf[0] == '\0') {
        strncpy(name_buf, "VirtIO Input Device", sizeof(name_buf) - 1);
    }
    kprintf("    [HW] Initialized VirtIO Input Device: \"%s\"\n", name_buf);

    /* 6. Configure Queue 0 (eventq) */
    VIRTIO_REG(base, VIRTIO_MMIO_QUEUE_SEL) = 0;
    uint32_t qmax = VIRTIO_REG(base, VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (qmax == 0) {
        kprintf("[VIRTIO:INPUT] Queue 0 not available on device 0x%08lx!\n", base);
        VIRTIO_REG(base, VIRTIO_MMIO_STATUS) |= VIRTIO_STATUS_FAILED;
        return;
    }

    uint32_t qnum = INPUT_QUEUE_SIZE;
    if (qmax < INPUT_QUEUE_SIZE) {
        qnum = qmax;
    }
    VIRTIO_REG(base, VIRTIO_MMIO_QUEUE_NUM) = qnum;

    /* Allocate physically contiguous page for descriptors, avail ring, used ring, and events */
    void *queue_mem = alloc_page();
    if (!queue_mem) {
        kprintf("[VIRTIO:INPUT] Failed to allocate queue page!\n");
        VIRTIO_REG(base, VIRTIO_MMIO_STATUS) |= VIRTIO_STATUS_FAILED;
        return;
    }
    memset(queue_mem, 0, PAGE_SIZE);

    struct vring_desc *desc = (struct vring_desc *)queue_mem;
    struct vring_avail *avail = (struct vring_avail *)((uintptr_t)queue_mem + (qnum * sizeof(struct vring_desc)));
    uintptr_t used_addr = ((uintptr_t)avail + sizeof(struct vring_avail) + 63) & ~63ULL;
    struct vring_used *used = (struct vring_used *)used_addr;
    uintptr_t events_addr = (used_addr + sizeof(struct vring_used) + 63) & ~63ULL;
    struct virtio_input_event *events = (struct virtio_input_event *)events_addr;

    /* Program queue physical addresses into MMIO registers */
    VIRTIO_REG(base, VIRTIO_MMIO_QUEUE_DESC_LOW) = (uint32_t)((uint64_t)desc);
    VIRTIO_REG(base, VIRTIO_MMIO_QUEUE_DESC_HIGH) = (uint32_t)(((uint64_t)desc) >> 32);

    VIRTIO_REG(base, VIRTIO_MMIO_QUEUE_DRIVER_LOW) = (uint32_t)((uint64_t)avail);
    VIRTIO_REG(base, VIRTIO_MMIO_QUEUE_DRIVER_HIGH) = (uint32_t)(((uint64_t)avail) >> 32);

    VIRTIO_REG(base, VIRTIO_MMIO_QUEUE_DEVICE_LOW) = (uint32_t)((uint64_t)used);
    VIRTIO_REG(base, VIRTIO_MMIO_QUEUE_DEVICE_HIGH) = (uint32_t)(((uint64_t)used) >> 32);

    VIRTIO_REG(base, VIRTIO_MMIO_QUEUE_READY) = 1;

    /* Pre-populate Queue 0 with 16 event buffers (VRING_DESC_F_WRITE), add to avail_ring, notify queue */
    for (uint32_t i = 0; i < qnum; i++) {
        desc[i].addr = (uint64_t)&events[i];
        desc[i].len = sizeof(struct virtio_input_event);
        desc[i].flags = VRING_DESC_F_WRITE;
        desc[i].next = 0;
        avail->ring[i] = (uint16_t)i;
    }
    __asm__ volatile("dmb sy" ::: "memory");
    avail->flags = 0;
    avail->idx = (uint16_t)qnum;
    __asm__ volatile("dmb sy" ::: "memory");

    /* Notify queue 0 */
    VIRTIO_REG(base, VIRTIO_MMIO_QUEUE_NOTIFY) = 0;

    /* 7. Set DRIVER_OK */
    VIRTIO_REG(base, VIRTIO_MMIO_STATUS) |= VIRTIO_STATUS_DRIVER_OK;

    /* Store device registration */
    struct virtio_input_dev *dev = &input_devices[num_input_devices];
    dev->base = base;
    strncpy(dev->name, name_buf, sizeof(dev->name) - 1);
    dev->qnum = qnum;
    dev->desc_table = desc;
    dev->avail_ring = avail;
    dev->used_ring = used;
    dev->events = events;
    dev->last_used_idx = 0;
    dev->active = 1;
    num_input_devices++;
}

void input_poll(void) {
    for (int d = 0; d < num_input_devices; d++) {
        struct virtio_input_dev *dev = &input_devices[d];
        if (!dev->active || !dev->used_ring) {
            continue;
        }

        uint32_t recycled = 0;
        while (dev->last_used_idx != dev->used_ring->idx) {
            __asm__ volatile("dmb sy" ::: "memory");
            uint16_t slot = dev->last_used_idx % dev->qnum;
            struct vring_used_elem elem = dev->used_ring->ring[slot];
            uint32_t desc_idx = elem.id;

            if (desc_idx < dev->qnum) {
                struct virtio_input_event ev = dev->events[desc_idx];

                if (ev.type == EV_ABS) {
                    if (ev.code == ABS_X) {
                        int val = (int)(((uint64_t)ev.value * 640ULL) / 32768ULL);
                        if (val < 0) val = 0;
                        if (val > 639) val = 639;
                        mouse_x = val;
                    } else if (ev.code == ABS_Y) {
                        int val = (int)(((uint64_t)ev.value * 480ULL) / 32768ULL);
                        if (val < 0) val = 0;
                        if (val > 479) val = 479;
                        mouse_y = val;
                    }
                } else if (ev.type == EV_REL) {
                    if (ev.code == REL_X) {
                        mouse_x += (int32_t)ev.value;
                        if (mouse_x < 0) mouse_x = 0;
                        if (mouse_x > 639) mouse_x = 639;
                    } else if (ev.code == REL_Y) {
                        mouse_y += (int32_t)ev.value;
                        if (mouse_y < 0) mouse_y = 0;
                        if (mouse_y > 479) mouse_y = 479;
                    }
                } else if (ev.type == EV_KEY) {
                    if (ev.code == BTN_LEFT) {
                        if (ev.value) {
                            mouse_buttons |= (1 << 0);
                        } else {
                            mouse_buttons &= ~(1 << 0);
                        }
                    } else if (ev.code == BTN_RIGHT) {
                        if (ev.value) {
                            mouse_buttons |= (1 << 1);
                        } else {
                            mouse_buttons &= ~(1 << 1);
                        }
                    } else if (ev.code < BTN_LEFT) {
                        /* Push raw key event (preserves scancode, value, modifiers) */
                        raw_event_push(&ev);

                        if (ev.code == KEY_LEFTSHIFT) {
                            if (ev.value == 0) {
                                shift_state &= ~(1 << 0);
                            } else {
                                shift_state |= (1 << 0);
                            }
                        } else if (ev.code == KEY_RIGHTSHIFT) {
                            if (ev.value == 0) {
                                shift_state &= ~(1 << 1);
                            } else {
                                shift_state |= (1 << 1);
                            }
                        } else if (ev.value == 1 || ev.value == 2) {
                            char c = scancode_to_ascii(ev.code, shift_state != 0);
                            if (c) {
                                kbd_push(c);
                            }
                        }
                    }
                }

                /* Recycle processed descriptor back into avail_ring */
                dev->desc_table[desc_idx].addr = (uint64_t)&dev->events[desc_idx];
                dev->desc_table[desc_idx].len = sizeof(struct virtio_input_event);
                dev->desc_table[desc_idx].flags = VRING_DESC_F_WRITE;
                dev->desc_table[desc_idx].next = 0;

                dev->avail_ring->ring[dev->avail_ring->idx % dev->qnum] = (uint16_t)desc_idx;
                __asm__ volatile("dmb sy" ::: "memory");
                dev->avail_ring->idx++;
                recycled++;
            }

            dev->last_used_idx++;
        }

        if (recycled > 0) {
            __asm__ volatile("dmb sy" ::: "memory");
            VIRTIO_REG(dev->base, VIRTIO_MMIO_QUEUE_NOTIFY) = 0;
        }
    }
}

int input_get_mouse(int *x, int *y, int *buttons) {
    input_poll();
    if (x) *x = mouse_x;
    if (y) *y = mouse_y;
    if (buttons) *buttons = mouse_buttons;
    return 1;
}

int input_getc_nonblock(char *c) {
    if (!c) return 0;
    input_poll();
    if (kbd_head == kbd_tail) {
        return 0;
    }
    *c = kbd_buffer[kbd_tail];
    __asm__ volatile("dmb sy" ::: "memory");
    kbd_tail = (kbd_tail + 1) % KBD_BUFFER_SIZE;
    return 1;
}

int input_get_key_event(struct virtio_input_event *ev) {
    if (!ev) return 0;
    input_poll();
    if (raw_head == raw_tail) {
        return 0;
    }
    *ev = raw_event_buffer[raw_tail];
    __asm__ volatile("dmb sy" ::: "memory");
    raw_tail = (raw_tail + 1) % RAW_EVENT_BUFFER_SIZE;
    return 1;
}
