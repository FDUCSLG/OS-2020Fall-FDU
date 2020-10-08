CROSS := aarch64-linux-gnu
CC := $(CROSS)-gcc
LD := $(CROSS)-ld
OBJDUMP := $(CROSS)-objdump
OBJCOPY := $(CROSS)-objcopy

# -fno-pie -fno-pic:
#     Remove .got and data.rel sections.
#     Run `objdump -t obj/kernel8.elf | sort` to see the difference
#
# -nostartfiles:
#     Don’t use standard startup files. Startup files are responsible for
#     setting an initial stack pointer, initializing static data, and jumping
#     to the main entry point. We are going to do all of this by ourselves.
#
# -mgeneral-regs-only:
#     Use only general-purpose registers. ARM processors also have NEON
#     registers. We don’t want the compiler to use them because they add
#     additional complexity (since, for example, we will need to store the
#     registers during a context switch).
#
# -MMD -MP:
#     generate .d files

CFLAGS := -Wall -g \
          -fno-pie -fno-pic -fno-stack-protector \
          -fno-zero-initialized-in-bss \
          -static -fno-builtin -nostdlib -ffreestanding -nostartfiles \
          -mgeneral-regs-only \
          -MMD -MP

V := @
# Run 'make V=1' to turn on verbose commands
ifeq ($(V),1)
override V =
endif

CFLAGS += -Iinc
SRC_DIRS := kern
BUILD_DIR = obj

KERN_ELF := $(BUILD_DIR)/kernel8.elf
KERN_IMG := $(BUILD_DIR)/kernel8.img

.PHONY: all clean

all: $(KERN_IMG)

# Automatically find sources and headers
SRCS := $(shell find $(SRC_DIRS) -name *.c -or -name *.S)
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)
-include $(DEPS)

$(BUILD_DIR)/%.c.o: %.c
	@echo + cc $<
	@mkdir -p $(dir $@)
	$(V)$(CC) $(CFLAGS) -c -o $@ $<
	
$(BUILD_DIR)/%.S.o: %.S
	@echo + as $<
	@mkdir -p $(dir $@)
	$(V)$(CC) $(CFLAGS) -c -o $@ $<

$(KERN_ELF): kern/linker.ld $(OBJS)
	@echo + ld $@
	$(V)$(LD) -o $@ -T $< $(OBJS)
	@echo + objdump $@
	$(V)$(OBJDUMP) -S -d $@ > $(basename $@).asm
	$(V)$(OBJDUMP) -x $@ > $(basename $@).hdr

$(KERN_IMG): $(KERN_ELF)
	@echo + objcopy $@
	$(V)$(OBJCOPY) -O binary $< $@

QEMU := qemu-system-aarch64 -M raspi3 -nographic -serial null -serial mon:stdio

qemu: $(KERN_IMG) 
	$(QEMU) -kernel $<

qemu-gdb: $(KERN_IMG)
	$(QEMU) -kernel $< -S -gdb tcp::1234

gdb: 
	aarch64-linux-gdb -x .gdbinit

clean:
	rm -r $(BUILD_DIR)
