# Initial process execs /init.

#include "syscallno.h"

.globl start

# exec(init, argv)
start:
    ldr     x1, =init
    ldr     x2, =argv
    mov     x0, #SYS_exec
    svc     0x00

exit:
    mov     x0, #SYS_exit
    svc     0x00
    b       exit

# char init[] = "/init\0";
init:
    .string "/init\0"

# char *argv[] = { init, 0 };
.p2align 4
argv:
    .word init
    .word 0
    .word 0
    .word 0

