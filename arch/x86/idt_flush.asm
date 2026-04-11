; arch/x86/idt_flush.asm
; Implements: REQ-TASK-01
;
; idt_flush(uint32_t idt_ptr_addr)
;   Loads the IDT pointer and returns.  Called from idt_init() in idt.c.
;   The argument arrives on the stack at [esp+4] (cdecl).

[BITS 32]
[GLOBAL idt_flush]

idt_flush:
    mov  eax, [esp+4]   ; idt_ptr_addr
    lidt [eax]          ; load IDT register
    ret
