; boot/boot.asm
; Multiboot header + kernel entry point

bits 32

; Multiboot constants
MULTIBOOT_MAGIC     equ 0x1BADB002
MULTIBOOT_FLAGS     equ 0x00000003
MULTIBOOT_CHECKSUM  equ -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

; Kernel stack size
STACK_SIZE          equ 0x4000      ; 16KB stack

section .multiboot
align 4
    dd MULTIBOOT_MAGIC
    dd MULTIBOOT_FLAGS
    dd MULTIBOOT_CHECKSUM

section .bss
align 16
stack_bottom:
    resb STACK_SIZE
stack_top:

section .text
global boot_start
extern kernel_main

boot_start:
    ; Set up the stack
    mov esp, stack_top

    ; Push multiboot info pointer and magic for kernel_main
    push ebx        ; multiboot info struct pointer
    push eax        ; multiboot magic number

    ; Call the C kernel
    call kernel_main

    ; If kernel_main returns, hang forever
.hang:
    cli
    hlt
    jmp .hang