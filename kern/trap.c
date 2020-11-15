#include "trap.h"

#include "arm.h"
#include "sysregs.h"
#include "mmu.h"
#include "syscall.h"
#include "peripherals/irq.h"

#include "uart.h"
#include "console.h"
#include "clock.h"
#include "timer.h"
#include "proc.h"

void
irq_init()
{
    cprintf("irq_init: - irq init\n");
    clock_init();
    put32(ENABLE_IRQS_1, AUX_INT);
    put32(GPU_INT_ROUTE, GPU_IRQ2CORE(0));
}

void
trap(struct trapframe *tf)
{
    struct proc *proc = thiscpu->proc;
    int src = get32(IRQ_SRC_CORE(cpuid()));
    if (src & IRQ_CNTPNSIRQ) timer(), timer_reset();
    else if (src & IRQ_TIMER) clock(), clock_reset();
    else if (src & IRQ_GPU) {
        if (get32(IRQ_PENDING_1) & AUX_INT) uart_intr();
        else goto bad;
    } else {
        switch (resr() >> EC_SHIFT) {
        case EC_SVC64:
            lesr(0);  /* Clear esr. */
            /* Jump to syscall to handle the system call from user process */
            /* TODO: Your code here. */
            break;
        default:
bad:
            panic("trap: unexpected irq.\n");
        }
    }
}

void
irq_error(uint64_t type)
{
    cprintf("irq_error: - irq_error\n");
    panic("irq_error: irq of type %d unimplemented. \n", type);
}
