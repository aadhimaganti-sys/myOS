#!/bin/bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$DIR"

# Ensure binary is built
if [ ! -f kernel8.elf ]; then
    echo "[*] Building kernel..."
    make
fi

# Ensure disk image exists
if [ ! -f disk.img ]; then
    echo "[*] Creating persistent storage disk.img (16 MB)..."
    dd if=/dev/zero of=disk.img bs=1M count=16 status=none
    printf "myOS-VirtIO-Block-Storage-v1.0" | dd of=disk.img bs=1 count=30 conv=notrunc status=none
fi

# Detect display environment
if [ -n "$DISPLAY" ] && xset q >/dev/null 2>&1; then
    echo "[+] GUI display detected ($DISPLAY). Starting QEMU window..."
    exec qemu-system-aarch64 \
        -M virt \
        -cpu cortex-a57 \
        -m 128M \
        -serial mon:stdio \
        -device ramfb \
        -device virtio-keyboard-device \
        -device virtio-tablet-device \
        -drive file=disk.img,if=none,format=raw,cache=writethrough,file.locking=off,id=hd0 \
        -device virtio-blk-device,drive=hd0 \
        -kernel kernel8.elf
else
    echo "[*] No active graphical X11/Wayland display server detected."
    echo "[*] Starting QEMU with VNC (:0 on port 5900) & Terminal mirror..."
    echo "[*] Connect any VNC viewer to localhost:5900 to interact with the GUI and mouse!"
    echo "------------------------------------------------------------------------"
    exec qemu-system-aarch64 \
        -M virt \
        -cpu cortex-a57 \
        -m 128M \
        -serial mon:stdio \
        -display vnc=:0 \
        -device ramfb \
        -device virtio-keyboard-device \
        -device virtio-tablet-device \
        -drive file=disk.img,if=none,format=raw,cache=writethrough,file.locking=off,id=hd0 \
        -device virtio-blk-device,drive=hd0 \
        -kernel kernel8.elf
fi
