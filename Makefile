# MiniOS Makefile
# Group 31 | IIT Jodhpur

CC      = i686-elf-gcc
AS      = nasm
LD      = i686-elf-ld

CFLAGS  = -m32 -ffreestanding -fno-stack-protector -fno-builtin \
          -nostdlib -nostdinc -Wall -Wextra -Iinclude \
          -I/home/raunak/opt/cross/lib/gcc/i686-elf/13.2.0/include \
          -g
ASFLAGS = -f elf32 -g
LDFLAGS = -T linker.ld -nostdlib

# ── kernel / mm / scheduler: wildcards safe here (no conflicts) ──
C_SOURCES_WILD = $(wildcard kernel/*.c) \
                 $(wildcard mm/*.c) \
                 $(wildcard scheduler/*.c)

# ── arch/x86 C files: explicit list ──────────────────────────────
#
#   EXCLUDED from build (kept on disk for Git history):
#     idt_minimal.c  — duplicate idt_entries[256], idt.c is superset
#
ARCH_C_OBJ = arch/x86/gdt.o \
             arch/x86/paging.o \
             arch/x86/isr_handler.o \
             arch/x86/isr14.o \
             arch/x86/idt.o \
             arch/x86/isr.o \
             arch/x86/pic.o \
             arch/x86/pit.o

# ── arch/x86 ASM files: explicit list ────────────────────────────
#
#   EXCLUDED from build (kept on disk for Git history):
#     idt_asm.asm    — duplicate idt_flush symbol, idt_flush.asm is identical
#
ARCH_ASM_OBJ = arch/x86/idt_flush.o \
               arch/x86/isr_stubs.o

# Other arch/x86 asm files from S1/S2 (paging_asm, gdt_flush, etc.)
# excluding idt_asm.asm (duplicate) and the two already listed above
ARCH_ASM_WILD = $(filter-out arch/x86/idt_asm.o, \
                  $(patsubst %.asm,%.o, \
                    $(filter-out arch/x86/idt_flush.asm arch/x86/isr_stubs.asm, \
                      $(wildcard arch/x86/*.asm))))

# ── boot asm ─────────────────────────────────────────────────────
BOOT_ASM_OBJ = $(patsubst %.asm,%.o,$(wildcard boot/*.asm))

# ── Final OBJ list ───────────────────────────────────────────────
OBJ = $(patsubst %.c,%.o,$(C_SOURCES_WILD)) \
      $(ARCH_C_OBJ) \
      $(ARCH_ASM_OBJ) \
      $(ARCH_ASM_WILD) \
      $(BOOT_ASM_OBJ)

# ── Targets ──────────────────────────────────────────────────────
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

gdb-debug: kernel.elf
	qemu-system-i386 -kernel kernel.elf -serial stdio -display none -m 64M -s -S

clean:
	rm -rf $(OBJ) kernel.elf isodir MiniOS.iso

.PHONY: all iso run debug gdb-debug clean