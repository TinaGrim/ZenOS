C_SOURCES = $(wildcard kernel/*.c drivers/*.c cpu/*.c libc/*.c)
HEADERS = $(wildcard kernel/*.h drivers/*.h cpu/*.h libc/*.h)
# Nice syntax for file extension replacement
OBJ = ${C_SOURCES:.c=.o} cpu/interrupt.o cpu/switch.o 

# Change this if your cross-compiler is somewhere else
CC = i386-elf-gcc
GDB = gdb
# -g: Use debugging symbols in gcc
# Add include paths so headers like libc/string.h are found
CFLAGS = -g

# First rule is run by default
os-image.bin: boot/main.bin kernel.bin kernel.elf user/user.bin disk.img
	cat boot/main.bin kernel.bin > os-image.bin
	dd if=user/user.bin of=disk.img bs=512 seek=100 conv=notrunc status=none

# Demo ring-3 user program: flat binary linked at USER_BIN_ADDR (0x100000),
# injected into a fixed sector range of disk.img (see USER_BIN_* in
# kernel.h). Sector 100 is past the ZFS1 filesystem's 16x4-sector regions
# (sectors 2..65), so it never collides with file data.
# Max 2048 bytes; 4 sectors.
user/user.bin: user/user.c user/user.lds
	mkdir -p user
	${CC} ${CFLAGS} -ffreestanding -c user/user.c -o user/user.o
	i386-elf-ld -T user/user.lds -o user/user.elf user/user.o
	i386-elf-objcopy -O binary user/user.elf $@
	@if [ $$(stat -c %s $@) -gt 2048 ]; then \
		echo "ERROR: user program exceeds 2048 bytes (4 sectors)"; \
		exit 1; \
	fi
	truncate -s 2048 $@

# '--oformat binary' deletes all symbols as a collateral, so we don't need
# to 'strip' them manually on this case.
# Pad to a fixed size (see KERNEL_LOAD_SECTORS in boot/main.asm): keeps
# int 0x13 disk reads from overrunning the image. Bump BOTH when the
# kernel grows past this.
KERNEL_SECTORS = 40
kernel.bin: boot/kernel_entry.o ${OBJ}
	i386-elf-ld -o $@ -Ttext 0x1000 $^ --oformat binary
	@if [ $$(stat -c %s $@) -gt $$((${KERNEL_SECTORS} * 512)) ]; then \
		echo "ERROR: kernel too big for ${KERNEL_SECTORS} sectors - increase KERNEL_SECTORS (makefile) and KERNEL_LOAD_SECTORS (boot/main.asm)"; \
		exit 1; \
	fi
	truncate -s $$((${KERNEL_SECTORS} * 512)) $@

# Used for debugging purposes
kernel.elf: boot/kernel_entry.o ${OBJ}
	i386-elf-ld -o $@ -Ttext 0x1000 $^ 

# Blank persistent disk image (created once; survives 'make clean')
disk.img:
	qemu-img create -f raw disk.img 10M

run: os-image.bin disk.img
	qemu-system-i386 -fda os-image.bin -hda disk.img
runc: clean os-image.bin disk.img
	qemu-system-i386 -fda os-image.bin -hda disk.img
# Open the connection to qemu and load our kernel-object file with symbols
debug: os-image.bin disk.img
	qemu-system-i386 -s -fda os-image.bin -hda disk.img &
	${GDB} -ex "target remote localhost:1234" -ex "symbol-file kernel.elf"

# Generic rules for wildcards
# To make an object, always compile from its .c
%.o: %.c ${HEADERS}
	${CC} ${CFLAGS} -ffreestanding -c $< -o $@

%.o: %.asm
	nasm $< -f elf -o $@

%.bin: %.asm
	nasm $< -f bin -o $@

clean:
	rm -rf *.bin *.dis *.o os-image.bin *.elf
	rm -rf kernel/*.o boot/*.bin drivers/*.o boot/*.o cpu/*.o libc/*.o
	rm -rf user/*.o user/*.elf user/user.bin
