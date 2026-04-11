/**
 * scheduler/task.c
 * Implements: REQ-TASK-03
 *
 * task_create() allocates a TCB and a kernel stack, then manually builds
 * the exact stack frame that context_switch()'s restore path expects:
 *
 * Frame layout (grows downward, kernel_esp points to lowest address):
 *
 *   HIGH ADDRESS (top of stack buffer)
 *   +------------------+
 *   |  EFLAGS (0x0202) |  <-- pushed last by task_create, first by IRET
 *   |  CS     (0x0008) |
 *   |  EIP    (fn ptr) |  <-- task entry point
 *   |  DS     (0x0010) |
 *   |  EAX    (0)      |  \
 *   |  ECX    (0)      |   |
 *   |  EDX    (0)      |   |  PUSHAD block
 *   |  EBX    (0)      |   |  (POPAD restores EDI first = lowest addr)
 *   |  ESP    (0)      |   |
 *   |  EBP    (0)      |   |
 *   |  ESI    (0)      |   |
 *   |  EDI    (0)      |  /  <-- kernel_esp points HERE
 *   LOW ADDRESS
 *
 * PUSHAD pushes: EAX ECX EDX EBX ESP EBP ESI EDI  (EAX highest, EDI lowest)
 * POPAD  pops:   EDI ESI EBP (skip ESP) EBX EDX ECX EAX
 *
 * Owner : Raunak Kumar (B24CS1062)
 * Sprint: S4
 */

#include "task.h"
#include "serial.h"
#include <stdint.h>

/* Provided by mm/ (S2) */
extern void *kmalloc(uint32_t size);
extern void  kfree(void *ptr);

/* Compile-time guard: offsets in context_switch.asm MUST match tcb_t */
_Static_assert(__builtin_offsetof(tcb_t, kernel_esp) ==  8, "TCB_KERNEL_ESP offset mismatch");
_Static_assert(__builtin_offsetof(tcb_t, page_dir)   == 12, "TCB_PAGE_DIR offset mismatch");

static uint32_t next_pid = 1;

/* ---------------------------------------------------------------
 * task_create — allocate TCB + kernel stack, build initial frame.
 * Implements: REQ-TASK-03
 * --------------------------------------------------------------- */
tcb_t *task_create(void (*fn)(void))
{
    /* Allocate TCB */
    tcb_t *task = (tcb_t *)kmalloc(sizeof(tcb_t));
    if (!task) {
        serial_puts("[TASK] ERROR: kmalloc failed for TCB\n");
        return 0;
    }

    /* Allocate kernel stack */
    uint8_t *stack_base = (uint8_t *)kmalloc(KERNEL_STACK_SIZE);
    if (!stack_base) {
        serial_puts("[TASK] ERROR: kmalloc failed for kernel stack\n");
        kfree(task);
        return 0;
    }

    /*
     * Build initial frame from the TOP of the stack downward.
     * Cast to uint32_t* so each --sp step moves 4 bytes.
     */
    uint32_t *sp = (uint32_t *)(stack_base + KERNEL_STACK_SIZE);

    /* ---- IRET frame (deepest = highest address, consumed last) ---- */
    *--sp = 0x00000202;        /* EFLAGS: IF=1 (bit 9), reserved bit 1 set */
    *--sp = 0x00000008;        /* CS: kernel code segment (GDT index 1)    */
    *--sp = (uint32_t)fn;      /* EIP: task entry point                    */

    /* ---- DS saved by context_switch before PUSHAD ---- */
    *--sp = 0x00000010;        /* DS: kernel data segment (GDT index 2)    */

    /* ---- PUSHAD block (EAX first = highest addr in block) ---- */
    *--sp = 0;                 /* EAX */
    *--sp = 0;                 /* ECX */
    *--sp = 0;                 /* EDX */
    *--sp = 0;                 /* EBX */
    *--sp = 0;                 /* ESP (dummy — POPAD discards this slot)   */
    *--sp = 0;                 /* EBP */
    *--sp = 0;                 /* ESI */
    *--sp = 0;                 /* EDI  <-- kernel_esp points here          */

    /* ---- Fill TCB ---- */
    task->pid          = next_pid++;
    task->state        = TASK_READY;
    task->kernel_esp   = (uint32_t)sp;
    task->kernel_stack = stack_base;
    task->next         = 0;

    /*
     * Inherit the current page directory (kernel tasks share kernel map).
     * For user-space tasks (future S5+), call paging_create_directory().
     */
    uint32_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    task->page_dir = cr3;

    serial_puts("[TASK] Created PID=");
    serial_puts_hex(task->pid);          /* FIX: was serial_puthex */
    serial_puts(" stack_base=");
    serial_puts_hex((uint32_t)stack_base); /* FIX: was serial_puthex */
    serial_puts(" kernel_esp=");
    serial_puts_hex(task->kernel_esp);   /* FIX: was serial_puthex */
    serial_puts("\n");

    return task;
}

/* ---------------------------------------------------------------
 * task_destroy — release TCB and kernel stack.
 * Implements: REQ-TASK-03
 * --------------------------------------------------------------- */
void task_destroy(tcb_t *task)
{
    if (!task) return;
    kfree(task->kernel_stack);
    kfree(task);
}
