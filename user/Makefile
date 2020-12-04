LIBC_A = ../libc/lib/libc.a
LIBC_SPEC = ../libc/lib/musl-gcc.specs
LIBC_LIBS = $(wildcard ../libc/lib/*)
LIBC_INCS = $(wildcard ../libc/obj/include/**/*) $(wildcard ../libc/include/**/*)
LIBC_DEPS = $(LIBC_LIBS) $(LIBC_INCS)

OBJ = ../obj/user
LIBC_SPEC_OUT = $(OBJ)/musl-gcc.specs

CC = aarch64-linux-gnu-gcc -specs $(LIBC_SPEC_OUT)
OBJDUMP = aarch64-linux-gnu-objdump
CFLAGS = -std=c99 -O3 -MMD -MP -static -I../libc/obj/include/

BIN := $(OBJ)/bin
SRC := src

USER_DIRS := $(shell find $(SRC) -maxdepth 1 -mindepth 1 -type d)
USER_BINS := $(USER_DIRS:$(SRC)/%=$(BIN)/%)

all:
	$(MAKE) -C ../libc
	mkdir -p $(dir $(LIBC_SPEC_OUT))
	# Replace "/usr/local/musl" to "../libc"
	sed -e "s/\/usr\/local\/musl/..\/libc/g" $(LIBC_SPEC) > $(LIBC_SPEC_OUT)
	$(MAKE) $(USER_BINS)

$(OBJ)/%.c.o: %.c $(LIBC_DEPS)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<
	
# Same as `bin/NAME: $(OBJ)/NAME/*.c.o $(LIBC_A)`
# NOTE: Static library should be at last.
.SECONDEXPANSION:
$(BIN)/%: $$(addsuffix .o, $$(addprefix $(OBJ)/, $$(shell find $(SRC)/% -name "*.c")))
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^
	$(OBJDUMP) -S -d $@ > $(OBJ)/src/$(notdir $@)/$(notdir $@).asm
	$(OBJDUMP) -x $@ > $(OBJ)/src/$(notdir $@)/$(notdir $@).hdr

clean:
	rm -rf $(OBJ) $(BIN)

.PHONY: all clean
