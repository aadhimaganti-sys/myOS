#ifndef _MMU_H
#define _MMU_H

#include "types.h"

/* Page size definitions */
#define PAGE_SIZE_4KB      4096ULL
#define SECTION_SIZE_2MB   0x200000ULL
#define SECTION_SIZE_1GB   0x40000000ULL

/* MAIR_EL1 memory attributes */
#define MT_DEVICE_nGnRE    0
#define MT_NORMAL_WB       1

/* Descriptor Types */
#define DESC_TYPE_TABLE    0x3ULL  /* Table descriptor (L0, L1, L2 pointing to next table) */
#define DESC_TYPE_BLOCK    0x1ULL  /* Block descriptor (L1 1GB, L2 2MB) */

/* Page/Block Attribute Flags */
#define DESC_ATTR_VALID    (1ULL << 0)
#define DESC_ATTR_AF       (1ULL << 10)  /* Access Flag: 1 = Accessed */
#define DESC_ATTR_SH_INNER (3ULL << 8)   /* Inner Shareable */
#define DESC_ATTR_SH_OUTER (2ULL << 8)   /* Outer Shareable */
#define DESC_ATTR_AP_RW_EL1 (0ULL << 6)  /* Read/Write at EL1, No EL0 */
#define DESC_ATTR_UXN      (1ULL << 54)  /* Unprivileged Execute Never */
#define DESC_ATTR_PXN      (1ULL << 53)  /* Privileged Execute Never */

#define DESC_ATTR_DEVICE   ((MT_DEVICE_nGnRE << 2) | DESC_ATTR_AP_RW_EL1 | DESC_ATTR_SH_OUTER | DESC_ATTR_AF | DESC_ATTR_UXN | DESC_ATTR_PXN | DESC_TYPE_BLOCK)
#define DESC_ATTR_NORMAL   ((MT_NORMAL_WB << 2)    | DESC_ATTR_AP_RW_EL1 | DESC_ATTR_SH_INNER | DESC_ATTR_AF | DESC_ATTR_UXN | DESC_TYPE_BLOCK)

void mmu_init(void);

#endif /* _MMU_H */
