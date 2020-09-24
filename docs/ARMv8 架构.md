## ARMv8 架构

　　ARM 同 MIPS 一样是一种精简指令架构（RISC），它相较复杂指令架构（CISC，如 X86）而言提供更简单的指令操作与访存模型（LOAD/STORE）。以对内存中某一个值加一为例，X86 的 `INC` 可直接将内存中的值加一，而 ARM 必须通过 `LOAD` 将内存中的值读入寄存器、`ADD` 将寄存器加一、`STORE` 将寄存器写回内存。虽然 CISC 的指令功能更强大，但简单的指令操作可以缩短 RISC 的时钟周期以加快硬件执行。

　　关于 ARM 架构的一些简单介绍可以参考 Stanford[^Stanford] *Phase1: ARM and a Leg(Subphase A-C)*。*A Guide to ARM64 / AArch64 Assembly on Linux with Shellcodes and Cryptography*[^Assembly] 也提供关于 ARM 架构的丰富内容。

### 参考文献

[^Stanford]:https://cs140e.sergio.bz/assignments/3-spawn/
[^Assembly]:https://modexp.wordpress.com/2018/10/30/arm64-assembly/