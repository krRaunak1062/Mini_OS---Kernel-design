/* Implements: REQ-MEM-01 */
#include "gdt.h"

#define GDT_ENTRIES 3

static gdt_entry_t gdt[GDT_ENTRIES];
static gdt_ptr_t   gdt_ptr;

/*
 * gdt_set_entry — pack one GDT descriptor.
 *
 * base:   where this segment starts in linear memory (0 for flat model)
 * limit:  how large the segment is (0xFFFFFFFF for 4GB flat)
 * access: the access byte (0x9A = kernel code, 0x92 = kernel data)
 * gran:   granularity byte (0xCF = 4KB pages, 32-bit, limit top nibble=F)
 */
static void gdt_set_entry(int i, uint32_t base, uint32_t limit,
                          uint8_t access, uint8_t gran)
{
    /* Split the base address across three non-contiguous fields */
    gdt[i].base_low  = (base  & 0x0000FFFF);
    gdt[i].base_mid  = (base  & 0x00FF0000) >> 16;
    gdt[i].base_high = (base  & 0xFF000000) >> 24;

    /* Split the limit across two non-contiguous fields */
    gdt[i].limit_low   = (limit & 0x0000FFFF);
    gdt[i].granularity = ((limit & 0x000F0000) >> 16) | (gran & 0xF0);

    gdt[i].access = access;
}

/*
 * gdt_init — set up and load the GDT.
 * Must be called before any other subsystem in kernel_main.
 *
 * Implements: REQ-MEM-01
 */
void gdt_init(void)
{
    /* Tell gdt_ptr where our table is and how big it is */
    gdt_ptr.limit = (sizeof(gdt_entry_t) * GDT_ENTRIES) - 1;
    gdt_ptr.base  = (uint32_t)&gdt;

    /*
     * Entry 0: null descriptor.
     * x86 REQUIRES entry 0 to be all zeroes. The CPU will triple-fault
     * if you ever load segment register 0x00 and then use it.
     */
    gdt_set_entry(0, 0, 0, 0x00, 0x00);

    /*
     * Entry 1: kernel code segment. Selector = 0x08
     * access 0x9A = 1 00 1 1010
     *               P DPL S Type
     *   P=1    segment is present
     *   DPL=00 ring 0 (kernel only)
     *   S=1    code/data (not a system segment)
     *   Type=1010 code segment, readable
     * gran 0xCF = 1100 xxxx → G=1 (4KB pages), DB=1 (32-bit)
     */
    gdt_set_entry(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);

    /*
     * Entry 2: kernel data segment. Selector = 0x10
     * access 0x92 = 1 00 1 0010
     *   same flags but Type=0010 = data segment, writable
     */
    gdt_set_entry(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    /* Hand the table to the CPU and flush the segment cache */
    gdt_flush((uint32_t)&gdt_ptr);
}