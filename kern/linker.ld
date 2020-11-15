OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(_start)

SECTIONS
{
    . = 0xFFFF000000080000;
    .text : {
        KEEP(*(.text.boot))
        *(.text .stub .gnu.linkonce.t.*)
    }
    PROVIDE(etext = .);
    .rodata : { *(.rodata .rodata.* .gnu.linkonce.r.*) }
	/* Adjust the address for the data segment to the next page */
	. = ALIGN(0x1000);
    PROVIDE(data = .);
    .data : { *(.data) }
    PROVIDE(edata = .);
    .bss : { *(.bss) }
    PROVIDE(end = .);
    /DISCARD/ : {
		*(.eh_frame .note.GNU-stack)
	}
}
