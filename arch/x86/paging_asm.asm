; arch/x86/paging_asm.asm
; CR3 load and CR0 paging enable stubs.
;
; Implements: REQ-MEM-02
; Owner : Aman Yadav (B24CS1006)
; Sprint: S2 — Virtual Memory & Kernel Heap
;
; WHY NASM (not inline asm)?
;   SRS §7.5 / NFR-MAINT-01 require all CR0/CR3 I/O to live in
;   arch/x86/ and be clearly separated from C code.  A standalone
;   NASM file makes that boundary explicit and easier to audit in S5.
;
; Calling convention: cdecl (i686-elf-gcc default)
;   - First argument at [esp+4]
;   - Callee preserves EBX, ESI, EDI, EBP
;   - Return value in EAX

[BITS 32]

global paging_load_directory   ; void paging_load_directory(uint32_t phys)
global paging_enable            ; void paging_enable(void)
global paging_disable           ; void paging_disable(void) [utility]
global paging_get_cr2           ; uint32_t paging_get_cr2(void)
global paging_get_cr3           ; uint32_t paging_get_cr3(void)

; ------------------------------------------------------------------ ;
; paging_load_directory(uint32_t phys_addr)                          ;
; ------------------------------------------------------------------ ;
; Loads the physical address of a page directory into CR3 (PDBR).
; This also flushes the TLB entirely (CR3 load always flushes TLB).
;
; Must be called BEFORE paging_enable().
; The address must be the PHYSICAL address of the page directory —
; not a virtual address.
paging_load_directory:
    mov eax, [esp+4]    ; first argument = physical address of PD
    mov cr3, eax        ; load into CR3 (Page Directory Base Register)
                        ; this flushes the entire TLB
    ret

; ------------------------------------------------------------------ ;
; paging_enable()                                                     ;
; ------------------------------------------------------------------ ;
; Sets bit 31 (PG = Paging Enable) of CR0.
; After this instruction the CPU translates ALL memory accesses
; through the page tables loaded in CR3.
;
; CRITICAL: call this ONLY after:
;   1. paging_load_directory() has been called with a valid PD
;   2. Identity map (0–4MB) is set up in that PD
;   3. Kernel higher-half map (0xC0000000) is set up in that PD
;   4. ISR 14 handler is installed in the IDT
paging_enable:
    mov eax, cr0
    or  eax, 0x80000000 ; set bit 31 (PG)
    mov cr0, eax        ; paging is now ACTIVE
                        ; next fetch is translated through page tables
    ret

; ------------------------------------------------------------------ ;
; paging_disable()   [utility — useful for debugging only]           ;
; ------------------------------------------------------------------ ;
; Clears CR0 bit 31 to turn paging off.
; NOTE: After calling this, all virtual→physical mappings are gone.
; Use only in early debug sessions before the kernel depends on
; virtual addresses.
paging_disable:
    mov eax, cr0
    and eax, 0x7FFFFFFF ; clear bit 31 (PG)
    mov cr0, eax
    ret

; ------------------------------------------------------------------ ;
; paging_get_cr2() → uint32_t                                        ;
; ------------------------------------------------------------------ ;
; Returns the current value of CR2, which holds the faulting virtual
; address after a page fault (ISR 14).
; Used by isr_14_handler() in arch/x86/isr14.c.
paging_get_cr2:
    mov eax, cr2
    ret

; ------------------------------------------------------------------ ;
; paging_get_cr3() → uint32_t                                        ;
; ------------------------------------------------------------------ ;
; Returns the current CR3 value (physical address of active PD).
; Useful for GDB verification and S5 integration tests.
paging_get_cr3:
    mov eax, cr3
    ret
