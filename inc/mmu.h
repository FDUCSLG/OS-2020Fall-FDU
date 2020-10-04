#ifndef INC_MMU_H
#define INC_MMU_H

/*
 * See Chapter 12 of ARM Cortex-A Series Programmer's Guide for ARMv8-A
 * and Chapter D5 of Arm Architecture Reference Manual Armv8, for Armv8-A architecture profile.
 */
#define PGSIZE 4096

#define MM_TYPE_BLOCK       0x1
#define MM_TYPE_TABLE       0x3

/* Access permission */
#define AF_USED           (1 << 10)      /* EL0/1/2/3 can read and write */

/* Access flags */
#define AP_RW             (1 << 6)

/* Memory region attributes */
#define MT_DEVICE_nGnRnE        0x0
#define MT_NORMAL_NC            0x1
#define MT_DEVICE_nGnRnE_FLAGS  0x00
#define MT_NORMAL_NC_FLAGS      0x44
#define MAIR_VALUE              (MT_DEVICE_nGnRnE_FLAGS << (8 * MT_DEVICE_nGnRnE)) | (MT_NORMAL_NC_FLAGS << (8 * MT_NORMAL_NC))

/* PTE flags */
#define PTE_NORMAL      (MM_TYPE_BLOCK | (MT_NORMAL_NC << 2) | AP_RW)
#define PTE_DEVICE      (MM_TYPE_BLOCK | (MT_DEVICE_nGnRnE << 2) | AP_RW)
#define PTE_TABLE       MM_TYPE_TABLE

/* Translation Control Register */
// Intermediate physical address size 32 bits, 4GB
#define TCR_IPS         (0b000 << 32)
// 
#define TCR_T0SZ        (64 - 48) 
// The size offset of the memory region addressed by TTBR1_EL1. 
// The region size is 2^(64-T1SZ) bytes.
#define TCR_T1SZ        ((64 - 48) << 16)
#define TCR_TG0_4K      (0b00 << 14)
// Granule size for ttbr1_el1 4KB
#define TCR_TG1_4K      (0b10 << 30)
#define TCR_VALUE       (TCR_T0SZ | TCR_T1SZ | TCR_TG0_4K | TCR_TG1_4K)

#endif
