#include "syscall.h"
#include "proc.h"
#include "console.h"

int
sys_exit()
{
    cprintf("sys_exit: in exit\n");
    exit();
    return 0;
}