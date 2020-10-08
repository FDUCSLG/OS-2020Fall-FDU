#ifndef INC_SYSREGS_H
#define INC_SYSREGS_H

/* SCTLR_EL1, System Control Register (EL1). */
#define SCTLR_RESERVED              ((3 << 28) | (3 << 22) | (1 << 20) | (1 << 11) | (1 << 8) | (1 << 7))
#define SCTLR_EE_LITTLE_ENDIAN      (0 << 25)
#define SCTLR_E0E_LITTLE_ENDIAN     (0 << 24)
#define SCTLR_I_CACHE               (1 << 12)
#define SCTLR_D_CACHE               (1 << 2)
#define SCTLR_MMU_DISABLED          (0 << 0)
#define SCTLR_MMU_ENABLED           (1 << 0)

#define SCTLR_VALUE_MMU_DISABLED    (SCTLR_RESERVED | SCTLR_EE_LITTLE_ENDIAN | SCTLR_E0E_LITTLE_ENDIAN | SCTLR_I_CACHE | SCTLR_D_CACHE | SCTLR_MMU_DISABLED)

/* HCR_EL2, Hypervisor Configuration Register (EL2). */
#define HCR_RW                      (1 << 31)
#define HCR_VALUE                   HCR_RW

/* SCR_EL3, Secure Configuration Register (EL3). */
#define SCR_RESERVED                (3 << 4)
#define SCR_RW                      (1 << 10)
#define SCR_HCE                     (1 << 8)
#define SCR_SMD                     (1 << 7)
#define SCR_NS                      (1 << 0)
#define SCR_VALUE                   (SCR_RESERVED | SCR_RW | SCR_HCE | SCR_SMD | SCR_NS)

/* SPSR_EL1/2/3, Saved Program Status Register. */
#define SPSR_MASK_ALL               (7 << 6)
#define SPSR_EL1h                   (5 << 0)
#define SPSR_EL2h                   (9 << 0)
#define SPSR_EL3_VALUE              (SPSR_MASK_ALL | SPSR_EL2h)
#define SPSR_EL2_VALUE              (SPSR_MASK_ALL | SPSR_EL1h)

/* Exception Class in ESR_EL1. */
#define EC_SHIFT                    26
#define EC_SVC64                    0x15
#define EC_DABORT                   0x24
#define EC_IABORT                   0x20

#endif
