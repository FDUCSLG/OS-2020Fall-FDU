# Lab4 多核与锁

回顾一下 lab3 中的一些问题：

- 栈指针 16 Byte 对齐问题，只用 stp/ldp
- c 和汇编的交互 trapframe 第一个成员是较低地址的
- trapframe 是否可以只存 caller-saved 寄存器？
- sp、SP_SEL、SP_EL1 的关系？
- SP_EL1 不能直接访问，得先把 SP_SEL 置 1，然后访问 sp 的值
- call trap(struct trapframe *tf) 中 tf 是 sp，trapframe 里面保存了一个用户栈指针 SP_EL0



本次实验我们会学习操作系统中多核和锁机制的相关知识，将会涉及如下几个部分：

- 了解 SMP 架构的定义
- 了解树莓派 3 上多核的初始状态以及如何开启（习题）
- 对之前实现的模块进行加锁以适配多核（习题）

首先，请将本次实验新增的代码并入你们的开发分支，即在你们自己的 dev 分支下，如下 pull 后进行 rebase 或 merge

```bash
git pull
git rebase origin/lab4
# Or git merge origin/lab4
```

如果出现 conflict，请按照 git 的提示进行即可。



## 4.1 SMP 架构

树莓派以及大部分笔记本都是 SMP（symmetric multiprocessing） 架构的，即所有的 CPU 拥有各自的寄存器（包括通用寄存器和系统寄存器），但共享相同的内存和 MMIO。尽管所有 CPU 的功能都是一样的，我们通常会把第一个启动的、运行 BIOS 初始化代码的 CPU 称为 BSP（ bootstrap processor），其他的 CPU 称为 APs（application processors）。但树莓派比较奇葩，是从 GPU 启动的，即 GPU 完成了类似 BIOS 的硬件初始化过程，然后才进行 CPU 的执行。

由于操作系统中有时候需要知道当前运行在哪个 CPU 上，ARMv8 用 MPIDR_EL1 寄存器来提供每个 CPU 自己的 ID。获取当前 CPU 的 ID 的方法见 `inc/arm.h:cpuid`。

更详细的内容请参考 [ARM Cortex-A Series Programmer's Guide for ARMv8-A](https://cs140e.sergio.bz/docs/ARMv8-A-Programmer-Guide.pdf) Chapter 14。



## 4.2 APs 的初始状态

qemu 启动时，会把 [armstub8.S](https://github.com/raspberrypi/tools/blob/master/armstubs/armstub8.S) 编译后的二进制直接拷贝到物理地址为 0 的地方，然后所有 CPU 均以 PC 为 0 开始（但有些 CPU 会被 armstub8.S 停住，只有一个 CPU 会跳到 0x80000，具体见下文）。

而在真实的树莓派 3 上，armstub8.S 编译后的可执行代码已经被集成在了 start.elf 或 bootcode.bin 中，上板子时，我们需要把这两个文件拷贝到 SD 卡的第一个分区（必须是 FAT32），这样启动后 GPU 就会把 armstub8.S 的二进制代码从中抽取出来并直接拷贝到物理地址为 0 的地方，然后所有 CPU 均以 PC 为 0 开始。

armstub8.S 的伪代码如下。简言之，cpuid 为零 的 CPU（相当于 BSP）将直接跳转到 0x80000 的地方，而 cpuid 非零的 CPU（相当于 APs）将进行死循环直到各自的入口值非零，每个 CPU 的入口值在内存中的地址为 `0xd8 + cpuid()`。

```c
extern int cpuid();

void boot_kernel(void (*entry)())
{
    entry();
}

void secondary_spin()
{
   	void *entry;
    while (entry = *(void **)(0xd8 + (cpuid() << 3)) == 0)
        asm volatile("wfe");
    boot_kernel(entry);
}

void main()
{
    if (cpuid() == 0) boot_kernel(0x80000);
    else secondary_spin();
}
```



## 4.3 开启 APs

至于如何开启 APs，我们的 BSP 在 `kern/entry.S:_start` 中修改了 APs 的入口值并用 `sev` 指令唤醒它们。

为了提高代码的复用性，入口值都被修改成了 `kern/start.S:mp_start`，相当于所有 CPU 都会从这里开始并行执行类似 [Lab1 启动](lab1.md) 中的过程，只不过每个 CPU 都有不同的栈指针。

从本次实验开始，开启 APs 之后的所有代码都会被所有 CPU 并行执行，**修改或读取全局变量时记得加锁**。

### 4.3.1 习题一

为了确保你完全掌握了多核的启动流程，请简要描述一下 `kern/entry.S` 中各个 CPU 的初始状态如何、经历了哪些变化？至少包括对 PC、栈指针、页表的描述。



## 4.4 同步和锁

在 SMP 架构下，我们通常需要对内存中某个位置上的互斥访问。ARMv8 为我们提供了一些简单的原子指令如读互斥 LDXR 和写互斥 STXR 来完成此类操作，但这些指令只能被用于被设定为 inner or outer sharable, inner and outer write-back, read and write allocate, non-transient 的 Normal Memory 中。而我们确实在 `inc/mmu.h:PTE_NORMAL` 和 `inc/mmu.h:TCR_VALUE` 中进行了相应的配置，于是这类原子指令可以正常使用。

我们在 `kern/spinlock.c` 中用 GCC 提供的 atomic 宏实现了一种最简单的锁——自旋锁，相应的汇编代码可在导出的 `obj/kernel8.asm` 中找到。当然你也可以尝试着实现 rwlock, mcs, mutex 等较为复杂的锁。

### 4.4.1 原子操作

下面列出了几种常见的原子操作的伪代码

Test-and-set (atomic exchange)

```c
int test_and_set(int *p, int new)
{
    int old = *p;
    *p = new;
    return old;
}
```

Fetch-and-add (FAA)

```c
int fetch_and_add(int *p, int inc)
{
    int old = *p;
    *p += inc;
    return old;
}
```

Compare-and-swap (CAS)

````c
int compare_and_swap(int *p, int cmp, int new)
{
    int old = *p;
    if (old == cmp)
        *p = new;
    return old;
}
````

### 4.4.2 自旋锁

```c
struct spinlock {
    int locked;
};
void spin_lock(struct spinlock *lk)
{
    while (test_and_set(lk->locked, 1)) ;
}
void spin_unlock(struct spinlock *lk)
{
    test_and_set(lk->locked, 0);
}
```

### 4.4.3 Ticket lock

```c
struct ticketlock {
    int ticket, turn;
};
void ticket_lock(struct ticketlock *lk)
{
    int t = fetch_and_add(lk->ticket, 1);
    while (lk->turn != t) ;
}
void ticket_unlock(struct ticketlock *lk)
{
    lk-turn++;
}
```

Ticket lock 中等待的 CPU 都在不停地访问同一个变量 `lk->turn`，一旦锁被释放并修改 `lk->turn`，则需要修改所有等待中的 CPU 的 cache，当 CPU 数较多时，性能较差

### 4.4.4 MCS 锁

这是一种基于队列的锁，对多核的缓存比较友好，并用 test-and-set 和 compare-and-swap 提高了效率，以下实现参考自 [CS140-Synchronization2](http://www.scs.stanford.edu/20wi-cs140/notes/synchronization2.pdf)

```c
struct mcslock {
    struct mcslock *next;
    int locked;
};
void mcs_lock(struct mcslock *lk, struct mcslock *i)
{
    i->next = i->locked = 0;
    struct mcslock *pre = test_and_set(&lk->next, i);
    if (pre) {
        i->locked = 1;
        pre->next = i;
    }
    while (i->locked) ;
}
void mcs_unlock(struct mcslock *lk, struct mcslock *i)
{
    if (i->next == 0)
        if (compare_and_swap(&lk->next, i, 0) == i)
            return;
    while (i->next == 0) ;
    i->next->locked = 0;
}
```

### 4.4.5 读写锁

下面用两个自旋锁（或支持睡眠的 mutex）来实现读优先的读写锁

```c
struct rwlock {
    struct spinlock r, w; /* Or mutex. */
    int cnt; /* Number of readers. */
};
void read_lock(struct rwlock *lk)
{
    acquire(&lk->r);
    if (lk->cnt++ == 0)
        acquire(&lk->w);
    release(&lk->r);
}
void read_unlock(struct rwlock *lk)
{
    acquire(&lk->r);
    if (--lk->cnt == 0)
        release(&lk-w);
    release(&lk->r);
}
void write_lock(struct rwlock *lk)
{
    acquire(&lk->w);
}
void write_unlock(struct rwlock *lk)
{
    release(&lk->w);
}
```

### 4.4.6 信号量

信号量（Semaphore）适用于控制一个仅支持有限个用户的共享资源，可以保证同一时刻最多有 cnt 个该资源被占用（cnt 的定义如下）。当 cnt 等于 1 时就是互斥锁（Mutex）。

```c
struct semaphore {
    /* If cnt < 0, -cnt indicates the number of waiters. */
    int cnt;
    struct spinlock lk;
};
void sem_init(struct semaphore *s, int cnt)
{
    s->cnt = cnt;
}
void sem_wait(struct semaphore *s)
{
    acquire(&s->lk);
    if (--s->cnt < 0)
        sleep(s, &s->lk); /* Sleep on s. */
    release(&s->lk);
}
void sem_signal(struct semaphore *s)
{
    acquire(&s->lk);
    if (s->cnt++ < 0)
        wakeup1(s); /* Wake up one process sleeping on s. */
    release(&s->lk);
}
```

### 4.4.7 习题二

请阅读 `kern/spinlock.c` 并思考一下，如果我们在内核中没有关中断的话，`kern/spinlock.c` 是否有问题？如果有的话，应该如何修改呢？

### 4.4.8 习题三

注意到所有 CPU 都会并行进入 `kern/main.c:main`，而其中有些初始化函数是只能被调用一次的，请简单描述一下你的判断和理由，并在 `kern/main.c` 中加锁来保证这一点。


