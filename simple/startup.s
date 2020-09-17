.global _start
_start:
    ldr x30, =stack_top
    mov sp, x30
    bl c_entry
    b .
