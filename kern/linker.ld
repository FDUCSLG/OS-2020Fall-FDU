SECTIONS
{
    . = 0xFFFF000000080000;
    .text : {
        KEEP(*(.text.boot))
        *(.text)
    }
    PROVIDE(etext = .);
    .rodata : { *(.rodata) }
    PROVIDE(data = .);
    .data : { *(.data) }
    PROVIDE(edata = .);
    .bss : { *(.bss) }
    PROVIDE(end = .);
}
