#ifndef INC_MMU_H
#define INC_MMU_H

/*
 * See Chapter 12 of ARM Cortex-A Series Programmer's Guide for ARMv8-A
 * and Chapter D4(D4.2, D4.3) of Arm Architecture Reference Manual Armv8, for Armv8-A architecture profile.
 */

/*
 * A virtual address 'la' has a four-part structure as follows:
 * +-----9-----+-----9-----+-----9-----+-----9-----+---------12---------+
 * |  Level 0  |  Level 1  |  Level 2  |  Level 3  | Offset within Page |
 * |   Index   |   Index   |   Index   |   Index   |                    |
 * +-----------+-----------+-----------+-----------+--------------------+
 *  \PTX(va, 0)/\PTX(va, 1)/\PTX(va, 2)/\PTX(va, 3)/
 */

#define PGSIZE 4096
#define PGSHIFT 12
#define L0SHIFT 39
#define L1SHIFT 30
#define L2SHIFT 21
#define L3SHIFT 12
#define ENTRYSZ 512

#define PTX(level, va) (((uint64_t)(va) >> (39 - 9 * level)) & 0x1FF)
#define L0X(va) (((uint64_t)(va) >> L0SHIFT) & 0x1FF)
#define L1X(va) (((uint64_t)(va) >> L1SHIFT) & 0x1FF)
#define L2X(va) (((uint64_t)(va) >> L2SHIFT) & 0x1FF)
#define L3X(va) (((uint64_t)(va) >> L3SHIFT) & 0x1FF)

/* accessibility */
#define PTE_P        (1<<0)      /* valid */
#define PTE_BLOCK    (0<<1)
#define PTE_PAGE     (1<<1)
#define PTE_TABLE    (1<<1)      /* entry gives address of the next level of translation table */
#define PTE_KERNEL   (0<<6)      /* privileged, supervisor EL1 access only */
#define PTE_USER     (1<<6)      /* unprivileged, EL0 access allowed */
#define PTE_RW       (0<<7)      /* read-write */
#define PTE_RO       (1<<7)      /* read-only */
#define PTE_AF       (1<<10)     /* P2066 access flags */
// Address in page table or page directory entry
#define PTE_ADDR(pte)   ((uint64_t)(pte) & ~0xFFF)
#define PTE_FLAGS(pte)  ((unsigned)(pte) &  0xFFF)

/* P2061 */
#define MM_TYPE_BLOCK       PTE_P | PTE_BLOCK

/* Memory region attributes */
#define MT_DEVICE_nGnRnE        0x0
#define MT_NORMAL               0x1
#define MT_DEVICE_nGnRnE_FLAGS  0x00
#define MT_NORMAL_FLAGS         0xFF
#define MAIR_VALUE              (MT_DEVICE_nGnRnE_FLAGS << (8 * MT_DEVICE_nGnRnE)) | (MT_NORMAL_FLAGS << (8 * MT_NORMAL))

/* PTE flags */
#define PTE_NORMAL      (MM_TYPE_BLOCK | (MT_NORMAL << 2) | PTE_AF)
#define PTE_DEVICE      (MM_TYPE_BLOCK | (MT_DEVICE_nGnRnE << 2) | PTE_AF)

/* Translation Control Register */
/*
 * Intermediate physical address size 32 bits, 4GB
 * If {I}PS is programmed to a value larger than the implemented PA size, then the PE behaves as if programmed with
 * the implemented PA size, but software must not rely on this behavior
 */
#define TCR_IPS         (0 << 32)

/*
 * For each enabled stage of address translation, 
 * the TCR_ELx.TxSZ fields specify the input address size:
 * • TCR_ELx.T0SZ specifies the size for the lower VA range, translated using TTBR0_ELx.
 * • TCR_ELx.T1SZ specifies the size for the upper VA range, translated using TTBR1_ELx.
 * The maximum TxSZ value is 39. If TxSZ is programmed to a value larger than 39 then it is
 * IMPLEMENTATION DEFINED whether:
 * • The implementation behaves as if the field is programmed to 39 for all purposes other than
 * reading back the value of the field.
 * • Any use of the TxSZ value generates a Level 0 Translation fault for the stage of translation
 * at which TxSZ is used.
 */
#define TCR_T0SZ        (64 - 48) 
#define TCR_T1SZ        ((64 - 48) << 16)
#define TCR_TG0_4K      (0 << 14)
/* Granule size for ttbr1_el1 4KB */
#define TCR_TG1_4K      (2 << 30)
#define TCR_SH0_INNER   (3 << 12)
#define TCR_SH1_INNER   (3 << 28)
#define TCR_ORGN0_IRGN0 ((1 << 10) | (1 << 8))
#define TCR_ORGN1_IRGN1 ((1 << 26) | (1 << 24))

#define TCR_VALUE       (TCR_T0SZ           | TCR_T1SZ          |   \
                         TCR_TG0_4K         | TCR_TG1_4K        |   \
                         TCR_SH0_INNER      | TCR_SH1_INNER     |   \
                         TCR_ORGN0_IRGN0    | TCR_ORGN1_IRGN1   |   \
                         TCR_IPS)


#endif
