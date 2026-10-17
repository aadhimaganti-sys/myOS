# Atlas Local Assistant

Atlas is an interactive AI companion and system controller for `myOS`.

## Running Atlas Locally (In Host Terminal)

From the root of this project or the `atlas` directory:
```bash
./atlas/run.sh
```
or
```bash
python3 atlas/main.py
```

### Host Features:
- Interactive chat with conversational memory
- `/review`: Full architectural analysis of the AArch64 bare-metal kernel
- `/files`: Lists project source files and headers
- `/read <file>`: Reads and line-numbers any source file
- Direct bridge for OS experimentation and debugging

## Running Atlas Inside the OS Kernel (myOS)

Atlas is also integrated directly into the `myOS` interactive shell!
When running the kernel in QEMU:
```bash
make run
```
At the shell prompt:
- `atlas`: Launches the interactive Atlas OS Subsystem & Controller
- `atlas info`: Queries hardware and CPU state
- `atlas mem`: Inspects dynamic heap and page frame allocator
- `atlas tasks`: Inspects and manages multitasking threads
- `atlas draw`: Renders graphics to the framebuffer
- `atlas test`: Performs an automated full-system health check
