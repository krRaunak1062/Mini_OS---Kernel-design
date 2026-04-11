/**
 * arch/x86/pit.c
 * Implements: REQ-TASK-02, REQ-TASK-04
 *
 * Programs PIT channel 0 for 100 Hz (10 ms tick).
 * S4 update: pit_irq0_handler() now calls sched_switch() every tick.
 *
 * Base oscillator : 1,193,182 Hz
 * Desired freq    : 100 Hz
 * Divisor         : 1193182 / 100 = 11931.82 → round to 11932
 * Actual freq     : 1193182 / 11932 ≈ 99.997 Hz  (error < 0.003%)
 *
 * Ports (SRS key-constants):
 *   0x43 — PIT mode/command register (write-only)
 *   0x40 — Channel 0 data port (read/write)
 */

#include "pit.h"
#include "pic.h"
#include "sched.h"   /* sched_switch() — REQ-TASK-04 */

/* Tick counter — volatile because IRQ handler writes, other code reads */
volatile uint32_t pit_tick_count = 0;

/* I/O helpers — in arch/x86/ per NFR-MAINT-01 */
static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

#define PIT_CMD_PORT  0x43
#define PIT_CH0_PORT  0x40
#define PIT_DIVISOR   11932

/* ---------------------------------------------------------------
 * pit_init — set channel 0 to square-wave mode at 100 Hz.
 * Must be called BEFORE STI.
 * Implements: REQ-TASK-02
 * --------------------------------------------------------------- */
void pit_init(void)
{
    /*
     * Command byte: 0x36
     *   Bits 7–6 : 00  → channel 0
     *   Bits 5–4 : 11  → lobyte/hibyte access mode
     *   Bits 3–1 : 011 → mode 3 (square wave generator)
     *   Bit  0   : 0   → binary (not BCD)
     */
    outb(PIT_CMD_PORT, 0x36);

    /* Send divisor: low byte first, then high byte */
    outb(PIT_CH0_PORT, (uint8_t)(PIT_DIVISOR & 0xFF));
    outb(PIT_CH0_PORT, (uint8_t)((PIT_DIVISOR >> 8) & 0xFF));
}

/* ---------------------------------------------------------------
 * pit_irq0_handler — fired every ~10 ms via IRQ0.
 *
 * EOI is sent BEFORE sched_switch() so the PIC is re-armed and
 * the next IRQ is not missed during the context switch.
 *
 * sched_switch() is a no-op when current_task == NULL (before
 * sched_start()), so it is safe to call unconditionally.
 *
 * Implements: REQ-TASK-02, REQ-TASK-04
 * --------------------------------------------------------------- */
void pit_irq0_handler(void)
{
    pit_tick_count++;
    pic_send_eoi(0);   /* EOI to master PIC BEFORE context switch */
    sched_switch();    /* round-robin: pick next task, context_switch() */
}

/* ---------------------------------------------------------------
 * pit_get_ticks — snapshot of tick counter (safe for callers).
 * Implements: REQ-TASK-02
 * --------------------------------------------------------------- */
uint32_t pit_get_ticks(void)
{
    return pit_tick_count;
}