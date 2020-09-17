GCCPREFIX := aarch64-linux-gnu-

CC := $(GCCPREFIX)gcc
LD := $(GCCPREFIX)ld
AS := $(GCCPREFIX)as

OBJCOPY := $(GCCPREFIX)objcopy

RM := rm -f

CFLAGS := -Wall -nostdlib -g

OBJ := hello.o startup.o

.PHONY: all

all: hello.img

hello.img: $(OBJ) Makefile
	$(LD) $(OBJ) -T link.ld -o hello.elf
	$(OBJCOPY) -O binary hello.elf $@

%.o: %.s
	$(AS) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

run: hello.img
	qemu-system-aarch64 -M raspi3 -kernel $< -nographic

qemu-gdb: hello.img
	qemu-system-aarch64 -M raspi3 -kernel $< -gdb tcp::1234 -nographic -S

gdb:
	# gdb-multiarch -n -x .gdbinit
	aarch64-linux-gdb -x .gdbinit

clean:
	$(RM) hello.img hello.elf $(OBJ)