# Toolchain prefix: defaults to aarch64-linux-gnu-, can be overridden with aarch64-none-elf-
CROSS_COMPILE ?= aarch64-linux-gnu-

CC      := $(CROSS_COMPILE)gcc
LD      := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump

CFLAGS  := -Wall -Wextra -O2 -ffreestanding -nostdinc -nostdlib -nostartfiles -mgeneral-regs-only -Iinclude
ASFLAGS := -Wall -Iinclude
LDFLAGS := --no-warn-rwx-segments

SRCS_S  := $(wildcard src/*.S)
SRCS_C  := $(wildcard src/*.c)
OBJS    := $(SRCS_S:.S=.o) $(SRCS_C:.c=.o)

TARGET  := kernel8.elf
BINARY  := kernel8.img

all: $(TARGET) $(BINARY)

%.o: %.S
	$(CC) $(ASFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): linker.ld $(OBJS)
	$(LD) $(LDFLAGS) -T linker.ld -o $@ $(OBJS)

$(BINARY): $(TARGET)
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f src/*.o $(TARGET) $(BINARY)

run: $(TARGET)
	@if [ -n "$$DISPLAY" ] && xset q >/dev/null 2>&1; then \
		echo "[*] Starting QEMU with graphical window..."; \
		qemu-system-aarch64 -M virt -cpu cortex-a57 -m 128M -serial mon:stdio -device ramfb -device virtio-keyboard-device -device virtio-tablet-device -kernel $(TARGET); \
	else \
		echo "[*] No graphical display detected. Starting QEMU with VNC (:0 on port 5900) & Terminal console..."; \
		echo "[*] Connect any VNC viewer to localhost:5900 to see the graphical desktop!"; \
		qemu-system-aarch64 -M virt -cpu cortex-a57 -m 128M -serial mon:stdio -display vnc=:0 -device ramfb -device virtio-keyboard-device -device virtio-tablet-device -kernel $(TARGET); \
	fi

run-gui: $(TARGET)
	qemu-system-aarch64 -M virt -cpu cortex-a57 -m 128M -serial mon:stdio -device ramfb -device virtio-keyboard-device -device virtio-tablet-device -kernel $(TARGET)

run-vnc: $(TARGET)
	qemu-system-aarch64 -M virt -cpu cortex-a57 -m 128M -serial mon:stdio -display vnc=:0 -device ramfb -device virtio-keyboard-device -device virtio-tablet-device -kernel $(TARGET)

run-headless: $(TARGET)
	qemu-system-aarch64 -M virt -cpu cortex-a57 -m 128M -nographic -device ramfb -device virtio-keyboard-device -device virtio-tablet-device -kernel $(TARGET)

.PHONY: all clean run run-gui run-vnc run-headless

