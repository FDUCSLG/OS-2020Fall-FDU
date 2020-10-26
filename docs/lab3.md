# Lab3 中断与异常

本次实验我们会学习操作系统里中断相关的知识，将会涉及如下几个部分

1. 理解中断的流程
2. 了解中断的配置
3. 设计 trap frame，实现保存并恢复 trap frame （习题）
4. 理解中断处理函数

首先，请将本次实验新增的代码并入你们的开发分支，即在你们自己的 dev 分支下，如下 pull 后进行 rebase 或 merge

```bash
git pull
git rebase origin/lab3
# Or git merge origin/lab3
```

如果出现 conflict，请按照 git 的提示进行即可。



## 3.1 中断的流程

为了更好的了解中断，我们需要知道 CPU 在中断时究竟做了些什么。ARMv8 架构下中断时，CPU会先**关闭所有中断**（查看 DAIF 寄存器的值可以发现 DAIF 都被 mask 掉了，当然我们也可以手动开启，但为便于实现，我们暂不支持内核态的中断），根据中断向量表进行跳转，保存一些中断信息，并进行栈的切换。更具体的，

- 中断后自动关中断，这意味着所有的中断只会在用户态发生，即**只有从 EL0 到 EL1 的中断**
- 根据不同的中断种类，跳到中断向量表中的不同位置
- 保存中断前的 PSTATE 至 SPSR_EL1，保存中断前的 PC 到 ELR_EL1，保存中断原因信息到 ESR_EL1
- 将栈指针 sp 从 SP_EL0 换为 SP_EL1，这也意味着 SPSEL_EL1 的值为 1



## 3.2 中断的配置

### 3.2.1 中断向量表

在 `kern/main.c` 中我们用 `lvbar` 加载了ARMv8 的中断向量表 `kern/vectors.S`，其作用是，当硬件收到中断信号时，会根据其中断类型（Synchronous/IRQ/FIQ/System Error）和当前 PSTATE（EL0/EL1，AArch32/AArch64），保存中断原因到 ESR_EL1，保存中断发生时的 PC 到 ELR_EL1，然后跳转到不同的地址上。描述这些地址的那张表称为中断向量表。

中断向量表的地址可以通过 VBAR_EL1 配置（注意这是一个**虚拟地址**），表中的每一项是一种中断类型的入口，大小为 128 bytes，共有 16 项。上节中提到过我们只支持从 EL0 到 EL1 的中断，且我们的内核只支持 AArch64，于是在这里只需关注如下两项即可，其他的详见 [ARM Cortex-A Series Programmer's Guide for ARMv8-A](https://cs140e.sergio.bz/docs/ARMv8-A-Programmer-Guide.pdf) Chapter 10.4

| Address        | Exception type | Description                           | Usage               |
| -------------- | -------------- | ------------------------------------- | ------------------- |
| VBAR_EL1+0x400 | Synchronous    | Interrupt from lower EL using AArch64 | System call         |
| VBAR_EL1+0x480 | IRQ/vIRQ       | Interrupt from lower EL using AArch64 | UART/timer/clock/sd |

于是在 `kern/vectors.S` 中除了这两项，其他的都会执行 `kern/trap.c:irq_error` 来报错。

### 3.2.2 中断路由

中断产生后（多核情况下）硬件应该中断哪个 CPU 或哪些 CPU ？这是我们需要告诉硬件的。树莓派比较特殊，所有的中断都会先发送到 GPU，再通过 GPU 来分配到具体哪些 CPU 上，详细请参考 [BCM2836](https://www.raspberrypi.org/documentation/hardware/raspberrypi/bcm2836/QA7_rev3.4.pdf)。我们只会用到如下几种中断，为了便于实现，我们将所有全局的中断都路由到了第一个核（CPU0）：

- UART 输入：在 qemu 中就是键盘输入，路由至 CPU0
- 全局时钟（clock）：这是个时钟是固定频率的，在现实情况下用于记录系统时间或进行系统监控等，路由至 CPU0
- 局部时钟（timer）：每个 CPU 都有的一种时钟中断，共四个，频率随 CPU 频率的变化而变化，用于记录进程的时间片，四个中断分别路由至四个 CPU
- SD卡设备：之后文件系统部分会加入，路由至 CPU0

上述已实现的中断的初始化和处理函数分别位于 `kern/uart.c`、`kern/clock.c`、`kern/timer.c`。



## 3.3 Trap frame

Trap frame 结构体的作用在于保存中断前的所有寄存器加上一些必要的中断信息。在我们的实验中，需要关注 31 个通用寄存器的值（x0~x30）以及系统寄存器 ELR_EL1，SPSR_EL1，SP_EL0。

### 3.3.1 习题一

请简要描述一下在你实现的操作系统中，中断时 CPU 进行了哪些操作。

### 3.3.2 习题二

请在 `inc/trap.h` 中设计你自己的 trap frame，并简要说明为什么这么设计。

### 3.3.3 习题三

请补全 `kern/trapasm.S` 中的代码，完成 trap frame 的构建、恢复。



## 3.4 中断的处理

所有的合法中断会被 `kern/vectors.S` 发往 `kern/trapasm.S`，进而调用 `kern/trap.c:trap` 函数，这就是我们的中断处理函数。其中根据不同的中断来源而调用相应的处理函数。

### 3.4.1 测试中断

如果想测试一下中断是否正确实现的话，我们需要手动在内核开启中断（**后续请务必记得关闭**，除非你想支持内核态中断），看看我们的 timer/clock/uart 是否能够正常中断，需要做如下修改：

1. 在 `kern/vectors.S` 中将 `verror(5)` 替换成 `ventry`，因为这一项代表在使用 SP_EL1 栈指针的 EL1 的 IRQ 中断。注意到 `kern/entry.S` 中 `msr spsel, #1` 指令已经将栈指针切换成了 SP_EL1，这就是我们的内核栈，之后用户进程使用的是 SP_EL0。
2. 在 `kern/main.c:main` 函数最后调用 `inc/arm.h` 中的 `sti` 函数开启中断。

正常的话，qemu 上会有 timer/clock 的输出，输入字母的话也会显示在上面。



## 3.5 参考资料

实验过程中需重点关注 [ARM Cortex-A Series Programmer's Guide for ARMv8-A](https://cs140e.sergio.bz/docs/ARMv8-A-Programmer-Guide.pdf) 中的 Chapter 9 & 10，其中

- Chapter 9 规范了 ARMv8 架构下汇编和 C 语言之间的交互标准，如各寄存器的用处（哪个是作为函数返回地址的？哪些是 caller/callee 保存的寄存器？），stack frame 结构。在设计 trap frame 的时候会用到相关知识
- Chapter 10 是关于中断的，其中 10.5 有一段简易的参考代码，类似我们的 trapasm.S
