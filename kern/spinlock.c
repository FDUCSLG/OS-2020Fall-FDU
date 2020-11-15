#include "arm.h"
#include "spinlock.h"
#include "console.h"
#include "proc.h"
#include "string.h"

/*
 * Check whether this cpu is holding the lock.
 */
int
holding(struct spinlock *lk)
{
    int hold;
    hold = lk->locked && lk->cpu == thiscpu;
    return hold;
}

void
initlock(struct spinlock *lk, char *name) {
    lk->name = name;
    lk->locked = 0;
    lk->cpu = 0;
}

void
acquire(struct spinlock *lk)
{
    if (holding(lk)) {
        panic("acquire: spinlock already held\n");
    }
    while (lk->locked || __atomic_test_and_set(&lk->locked, __ATOMIC_ACQUIRE))
        ;
    lk->cpu = thiscpu;
}

void
release(struct spinlock *lk)
{
    if (!holding(lk)) {
        panic("release: not locked\n");
    }
    lk->cpu = NULL;
    __atomic_clear(&lk->locked, __ATOMIC_RELEASE);
}