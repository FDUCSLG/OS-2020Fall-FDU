#ifndef INC_SLEEPLOCK_H
#define INC_SLEEPLOCK_H

#include "spinlock.h"
#include "proc.h"

/* Long-term locks for processes */
struct sleeplock {
    int locked;         /* Is the lock held? */
    struct spinlock lk; /* Spinlock protecting this sleep lock */
    int pid;
};

void initsleeplock(struct sleeplock *lk, char *name);
void acquiresleep(struct sleeplock *lk);
void releasesleep(struct sleeplock *lk);
int holdingsleep(struct sleeplock *lk);
#endif
