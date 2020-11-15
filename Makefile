CROSS := aarch64-linux-gnu
CC := $(CROSS)-gcc
LD := $(CROSS)-ld
OBJDUMP := $(CROSS)-objdump
OBJCOPY := $(CROSS)-objcopy


COPY := cp -f

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

# link the libgcc.a for __aeabi_idiv. ARM has no native support for div
LIBS = $(LIBGCC)

CFLAGS := -Wall -g \
          -fno-pie -no-pie -fno-pic \
		  -fno-omit-frame-pointer -fno-stack-protector \
          -fno-zero-initialized-in-bss \
		  -fno-strict-aliasing \
          -static -fno-builtin -nostdlib -ffreestanding -nostartfiles \
          -mgeneral-regs-only \
          -MMD -MP

ASFLAGS := -march=armv8-a

V := @
# Run 'make V=1' to turn on verbose commands
ifeq ($(V),1)
override V =
endif

V := @
# Run 'make V=1' to turn on verbose commands
ifeq ($(V),1)
override V =
endif

CFLAGS += -Iinc -mcmodel=large -mpc-relative-literal-loads
ASFLAGS += -Iinc
SRC_DIRS := kern
USR_DIRS := user
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

$(BUILD_DIR)/$(USR_DIRS)/initcode: $(USR_DIRS)/initcode.S
	@echo + as $<
	@mkdir -p $(dir $@)
	$(V)$(CC) $(CFLAGS) -c -o $(BUILD_DIR)/$(USR_DIRS)/initcode.o $<
	@echo + ld $(BUILD_DIR)/$(USR_DIRS)/initcode.out
	$(V)$(LD) -N -e start -Ttext 0 -o $(BUILD_DIR)/$(USR_DIRS)/initcode.out $(BUILD_DIR)/$(USR_DIRS)/initcode.o
	@echo + objcopy $@
	$(V)$(OBJCOPY) -S -O binary --prefix-symbols="_binary_$@" $(BUILD_DIR)/$(USR_DIRS)/initcode.out $@
	@echo + objdump $(BUILD_DIR)/$(USR_DIRS)/initcode.o
	$(V)$(OBJDUMP) -S $(BUILD_DIR)/$(USR_DIRS)/initcode.o > $(BUILD_DIR)/$(USR_DIRS)/initcode.asm

$(KERN_ELF): kern/linker.ld $(OBJS) $(BUILD_DIR)/$(USR_DIRS)/initcode
	@echo + ld $@
	$(V)$(LD) -T $< -o $@ $(OBJS) $(LIBS) -b binary $(BUILD_DIR)/$(USR_DIRS)/initcode
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
	gdb-multiarch -n -x .gdbinit

clean:
	rm -r $(BUILD_DIR)
