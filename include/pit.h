#ifndef PIT_H
#define PIT_H

#include <stdint.h>

/** Global tick counter — incremented by IRQ0 every 10ms. */
extern volatile uint32_t pit_tick_count;

/**
 * pit_init() — configure PIT channel 0 to fire at 100 Hz.
 * Divisor = 11932 → ~100 Hz from 1.193182 MHz base clock.
 * Implements: REQ-TASK-02
 */
void pit_init(void);

/**
 * pit_irq0_handler() — called from irq_handler() for IRQ 0.
 * Increments pit_tick_count; will call sched_switch() once S4 lands.
 * Implements: REQ-TASK-02
 */
void pit_irq0_handler(void);

/**
 * pit_get_ticks() — return current tick count (thread-safe snapshot).
 * Implements: REQ-TASK-02
 */
uint32_t pit_get_ticks(void);

#endif /* PIT_H */
