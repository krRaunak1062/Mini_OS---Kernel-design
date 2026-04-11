; arch/x86/context_switch.asm
; Implements: REQ-TASK-05  (NFR-PERF-01: must complete in < 50µs)
;
; void context_switch(tcb_t *old_task, tcb_t *new_task)
;
; ---------------------------------------------------------------
; TCB FIELD OFFSETS — must match tcb_t in include/task.h exactly.
; Verified by _Static_assert in scheduler/task.c.
; ---------------------------------------------------------------
TCB_PID        equ  0
TCB_STATE      equ  4
TCB_KERNEL_ESP equ  8    ; uint32_t  saved stack pointer
TCB_PAGE_DIR   equ  12   ; uint32_t  physical address for CR3
TCB_NEXT       equ  16
TCB_STACK_BASE equ  20
;
; ---------------------------------------------------------------
; CALLING CONVENTION: cdecl (i686-elf-gcc default)
;   [esp+0]  return address      (before any pushes)
;   [esp+4]  old_task  (arg1)
;   [esp+8]  new_task  (arg2)
;
; WHAT WE PUSH (48 bytes total):
;   pushfd          +4   eflags
;   push  0x08      +4   cs
;   push  .resume   +4   eip  (resume point for THIS task when switched back)
;   push  ds        +4   ds
;   pushad          +32  edi esi ebp esp ebx edx ecx eax
;
; After all pushes the stack looks like:
;   [esp+0 ]  edi   \
;   [esp+4 ]  esi    |
;   [esp+8 ]  ebp    | PUSHAD block
;   [esp+12]  esp    |  (dummy; POPAD skips this)
;   [esp+16]  ebx    |
;   [esp+20]  edx    |
;   [esp+24]  ecx    |
;   [esp+28]  eax   /
;   [esp+32]  ds
;   [esp+36]  eip (.resume)  \
;   [esp+40]  cs  (0x08)      | IRET frame
;   [esp+44]  eflags         /
;   [esp+48]  return address
;   [esp+52]  old_task
;   [esp+56]  new_task
;
; For NEW tasks, kernel_esp was set by task_create() to point to
; a hand-crafted frame with the same layout (fn instead of .resume).
; ---------------------------------------------------------------

[BITS 32]
[GLOBAL context_switch]

context_switch:
    ; ---- Step 1: build outgoing task's save frame ----
    ;
    ; Push an IRET frame so THIS task can be resumed at .resume
    ; when it is next scheduled.
    pushfd                       ; save current EFLAGS
    push  dword 0x08             ; kernel CS  (code segment selector)
    push  dword .resume          ; EIP: resume point for this task

    push  ds                     ; save DS (segment register)
    pushad                       ; save EAX ECX EDX EBX ESP EBP ESI EDI
                                 ; (EAX pushed first → highest addr;
                                 ;  EDI pushed last  → lowest addr = top)

    ; ---- Step 2: save esp into old_task->kernel_esp ----
    ;
    ; After all pushes, old_task is at [esp+52], new_task at [esp+56].
    ; (48 bytes pushed + 4-byte return address already at [esp+48])
    mov   eax, [esp + 52]              ; load old_task pointer
    mov   [eax + TCB_KERNEL_ESP], esp  ; save current stack pointer

    ; ---- Step 3: read new_task before switching stack ----
    mov   ecx, [esp + 56]              ; load new_task pointer (still on old stack)

    ; ---- Step 4: switch to new_task's kernel stack ----
    mov   esp, [ecx + TCB_KERNEL_ESP]  ; ESP now points into new_task's stack

    ; ---- Step 5: switch address space (CR3) ----
    ;
    ; All CR3 writes live in arch/x86/ per NFR-MAINT-01.
    ; ecx still holds new_task pointer.
    mov   edx, [ecx + TCB_PAGE_DIR]
    mov   cr3, edx                     ; flush TLB, activate new page directory

    ; ---- Step 6: restore new_task's register state ----
    ;
    ; Stack layout at this point (same frame layout as step 1, or
    ; the hand-crafted frame from task_create for new tasks):
    ;   [esp+0 ]  edi  \
    ;   [esp+4 ]  esi   |  PUSHAD block
    ;   [esp+8 ]  ebp   |
    ;   [esp+12]  esp   |  (dummy — POPAD skips this slot)
    ;   [esp+16]  ebx   |
    ;   [esp+20]  edx   |
    ;   [esp+24]  ecx   |
    ;   [esp+28]  eax  /
    ;   [esp+32]  ds
    ;   [esp+36]  eip   \
    ;   [esp+40]  cs     | IRET frame
    ;   [esp+44]  eflags/
    popad                          ; restore EDI ESI EBP (skip ESP) EBX EDX ECX EAX

    ; Restore DS, ES, FS, GS from the ds slot.
    ; Use AX (16-bit) to read only the low 16 bits of the saved value.
    ; EAX was just restored by POPAD; we clobber it only until IRET.
    mov   ax, [esp]                ; read saved ds (16-bit)
    add   esp, 4                   ; pop ds slot
    mov   ds, ax
    mov   es, ax
    mov   fs, ax
    mov   gs, ax

    ; ---- Step 7: IRET to new task ----
    ;
    ; Stack now:  [esp+0] eip,  [esp+4] cs,  [esp+8] eflags
    ; For existing tasks: eip = .resume  → falls through to RET.
    ; For new tasks:      eip = fn_ptr   → jumps to task function.
    iret

; ---- Resume point for previously-switched-out tasks ----
;
; When an existing task is rescheduled, IRET lands here.
; We return to sched_switch() → pit_irq0_handler() → IRQ common
; handler → task's interrupted point (via the outer IRET in isr_stubs).
.resume:
    ret