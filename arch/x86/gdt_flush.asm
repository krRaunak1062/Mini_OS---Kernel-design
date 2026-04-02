; Implements: REQ-MEM-01
; void gdt_flush(uint32_t gdt_ptr_addr)
;   Loads the GDTR, then does a far jump to flush the CPU's
;   internal segment descriptor cache for CS.

[BITS 32]
[GLOBAL gdt_flush]

gdt_flush:
    mov  eax, [esp+4]    ; First function argument = pointer to gdt_ptr_t
    lgdt [eax]           ; Load the GDT register (GDTR) — but CS still stale!

    ; Far jump: "jmp <selector>:<offset>"
    ; This forces the CPU to reload CS from GDT entry 1 (selector 0x08)
    ; After this instruction, CS points to your new kernel code segment.
    jmp  0x08:.flush_cs

.flush_cs:
    ; Now reload all the data segment registers too.
    ; They all point to kernel data segment (selector 0x10).
    mov  ax, 0x10
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    mov  ss, ax
    ret