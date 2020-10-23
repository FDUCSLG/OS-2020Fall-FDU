#ifndef INC_SPINLOCK_H
#define INC_SPINLOCK_H

struct spinlock {
    volatile int locked;
};
void acquire(struct spinlock *);
void release(struct spinlock *);

#endif
