ENTRY(boot_start)

SECTIONS
{
    . = 0x00100000;

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