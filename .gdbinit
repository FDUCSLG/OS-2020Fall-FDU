set architecture aarch64
target remote localhost:1234
file obj/kernel8.elf

# Uncomment the following line and change
# PAHT_TO_PWNDBG to your pwndbg directory to enable pwndbg
# source /home/sunflower/Downloads/pwndbg/gdbinit.py