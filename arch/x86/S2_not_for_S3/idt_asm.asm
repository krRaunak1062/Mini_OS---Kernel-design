; arch/x86/idt_asm.asm
; LIDT instruction wrapper.
;
; Owner : Aman Yadav (B24CS1006) — S2 minimal
;         Raunak Kumar (B24CS1062) — S3 will use or replace this
;
; Implements: REQ-MEM-05 (part of IDT load for page fault handler)

[BITS 32]
global idt_flush

; idt_flush(uint32_t idtr_addr)
; Loads the IDTR register from the 6-byte IDTR struct at idtr_addr.
; After this the CPU uses the new IDT for all exceptions and IRQs.
idt_flush:
    mov eax, [esp+4]    ; idtr_addr = pointer to {limit, base}
    lidt [eax]          ; load IDT register
    ret
