#include "mmu.h"
#include "printf.h"
#include "string.h"

/* 4-Level Page Tables (4KB granule, aligned to 4096-byte boundaries) */
__attribute__((aligned(4096))) static uint64_t l0_table[512];
__attribute__((aligned(4096))) static uint64_t l1_table[512];
__attribute__((aligned(4096))) static uint64_t l2_dev_table[512];
__attribute__((aligned(4096))) static uint64_t l2_ram_table[512];

void mmu_init(void) {
    /* Zero out all page tables */
    memset(l0_table, 0, sizeof(l0_table));
    memset(l1_table, 0, sizeof(l1_table));
    memset(l2_dev_table, 0, sizeof(l2_dev_table));
    memset(l2_ram_table, 0, sizeof(l2_ram_table));

    /*
     * 1. Level 0 Entry 0 (covers first 512GB from 0x00000000 to 0x7FFFFFFFFF):
     *    Points to Level 1 table.
     */
    l0_table[0] = ((uint64_t)l1_table & ~0xFFFULL) | DESC_TYPE_TABLE;

    /*
     * 2. Level 1 Entries (each covers 1GB):
     *    - Entry 0: [0x00000000 - 0x3FFFFFFF] -> MMIO Devices (GIC, UART, VirtIO)
     *               Points to l2_dev_table.
     *    - Entry 1: [0x40000000 - 0x7FFFFFFF] -> RAM (Kernel, Stack, Heap)
     *               Points to l2_ram_table.
     */
    l1_table[0] = ((uint64_t)l2_dev_table & ~0xFFFULL) | DESC_TYPE_TABLE;
    l1_table[1] = ((uint64_t)l2_ram_table & ~0xFFFULL) | DESC_TYPE_TABLE;

    /*
     * 3. Level 2 Device Table:
     *    512 entries of 2MB each = 1GB [0x00000000 - 0x3FFFFFFF]
     *    Configured as Device-nGnRE memory, execute-never.
     */
    for (uint64_t i = 0; i < 512; i++) {
        uint64_t pa = i * SECTION_SIZE_2MB;
        l2_dev_table[i] = pa | DESC_ATTR_DEVICE;
    }

    /*
     * 4. Level 2 RAM Table:
     *    512 entries of 2MB each = 1GB [0x40000000 - 0x7FFFFFFF]
     *    Configured as Normal Inner/Outer Cacheable Write-Back memory.
     */
    for (uint64_t i = 0; i < 512; i++) {
        uint64_t pa = 0x40000000ULL + (i * SECTION_SIZE_2MB);
        l2_ram_table[i] = pa | DESC_ATTR_NORMAL;
    }

    /*
     * Configure MAIR_EL1 (Memory Attribute Indirection Register):
     * Index 0: 0x04 = Device-nGnRE
     * Index 1: 0xFF = Normal Inner/Outer Write-Back Cacheable
     */
    uint64_t mair = (0x04ULL << (MT_DEVICE_nGnRE * 8)) |
                    (0xFFULL << (MT_NORMAL_WB * 8));
    __asm__ volatile("msr mair_el1, %0" :: "r"(mair));

    /*
     * Configure TCR_EL1 (Translation Control Register):
     * T0SZ = 16 (48-bit Virtual Addresses: 2^(64-16) = 256TB)
     * TG0  = 0b00 (4KB granule for TTBR0_EL1)
     * IRGN0 = 0b01 (Inner Write-Back Read-Allocate Write-Allocate)
     * ORGN0 = 0b01 (Outer Write-Back Read-Allocate Write-Allocate)
     * SH0  = 0b11 (Inner Shareable)
     * EPD1 = 1 (Disable TTBR1 walks for kernel-only low mappings)
     * IPS  = 0b010 (40-bit Physical Address space, 1TB)
     */
    uint64_t tcr = (16ULL << 0)         /* T0SZ = 16 */
                 | (1ULL << 8)          /* IRGN0 = Inner WB-WA */
                 | (1ULL << 10)         /* ORGN0 = Outer WB-WA */
                 | (3ULL << 12)         /* SH0 = Inner Shareable */
                 | (0ULL << 14)         /* TG0 = 4KB */
                 | (1ULL << 23)         /* EPD1 = 1 (disable TTBR1) */
                 | (2ULL << 32);        /* IPS = 40-bit PA */

    __asm__ volatile("msr tcr_el1, %0" :: "r"(tcr));
    __asm__ volatile("isb");

    /* Set TTBR0_EL1 to the physical address of Level 0 table */
    __asm__ volatile("msr ttbr0_el1, %0" :: "r"((uint64_t)l0_table));
    __asm__ volatile("isb");

    /* Invalidate TLB entries for EL1 */
    __asm__ volatile("tlbi vmalle1is");
    __asm__ volatile("dsb ish");
    __asm__ volatile("isb");

    /* Invalidate Instruction Cache */
    __asm__ volatile("ic iallu");
    __asm__ volatile("dsb ish");
    __asm__ volatile("isb");

    /*
     * Enable MMU and Caches in SCTLR_EL1:
     * Bit 0: M  (MMU Enable)
     * Bit 2: C  (Data Cache Enable)
     * Bit 12: I (Instruction Cache Enable)
     */
    uint64_t sctlr;
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    sctlr |= (1ULL << 0);  /* M */
    sctlr |= (1ULL << 2);  /* C */
    sctlr |= (1ULL << 12); /* I */
    __asm__ volatile("msr sctlr_el1, %0" :: "r"(sctlr));
    __asm__ volatile("isb");

    kprintf("[+] 4-Level MMU initialized and enabled (Identity Paged)\n");
    kprintf("    MAIR_EL1:   0x%016lx (Attr0=Device-nGnRE, Attr1=Normal-WB)\n", mair);
    kprintf("    TCR_EL1:    0x%016lx (48-bit VA, 4KB granule, 40-bit PA)\n", tcr);
    kprintf("    TTBR0_EL1:  0x%016lx (Level 0 Table at %p)\n", (uint64_t)l0_table, (void *)l0_table);
    kprintf("    SCTLR_EL1:  0x%016lx [MMU=ON, D-Cache=ON, I-Cache=ON]\n", sctlr);
}
