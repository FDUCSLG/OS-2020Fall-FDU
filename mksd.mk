SD_IMG ?=
KERN_IMG ?=

BOOT_IMG := $(BUILD_DIR)/boot.img
FS_IMG := $(BUILD_DIR)/fs.img

SECTOR_SIZE := 512

# The total sd card image is 128 MB, 64 MB for boot sector and 64 MB for file system.
SECTORS := 256*1024
BOOT_OFFSET := 2048
BOOT_SECTORS= 128*1024
FS_OFFSET := $$(($(BOOT_OFFSET)+$(BOOT_SECTORS)))
FS_SECTORS := $$(($(SECTORS)-$(FS_OFFSET)))

.DELETE_ON_ERROR: $(BOOT_IMG) $(SD_IMG)

# TODO: Detect img size automatically
$(BOOT_IMG): $(KERN_IMG) $(shell find boot/*)
	dd if=/dev/zero of=$@ seek=$$(($(BOOT_SECTORS) - 1)) bs=$(SECTOR_SIZE) count=1
	# -F 32 specify FAT32
	# -s 1 specify one sector per cluster so that we can create a smaller one
	mkfs.vfat -F 32 -s 1 $@
	# Copy files into boot partition
	$(foreach x, $^, mcopy -i $@ $(x) ::$(notdir $(x));)

$(FS_IMG): $(shell find obj/user/bin -type f)
	echo $^
	cc $(shell find user/src/mkfs/ -name "*.c") -o obj/mkfs
	./obj/mkfs $@ $^

$(SD_IMG): $(BOOT_IMG) $(FS_IMG)
	dd if=/dev/zero of=$@ seek=$$(($(SECTORS) - 1)) bs=$(SECTOR_SIZE) count=1
	printf "                                                                \
	  $(BOOT_OFFSET), $$(($(BOOT_SECTORS)*$(SECTOR_SIZE)/1024))K, c,\n      \
	  $(FS_OFFSET), $$(($(FS_SECTORS)*$(SECTOR_SIZE)/1024))K, L,\n          \
	" | sfdisk $@
	dd if=$(BOOT_IMG) of=$@ seek=$(BOOT_OFFSET) conv=notrunc
	dd if=$(FS_IMG) of=$@ seek=$(FS_OFFSET) conv=notrunc

