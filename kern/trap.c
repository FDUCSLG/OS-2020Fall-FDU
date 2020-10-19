#include "trap.h"

#include "arm.h"
#include "sysregs.h"
#include "mmu.h"
#include "peripherals/irq.h"

#include "uart.h"
#include "console.h"
#include "clock.h"
#include "timer.h"

void
irq_init()
{
    cprintf("- irq init\n");
    clock_init();
    put32(ENABLE_IRQS_1, AUX_INT);
    put32(GPU_INT_ROUTE, GPU_IRQ2CORE(0));
}

void
trap(struct trapframe *tf)
{
    int src = get32(IRQ_SRC_CORE(cpuid()));
    if (src & IRQ_CNTPNSIRQ) timer(), timer_reset();
    else if (src & IRQ_TIMER) clock(), clock_reset();
    else if (src & IRQ_GPU) {
        if (get32(IRQ_PENDING_1) & AUX_INT) uart_intr();
        else goto bad;
    } else {
        switch (resr() >> EC_SHIFT) {
        case EC_SVC64:
            cprintf("hello, world\n");
            lesr(0);  /* Clear esr. */
            break;
        default:
bad:
            panic("unexpected irq.\n");
        }
    }
}

void
irq_error(uint64_t type)
{
    cprintf("- irq_error\n");
    panic("irq of type %d unimplemented. \n", type);
}
