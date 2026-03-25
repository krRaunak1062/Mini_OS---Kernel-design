# MiniOS Makefile
# Group 31 | IIT Jodhpur

CC      = i686-elf-gcc
AS      = nasm
LD      = i686-elf-ld

CFLAGS  = -m32 -ffreestanding -fno-stack-protector -fno-builtin \
          -nostdlib -nostdinc -Wall -Wextra -Iinclude
ASFLAGS = -f elf32
LDFLAGS = -T linker.ld -nostdlib

# Source files
C_SOURCES   = $(wildcard kernel/*.c) \
              $(wildcard mm/*.c) \
              $(wildcard scheduler/*.c) \
              $(wildcard arch/x86/*.c)

ASM_SOURCES = $(wildcard boot/*.asm) \
              $(wildcard arch/x86/*.asm)

# Object files
OBJ = $(C_SOURCES:.c=.o) $(ASM_SOURCES:.asm=.o)

# Targets
all: kernel.elf

kernel.elf: $(OBJ)
	$(LD) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@

iso: kernel.elf
	mkdir -p isodir/boot/grub
	cp kernel.elf isodir/boot/kernel.elf
	echo 'set timeout=0'                          > isodir/boot/grub/grub.cfg
	echo 'set default=0'                         >> isodir/boot/grub/grub.cfg
	echo 'menuentry "MiniOS" {'                  >> isodir/boot/grub/grub.cfg
	echo '    multiboot /boot/kernel.elf'        >> isodir/boot/grub/grub.cfg
	echo '    boot'                              >> isodir/boot/grub/grub.cfg
	echo '}'                                     >> isodir/boot/grub/grub.cfg
	grub-mkrescue -o MiniOS.iso isodir

run: MiniOS.iso
	qemu-system-i386 -cdrom MiniOS.iso -serial stdio

debug: MiniOS.iso
	qemu-system-i386 -cdrom MiniOS.iso -serial stdio -s -S

clean:
	rm -rf $(OBJ) kernel.elf isodir MiniOS.iso

.PHONY: all iso run debug clean