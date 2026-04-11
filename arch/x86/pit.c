/**
 * arch/x86/pit.c
 * Implements: REQ-TASK-02
 *
 * Programs PIT channel 0 for 100 Hz (10 ms tick).
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
 * S4 NOTE: once the scheduler exists, add:
 *   if (pit_tick_count % 1 == 0) sched_switch();
 * (every tick for a 10ms quantum).
 * Implements: REQ-TASK-02
 * --------------------------------------------------------------- */
void pit_irq0_handler(void)
{
    pit_tick_count++;
    /* sched_switch() will be called here once S4 lands */
    pic_send_eoi(0);    /* IRQ0 → EOI to master PIC only */
}

/* ---------------------------------------------------------------
 * pit_get_ticks — snapshot of tick counter (safe for callers).
 * Implements: REQ-TASK-02
 * --------------------------------------------------------------- */
uint32_t pit_get_ticks(void)
{
    return pit_tick_count;
}
