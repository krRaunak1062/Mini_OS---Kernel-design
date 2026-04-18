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
    pushfd
    or  dword [esp], 0x200    ; force IF=1 so task resumes with interrupts ON

    push  dword 0x08
    push  dword .resume

    push  ds
    pushad

    ; ---- Step 2: save esp into old_task->kernel_esp ----
    mov   eax, [esp + 52]              ; load old_task pointer
    mov   [eax + TCB_KERNEL_ESP], esp  ; save current stack pointer

    ; ---- Step 3: read new_task before switching stack ----
    mov   ecx, [esp + 56]              ; load new_task pointer

    ; ---- Step 4: switch to new_task's kernel stack ----
    mov   esp, [ecx + TCB_KERNEL_ESP]

    ; ---- Step 5: switch address space (CR3) ----
    mov   edx, [ecx + TCB_PAGE_DIR]
    mov   cr3, edx

    ; ---- Step 6: restore new_task's register state ----
    ;
    ; Stack layout (same frame, either from task_create or prior switch):
    ;   [esp+0 ]  edi  \
    ;   [esp+4 ]  esi   |  PUSHAD block
    ;   [esp+8 ]  ebp   |
    ;   [esp+12]  esp   |  (dummy)
    ;   [esp+16]  ebx   |
    ;   [esp+20]  edx   |
    ;   [esp+24]  ecx   |
    ;   [esp+28]  eax  /
    ;   [esp+32]  ds
    ;   [esp+36]  eip  (.resume or fn_ptr)
    ;   [esp+40]  cs   (0x08)
    ;   [esp+44]  eflags
    popad

    mov   ax, [esp]
    add   esp, 4
    mov   ds, ax
    mov   es, ax
    mov   fs, ax
    mov   gs, ax

    ; ---- Step 7: IRET to new task ----
    ;
    ; For new tasks:      eip = fn_ptr  -> jumps into task function
    ; For existing tasks: eip = .resume -> falls through (see below)
    iret

; ---------------------------------------------------------------
; .resume — landing point when an existing task is switched back in.
;
; IRET lands here with the task's registers fully restored and
; interrupts re-enabled (IF was in saved eflags).
;
; WHY ret IS CORRECT HERE:
;   context_switch() is a normal cdecl function called from sched_switch().
;   The CPU pushed a return address (back into sched_switch) BEFORE we
;   pushed our 48-byte save frame (pushfd + push cs + push .resume +
;   push ds + pushad).
;
;   When sched_switch() later re-selects this task and calls
;   context_switch(old2, new2=this_task), the restore path:
;     popad          → restores edi..eax from this task's save frame
;     discard DS     → add esp, 4 (pop ds into ax, reload seg regs)
;     iret           → pops EIP=.resume, CS=0x08, EFLAGS → lands here
;
;   At that moment, ESP points at the original cdecl return address
;   that was on the stack when this task first called context_switch().
;   ret pops it → returns to sched_switch() → pit_irq0_handler()
;   → irq_handler() → irq_common_stub → iret back into the task's
;   actual interrupted execution point. Correct.
; ---------------------------------------------------------------
.resume:
    ret