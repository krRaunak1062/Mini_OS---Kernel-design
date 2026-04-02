#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include <stdint.h>

/* Flags in multiboot_info_t.flags */
#define MULTIBOOT_INFO_MEM_MAP  (1 << 6)   /* mmap_* fields valid */

typedef struct __attribute__((packed)) {
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;    /* 1 = available RAM, anything else = reserved */
} multiboot_memory_map_t;

typedef struct __attribute__((packed)) {
    uint32_t flags;
    uint32_t mem_lower;     /* KB below 1MB */
    uint32_t mem_upper;     /* KB above 1MB */
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;   /* byte length of memory map */
    uint32_t mmap_addr;     /* physical address of first entry */
    /* more fields exist but we don't need them for S1/S2 */
} multiboot_info_t;

#define MULTIBOOT_MEMORY_AVAILABLE 1

#endif /* MULTIBOOT_H */
