#ifndef KERN_VM_H
#define KERN_VM_H

#include <stdint.h>
#include "proc.h"

void vm_free(uint64_t *, int);
void uvm_switch(struct proc *);
void uvm_init(uint64_t *, char *, int);

uint64_t *pgdir_init();

#endif