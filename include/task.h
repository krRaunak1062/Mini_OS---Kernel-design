#ifndef TASK_H
#define TASK_H

/**
 * include/task.h
 * Task Control Block definition for MiniOS Sprint 4.
 * Implements: REQ-TASK-03
 *
 * Owner : Raunak Kumar (B24CS1062)
 * Sprint: S4 — Scheduler, TCB & context switch
 *
 * FIELD OFFSETS (must match context_switch.asm exactly):
 *   pid          offset  0
 *   state        offset  4
 *   kernel_esp   offset  8   <-- TCB_KERNEL_ESP in context_switch.asm
 *   page_dir     offset 12   <-- TCB_PAGE_DIR   in context_switch.asm
 *   next         offset 16
 *   kernel_stack offset 20
 */

#include <stdint.h>

/**
 * task_state_t — lifecycle states for a task.
 * Implements: REQ-TASK-03
 */
typedef enum {
    TASK_READY      = 0,   /* on the ready queue, waiting for CPU      */
    TASK_RUNNING    = 1,   /* currently executing on the CPU            */
    TASK_BLOCKED    = 2,   /* waiting for I/O or an event (future use) */
    TASK_TERMINATED = 3    /* finished; TCB pending reclamation         */
} task_state_t;

/**
 * tcb_t — Task Control Block.
 *
 * DO NOT reorder fields without updating TCB_KERNEL_ESP / TCB_PAGE_DIR
 * constants in arch/x86/context_switch.asm AND the _Static_assert
 * checks in scheduler/task.c.
 *
 * Implements: REQ-TASK-03
 */
typedef struct tcb {
    uint32_t      pid;           /* process/task identifier                */
    task_state_t  state;         /* current lifecycle state                */
    uint32_t      kernel_esp;    /* saved kernel stack pointer             */
    uint32_t      page_dir;      /* physical address of page directory     */
    struct tcb   *next;          /* circular ready-queue link              */
    uint8_t      *kernel_stack;  /* base of kmalloc'd kernel stack (kfree) */
} tcb_t;

/** 8 KB kernel stack per task */
#define KERNEL_STACK_SIZE  8192

/**
 * task_create() — allocate a TCB + kernel stack, craft the initial
 * stack frame, and return a pointer ready for sched_add_task().
 *
 * @fn : entry-point function (must never return — use infinite loop)
 * Returns NULL on allocation failure.
 * Implements: REQ-TASK-03
 */
tcb_t *task_create(void (*fn)(void));

/**
 * task_destroy() — free the TCB and its kernel stack.
 * Caller must ensure the task is no longer on the ready queue.
 * Implements: REQ-TASK-03
 */
void task_destroy(tcb_t *task);

#endif /* TASK_H */