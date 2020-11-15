#ifndef INC_SYSCALL_H
#define INC_SYSCALL_H

#include <stdint.h>
#include "syscallno.h"

int argstr(int, char **);
int argint(int, uint64_t *);
int fetchstr(uint64_t, char **);

int syscall();

#endif