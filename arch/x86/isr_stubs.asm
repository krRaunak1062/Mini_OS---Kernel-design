; arch/x86/isr_stubs.asm
; NASM stubs for CPU exception vectors 0–31.
;
; Implements: REQ-TASK-01 (interrupt infrastructure — shared with S3)
;             REQ-MEM-05  (ISR 14 page fault stub)
;             NFR-REL-01  (ISR 13 GPF stub)
;
; Owner : Aman Yadav (B24CS1006) for ISR 14 stub (S2)
;         Raunak Kumar (B24CS1062) for full IDT wiring (S3)
;
; x86 exception entry protocol:
;   - CPU pushes SS, ESP (if CPL change), EFLAGS, CS, EIP onto stack.
;   - For exceptions WITH an error code (8, 10–14, 17, 30):
;       CPU also pushes the error code automatically.
;   - For exceptions WITHOUT an error code: we push a dummy 0
;       to keep the stack frame uniform.
;   - We then push the vector number and jump to common_isr_stub.
;
; Stack layout on entry to common_isr_stub (top = low address):
;   [esp+0]  : vector number (pushed by our stub)
;   [esp+4]  : error code    (real or dummy 0)
;   [esp+8]  : EIP           (CPU auto-push)
;   [esp+12] : CS            (CPU auto-push)
;   [esp+16] : EFLAGS        (CPU auto-push)
;   (+ ESP, SS if ring change)

[BITS 32]

; ---- Exported symbols used by IDT init (S3) ---- ;
global isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7
global isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15
global isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
global isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31

; C handler called from common_isr_stub
extern isr_common_handler   ; defined in arch/x86/isr_handler.c (S3)
                             ; For S2, only isr_14_handler is needed —
                             ; isr_common_handler dispatches to it.

; ================================================================== ;
; MACRO: ISR stub WITHOUT error code (CPU does not push one)         ;
; ================================================================== ;
%macro ISR_NOERRCODE 1
isr%1:
    push dword 0    ; dummy error code — keeps stack frame uniform
    push dword %1   ; vector number
    jmp  common_isr_stub
%endmacro

; ================================================================== ;
; MACRO: ISR stub WITH error code (CPU pushes it automatically)      ;
; ================================================================== ;
%macro ISR_ERRCODE 1
isr%1:
                    ; error code already on stack from CPU
    push dword %1   ; vector number
    jmp  common_isr_stub
%endmacro

; ================================================================== ;
; Exception stubs — which ones have error codes:                     ;
;   8 (DF), 10 (TS), 11 (NP), 12 (SS), 13 (GP), 14 (PF),          ;
;   17 (AC), 30 (SX)                                                 ;
; ================================================================== ;
ISR_NOERRCODE  0    ; #DE  Divide Error
ISR_NOERRCODE  1    ; #DB  Debug
ISR_NOERRCODE  2    ;      NMI
ISR_NOERRCODE  3    ; #BP  Breakpoint
ISR_NOERRCODE  4    ; #OF  Overflow
ISR_NOERRCODE  5    ; #BR  BOUND range exceeded
ISR_NOERRCODE  6    ; #UD  Invalid Opcode
ISR_NOERRCODE  7    ; #NM  Device Not Available
ISR_ERRCODE    8    ; #DF  Double Fault
ISR_NOERRCODE  9    ;      Coprocessor Segment Overrun (legacy)
ISR_ERRCODE   10    ; #TS  Invalid TSS
ISR_ERRCODE   11    ; #NP  Segment Not Present
ISR_ERRCODE   12    ; #SS  Stack-Segment Fault
ISR_ERRCODE   13    ; #GP  General Protection Fault  ← GPF handler (S2/S3)
ISR_ERRCODE   14    ; #PF  Page Fault                ← page fault handler (S2)
ISR_NOERRCODE 15    ;      Reserved
ISR_NOERRCODE 16    ; #MF  x87 Floating-Point Exception
ISR_ERRCODE   17    ; #AC  Alignment Check
ISR_NOERRCODE 18    ; #MC  Machine Check
ISR_NOERRCODE 19    ; #XM  SIMD Floating-Point Exception
ISR_NOERRCODE 20    ;      Virtualisation Exception
ISR_NOERRCODE 21    ;      Reserved
ISR_NOERRCODE 22    ;      Reserved
ISR_NOERRCODE 23    ;      Reserved
ISR_NOERRCODE 24    ;      Reserved
ISR_NOERRCODE 25    ;      Reserved
ISR_NOERRCODE 26    ;      Reserved
ISR_NOERRCODE 27    ;      Reserved
ISR_NOERRCODE 28    ;      Reserved
ISR_NOERRCODE 29    ;      Reserved
ISR_ERRCODE   30    ; #SX  Security Exception
ISR_NOERRCODE 31    ;      Reserved

; ================================================================== ;
; common_isr_stub                                                     ;
; ================================================================== ;
; Saves ALL general-purpose registers + data segments, then calls
; the C dispatcher isr_common_handler(registers_t *regs).
;
; Stack on entry (already has: error_code, vector from macro above,
; plus EIP/CS/EFLAGS from CPU):
common_isr_stub:
    ; Save data segment
    push ds
    push es
    push fs
    push gs

    ; Save all general-purpose registers
    ; Order matches registers_t layout in include/isr.h
    pushad              ; EAX ECX EDX EBX ESP EBP ESI EDI

    ; Load kernel data segment (0x10 = second GDT entry, kernel data)
    mov  ax, 0x10
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax

    ; Pass pointer to the saved register frame as first argument
    mov  eax, esp
    push eax            ; push registers_t * as argument

    call isr_common_handler   ; C dispatcher in arch/x86/isr_handler.c

    add  esp, 4         ; clean up argument

    ; Restore state in reverse order
    popad               ; restore EAX–EDI
    pop  gs
    pop  fs
    pop  es
    pop  ds

    ; Remove vector + error_code that our stubs pushed
    add  esp, 8

    ; Return from interrupt — restores EIP, CS, EFLAGS (+ SS, ESP if ring change)
    iret
