# MiniOS Makefile
# Group 31 | IIT Jodhpur
# Updated for Sprint 5.
# NOTE: s5_integration.c lives in kernel/ (not a separate integration/ dir).

CC      = i686-elf-gcc
AS      = nasm
LD      = i686-elf-ld

CFLAGS  = -m32 -ffreestanding -fno-stack-protector -fno-builtin \
          -nostdlib -nostdinc -Wall -Wextra -Iinclude \
          -I/home/raunak/opt/cross/lib/gcc/i686-elf/13.2.0/include \
          -g
ASFLAGS = -f elf32 -g
LDFLAGS = -T linker.ld -nostdlib

# ── kernel / mm / scheduler: wildcards safe here ─────────────────────
# s5_integration.c is in kernel/ — picked up by kernel/*.c below.
C_SOURCES_WILD = $(wildcard kernel/*.c) \
                 $(wildcard mm/*.c) \
                 $(wildcard scheduler/*.c)

# ── arch/x86 C files: explicit list ─────────────────────────────────
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

# ── arch/x86 ASM files: explicit list ───────────────────────────────
#
#   EXCLUDED from build (kept on disk for Git history):
#     idt_asm.asm    — duplicate idt_flush symbol
#
#   S4: context_switch.asm added here (explicit) and excluded from
#       ARCH_ASM_WILD below to prevent it being linked twice.
#
ARCH_ASM_OBJ = arch/x86/idt_flush.o \
               arch/x86/isr_stubs.o \
               arch/x86/context_switch.o

# Other arch/x86 asm files from S1/S2 (paging_asm, gdt_flush, etc.)
# Excludes: idt_asm.asm (duplicate of idt_flush), and the three
# explicit files above (idt_flush, isr_stubs, context_switch).
ARCH_ASM_WILD = $(filter-out arch/x86/idt_asm.o, \
                  $(patsubst %.asm,%.o, \
                    $(filter-out arch/x86/idt_flush.asm \
                                 arch/x86/isr_stubs.asm \
                                 arch/x86/context_switch.asm, \
                      $(wildcard arch/x86/*.asm))))

# ── boot asm ────────────────────────────────────────────────────────
BOOT_ASM_OBJ = $(patsubst %.asm,%.o,$(wildcard boot/*.asm))

# ── Final OBJ list ───────────────────────────────────────────────────
OBJ = $(patsubst %.c,%.o,$(C_SOURCES_WILD)) \
      $(ARCH_C_OBJ) \
      $(ARCH_ASM_OBJ) \
      $(ARCH_ASM_WILD) \
      $(BOOT_ASM_OBJ)

# ── Targets ──────────────────────────────────────────────────────────
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

# -cpu qemu32  : use host-compatible CPU features
# -smp 1       : single virtual CPU (prevents SMP race conditions)
# -m 64M       : cap guest RAM at 64 MB
run: MiniOS.iso
	qemu-system-i386 -cdrom MiniOS.iso -serial stdio -cpu qemu32 -smp 1 -m 64M

debug: MiniOS.iso
	qemu-system-i386 -cdrom MiniOS.iso -serial stdio -s -S

gdb-debug: kernel.elf
	qemu-system-i386 -kernel kernel.elf -serial stdio -display none -m 64M -s -S

clean:
	rm -rf $(OBJ) kernel.elf isodir MiniOS.iso

.PHONY: all iso run debug gdb-debug clean