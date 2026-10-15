.section ".text.boot"
.global _start

_start:
    /* Check MPIDR_EL1 to ensure only Core 0 proceeds */
    mrs     x0, mpidr_el1
    and     x0, x0, #0xFF
    cbz     x0, master_core

hang_secondary:
    wfe
    b       hang_secondary

master_core:
    /* Check Current Exception Level */
    mrs     x0, CurrentEL
    lsr     x0, x0, #2
    cmp     x0, #3
    b.eq    from_el3
    cmp     x0, #2
    b.eq    from_el2
    b       el1_entry

from_el3:
    /* Configure EL3 to EL2 drop */
    mov     x1, #(1 << 10) | (1 << 8) | (1 << 0) /* RW=1 (64-bit), HCE=1, NS=1 */
    msr     scr_el3, x1
    mov     x1, #0x3c9                           /* DAIF masked, EL2h */
    msr     spsr_el3, x1
    adr     x1, from_el2
    msr     elr_el3, x1
    eret

from_el2:
    /* Allow EL1 access to physical counter and timer */
    mrs     x1, cnthctl_el2
    orr     x1, x1, #0x3                         /* EL1PCTEN, EL1PCEN */
    msr     cnthctl_el2, x1
    msr     cntvoff_el2, xzr

    /* Configure EL2 to EL1 drop */
    mov     x1, #(1 << 31)                       /* HCR_EL2.RW = 1 (AArch64 execution state) */
    msr     hcr_el2, x1
    mov     x1, #0x3c5                           /* DAIF masked, EL1h */
    msr     spsr_el2, x1
    adr     x1, el1_entry
    msr     elr_el2, x1
    eret

el1_entry:
    /* Enable FP/SIMD instructions in EL1 without trapping */
    mov     x0, #(3 << 20)                       /* FPEN = 0b11 */
    msr     cpacr_el1, x0
    isb

    /* Initialize Stack Pointer */
    ldr     x0, =__stack_top
    mov     sp, x0

    /* Zero out .bss section using 64-bit stores */
    ldr     x0, =__bss_start
    ldr     x1, =__bss_end
bss_clear_loop:
    cmp     x0, x1
    b.ge    bss_clear_done
    str     xzr, [x0], #8
    b       bss_clear_loop
bss_clear_done:

    /* Install initial vector table into VBAR_EL1 */
    ldr     x0, =boot_vectors
    msr     vbar_el1, x0
    isb

    /* Jump to C kernel entry point */
    bl      kernel_main

hang:
    wfe
    b       hang

/*
 * Initial aligned Exception Vector Table for boot.
 * 2048-byte aligned as required by ARMv8-A architecture.
 */
.align 11
.global boot_vectors
boot_vectors:
    /* Current EL with SP0 */
    .align 7
    b boot_exception_handler
    .align 7
    b boot_exception_handler
    .align 7
    b boot_exception_handler
    .align 7
    b boot_exception_handler

    /* Current EL with SPx */
    .align 7
    b boot_exception_handler
    .align 7
    b boot_exception_handler
    .align 7
    b boot_exception_handler
    .align 7
    b boot_exception_handler

    /* Lower EL using AArch64 */
    .align 7
    b boot_exception_handler
    .align 7
    b boot_exception_handler
    .align 7
    b boot_exception_handler
    .align 7
    b boot_exception_handler

    /* Lower EL using AArch32 */
    .align 7
    b boot_exception_handler
    .align 7
    b boot_exception_handler
    .align 7
    b boot_exception_handler
    .align 7
    b boot_exception_handler

boot_exception_handler:
    wfe
    b boot_exception_handler
