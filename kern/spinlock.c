#include "arm.h"
#include "spinlock.h"
#include "console.h"

void 
acquire(struct spinlock *lk)
{
    while (lk->locked || __atomic_test_and_set(&lk->locked, __ATOMIC_ACQUIRE))
        ;
}

void 
release(struct spinlock *lk)
{
    if (!lk->locked)
        panic("release: not locked\n");
    __atomic_clear(&lk->locked, __ATOMIC_RELEASE);
}
