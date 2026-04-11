#ifndef SCHED_H
#define SCHED_H

/**
 * include/sched.h
 * Round-robin scheduler public API for MiniOS Sprint 4.
 * Implements: REQ-TASK-04, REQ-TASK-05
 *
 * Owner : Raunak Kumar (B24CS1062)
 * Sprint: S4 — Scheduler, TCB & context switch
 */

#include "task.h"

/**
 * current_task — pointer to the TCB that is currently running.
 * NULL before sched_start() is called.
 * Implements: REQ-TASK-04
 */
extern tcb_t *current_task;

/**
 * sched_init() — initialise scheduler data structures.
 * Call once from kernel_main before creating any tasks.
 * Implements: REQ-TASK-04
 */
void sched_init(void);

/**
 * sched_add_task() — insert a READY task into the circular ready queue.
 * Safe to call before sched_start().
 * Implements: REQ-TASK-04
 */
void sched_add_task(tcb_t *task);

/**
 * sched_next() — return the next READY task in round-robin order.
 * Does NOT remove the task from the queue.
 * Returns current_task if it is the only ready task.
 * Implements: REQ-TASK-04
 */
tcb_t *sched_next(void);

/**
 * sched_switch() — called from pit_irq0_handler() every 10ms tick.
 * Picks the next task and calls context_switch(old, new).
 * No-op if scheduler not started or only one task exists.
 * Implements: REQ-TASK-04, REQ-TASK-05
 */
void sched_switch(void);

/**
 * sched_start() — begin scheduling: set current_task to the first
 * task in the queue and jump into it.  Never returns.
 * Implements: REQ-TASK-04
 */
void sched_start(void);

/**
 * context_switch() — NASM routine in arch/x86/context_switch.asm.
 * Saves old task's full register state onto its kernel stack,
 * loads new task's kernel stack, switches CR3, restores registers,
 * and resumes via IRET.
 * Implements: REQ-TASK-05
 */
extern void context_switch(tcb_t *old_task, tcb_t *new_task);

#endif /* SCHED_H */