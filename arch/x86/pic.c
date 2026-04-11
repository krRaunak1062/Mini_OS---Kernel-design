/**
 * arch/x86/pic.c
 * Implements: REQ-TASK-02
 *
 * 8259A PIC remapping.  After reset, IRQ 0–7 fire on vectors 8–15,
 * colliding with CPU exception vectors.  We remap:
 *   Master PIC  →  vectors 0x20–0x27  (IRQ 0–7)
 *   Slave  PIC  →  vectors 0x28–0x2F  (IRQ 8–15)
 *
 * Port layout (SRS §key-constants):
 *   Master: command = 0x20, data = 0x21
 *   Slave : command = 0xA0, data = 0xA1
 */

#include "pic.h"

/* I/O port helpers — implemented in arch/x86/ per NFR-MAINT-01 */
static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t val;
    __asm__ volatile ("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/* Small I/O delay — one PIT write is ~1–4 µs, enough for old PICs */
static inline void io_wait(void)
{
    outb(0x80, 0x00);   /* port 0x80 = POST debug port, harmless write */
}

/* PIC port definitions */
#define PIC1_CMD  0x20
#define PIC1_DAT  0x21
#define PIC2_CMD  0xA0
#define PIC2_DAT  0xA1

/* ICW1 bits */
#define ICW1_ICW4   0x01   /* ICW4 will be sent           */
#define ICW1_INIT   0x10   /* Initialization command       */

/* ICW4 bits */
#define ICW4_8086   0x01   /* 8086/88 mode (vs MCS-80/85) */

/* ---------------------------------------------------------------
 * pic_remap — re-initialise both PICs with new base vectors.
 * Implements: REQ-TASK-02
 * --------------------------------------------------------------- */
void pic_remap(void)
{
    /* Save current masks so we can restore them after remapping */
    uint8_t mask1 = inb(PIC1_DAT);
    uint8_t mask2 = inb(PIC2_DAT);

    /* ICW1: start initialisation sequence (cascade mode) */
    outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4);  io_wait();
    outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4);  io_wait();

    /* ICW2: new base vector offsets */
    outb(PIC1_DAT, 0x20);  io_wait();   /* master → IRQ 0 = vector 0x20 */
    outb(PIC2_DAT, 0x28);  io_wait();   /* slave  → IRQ 8 = vector 0x28 */

    /* ICW3: tell master about slave on IRQ2, tell slave its cascade id */
    outb(PIC1_DAT, 0x04);  io_wait();   /* master: slave on IRQ2 (bit 2) */
    outb(PIC2_DAT, 0x02);  io_wait();   /* slave:  cascade identity = 2  */

    /* ICW4: 8086 mode */
    outb(PIC1_DAT, ICW4_8086);  io_wait();
    outb(PIC2_DAT, ICW4_8086);  io_wait();

    /* Restore saved masks */
    outb(PIC1_DAT, mask1);
    outb(PIC2_DAT, mask2);
}

/* ---------------------------------------------------------------
 * pic_send_eoi — signal End-Of-Interrupt.
 * For IRQ >= 8, EOI must go to slave then master.
 * Implements: REQ-TASK-02
 * --------------------------------------------------------------- */
void pic_send_eoi(uint8_t irq)
{
    if (irq >= 8) {
        outb(PIC2_CMD, 0x20);   /* EOI to slave  */
    }
    outb(PIC1_CMD, 0x20);       /* EOI to master */
}

/* ---------------------------------------------------------------
 * pic_mask_irq / pic_unmask_irq
 * Implements: REQ-TASK-02
 * --------------------------------------------------------------- */
void pic_mask_irq(uint8_t irq)
{
    uint16_t port = (irq < 8) ? PIC1_DAT : PIC2_DAT;
    uint8_t  bit  = (irq < 8) ? irq : (irq - 8);
    outb(port, inb(port) | (1 << bit));
}

void pic_unmask_irq(uint8_t irq)
{
    uint16_t port = (irq < 8) ? PIC1_DAT : PIC2_DAT;
    uint8_t  bit  = (irq < 8) ? irq : (irq - 8);
    outb(port, inb(port) & ~(1 << bit));
}
