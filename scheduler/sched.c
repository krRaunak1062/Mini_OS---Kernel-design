/**
 * scheduler/sched.c
 * Implements: REQ-TASK-04, REQ-TASK-05
 *
 * Round-robin scheduler with a circular singly-linked ready queue.
 *
 * Queue structure:
 *   queue_tail->next  →  head  →  task1  →  task2  →  ...  →  tail (= queue_tail)
 *   queue_tail always points to the LAST inserted task.
 *   The HEAD is always queue_tail->next.
 *
 * On every PIT tick, sched_switch() advances current_task to the
 * next READY task and calls context_switch(old, new).
 *
 * Owner : Raunak Kumar (B24CS1062)
 * Sprint: S4
 */

#include "sched.h"
#include "task.h"
#include "serial.h"
#include <stdint.h>

/* ---------------------------------------------------------------
 * Globals
 * --------------------------------------------------------------- */
tcb_t *current_task  = 0;     /* running task; NULL until sched_start() */
static tcb_t *queue_tail = 0; /* tail of circular ready queue           */
static uint32_t task_count = 0;

/* ---------------------------------------------------------------
 * sched_init — zero scheduler state.
 * Implements: REQ-TASK-04
 * --------------------------------------------------------------- */
void sched_init(void)
{
    current_task = 0;
    queue_tail   = 0;
    task_count   = 0;
    serial_puts("[SCHED] Initialised\n");
}

/* ---------------------------------------------------------------
 * sched_add_task — insert task at the tail of the circular queue.
 * Implements: REQ-TASK-04
 * --------------------------------------------------------------- */
void sched_add_task(tcb_t *task)
{
    if (!task) return;

    /* Disable interrupts while modifying queue */
    __asm__ volatile ("cli");

    if (queue_tail == 0) {
        /* First task: points to itself */
        task->next = task;
        queue_tail = task;
    } else {
        /* Insert after tail, before current head */
        task->next       = queue_tail->next;  /* new->next = old head       */
        queue_tail->next = task;              /* old tail->next = new task  */
        queue_tail       = task;              /* advance tail to new task   */
    }
    task->state = TASK_READY;
    task_count++;

    __asm__ volatile ("sti");

    serial_puts("[SCHED] Task added PID=");
    serial_puts_hex(task->pid);   /* FIX: was serial_puthex */
    serial_puts("\n");
}

/* ---------------------------------------------------------------
 * sched_next — return the NEXT ready task after current_task.
 * Walks the circular list, skipping non-READY tasks.
 * Returns current_task if nothing else is READY (fallback).
 * Implements: REQ-TASK-04
 * --------------------------------------------------------------- */
tcb_t *sched_next(void)
{
    if (!current_task || !current_task->next) return current_task;

    tcb_t   *candidate = current_task->next;
    uint32_t checked   = 0;

    /* Walk at most task_count steps to avoid infinite loop */
    while (checked < task_count) {
        if (candidate->state == TASK_READY ||
            candidate->state == TASK_RUNNING) {
            return candidate;
        }
        candidate = candidate->next;
        checked++;
    }
    return current_task;   /* fallback: stay on current task */
}

/* ---------------------------------------------------------------
 * sched_switch — called from pit_irq0_handler() every tick.
 * Implements: REQ-TASK-04, REQ-TASK-05
 * --------------------------------------------------------------- */
void sched_switch(void)
{
    if (!current_task) return;   /* scheduler not started yet */
    if (task_count < 2) return;  /* only one task — nothing to switch to */

    tcb_t *old_task = current_task;
    tcb_t *new_task = sched_next();

    if (old_task == new_task) return;   /* same task, no-op */

    /* Update states */
    if (old_task->state == TASK_RUNNING)
        old_task->state = TASK_READY;

    new_task->state = TASK_RUNNING;
    current_task    = new_task;

    /*
     * Hand off to NASM context switch.
     * context_switch saves old's regs onto old's kernel stack,
     * switches ESP to new's kernel stack, switches CR3,
     * restores new's regs, and resumes via IRET.
     */
    context_switch(old_task, new_task);
}

/* ---------------------------------------------------------------
 * sched_start — enter the first task. Never returns.
 *
 * We perform a "bootstrap" context switch: craft a dummy TCB as
 * "old" so context_switch() has somewhere to save the initial
 * kernel context (which is then discarded forever).
 * After IRET, execution is inside the first task's function.
 * Implements: REQ-TASK-04
 * --------------------------------------------------------------- */
void sched_start(void)
{
    if (!queue_tail) {
        serial_puts("[SCHED] sched_start: no tasks!\n");
        return;
    }

    /* Head of queue = first task to run */
    tcb_t *first = queue_tail->next;
    first->state = TASK_RUNNING;
    current_task = first;

    serial_puts("[SCHED] Starting first task PID=");
    serial_puts_hex(first->pid);   /* FIX: was serial_puthex */
    serial_puts("\n");

    /*
     * Bootstrap dummy TCB for the outgoing "kernel main" context.
     * Its saved state is never restored — we only need the struct
     * to satisfy context_switch()'s old_task parameter.
     */
    tcb_t boot_tcb;
    boot_tcb.pid          = 0;
    boot_tcb.state        = TASK_TERMINATED;
    boot_tcb.kernel_esp   = 0;
    boot_tcb.page_dir     = 0;
    boot_tcb.next         = 0;
    boot_tcb.kernel_stack = 0;

    /* Jump in — execution continues inside first->fn after IRET */
    context_switch(&boot_tcb, first);

    /* Unreachable */
    __asm__ volatile ("cli; hlt");
}
