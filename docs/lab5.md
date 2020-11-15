## Lab5 进程管理

[toc]

### 5.1 实验内容简介

#### 5.1.1 实验目标

　　在本次实验中，需要实现教学操作系统内核的进程管理部分，分为两部分：

- 内核的进程管理模块，负责进程的创建、调度（执行）
- 内核的（简单）系统调用模块，负责响应用户进程请求的系统调用

最终实现将 `user/initcode.S` 作为第一个用户进程予以调度，并响应其执行过程中的系统调用。建议在实验中先切换为单处理器模式，确保单处理器下所有功能均正常，再切换为多处理器模式，以将并发问题与其他功能性问题做到分离。

#### 5.1.2 Git

　　首先，请将本次实验新增的代码并入你们的开发分支

```bash
git pull
git rebase origin/lab5
# Or git merge origin/lab5
```

如果出现 conflict，请按照 git 的提示进行即可。

### 5.2 进程管理

​		我们在 `inc/proc.h` 定义了 xv6 中进程的概念（本实验中仅需关注如下内容），每个进程都拥有自己的内核栈，页表（与对应的地址空间），以及对应的上下文信息。

```c++
struct proc {
    uint64_t sz;             /* Size of process memory (bytes)          */
    uint64_t *pgdir;         /* Page table                              */
    char *kstack;            /* Bottom of kernel stack for this process */
    enum procstate state;    /* Process state                           */
    int pid;                 /* Process ID                              */
    struct proc *parent;     /* Parent process                          */
    struct trapframe *tf;    /* Trapframe for current syscall           */
    struct context *context; /* swtch() here to run process             */
};
```

#### 5.2.1 问题一

　　在 proc（即 PCB）中仅存储了进程的 trapframe 与 context 指针，请说明 trapframe 与 context 的实例存在何处，为什么要这样设计？（Hint：如果在 proc 中存储 `struct proc parent` 与 `struct trapframe tf` 能否实现 trap 与 context switch）

#### 5.2.2 Context switch

　　context switch（即上下文切换）是操作系统多道程序设计（multitasking/multiprogramming）的重要组成部分，它描述了从一个进程切换到另一个进程的过程，包括切换通用寄存器堆、运行时栈（即每个进程的内核栈）以及 PC（lr/x30 寄存器）。

<img src="Pic/Context-switch.png">

当用户进程放弃 CPU 时，该进程的内核线程（kernel thread）会调用 `kern/swtch.S` 中的 `swtch` 来存储当前进程的上下文信息，并切换到内核调度器的上下文，每个进程的上下文信息均由 `struct proc` 的 `struct context*` 描述。请完成 `inc/proc.h` 中 `struct context` 的定义以及 `kern/swtch.S` 中 context switch 的实现。

#### 5.2.3 问题二

　　在 `kern/proc.c` 中将 `swtch` 声明为 `void swtch(struct context **, struct context *)`，请说明为什么要这样设计（Hint：如果声明为 `void swtch(struct context *, struct context *)` 有什么问题）？`context` 中仅需要存储 callee-saved registers，请结合 PCS，并且与 trapframe 作对比，解释为什么 trapframe 需要存储这么多信息解释一下为什么？

>  For the Arm architecture, the Procedure Call Standard, or PCS specifies:
>
> - Which registers are used to pass arguments into the function.
> - Which registers are used to return a value to the function doing the calling, known as the caller.
> - Which registers the function being called, which is known as the callee, can corrupt.
> - Which registers the callee cannot corrupt.

　　PCS 是 ARM 架构在函数调用过程中通用寄存器的使用规范，当我们自己手写汇编，同时会借助其他开源工具如编译器生成汇编指令时，应当尽量遵守这个规范，以保证代码整体正确、高效。[^PCS]

#### 5.2.4 创建与调度进程

　　请根据 `kern/proc.c` 中相应代码的注释完成内核进程管理模块以支持调度第一个用户进程 `user/initcode.S`。因为当前内核中还没有文件系统，通过修改 Makefile 文件将 `initcode.S` 链接进了 kernel 中。大致流程为：首先将 `initcode.S` 编译、转换为 `obj/user` 下的二进制文件，再与 `kern` 目录下编译出的内核代码共同链接成内核的可执行文件，`make qemu` 后可在 `obj/kernel8.hdr` 的 symbol table 中发现与之相关的三个 symbol：\_binary_obj_user_initcode_size、\_binary_obj_user_initcode_start、\_binary_obj_user_initcode_end 来告知内核 `initcode` 的位置信息。

可参照如下顺序实现

- 

### 5.3 系统调用

　　目前内核已经支持基本的异常处理，在本实验中还需要进一步完善内核的系统调用模块。用户进程通过系统调用来向操作系统内核请求服务以完成需要更高权限运行的任务。在 armv8 中，用户进程通过 `svc` 指令来请求系统调用，内核会根据 `ESR_EL1` 中的 `EC[31:26]` 以分派给相应的 handler 进行处理[^ESR_EL1]。

　　用户进程在请求系统调用时，应告知内核相应的 system call number 以及系统调用所需的参数信息，system call number 的宏定义可见 `inc/syscallno.h`。

allocproc is written so that it can be used by fork as well as when creating the first process



why initialize the pcb like this?

**why release lock in forkret?** keep lock for illusion that scheduler always hold ptable.lock



allocproc sets up the new process with a specially prepared kernel stack and set of kernel registers that cause it to ‘‘return’’ to user space when it first runs. allocproc does part of this work by setting up return program counter values that will cause the new process’s kernel thread to first execute in forkret and then in trapret (2507-2512).

The kernel thread will start executing with register contents copied from p->context. Thus setting p- >context->eip to forkret will cause the kernel thread to execute at the start of forkret.



> The context switch code (3058) sets the stack pointer to point just beyond the end of p->context. allocproc places p->context on the stack, and puts a pointer to trapret just above it

something changes



for context



Chap 1 Code: creating the first process

proc_alloc 分配一个新的进程
user_init 初始化一个用户进程
scheduler 调度器
sched
forkret
exit
swtch-trapasm
kvm_init
uvm_init
initcode.S

syscall-sysfile-sysproc



在成功实现所有模块后，`make qemu` 在单处理器模式下应显示

```shell
main: [CPU0] is init kernel
main: Allocator: Init success.
irq_init: - irq init
main: [CPU0] Init success.
sys_exec: executing /init with parameters: /init 
sys_exit: in exit
```

在多处理器模式下应显示

```shell
qemu-system-aarch64 -M raspi3 -nographic -serial null -serial mon:stdio -kernel obj/kernel8.img
main: [CPU1] is init kernel
main: Allocator: Init success.
irq_init: - irq init
main: [CPU1] Init success.
main: [CPU2] Init success.
main: [CPU3] Init success.
main: [CPU0] Init success.
sys_exec: executing /init with parameters: /init 
sys_exit: in exit
```

### 5.x 参考文献

[^PCS]:https://developer.arm.com/architectures/learn-the-architecture/aarch64-instruction-set-architecture/procedure-call-standard
[^ESR_EL1]:https://developer.arm.com/docs/ddi0595/h/aarch64-system-registers/esr_el1