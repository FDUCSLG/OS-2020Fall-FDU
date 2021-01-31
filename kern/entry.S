#include "mmu.h"
#include "memlayout.h"
#include "sysregs.h"

.section ".text.boot"
.global _start
_start:
    /*
     * Wake up other processors.
     * See https://github.com/raspberrypi/tools/blob/master/armstubs/armstub8.S
     * which has been incorporated in start.elf (or bootcode.bin ?)
     */
    adr     x0, mp_start
    mov     x1, #1
    mov     x2, #2
    mov     x3, #3
    mov     x9, 0xd8
    str     x0, [x9, x1, lsl #3]
    str     x0, [x9, x2, lsl #3]
    str     x0, [x9, x3, lsl #3]

    dsb     sy
    isb

    sev

mp_start:
    /* Get current exception level by CurrentEL[3:2]. */
    mrs     x9, CurrentEL
    and     x9, x9, #0xc
    cmp     x9, #8
    beq     el2
    bgt     el3

el3:
    mov     x9, #SCR_VALUE
    msr     scr_el3, x9
    mov     x9, #SPSR_EL3_VALUE
    msr     spsr_el3, x9
    adr     x9, el2
    msr     elr_el3, x9
    eret

el2:
    /*
     * HCR_EL2.RW, bit[31] = 1: The Execution state for EL1 is AArch64.
     * The Execution state for EL0 is determined by the current
     * value of PSTATE.nRW when executing at EL0.
     */
    mov     x9, #HCR_VALUE
    msr     hcr_el2, x9

    /* Setup SCTLR access. */
    ldr     x9, =SCTLR_VALUE_MMU_DISABLED
    msr     sctlr_el1, x9

    /* Enable SIMD instructions. */
    ldr     x9, =CPACR_VALUE
    msr     cpacr_el1, x9

    /* Change execution level to EL1. */
    mov     x9, #SPSR_EL2_VALUE
    msr     spsr_el2, x9
    adr     x9, el1
    msr     elr_el2, x9
    eret

el1:
    adr     x9, kpgdir

    /* Higher and lower half map to same physical memory region. */
    msr     ttbr0_el1, x9
    msr     ttbr1_el1, x9

    ldr     x9, =TCR_VALUE
    msr     tcr_el1, x9

    ldr     x9, =MAIR_VALUE
    msr     mair_el1, x9

    isb

    /* Enable MMU. */
    mrs     x9, sctlr_el1
    orr     x9, x9, #SCTLR_MMU_ENABLED
    msr     sctlr_el1, x9

    isb

    /* Read CPU ID. */
    mrs     x0, mpidr_el1
    and     x0, x0, #3

    /* sp = _start - cpuid() * PGSIZE; */
    mov     x10, #PGSIZE
    mul     x10, x10, x0
    ldr     x9, =_start
    sub     x9, x9, x10

    /* Set up stack pointer, must always be 16 bytes aligned. */
    msr     spsel, #1
    mov     sp, x9

    ldr     x9, =main
    br      x9
