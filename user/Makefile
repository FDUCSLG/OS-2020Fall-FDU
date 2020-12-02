CC = aarch64-linux-gnu-gcc
OBJDUMP = aarch64-linux-gnu-objdump
CFLAGS = -std=c99 -O3 -MMD -MP \
  -nostdinc -ffreestanding -static \
  -I../libc/include/ -I../libc/obj/include/
LIBC_A = ../libc/lib/libc.a
OBJ = ../obj/user

BIN := bin
SRC := src

USER_DIRS := $(shell find $(SRC) -maxdepth 1 -mindepth 1 -type d)
USER_BINS := $(USER_DIRS:$(SRC)/%=$(BIN)/%)

all:
	$(MAKE) -C ../libc
	$(MAKE) $(USER_BINS)

$(OBJ)/%.c.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<
	
# Same as `bin/NAME: $(OBJ)/NAME/*.c.o $(LIBC_A)`
# NOTE: Static library should be at last.
.SECONDEXPANSION:
$(BIN)/%: $$(addsuffix .o, $$(addprefix $(OBJ)/, $$(shell find $(SRC)/% -name "*.c"))) $(LIBC_A)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^ 
	$(OBJDUMP) -S -d $@ > $(OBJ)/src/$(notdir $@)/$(notdir $@).asm
	$(OBJDUMP) -x $@ > $(OBJ)/src/$(notdir $@)/$(notdir $@).hdr

clean:
	rm -rf $(OBJ) $(BIN)

.PHONY: all clean
