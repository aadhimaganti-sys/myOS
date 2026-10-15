.section ".text"

.macro SAVE_CONTEXT
    sub     sp, sp, #288
    stp     x0, x1, [sp, #0]
    stp     x2, x3, [sp, #16]
    stp     x4, x5, [sp, #32]
    stp     x6, x7, [sp, #48]
    stp     x8, x9, [sp, #64]
    stp     x10, x11, [sp, #80]
    stp     x12, x13, [sp, #96]
    stp     x14, x15, [sp, #112]
    stp     x16, x17, [sp, #128]
    stp     x18, x19, [sp, #144]
    stp     x20, x21, [sp, #160]
    stp     x22, x23, [sp, #176]
    stp     x24, x25, [sp, #192]
    stp     x26, x27, [sp, #208]
    stp     x28, x29, [sp, #224]

    /* Save x30 and original SP */
    add     x0, sp, #288
    str     x30, [sp, #240]
    str     x0,  [sp, #248]

    /* Read and save ELR_EL1 and SPSR_EL1 */
    mrs     x1, elr_el1
    mrs     x2, spsr_el1
    stp     x1, x2, [sp, #256]

    /* Read and save ESR_EL1 and FAR_EL1 */
    mrs     x3, esr_el1
    mrs     x4, far_el1
    stp     x3, x4, [sp, #272]
.endm

.macro RESTORE_CONTEXT
    ldp     x1, x2, [sp, #256]
    msr     elr_el1, x1
    msr     spsr_el1, x2

    ldp     x0, x1, [sp, #0]
    ldp     x2, x3, [sp, #16]
    ldp     x4, x5, [sp, #32]
    ldp     x6, x7, [sp, #48]
    ldp     x8, x9, [sp, #64]
    ldp     x10, x11, [sp, #80]
    ldp     x12, x13, [sp, #96]
    ldp     x14, x15, [sp, #112]
    ldp     x16, x17, [sp, #128]
    ldp     x18, x19, [sp, #144]
    ldp     x20, x21, [sp, #160]
    ldp     x22, x23, [sp, #176]
    ldp     x24, x25, [sp, #192]
    ldp     x26, x27, [sp, #208]
    ldp     x28, x29, [sp, #224]
    ldr     x30, [sp, #240]

    add     sp, sp, #288
    eret
.endm

.align 11
.global exception_vectors
exception_vectors:
    /* Current EL with SP0 */
    .align 7
    b entry_el1_sync_sp0
    .align 7
    b entry_el1_irq_sp0
    .align 7
    b entry_el1_fiq_sp0
    .align 7
    b entry_el1_serror_sp0

    /* Current EL with SPx (Standard Kernel Execution) */
    .align 7
    b entry_el1_sync
    .align 7
    b entry_el1_irq
    .align 7
    b entry_el1_fiq
    .align 7
    b entry_el1_serror

    /* Lower EL using AArch64 (User Mode Execution) */
    .align 7
    b entry_el0_sync_64
    .align 7
    b entry_el0_irq_64
    .align 7
    b entry_el0_fiq_64
    .align 7
    b entry_el0_serror_64

    /* Lower EL using AArch32 */
    .align 7
    b entry_el0_sync_32
    .align 7
    b entry_el0_irq_32
    .align 7
    b entry_el0_fiq_32
    .align 7
    b entry_el0_serror_32

/* Vector Handlers */
entry_el1_sync_sp0:
entry_el0_sync_64:
entry_el0_sync_32:
entry_el1_sync:
    SAVE_CONTEXT
    mov     x0, sp
    bl      handle_sync_exception
    RESTORE_CONTEXT

entry_el1_irq_sp0:
entry_el0_irq_64:
entry_el0_irq_32:
entry_el1_irq:
    SAVE_CONTEXT
    mov     x0, sp
    bl      handle_irq
    RESTORE_CONTEXT

entry_el1_fiq_sp0:
entry_el0_fiq_64:
entry_el0_fiq_32:
entry_el1_fiq:
    SAVE_CONTEXT
    mov     x0, sp
    bl      handle_fiq
    RESTORE_CONTEXT

entry_el1_serror_sp0:
entry_el0_serror_64:
entry_el0_serror_32:
entry_el1_serror:
    SAVE_CONTEXT
    mov     x0, sp
    bl      handle_serror
    RESTORE_CONTEXT

/* Global Vector Controls */
.global irq_vector_init
irq_vector_init:
    ldr     x0, =exception_vectors
    msr     vbar_el1, x0
    isb
    ret

.global irq_enable
irq_enable:
    msr     daifclr, #2
    isb
    ret

.global irq_disable
irq_disable:
    msr     daifset, #2
    isb
    ret
