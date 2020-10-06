#include "mmu.h"
#include "memlayout.h"
#include "sysregs.h"

.section ".text.boot"

.globl _start

_start:
    /* Read cpu id, stop slave cores. */
    mrs     x1, mpidr_el1
    and     x1, x1, #3
    cbnz    x1, halt

    /* Get current exception level by CurrentEL[3:2]. */
    mrs     x1, CurrentEL
    and     x1, x1, #0xc
    cmp     x1, #8
    beq     el2
    bgt     el3

el3:
    mov     x2, #0x5b1
    msr     scr_el3, x2
    mov     x2, #0x3c9
    msr     spsr_el3, x2
    adr     x2, el2
    msr     elr_el3, x2
    eret

el2:
    /* Enable CNTP for EL1. */
    mrs     x0, cnthctl_el2
    orr     x0, x0, #3
    msr     cnthctl_el2, x0
    msr     cntvoff_el2, xzr
    /* Enable AArch64 in EL1. */
    mov     x0, #(1 << 31)      /* AArch64 */
    orr     x0, x0, #(1 << 1)   /* SWIO hardwired on Pi3 */
    msr     hcr_el2, x0
    mrs     x0, hcr_el2
    /* Setup SCTLR access. */
    mov     x2, #0x0800
    movk    x2, #0x30d0, lsl #16
    msr     sctlr_el1, x2
    /* Change execution level to EL1. */
    mov     x2, #0x3c4
    msr     spsr_el2, x2
    adr     x2, el1
    msr     elr_el2, x2
    eret

el1:
    adr     x0, kpgdir

    /* Higher and lower half map to same address. */
    msr     ttbr0_el1, x0
    msr     ttbr1_el1, x0

    ldr     x0, =(TCR_VALUE)        
    msr     tcr_el1, x0

    ldr     x0, =(MAIR_VALUE)
    msr     mair_el1, x0

    /* Modify System Control Register to enable MMU. */
    mrs     x0, sctlr_el1
    orr     x0, x0, #SCTLR_MMU_ENABLED 
    msr     sctlr_el1, x0

    /* Set up stack pointer. */
    ldr     x1, =_start
    mov     sp, x1

    /*
     * Jump to C code, should not return.
     * Why not directly run `b main`?
     */ 
    ldr     x1, =main
    br      x1

halt:
    wfe
    b       halt
