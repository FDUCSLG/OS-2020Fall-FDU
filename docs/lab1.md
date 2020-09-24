# Lab 1 启动

在本实验中，我们正式启动内核。我们会学习 ARM v8 架构、树莓派 3 的启动流程、链接器脚本，然后为内核提供一个 C 的运行环境并跳转到 C 代码里面。

## 1.1 ARM v8

ARM 同 MIPS 一样是一种精简指令架构（RISC），它相较复杂指令架构（CISC，如 X86）而言提供更简单的指令操作与访存模型（LOAD/STORE）。以对内存中某一个值加一为例，X86 的 `INC` 可直接将内存中的值加一，而 ARM 必须通过 `LOAD` 将内存中的值读入寄存器、`ADD` 将寄存器加一、`STORE` 将寄存器写回内存。虽然 CISC 的指令功能更强大，但简单的指令操作可以缩短 RISC 的时钟周期以加快硬件执行。

关于 ARM 架构的一些简单介绍可以参考 Stanford[^Stanford] *Phase1: ARM and a Leg(Subphase A-C)*。*A Guide to ARM64 / AArch64 Assembly on Linux with Shellcodes and Cryptography*[^Assembly] 也提供关于 ARM 架构的丰富内容。

[^Stanford]: https://cs140e.sergio.bz/assignments/3-spawn/
[^Assembly]: https://modexp.wordpress.com/2018/10/30/arm64-assembly/

## 1.2 树莓派 3 与 BCM2837

除了 arm 架构外，我们还要了解树莓派 3 板子（基于 BCM2837）的硬件信息，本实验中我们只需知道如下两点：

- 如何启动？即树莓派怎么加载内核的？
- 物理内存中哪些是可用的（如用于给用户进程分配内存），哪些是和设备相关的 MMIO（Memory-mapped IO）？

### 1.2.1 启动流程

对于 qemu 模拟的树莓派 3，就是简单的把 kernel8.img 复制到 0x80000 的内存地址处，然后跳到 0x80000 开始执行。

而真实的树莓派是从 GPU 的ROM启动的（类似 BIOS），然后它会读取 SD 卡的第一个分区（必须为 FAT32 分区），依次读取并执行其中的 bootcode.bin，start.elf 和内核镜像，然后将内核镜像直接加载到内存的某个位置。其中 bootcode.bin 和 start.elf 可理解为官方提供的 [Boot Loader](https://github.com/raspberrypi/firmware/tree/master/boot)，并不是开源的，但我们可以进行一些配置（如内核镜像的名称及其加载到内存中的位置），具体请参考 [Boot Folder](https://www.raspberrypi.org/documentation/configuration/boot_folder.md)。

### 1.2.2 内存布局

| ARM 物理地址             | 描述                       |
| ------------------------ | -------------------------- |
| [0x0, 3F000000)          | Free memory                |
| [0x3F000000, 0x40000000) | MMIO                       |
| [0x40000000, 0x40020000) | ARM timer, IRQs, mailboxes |

这里只列举了目前我们所需要用到的地址空间，具体请参考 [BCM2837](https://www.raspberrypi.org/documentation/hardware/raspberrypi/bcm2837/README.md)



## 1.3 ELF 和 Linker Scripts

如对链接过程不熟悉的话，请先复习一下 CSAPP Chapter 7 Linking

### 1.3.1 Linker Scripts

我们用链接器脚本（kern/linker.ld）解决了如下两个问题：

1. 对于编译器而言，例如 `b main` （跳到名为 main 的函数）的指令应该编译成什么？显然，生成对应的机器码需要知道 main 的地址。而链接器脚本就是用来控制这些地址的。
2. 回顾一下树莓派的启动流程，加载内核时是把 kernel8.img 直接复制到物理地址 0x80000 处，然后从 0x80000 开始执行。于是，kernel8.img 的开头处就是我们内核的“入口”（kern/entry.S），换言之，我们需要将 entry.S 放在内核镜像文件的开头，并用 objcopy 去掉 ELF 头。

下面仅介绍我们所用到的语法，具体请参考 [Linker Scripts](https://sourceware.org/binutils/docs/ld/Scripts.html)。

- `.` 被称为 location counter，代表了当前位置代码的实际上会运行在内存中的哪个地址。如 `. = 0xFFFF000000080000;` 代表我们告诉链接器内核的第一条指令运行在 0xFFFF000000080000。但事实上，我们一开始是运行在物理地址 0x80000（为什么仍能正常运行？），开启页表后，我们才会跳到高地址（虚拟地址），大部分代码会在那里完成。
- text 段中的第一行为 `KEEP(*(.text.boot))` 是指把 kern/entry.S 中我们自定义的 `.text.boot` 段放在 text 段的开头。`KEEP` 则是告诉链接器始终保留此段。
- `PROVIDE(etext = .)` 声明了一个符号 `etext`（但并不会为其分配具体的内存空间，即在 C 代码中我们不能对其进行赋值），并将其地址设为当前的 location counter。这使得我们可以在汇编或 C 代码中得到内核各段（text/data/bss）的地址范围，我们在初始化 BSS 的时候需要用到这些符号。

### 1.3.2 ELF 格式

运行 `make` 后，我们可以在 obj/kernel8.hdr 查看 ELF 头。在查看 SYMBOL TABLE 时，Linux 下可用 `sort <obj/kernel8.hdr` 查看对地址排序后的结果。

注意区分虚拟地址（VMA）和加载地址（LMA）：

- VMA是执行时的地址，会影响到代码中地址的编译。 链接器脚本中的 `.` 就是用来影响 VMA 的。
- LMA是加载的地址，这个地址只会出现在 ELF 头中，只是告诉 loader 需要把这个 elf 文件拷贝到内存中的哪个位置。由于树莓派加载的是去掉 ELF 头后的 obj/kernel8.img，我们不需要关心 LMA。



## 1.4 代码注释

### 1.4.1 调整 Exception Level

我们只会用到 EL0 和 EL1（类似 x86 的 ring3 和 ring0），分别代表用户态和内核态。在树莓派 3 硬件上，我们会从 EL2 启动，但在 qemu 上可能会从 EL3 启动。于是在 kern/entry.S 中，我们需要先判断一下当前的 EL，然后逐步降至 EL1。

### 1.4.2 初始页表

操作系统内核的代码通常位于高地址（为什么这么设计？），而鉴于 Arm v8 的 MMU 机制，即可以根据地址的高 16 位来自动切换不同的页表，我们将内核放在虚拟地址 [0xFFFF'0000'0000'0000, 0xFFFF'FFFF'FFFF'FFFF]，剩余的虚拟地址 [0x0 ~ 0x0000'FFFF'FFFF'FFFF] 则留给用户代码。

为了便于实现，我们在 kern/kpgdir.c 中直接硬编码了一个页表，将两块 1GB 的内存 [0, 1GB) 和 [KERNBASE, KERNBASE+1GB)，均映射到物理地址的 [0, 1GB)，其中 KERNBASE 为 0xFFFF'0000'0000'0000，如下

| 虚拟地址                                       | 物理地址           |
| ---------------------------------------------- | ------------------ |
| [0x0, 0x4000'0000)                             | [0x0, 0x4000'0000) |
| [0xFFFF'0000'0000'0000, 0xFFFF'0000'4000'0000) | [0x0, 0x4000'0000) |

为什么需要上表中的第一个映射（恒等映射）呢？第一个原因是开启 MMU 后我们的 PC 仍在低地址，若没有此映射，开启 MMU 后的第一条指令就会出错，第二个原因是便于直接访问物理内存。

其实我们还把 [KERNBASE+1GB, KERNBASE+2GB) 映射到了 [1GB, 2GB)，这在之后配置时钟中断时会用到。具体如何配置页表，我们会在后续 lab 中介绍。

### 1.4.3 进入 C 代码

开启页表后，我们把栈指针指向 _start，即 [0xFFFF'0000'0000'0000, 0xFFFF'0000'0008'0000) 为初始的内核栈，然后就可以安全地跳到高地址处的 C 代码了（kern/main.c:main），main 函数不会返回。

### 1.4.3 习题一

未初始化的全局变量和局部静态变量通常会被编译器安排在 BSS 段中，而为了减小可执行文件的大小，编译器不会把这段编入 ELF 文件中。我们需要手动对这其进行零初始化，请补全 kern/main.c 中的代码，你可能需要根据 kern/linker.ld 了解 BSS 段的起始和终止位置。

### 1.4.4 习题二

请补全 kern/main.c 中的代码，完成 console 的初始化并输出 "hello, world\n"，你可能需要阅读 inc/console.h, kern/console.c。

