#ifndef INC_MEMLAYOUT_H
#define INC_MEMLAYOUT_H

#define EXTMEM 0x80000                /* Start of extended memory */
#define PHYSTOP 0x3F000000            /* Top physical memory */

#define KERNBASE 0xFFFF000000000000   /* First kernel virtual address */
#define KERNLINK (KERNBASE+EXTMEM)    /* Address where kernel is linked */

#define V2P_WO(x) ((x) - KERNBASE)    /* Same as V2P, but without casts */
#define P2V_WO(x) ((x) + KERNBASE)    /* Same as P2V, but without casts */

#ifndef __ASSEMBLER__

#include <stdint.h>
#define V2P(a) (((uint64_t) (a)) - KERNBASE)
#define P2V(a) ((void *)(((char *) (a)) + KERNBASE))

#endif

#endif
