# Talk 1 内存顺序

回顾 lab 4 中的一些问题：

- 虽然开了多核，但内存只有一个，编译后内核还是只有一份的，就放在 0x80000
- 关于 BSS 段，通常编译后 BSS 段是不会在可执行文件中的，而是只在 ELF 头中标一个范围，其初始化是由加载此程序的 loader 来完成的
- 问题二，参考 xv6 
- 习题三，main.c 需要保证 memset 只执行一次，这需要锁，于是这个锁（全局变量）需要显示初始化一下，这样就不会被编译到 BSS 段中了（因为 BSS 段中的值是不确定的），如 `struct spinlock my_lock = {0};`
- 寄存器是每个 CPU 都有一套的，于是对于系统寄存器的操作是每个 CPU 都要执行一次的
- 我们目前还在 EL1（内核态），并未进入 EL0



现代计算机体系对于内存访问进行了玩命优化，于是有了如下两个层面的优化

- 编译器优化，可能会导致编译产生的汇编代码的乱序
- 处理器优化，包括指令乱序执行、cache 一致性

可以看出这两者优化都是基于乱序，但完全乱序显然不行，于是为了便于编程并保证正确性，大部分编译器和处理器会遵循一定的原则，即「不能修改单线程的行为」（[Thou shalt not modify the behavior of a single-threaded program.](https://preshing.com/20120625/memory-ordering-at-compile-time/)）。但在多线程的程序中会有一些问题，例如下述程序执行后 r1、r2 可能同时为 0。

```c
X = 0, Y = 0;

Thread 1: 
X = 1;
r1 = Y;

Thread 2: 
Y = 1;
r2 = X;
```

于是我们需要显式的告诉编译器（C++ 内存模型，最终也是通过处理器提供的指令来实现）或处理器（指令屏障和内存屏障）一些额外的信息，因为编译器和处理器并不知道哪些数据是在线程间共享的，这是需要我们手动控制的。



## 屏障

在大多数弱内存模型（简称WMO，Weak Memory Ordering）的架构上，可通过汇编指令来告诉处理器，我们期望的内存访问读取的顺序。ARMv8 上用屏障来实现，有如下三种屏障相关的指令：

- Instruction Synchronization Barrier (ISB)：清空处理器中的指令流水，可确保在 ISB 指令完成后，才从 cache 或内存中读取位于 ISB 指令后的其他所有指令，通常作为修改系统寄存器（如 MMU）时的屏障
- Data Memory Barrier (DMB)：在当前 shareability domain 内，该指令之前的所有内存指令一定先于其后的内存指令，但对非内存指令没有要求
- Data Synchronization Barrier (DSB)：在当前 shareability domain 内，该指令之前的所有内存指令一定先于其后的内存指令，其后的所有指令均需要等到 DSB 指令完成后才能执行

### One-way barriers

ARMv8 还提供两种内存操作中常用的单向屏障（One-way barriers）如下，单向屏障的性能优于之前的 DMB，且天然适合锁的实现。

- Load-acquire (LDAR): All loads and stores that are after an LDAR in program order, and that match the shareability domain of the target address, must be observed after the LDAR.

- Store-release (STLR): All loads and stores preceding an STLR that match the shareability domain of the target address, must be observed before the STLR.

其中 program order 就是我们所写的汇编代码的顺序（而扔到处理器上执行的顺序是乱序后的），shareability domain 是需要我们告诉 MMU 的（见 `inc/mmu.h` 中对于 TCR 的配置，把所有内存都统一配置成了 inner shareable domain，即所有核）



## C++ 内存模型

### 术语

sequence-before：同一个线程中的多个语句之间就是sequenced-before关系，如下  ① sequenced-before ②：

```c
int i = 7; // ①
i++;       // ②
```

happens-before：顾名思义，全局事件发生的顺序关系，即多线程版的 sequence-before

### Sequentially Consistent

对应 `std::memory_order_seq_cst`，是最严格的内存模型，C++ 原子操作默认采用这种模型，保证了

- 程序指令与源码顺序一致
- 所有线程的所有操作存在一个全局的顺序，即前面的原子操作 happens-before 后面的原子操作

如下例子的 assert 一定不会失败

```c
Thread 1:
y.store(20);
x.store(10);

Thread 2: 
if (x.load() == 10) {
    assert(y.load() == 20);
    y.store(10);
}

Thread 3:
if (y.load() == 10) {
    assert(x.load() == 10);
}
```

### Relaxed

对应 `std::memory_order_relaxed`，最宽松的内存模型，除了保证之前提到的「不修改单线程的行为」原则外无任何约束，编译器可以尽情的优化

### Acquire/Release

第三种方案间于前两种之间，类似 Sequentially Consistent 模式，保证了

- 同一个对象上的原子操作不允许被乱序
- Release 操作禁止了所有在它之前的读写操作与在它之后的写操作乱序
- Acquire 操作禁止了所有在它之前的读操作与在它之后的读写操作乱序

```c
x = 0, y = 0;

Thread 1:
y.store(20, memory_order_release);

Thread 2:
x.store(10, memory_order_release);

Thread 3:
assert(y.load (memory_order_acquire) == 20 && x.load (memory_order_acquire) == 0)

Thread 4:
assert(y.load (memory_order_acquire) == 0 && x.load (memory_order_acquire) == 10)
```

上述两个 assert 可能都成功，但若采用 Sequentially Consistent 模式的话，至多一个成功



## 参考资料

1. [C++ 内存模型](https://paul.pub/cpp-memory-model/)
2. [The C++11 Memory Model and GCC](https://gcc.gnu.org/wiki/Atomic/GCCMM)  关于 `__sync` 和 `__atomic` 宏
3.  [ARM Cortex-A Series Programmer's Guide for ARMv8-A](https://cs140e.sergio.bz/docs/ARMv8-A-Programmer-Guide.pdf) Chapter 13 Memory Order
4. [ARM Exclusive - 互斥锁与读写一致性的底层实现原理](https://juejin.im/post/6844903970536685576)

