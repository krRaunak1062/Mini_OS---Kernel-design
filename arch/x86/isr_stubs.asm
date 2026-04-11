; arch/x86/isr_stubs.asm
; NASM stubs for CPU exception vectors 0-31 AND hardware IRQ 0-15.
;
; Implements: REQ-TASK-01, REQ-TASK-02, REQ-MEM-05, NFR-REL-01
;
; Owners:
;   Aman Yadav   (B24CS1006) — original S2 exception stubs
;   Raunak Kumar (B24CS1062) — S3 fixes and IRQ stubs
;
; ---------------------------------------------------------------
; CRITICAL: push order must match registers_t field order exactly.
;
; registers_t layout (include/isr.h), lowest offset first:
;   ds, edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax,  <- struct offsets 0..32
;   int_no, err_code,                                   <- offsets 36, 40
;   eip, cs, eflags                                     <- offsets 44, 48, 52 (CPU)
;
; Stack grows DOWN. The LAST thing pushed sits at the LOWEST address
; and maps to the LOWEST struct offset.
;
; So the save sequence must be (last-push = ds):
;   pushad          -> EDI..EAX at lowest addresses  (offsets 4..32)
;   push gs         -> gs  (offset... we don't use these in the struct,
;   push fs            so we save/restore them but they are ABOVE ds
;   push es            in the struct; simplest approach: don't expose
;   push ds         -> ds lands at lowest address = offset 0  ✓
;
; And the restore sequence is the exact mirror:
;   pop ds
;   pop es
;   pop fs
;   pop gs
;   popad
;
; ---------------------------------------------------------------
; Ring-0 → Ring-0 interrupt: CPU does NOT push ss/useresp.
; registers_t must NOT have those fields (fixed in isr.h).
; ---------------------------------------------------------------

[BITS 32]

; Exception stubs — vectors 0-31 (underscore names match isr.h / idt.c)
global isr_0,  isr_1,  isr_2,  isr_3,  isr_4,  isr_5,  isr_6,  isr_7
global isr_8,  isr_9,  isr_10, isr_11, isr_12, isr_13, isr_14, isr_15
global isr_16, isr_17, isr_18, isr_19, isr_20, isr_21, isr_22, isr_23
global isr_24, isr_25, isr_26, isr_27, isr_28, isr_29, isr_30, isr_31

; IRQ stubs — IRQ 0-15 → vectors 32-47 (after PIC remap)
global irq_0,  irq_1,  irq_2,  irq_3,  irq_4,  irq_5,  irq_6,  irq_7
global irq_8,  irq_9,  irq_10, irq_11, irq_12, irq_13, irq_14, irq_15

extern isr_handler   ; arch/x86/isr.c — S3 exception dispatcher
extern irq_handler   ; arch/x86/isr.c — S3 IRQ dispatcher

; ================================================================== ;
; MACRO: exception stub WITHOUT CPU error code                        ;
; ================================================================== ;
%macro ISR_NOERRCODE 1
isr_%1:
    push dword 0    ; dummy error code — uniform frame
    push dword %1   ; vector number  → becomes int_no
    jmp  common_isr_stub
%endmacro

; ================================================================== ;
; MACRO: exception stub WITH CPU error code (CPU already pushed it)  ;
; ================================================================== ;
%macro ISR_ERRCODE 1
isr_%1:
    push dword %1   ; vector number → int_no (error code already on stack)
    jmp  common_isr_stub
%endmacro

; ================================================================== ;
; MACRO: IRQ stub — pushes IRQ number (0-15), dummy err code 0       ;
; irq_handler() receives int_no = IRQ number directly (0-15),        ;
; so it must NOT subtract 32. See irq_handler() in isr.c.            ;
; ================================================================== ;
%macro IRQ_STUB 1
irq_%1:
    push dword 0    ; dummy error code
    push dword %1   ; IRQ number 0-15 → int_no
    jmp  common_irq_stub
%endmacro

; ================================================================== ;
; CPU exception stubs                                                 ;
; ================================================================== ;
ISR_NOERRCODE  0    ; #DE Divide Error
ISR_NOERRCODE  1    ; #DB Debug
ISR_NOERRCODE  2    ;     NMI
ISR_NOERRCODE  3    ; #BP Breakpoint
ISR_NOERRCODE  4    ; #OF Overflow
ISR_NOERRCODE  5    ; #BR BOUND range exceeded
ISR_NOERRCODE  6    ; #UD Invalid Opcode
ISR_NOERRCODE  7    ; #NM Device Not Available
ISR_ERRCODE    8    ; #DF Double Fault
ISR_NOERRCODE  9    ;     Coprocessor Segment Overrun
ISR_ERRCODE   10    ; #TS Invalid TSS
ISR_ERRCODE   11    ; #NP Segment Not Present
ISR_ERRCODE   12    ; #SS Stack-Segment Fault
ISR_ERRCODE   13    ; #GP General Protection Fault
ISR_ERRCODE   14    ; #PF Page Fault
ISR_NOERRCODE 15    ;     Reserved
ISR_NOERRCODE 16    ; #MF x87 FP Exception
ISR_ERRCODE   17    ; #AC Alignment Check
ISR_NOERRCODE 18    ; #MC Machine Check
ISR_NOERRCODE 19    ; #XM SIMD FP Exception
ISR_NOERRCODE 20    ;     Virtualisation Exception
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_ERRCODE   30    ; #SX Security Exception
ISR_NOERRCODE 31    ;     Reserved

; ================================================================== ;
; Hardware IRQ stubs — IRQ 0-15                                       ;
; ================================================================== ;
IRQ_STUB  0    ; PIT timer      (vector 32)
IRQ_STUB  1    ; Keyboard       (vector 33)
IRQ_STUB  2    ; Cascade        (vector 34)
IRQ_STUB  3    ; COM2           (vector 35)
IRQ_STUB  4    ; COM1           (vector 36)
IRQ_STUB  5    ; LPT2           (vector 37)
IRQ_STUB  6    ; Floppy         (vector 38)
IRQ_STUB  7    ; LPT1/Spurious  (vector 39)
IRQ_STUB  8    ; RTC            (vector 40)
IRQ_STUB  9    ; Available      (vector 41)
IRQ_STUB 10    ; Available      (vector 42)
IRQ_STUB 11    ; Available      (vector 43)
IRQ_STUB 12    ; PS/2 mouse     (vector 44)
IRQ_STUB 13    ; FPU            (vector 45)
IRQ_STUB 14    ; Primary ATA    (vector 46)
IRQ_STUB 15    ; Secondary ATA  (vector 47)

; ================================================================== ;
; common_isr_stub — shared entry for all CPU exceptions (0-31)        ;
; ================================================================== ;
; Stack on entry (top = lowest address):
;   [esp+0] int_no   (vector, pushed by macro)
;   [esp+4] err_code (real or dummy 0)
;   [esp+8] EIP, [esp+12] CS, [esp+16] EFLAGS  (CPU auto-push, ring-0)
;
; Save order — PUSHAD first, then segment regs, so that ds ends up
; at the LOWEST address on the stack, matching registers_t.ds at offset 0.
common_isr_stub:
    pushad              ; saves EAX..EDI; EDI ends up at lowest addr
    push gs
    push fs
    push es
    push ds             ; ds now at lowest addr = registers_t.ds offset 0 ✓

    mov  ax, 0x10       ; kernel data segment
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax

    mov  eax, esp       ; pointer to registers_t on stack
    push eax
    call isr_handler    ; arch/x86/isr.c
    add  esp, 4

    ; Restore in exact reverse order
    pop  ds
    pop  es
    pop  fs
    pop  gs
    popad

    add  esp, 8         ; discard int_no + err_code
    iret

; ================================================================== ;
; common_irq_stub — shared entry for all hardware IRQs (0-15)         ;
; ================================================================== ;
; Identical save/restore sequence; calls irq_handler instead.
common_irq_stub:
    pushad
    push gs
    push fs
    push es
    push ds

    mov  ax, 0x10
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax

    mov  eax, esp
    push eax
    call irq_handler    ; arch/x86/isr.c
    add  esp, 4

    pop  ds
    pop  es
    pop  fs
    pop  gs
    popad

    add  esp, 8
    iret
