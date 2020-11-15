#ifndef INC_PROC_H
#define INC_PROC_H

#include <stdint.h>
#include "arm.h"
#include "trap.h"

#define NCPU   4        /* maximum number of CPUs */
#define NPROC 64        /* maximum number of processes */
#define KSTACKSIZE 4096 /* size of per-process kernel stack */

#define thiscpu (&cpus[cpuid()])

struct cpu {
    struct context *scheduler;  /* swtch() here to enter scheduler */
    struct proc *proc;          /* The process running on this cpu or null */
};

extern struct cpu cpus[NCPU];

/*
 * Saved registers for kernel context switches.
 * Don't need to save X1-X15 since accorrding to 
 * the x86 convention it is the caller to save them.
 * Contexts are stored at the top of the stack they describe,
 * the stack pointer is the address of the context.
 * The layout of the context matches the layout of the stack in swtch.S
 */
struct context {
    /* TODO: Your code here. */
};

enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

struct proc {
    uint64_t sz;             /* Size of process memory (bytes)          */
    uint64_t *pgdir;         /* Page table                              */
    char *kstack;            /* Bottom of kernel stack for this process */
    enum procstate state;    /* Process state                           */
    int pid;                 /* Process ID                              */
    struct proc *parent;     /* Parent process                          */
    struct trapframe *tf;    /* Trapframe for current syscall           */
    struct context *context; /* swtch() here to run process             */
    void *chan;              /* If non-zero, sleeping on chan           */
    int killed;              /* If non-zero, have been killed           */
    char name[16];           /* Process name (debugging)                */
};

void proc_init();
void user_init();
void scheduler();

void exit();

#endif