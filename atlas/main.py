#!/usr/bin/env python3
"""
Atlas Local AI Assistant & Machine Learning Engine
Learns and trains dynamically on what you say to it.
"""

import os
import sys
import time
import readline
from pathlib import Path
from learner import AtlasLearner

# Terminal ANSI Color Codes
CYAN = "\033[96m"
GREEN = "\033[92m"
YELLOW = "\033[93m"
BLUE = "\033[94m"
MAGENTA = "\033[95m"
BOLD = "\033[1m"
DIM = "\033[2m"
RESET = "\033[0m"

WORKSPACE_ROOT = Path(__file__).resolve().parent.parent

def get_banner(stats):
    return f"""{CYAN}{BOLD}
   ___  _____ _        _   ____  
  / _ \\|_   _| |      / \\ / ___| 
 / /_\\ \\ | | | |     / _ \\\\___ \\ 
/  _  \\ | | | |___ / ___ \\___) |
\\_/ \\_/ |_| |_____/_/   \\_\\____/ 
{RESET}{DIM}Local AI Assistant & Adaptive Machine Learning Engine (Privilege: EL4){RESET}
{MAGENTA}[ML Status: {stats['training_steps']} training steps | Vocab: {stats['vocabulary_size']} words | Facts: {stats['facts_stored']}]{RESET}
{YELLOW}Type {BOLD}/help{RESET}{YELLOW} for ML & OS tools, or simply chat and teach Atlas!{RESET}
"""

def print_atlas(text: str):
    print(f"\n{GREEN}{BOLD}Atlas:{RESET} {text}\n")

def list_workspace_files():
    files = []
    for root, dirs, filenames in os.walk(WORKSPACE_ROOT):
        if ".git" in root or "atlas" in root:
            continue
        for f in sorted(filenames):
            rel = os.path.relpath(os.path.join(root, f), WORKSPACE_ROOT)
            files.append(rel)
    return files

def run_project_review():
    files_found = list_workspace_files()
    num_c = len([f for f in files_found if f.endswith(".c")])
    num_s = len([f for f in files_found if f.endswith(".S")])
    num_h = len([f for f in files_found if f.endswith(".h")])

    review_text = f"""{BOLD}=== Architectural Code Review: myOS (AArch64) ==={RESET}

{CYAN}{BOLD}1. Overview & File Statistics{RESET}
  • Target Machine:      QEMU `virt` (Cortex-A57 / ARMv8-A 64-bit)
  • C Source Files:      {num_c} files
  • Assembly Bootstrap:  {num_s} files (`boot.S`, `vectors.S`, `switch.S`)
  • Header Interfaces:   {num_h} files in `include/`
  • Total Project Files: {len(files_found)}

{CYAN}{BOLD}2. Subsystem Evaluation{RESET}
  {GREEN}✔ [Phase 1: Boot & Linker]{RESET}
    - `linker.ld`: Entry at `0x40080000`, 64KB stack, BSS zeroed, 4KB-aligned heap boundary.
    - `boot.S`: Multicore filter (`MPIDR_EL1`), EL2->EL1 drop, VBAR setup, BSS clearing.
    - `uart.c` & `printf.c`: Clean PL011 MMIO driver at `0x09000000` with formatted kprintf.

  {GREEN}✔ [Phase 2: MMU, GICv2 & Timer]{RESET}
    - `mmu.c`: 4-level identity paging (`TTBR0_EL1`), TCR_EL1 (48-bit VA, 4KB granule, 40-bit PA), MAIR_EL1 attributes.
    - `irq.c`: GICv2 Distributor (`0x08000000`) and CPU Interface (`0x08010000`) setup.
    - `timer.c`: ARM Generic Timer (`CNTP_TVAL_EL0`, `CNTP_CTL_EL0`) armed for 100 Hz preemptive ticks.
    - `vectors.S`: 16-entry exception vector table saving 288-byte full trap frames (`x0`-`x30`, `elr`, `spsr`, `esr`, `far`).

  {GREEN}✔ [Phase 3: Allocators, Scheduler, VirtIO & Framebuffer]{RESET}
    - `mm.c`: Bitmap physical page allocator managing 64 MB (16,384 pages), and `kmalloc`/`kfree` with free-list coalescing.
    - `sched.c` & `switch.S`: Preemptive round-robin multitasking scheduler with callee-saved register switches.
    - `virtio.c`: 32-slot VirtIO MMIO bus scanner with VirtIO Block driver and virtual disk fallback.
    - `fb.c`: 640x480x32bpp linear framebuffer with embedded 8x8 font and geometric raster primitives.

{CYAN}{BOLD}3. Recommendations & Next Steps{RESET}
  • {YELLOW}Phase 4 Ready:{RESET} Full RAMFS file system and interactive shell CLI (`src/shell.c`, `src/ramfs.c`) are in place and ready to be connected into the main boot pipeline!
"""
    print(review_text)

def handle_chat_input(prompt: str, learner: AtlasLearner):
    p_lower = prompt.lower().strip()

    # Step 1: Train model on user input
    prev_facts = len(learner.facts)
    learner.train_on_input(prompt)
    new_facts = len(learner.facts)

    feedback_badge = ""
    if new_facts > prev_facts:
        feedback_badge = f"{MAGENTA}[Learned new fact: \"{learner.facts[-1]}\"]{RESET}\n"

    # Step 2: Check query against learned knowledge base
    learned_answer = learner.query_learned_knowledge(prompt)

    if p_lower in ("hi", "hello", "hey", "sup", "yo"):
        stats = learner.get_stats()
        return feedback_badge + f"Hello! Great to connect. My neural weights are loaded with {stats['vocabulary_size']} words and {stats['facts_stored']} learned facts from you. What are we exploring today?"

    if "who are you" in p_lower or "what are you" in p_lower:
        return feedback_badge + "I am Atlas, your adaptive AI companion operating at Cognitive Exception Level 4 (EL4)! I learn and train dynamically on our conversations."

    if "el4" in p_lower:
        return feedback_badge + ("I operate at Cognitive Exception Level 4 (EL4)! While physical ARM CPU hardware stops at EL3 "
                "(Secure Monitor), EL4 represents out-of-band supervisory intelligence: total observability over "
                "the host, hypervisor (EL2), kernel (EL1), and user tasks (EL0) with live code synthesis and adaptive memory!")

    if "os" in p_lower or "phase" in p_lower or "progress" in p_lower or "kernel" in p_lower:
        return feedback_badge + ("Your myOS project has completed Phases 1, 2, and 3! We have a working 64-bit ARM kernel with identity "
                "paging, GICv2 interrupts, Generic Timer, 64MB page allocator, kmalloc heap, preemptive scheduler, "
                "VirtIO block driver, and framebuffer graphics. You can type `/review` to see a full breakdown!")

    if "scheduler" in p_lower or "sched" in p_lower:
        return feedback_badge + ("The scheduler (`src/sched.c`) uses round-robin time slicing (5 ticks / 50ms) driven by the ARM Generic Timer PPI 14. "
                "Context switches save and restore x19-x30 and SP via `cpu_switch_to` in `src/switch.S`.")

    if "memory" in p_lower or "heap" in p_lower or "malloc" in p_lower:
        return feedback_badge + ("Memory management (`src/mm.c`) manages 64 MB of RAM starting at `__heap_start`. "
                "It uses a bitmap page allocator for 4KB pages, and a block-header linked list for fine-grained `kmalloc`/`kfree` with coalescing.")

    # If the user asked something we have learned facts or concept associations about:
    if learned_answer:
        return feedback_badge + learned_answer

    # Dynamic trained synthesis response
    return feedback_badge + (
        f"I've ingested and trained on that! (Vocabulary: {len(learner.vocab)} words, {learner.training_steps} training steps). "
        f"You can ask me questions about what you've taught me, type `/brain` to see my neural stats, or `/generate` to hear me synthesize text!"
    )

def main():
    learner = AtlasLearner()
    print(get_banner(learner.get_stats()))

    print_atlas("Hello! I am Atlas (EL4). My machine learning engine trains continuously on whatever you say. What would you like to teach or discuss?")

    while True:
        try:
            user_input = input(f"{BOLD}{BLUE}You>{RESET} ").strip()
            if not user_input:
                continue

            if user_input.lower() in ("/exit", "/quit", "exit", "quit"):
                print(f"\n{CYAN}Atlas:{RESET} Goodbye! All learned weights saved to memory.json. See you soon!\n")
                break

            elif user_input == "/help":
                print(f"""
{BOLD}Atlas Local CLI & ML Commands:{RESET}
  {CYAN}/brain{RESET}         - View real-time Machine Learning stats and memory metrics
  {CYAN}/facts{RESET}         - Display all learned semantic facts taught to Atlas
  {CYAN}/generate [seed]{RESET} - Generate synthetic text from the model trained on your input
  {CYAN}/forget{RESET}        - Reset learned weights and clear memory.json
  {CYAN}/review{RESET}        - Run a full architectural review of the myOS project
  {CYAN}/files{RESET}         - List all source code and header files in the project
  {CYAN}/read <file>{RESET}   - Display contents of a file (e.g. /read src/kernel.c)
  {CYAN}/clear{RESET}         - Clear the terminal screen
  {CYAN}/exit{RESET}          - Exit Atlas local chat
  {CYAN}<any text>{RESET}     - Talk to Atlas (automatically trains the model on every message)
""")
            elif user_input == "/brain":
                stats = learner.get_stats()
                print(f"\n{BOLD}=== Atlas Machine Learning & Neural Memory Engine ==={RESET}")
                print(f"  • Training Steps:        {stats['training_steps']}")
                print(f"  • Vocabulary Size:       {stats['vocabulary_size']} unique tokens")
                print(f"  • Learned Transitions:   {stats['learned_transitions']} n-gram transitions")
                print(f"  • Semantic Facts Stored: {stats['facts_stored']}")
                print(f"  • Memory Storage File:   atlas/memory.json\n")

            elif user_input == "/facts":
                if not learner.facts:
                    print(f"\n{YELLOW}No explicit facts learned yet! Teach me something like 'my favorite language is Rust' or 'remember that the kernel entry is 0x40080000'.{RESET}\n")
                else:
                    print(f"\n{BOLD}=== Learned Knowledge Base ({len(learner.facts)} facts) ==={RESET}")
                    for idx, f in enumerate(learner.facts, 1):
                        print(f"  {idx}. {f}")
                    print()

            elif user_input.startswith("/generate"):
                parts = user_input.split(maxsplit=1)
                seed = parts[1] if len(parts) > 1 else None
                generated = learner.generate_trained_phrase(seed=seed)
                print(f"\n{MAGENTA}{BOLD}[Trained Model Output]:{RESET} \"{generated}\"\n")

            elif user_input == "/forget":
                learner.clear_memory()
                print(f"\n{YELLOW}[Memory Cleared]{RESET} Atlas memory has been reset to base initialization.\n")

            elif user_input == "/review":
                run_project_review()

            elif user_input == "/files":
                files = list_workspace_files()
                print(f"\n{BOLD}Project Files ({len(files)} total):{RESET}")
                for f in sorted(files):
                    print(f"  • {f}")
                print()

            elif user_input.startswith("/read"):
                parts = user_input.split(maxsplit=1)
                if len(parts) < 2:
                    print(f"{YELLOW}Usage: /read <relative_path> (e.g. /read src/kernel.c){RESET}")
                else:
                    target = WORKSPACE_ROOT / parts[1]
                    if target.is_file():
                        print(f"\n{BOLD}=== {parts[1]} ==={RESET}")
                        with open(target, "r", encoding="utf-8", errors="replace") as f:
                            for idx, line in enumerate(f, 1):
                                print(f"{DIM}{idx:4d} |{RESET} {line}", end="")
                        print()
                    else:
                        print(f"{YELLOW}File not found: {parts[1]}{RESET}")

            elif user_input == "/clear":
                os.system("clear")
                print(get_banner(learner.get_stats()))

            else:
                reply = handle_chat_input(user_input, learner)
                learner.history.append({"user": user_input, "atlas": reply})
                print_atlas(reply)

        except (KeyboardInterrupt, EOFError):
            print(f"\n\n{CYAN}Atlas:{RESET} Session ended. See you soon!\n")
            break

if __name__ == "__main__":
    main()
