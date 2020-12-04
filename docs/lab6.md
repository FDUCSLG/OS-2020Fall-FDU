# Lab6 驱动和 libc

[TOC]

## 6.1 实验内容简介

### 6.1.1 实验目标

本次实验中，我们会实现一个简易的 sd 卡驱动，了解 MBR 分区格式，并学习 libc 与操作系统之间的接口。

### 6.1.2 准备工作

首先，请将本次实验新增的代码并入你们的开发分支，如下

```bash
git pull
git rebase origin/lab6
# Or git merge origin/lab6
```

如果出现 conflict，请按照 git 的提示进行即可。

- 安装 `mtools` （Ubuntu 下执行 `sudo apt install -y mtools` 即可）以供 SD 卡镜像制作
- 执行 `make init` 初始化 libc（只用执行一次即可）
- 之后正常执行 `make` 编译内核即可，第一次编译 libc 的时候会比较慢，如果等不及的话可用 `make -j10` 并行编译



## 6.2 I/O 框架

本节抽象地介绍了简易的基于队列的异步 I/O 框架。

### 6.2.1 异步读写

对于设备驱动，往往需要实现对其的读写操作，而由于 I/O 速度较慢，为了避免阻塞当前 CPU，我们可以采用异步的方式来实现，如下

- 读：向设备发送读请求和目标地址，然后等待中断直到设备完成读取，再读取设备返回的数据
- 写：向设备发送写请求、目标地址以及待写入的数据，等待中断直到设备完成写入

当然其中还有很多细节需要考虑，如中断后清除相应的 flag 来避免重复的中断以及若设备发生错误中断则重新执行当前操作等。

### 6.2.2 请求队列

对于多个进程均想使用 I/O 的情况，则需要一个请求队列。进程作为生产者，每次请求都将一个 buffer 入队，然后阻塞（睡眠）直到驱动来唤醒它。而 I/O 驱动作为消费者，每次取出并解析队首的 buffer，向设备发出具体的读写操作，完成后唤醒对应的进程。

### 6.2.3 习题 1

请补全 `inc/buf.h` 以便于 SD 卡驱动中请求队列的实现，即每个请求都是一个 buf，所有请求排成一队。队列的实现可以手写，但建议将其单独抽成一个头文件，方便复用，具体请参考 [list.h](https://github.com/torvalds/linux/blob/master/include/linux/list.h)



## 6.3 块设备驱动

在块设备中，我们会用到如下几个简单的术语：

- 块大小：每个块的大小，通常为 512 B
- 逻辑区块地址（[Logical block addressing, LBA](https://wiki.osdev.org/LBA)）是从 0 开始的块下标，即第几块

对于内核的其他模块而言，异步的块设备驱动通常包括三个函数，初始化函数如 `sd_init`、设备读写请求函数如 `sdrw`、设备中断处理函数如 `sd_intr`。设备的初始化比较硬件，暂且略过，让我们从读写函数开始讲起。

### 6.3.1 读写请求

这是给内核其他模块（如文件系统）使用设备的接口，其接受一个 buffer，然后进行类似 6.2.1 中的步骤，在等待过程中我们会用 sleep/wakeup 来实现

### 6.3.2 Sleep

sleep 和 wakeup 是互斥锁的实现方式。复习一下我们学过的条件变量，这是操作系统给用户程序提供的 [同步原语](https://en.wikipedia.org/wiki/Event_(synchronization_primitive))，在我们的 libc 中，其依赖的系统调用是 futex（见 `libc/src/thread/pthread_cond_*.c`，不过我们的内核还未实现这个系统调用），而这通常会在内核中类似 sleep、wakeup 的过程来完成进程的睡眠等待和唤醒。

这两个操作类似于条件变量的 [wait](https://linux.die.net/man/3/pthread_cond_wait) 和 [signal](https://linux.die.net/man/3/pthread_cond_signal) 操作，其中 sleep 的函数声明是 `void sleep(void *chan, struct spinlock *lk)`，第一个是指需要等待的资源的地址（为什么这么设计？因为只要是内存中的资源，我们总能用它的地址来作为其唯一标识符），第二个是一个锁（用来保证从调用 sleep 开始到进程睡眠的过程是原子的）。wakeup 函数同理。

### 6.3.3 设备中断

设备中断的到来意味着 buffer 队首的请求执行完毕，于是我们会根据队首是请求类型进行不同的操作

- 读请求，合法的中断说明数据已从设备中读出，我们将其从 MMIO 中读取到 buffer 的 data 字段上即可
- 写请求，合法的中断说明数据已经写入了设备

然后清中断 flag，并继续进行下一个 buffer 的读写请求（如果有的话）

### 6.3.4 习题 2

请完成 `kern/proc.c` 中的 `sleep` 和 `wakeup` 函数，并简要描述并分析你的设计

### 6.3.5 习题 3

请完成 `kern/sd.c` 中的 `sd_init`，`sd_intr`，`sdrw`，然后分别在合适的地方调用 `sd_init` 和 `sd_test` 完成 SD 卡初始化并通过测试



## 6.4 制作启动盘

现代操作系统通常是作为一个硬盘镜像来发布的，而我们的也不例外。但在制作镜像时，需要注意遵守树莓派的规则，即第一个分区为启动分区，文件系统必须为 FAT32，剩下的分区可由我们自由分配。为了简便，我们采用主引导记录（[Master boot record, MBR](https://en.wikipedia.org/wiki/Master_boot_record)） 来进行分区，第一个分区和第二个分区均约为 64 MB，第二分区是根目录所在的文件系统（我们会在后续 lab 完成这一部分）。简言之，SD 卡上的布局如下，具体见 `mksd.mk`

```
 512B            FAT32       Your fancy fs
+-----+-----+--------------+----------------+
| MBR | ... | boot partion | root partition |
+-----+-----+--------------+----------------+
 \   1MB   / \    64MB    / \     63MB     /
  +-------+   +----------+   +------------+
```

### 6.4.1 MBR

MBR 位于设备的前 512B，有多种格式，不过大同小异，一种常见的格式如下表

| Address | Description                              | Size (bytes) |
| ------- | ---------------------------------------- | ------------ |
| 0x0     | Bootstrap code area and disk information | 446          |
| 0x1BE   | Partition entry 1                        | 16           |
| 0x1CE   | Partition entry 2                        | 16           |
| 0x1DE   | Partition entry 3                        | 16           |
| 0x1EE   | Partition entry 4                        | 16           |
| 0x1FE   | 0x55                                     | 1            |
| 0x1FF   | 0xAA                                     | 1            |

但这里我们只需要获得第二个分区的信息，即上表中的 Partition entry 2，这 16B 中有该分区的具体信息，包括它的起始 LBA 和分区大小（共含多少块）如下

| Offset (bytes) | Field length (bytes) | Description                                   |
| -------------- | -------------------- | --------------------------------------------- |
| ...            | ...                  | ...                                           |
| 0x8            | 4                    | LBA of first absolute sector in the partition |
| 0xC            | 4                    | Number of sectors in partition                |

### 6.4.2 启动分区

这是树莓派相关的，我们用 mtools 把 `boot/` 目录下官方提供的固件和配置文件 `config.txt` 以及内核镜像 `obj/kernel8.img` 复制到了该分区里。

### 6.4.3 根目录分区

这是我们的文件系统所在，（下一个 lab）需要我们手动完成文件系统设计和构建，见 `user/src/mkfs`，它是一个命令行工具

- `Makefile` 会调用 `user/Makefile`，后者会遍历 `user/src/XXX` 并用交叉编译器分别编译其目录下的所有文件为可执行文件 `user/bin/XXX`
- `mksd.mk` 会将 `user/src/mksd` 用当前编译器编译到 `obj/mksd`，然后执行 `obj/mksd obj/fs.img user/bin/A user/bin/B ...`，即用 `user/bin/A`、`user/bin/B` 等文件来构建我们的第二分区镜像 `obj/fs.img`
- 目前 `user/src/mksd` 的实现比较 naive，就是所有文件一个接一个地着写入 fs.img，后续实验中需要修改以符合自己设计的文件系统格式

### 6.4.4 习题 4

请在 `sd_init` 中解析 MBR 获得第二分区起始块的 LBA 和分区大小以便后续使用



## 6.5 libc

由于助教并不想写 C 库，于是搬运了一个完整的 C 库 [musl](https://musl.libc.org/) 作为一个 [git submodule](https://git-scm.com/book/en/v2/Git-Tools-Submodules) 以供使用。如此一来，我们只需要实现对应的系统调用，就能实现一个真正可用的操作系统了！

### 6.5.1 程序入口

在内核中 exec 启动一个用户态程序时，应该初始化 PC 到什么值呢？也就是用户态执行的第一条指定在哪？这个信息会被编译器编译到 ELF 头中，GCC 中默认是 _start 符号，可通过 linker script 中的 [ENTRY](https://sourceware.org/binutils/docs/ld/Entry-Point.html) 字段来自定义。内核的加载器（loader）通过解析这个 ELF 头即可得知。

### 6.5.2 crt

C 运行时（C Runtime）是一段运行于 main 函数前后的代码片段，gcc 会默认链接到可执行文件中（如 `libc/lib/crt1.o`），这是和编译器相关的，其他语言也可能有自己的运行时，如 C++ 需要运行时的支持来完成异常处理、内存管理（全局的构造、析构函数）。关于 main 之前的运行时，我们需要依次关注如下几个文件

- libc/arch/aarch64/crt_arch.h
- libc/crt/crt1.c
- libc/src/env/__libc_start_main.c

其中包含了一些初始化的代码，如 BSS 段清零、初始化 [TLS](https://en.wikipedia.org/wiki/Thread-local_storage)。我们可以**先修改 crt1.c 进行测试**，因为完整的启动流程需要实现多个系统调用。

### 6.5.3 系统调用

内核和 C 库通过系统调用交互，musl 中系统调用的实现在 `libc/arch/aarch64/syscall_arch.h`。对于内核来说，需要知道如下信息

- syscall number：在 `libc/obj/include/bits/syscall.h` 中
- syscall 函数声明：在 `libc/include/unistd.h`，调度相关的会在其他地方

我们以如何接入 printf 为例，我们需要知道它对应的系统调用是什么，在一切皆文件的 Unix 系统中，这其实就是向 stdout 文件写一些字符串（注意到 libc 帮我们完成了很多工作，如字符串 %d 格式化），至于这个文件是什么、具体的系统调用是什么，请自行在 `libc/src/stdio` 目录下全局搜索一下 stdout 后进行探索。

