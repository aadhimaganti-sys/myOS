# myOS — AArch64 Bare-Metal Operating System

`myOS` is a self-contained, bootable 64-bit ARM (`AArch64`) bare-metal operating system written from scratch in C and ARMv8-A assembly for the QEMU `virt` machine (`cortex-a57`).

---

## Key Features

- **Bare-Metal AArch64 Boot & MMU**: Drops cleanly into `EL1`, sets up a 4-level identity-mapped page table (`TTBR0_EL1`), enables caches, and installs a full 16-entry Exception Vector Table (`VBAR_EL1`).
- **Interrupts & Preemptive Multitasking**: GICv2 interrupt controller and ARM Generic Timer (`100 Hz` tick) driving a round-robin preemptive/cooperative kernel thread scheduler (`cpu_switch_to`).
- **Memory Management**: 4 KB physical page frame allocator (`alloc_page` / `free_page`) and fine-grained heap allocator (`kmalloc` / `kzalloc` / `kfree`) with block coalescing.
- **Storage & Filesystem**:
  - **VirtIO Block Driver (`virtio-blk`)**: Persistent sector read/write backed by `disk.img`.
  - **RAMFS**: In-memory read/write virtual filesystem pre-populated with system files and documentation.
- **Graphical Desktop & Window Manager (`ramfb`)**:
  - `1024x768` 32-bit XRGB8888 framebuffer graphics engine (`fb.c`), desktop GUI (`gui.c`), and interactive window manager (`wm.c`).
  - Unified input driver supporting **VirtIO Keyboard**, **VirtIO Tablet/Mouse**, and **PL011 UART** serial input.
  - Press `.` in the terminal or switch interactively to use the graphical desktop environment.
- **KitKoin (`KIT`) Cryptocurrency & P2P Node**:
  - Built-in SHA-256 Proof-of-Work blockchain (`coin.c`, `sha256.c`), multi-wallet management, transaction mempool, block halving, background automining thread, disk persistence, and simulated P2P network protocol (`p2p.c`).
- **Atlas AI & Google Antigravity (`AGY`) Subsystems**:
  - In-kernel multi-agent orchestrator (`agy.c`) and system assistant (`atlas`), plus a host-side neural network assistant in `atlas/`.
- **Interactive CLI Shell**: Full command-line shell with command history, user/wallet management, system diagnostics, filesystem commands, and hardware inspection.

---

## Project File Structure

### Root Directory
| File | Description |
| :--- | :--- |
| `Makefile` | Build system and QEMU launch targets (`all`, `clean`, `run`, `run-gui`, `run-vnc`, `run-headless`) |
| `linker.ld` | Linker script placing kernel entry `_start` at `0x40080000` and defining `.text`, `.rodata`, `.data`, `.bss`, stack, and heap |
| `run.sh` | Convenience script to build `kernel8.elf`, initialize `disk.img`, and launch QEMU (auto-detecting GUI vs. VNC) |
| `prompt.md` | Original architectural specification for the OS kernel |
| `disk.img` | 16 MB persistent raw disk image attached via VirtIO block device |
| `kernel8.elf` / `kernel8.img` | Compiled ELF binary and flat binary kernel image |

### Kernel Headers (`include/`)
| Header | Description |
| :--- | :--- |
| `include/kernel.h` | Core kernel definitions, low-level system register accessors, and linker symbols |
| `include/types.h` / `stdint.h` / `stddef.h` / `stdarg.h` | Freestanding C standard types and variadic argument macros |
| `include/mmu.h` | 4-level AArch64 MMU page table definitions and setup API |
| `include/irq.h` | Exception vector table, GICv2 distributor/CPU interface registers, and IRQ handlers |
| `include/timer.h` | ARM Generic Timer (`CNTP_TVAL_EL0` / `CNTP_CTL_EL0`) driver interface |
| `include/mm.h` | Physical page allocator and `kmalloc`/`kfree` dynamic heap allocator |
| `include/sched.h` | Task control block (`struct task`), context structs, and thread scheduler APIs |
| `include/syscall.h` | Synchronous `svc #0` system call definitions and dispatcher |
| `include/uart.h` | PL011 UART (`0x09000000`) serial driver interface |
| `include/printf.h` / `string.h` | Standalone `kprintf` / `snprintf` and standard string/memory utilities |
| `include/virtio.h` | VirtIO MMIO transport, `virtio-blk` storage, and `virtio-input` definitions |
| `include/drivers.h` | Hardware driver registry and diagnostic reporting |
| `include/input.h` / `keyboard.h` | Unified keyboard & mouse/tablet input event queues |
| `include/fb.h` | `ramfb` (`qemu-fw-cfg`) 1024x768 32-bit framebuffer graphics primitives |
| `include/gui.h` / `wm.h` | Graphical desktop environment, terminal mirror, and multi-window manager |
| `include/ramfs.h` | In-memory read/write filesystem API |
| `include/shell.h` | Interactive CLI shell interface |
| `include/lang.h` | Built-in language interpreter/scripting support |
| `include/agy.h` | Google Antigravity multi-agent orchestrator definitions |
| `include/coin.h` / `p2p.h` / `sha256.h` | KitKoin (`KIT`) PoW blockchain, P2P network node, and SHA-256 cryptography |

### Kernel Sources (`src/`)
| Source File | Description |
| :--- | :--- |
| `src/boot.S` | AArch64 assembly bootstrap (core parking, EL2->EL1 drop, BSS clear, stack setup) |
| `src/vectors.S` | 16-entry AArch64 exception vector table (`VBAR_EL1`) and context save/restore |
| `src/switch.S` | Callee-saved register context switch (`cpu_switch_to`) |
| `src/kernel.c` | `kernel_main` entry point, subsystem initialization, and boot diagnostics |
| `src/mmu.c` | 4-level MMU identity mapping and cache initialization |
| `src/irq.c` | GICv2 controller setup and synchronous/IRQ exception dispatch |
| `src/timer.c` | System tick timer handler and delay routines |
| `src/mm.c` | Bitmap physical page allocator and doubly-linked heap allocator |
| `src/sched.c` | Round-robin preemptive & cooperative thread scheduler |
| `src/syscall.c` | `svc #0` system call handlers |
| `src/uart.c` | PL011 UART serial I/O driver |
| `src/printf.c` | Formatted printing (`kprintf`, `vsnprintf`) |
| `src/string.c` | Freestanding `memset`, `memcpy`, `strlen`, `strcmp`, `strncpy`, etc. |
| `src/virtio.c` | VirtIO MMIO bus probing, block I/O (`virtio-blk`), and input devices (`virtio-input`) |
| `src/drivers.c` | Device driver diagnostics and inspection |
| `src/input.c` / `src/keyboard.c` | Keyboard and mouse/tablet event handling |
| `src/fb.c` | `ramfb` initialization via QEMU `fw_cfg` DMA and 2D drawing routines |
| `src/gui.c` / `src/wm.c` | Graphical desktop environment, apps, and window manager |
| `src/ramfs.c` | In-memory filesystem implementation |
| `src/shell.c` | Interactive CLI shell and built-in system commands |
| `src/lang.c` | Embedded interpreter / language subsystem |
| `src/agy.c` | Autonomous subagent simulation and task orchestration |
| `src/coin.c` / `src/p2p.c` / `src/sha256.c` | KitKoin blockchain consensus, mining, disk persistence, P2P gossip, and SHA-256 |

### Host-Side Atlas Assistant (`atlas/`)
| File | Description |
| :--- | :--- |
| `atlas/main.py` | Interactive host-side AI companion and code inspector for `myOS` |
| `atlas/neural_net.py` | Pure-NumPy neural network model |
| `atlas/learner.py` | Online learning and memory management |
| `atlas/neural_weights.npz` | Saved neural network weights |
| `atlas/memory.json` | Persistent conversational memory |
| `atlas/run.sh` | Launcher script for the host-side Atlas assistant |
| `atlas/README.md` | Documentation for running Atlas on the host or inside `myOS` |

---

## Building and Running

### Prerequisites
- `aarch64-linux-gnu-gcc` (or `aarch64-none-elf-gcc`)
- `qemu-system-aarch64`

### Quick Start
Run the helper script (automatically builds the kernel, creates `disk.img` if missing, and launches QEMU):
```bash
./run.sh
```
Or use `make`:
```bash
make          # Compile kernel8.elf and kernel8.img
make run      # Run in QEMU (auto-detects X11/Wayland GUI or falls back to VNC :0 on port 5900)
make run-vnc  # Force VNC server on localhost:5900 + serial terminal in stdio
make run-headless # Run strictly in terminal (-nographic)
make clean    # Remove object files and kernel binaries
```

---

## Useful Shell Commands Inside `myOS`

Once booted, you can type commands into the interactive serial/GUI shell:
- `help` — List all available shell commands
- `info` — Inspect CPU registers (`CurrentEL`, `MIDR_EL1`, `SP`), timer, and memory usage
- `gui` (or press `.`) — Launch/refresh the interactive graphical desktop and Window Manager
- `ls`, `cat <file>`, `write <file> <text>`, `touch <file>`, `rm <file>` — Manage files in RAMFS
- `ps`, `tasks`, `yield`, `alloc <bytes>`, `free <addr>` — Process and memory management
- `drivers` — View loaded hardware drivers (`PL011`, `GICv2`, `VirtIO`, `ramfb`)
- `kitkoin` / `kit`, `wallet`, `balance`, `switch` — Use the KitKoin (`KIT`) blockchain, wallets, and miner
- `atlas` — Launch the integrated Atlas OS Assistant (`atlas info`, `atlas mem`, `atlas test`, etc.)
- `agy`, `status`, `agents`, `prompt`, `spawn`, `model` — Interact with the Antigravity multi-agent suite
- `svc` — Trigger a synchronous `svc #0` system call test
- `reboot` — Reset the system via ARM PSCI
