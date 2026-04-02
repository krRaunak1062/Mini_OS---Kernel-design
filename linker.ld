ENTRY(boot_start)

SECTIONS
{
    . = 0x00100000;    /* load address from S0 */

    kernel_start = .;    /* sprint 1 addition */

    .text ALIGN(4096) :
    {
        *(.multiboot)
        *(.text)
    }

    .rodata ALIGN(4096) :
    {
        *(.rodata)
    }

    .data ALIGN(4096) :
    {
        *(.data)
    }

    .bss ALIGN(4096) :
    {
        *(COMMON)
        *(.bss)
    }

    kernel_end = .;
}