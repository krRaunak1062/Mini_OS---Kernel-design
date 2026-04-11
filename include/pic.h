#ifndef PIC_H
#define PIC_H

#include <stdint.h>

/**
 * pic_remap() — remap PIC IRQs to vectors 0x20–0x2F to avoid
 * collision with CPU exception vectors 0–31.
 * Implements: REQ-TASK-02
 */
void pic_remap(void);

/**
 * pic_send_eoi() — send End-Of-Interrupt to the appropriate PIC(s).
 * Must be called at the end of every IRQ handler.
 * Implements: REQ-TASK-02
 */
void pic_send_eoi(uint8_t irq);

/**
 * pic_mask_irq()   — disable a specific IRQ line.
 * pic_unmask_irq() — enable  a specific IRQ line.
 * Implements: REQ-TASK-02
 */
void pic_mask_irq(uint8_t irq);
void pic_unmask_irq(uint8_t irq);

#endif /* PIC_H */
