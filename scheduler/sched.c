/**
 * scheduler/sched.c
 * Implements: REQ-TASK-04, REQ-TASK-05
 *
 * Round-robin scheduler with a circular singly-linked ready queue.
 *
 * Queue structure:
 *   queue_tail->next  →  head  →  task1  →  task2  →  ...  →  tail
 *   queue_tail always points to the LAST inserted task.
 *   The HEAD is always queue_tail->next.
 *
 * On every PIT tick, pit_irq0_handler() calls sched_switch(), which
 * picks the next READY task and calls context_switch(old, new).
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
 * sched_init
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

    /*
     * R-14 FIX: Save and restore EFLAGS (interrupt flag) rather than
     * unconditionally re-enabling interrupts with STI.
     *
     * If sched_add_task() is called from a context where interrupts are
     * already disabled (e.g. from inside kernel_main before STI, or from
     * a CLI'd section), a bare STI at the end would re-enable them
     * unexpectedly, allowing IRQ0 to fire and call sched_switch() before
     * the task is fully enqueued — a race on queue_tail.
     *
     * pushf saves current EFLAGS (including IF) onto the stack.
     * cli  disables interrupts to protect the queue mutation.
     * popf restores the original EFLAGS, re-enabling IF only if it was
     *      set before this call.
     */
    uint32_t saved_flags;
    __asm__ volatile (
        "pushf\n"
        "pop %0\n"
        "cli\n"
        : "=r"(saved_flags) : : "memory"
    );

    if (queue_tail == 0) {
        task->next = task;
        queue_tail = task;
    } else {
        task->next       = queue_tail->next;
        queue_tail->next = task;
        queue_tail       = task;
    }
    task->state = TASK_READY;
    task_count++;

    __asm__ volatile (
        "push %0\n"
        "popf\n"
        : : "r"(saved_flags) : "memory"
    );

    serial_puts("[SCHED] Task added PID=");
    serial_puts_hex(task->pid);
    serial_puts("\n");
}

/* ---------------------------------------------------------------
 * sched_next — return the next READY task after current_task.
 *
 * Only returns TASK_READY tasks — never TASK_RUNNING.
 * This prevents the current task from being re-selected, which
 * would cause it to run twice in a row and starve other tasks.
 *
 * Returns current_task as a fallback only if no other task is READY.
 * Implements: REQ-TASK-04
 * --------------------------------------------------------------- */
tcb_t *sched_next(void)
{
    if (!current_task || !current_task->next) return current_task;

    tcb_t   *candidate = current_task->next;
    uint32_t checked   = 0;

    while (checked < task_count) {
        if (candidate->state == TASK_READY) {
            return candidate;
        }
        candidate = candidate->next;
        checked++;
    }

    /* No other READY task found — stay on current */
    return current_task;
}

/* ---------------------------------------------------------------
 * sched_switch — called from pit_irq0_handler() every 10ms tick.
 *
 * EOI has already been sent by pit_irq0_handler() before this call.
 * context_switch() saves the old task's state and resumes the new one.
 * For new tasks, context_switch()'s iret jumps directly to the task
 * function and this call never returns for that invocation.
 * For existing tasks, context_switch()'s iret lands at .resume in
 * context_switch.asm, which falls through, and this function returns
 * normally into pit_irq0_handler(), then irq_handler(), then the
 * irq_common_stub which irets back into the task's previous position.
 *
 * Implements: REQ-TASK-04, REQ-TASK-05
 * --------------------------------------------------------------- */
void sched_switch(void)
{
    if (!current_task) return;   /* scheduler not started yet */
    if (task_count < 2) return;  /* only one task — no switch needed */

    tcb_t *old_task = current_task;
    tcb_t *new_task = sched_next();

    if (old_task == new_task) return;

    if (old_task->state == TASK_RUNNING)
        old_task->state = TASK_READY;

    new_task->state = TASK_RUNNING;
    current_task    = new_task;

    context_switch(old_task, new_task);
}

/* ---------------------------------------------------------------
 * sched_start — enter the first task. Never returns.
 *
 * Bootstrap: craft a dummy TCB as "old" so context_switch() has
 * somewhere to save the initial kernel context (discarded forever).
 * Implements: REQ-TASK-04
 * --------------------------------------------------------------- */
void sched_start(void)
{
    if (!queue_tail) {
        serial_puts("[SCHED] sched_start: no tasks!\n");
        return;
    }

    tcb_t *first = queue_tail->next;
    first->state = TASK_RUNNING;
    current_task = first;

    serial_puts("[SCHED] Starting first task PID=");
    serial_puts_hex(first->pid);
    serial_puts("\n");

    /*
     * Bootstrap dummy TCB — context_switch() saves the kernel-main
     * context here, but it is never restored.
     */
    tcb_t boot_tcb;
    boot_tcb.pid          = 0;
    boot_tcb.state        = TASK_TERMINATED;
    boot_tcb.kernel_esp   = 0;
    boot_tcb.page_dir     = 0;
    boot_tcb.next         = 0;
    boot_tcb.kernel_stack = 0;

    context_switch(&boot_tcb, first);

    /* Unreachable */
    __asm__ volatile ("cli; hlt");
}