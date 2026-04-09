# Sprint 2 — Virtual Memory & Kernel Heap
## MiniOS | Group 31 | IIT Jodhpur
**Owner:** Aman Yadav (B24CS1006)
**Prereq:** S1 complete (GDT loaded, mm_alloc_frame / mm_free_frame working)

---

## Files produced in this sprint

```
arch/x86/
  paging.c          — paging_create_directory, map_page, identity map, kernel map, free_directory
  paging_asm.asm    — paging_load_directory (CR3), paging_enable (CR0), paging_get_cr2/cr3
  isr14.c           — isr_14_handler: logs fault address + error code, halts
  isr_stubs.asm     — NASM stubs for ISR 0–31 (common_isr_stub)
  isr_handler.c     — C dispatcher table, isr_handlers_init()
  idt_minimal.c     — minimal IDT for S2 (ISR 0, 13, 14 only)
  idt_asm.asm       — idt_flush() LIDT wrapper

mm/
  heap.c            — mm_heap_init(), kmalloc(), kfree(), mm_heap_dump()

include/
  paging.h          — full public API with REQ-XX tags
  mm.h              — physical allocator + heap declarations
  serial.h          — COM1 logger (implemented in S0)
  isr.h             — registers_t, isr_handler_t

kernel_main.c       — S2 integration + test_s2() test suite
```

---

## Step-by-step: what to do

### Step 1 — confirm S1 is done
Before touching any S2 code, verify in GDB:
```
(gdb) break mm_alloc_frame
(gdb) continue
(gdb) finish
(gdb) print $eax     # should be a non-zero physical address
```
Also confirm `gdt_init()` runs without a triple fault.

---

### Step 2 — add isr_stubs.asm to build
`arch/x86/isr_stubs.asm` defines the 32 exception stubs and `common_isr_stub`.
Add it to the Makefile ASM_SRCS list. It depends on `isr_common_handler` (in isr_handler.c).

Build check:
```bash
make all 2>&1 | head -30
# Should show [AS] arch/x86/isr_stubs.asm with no errors
```

---

### Step 3 — build idt_minimal + isr_handler
These two C files implement the minimal IDT and the C dispatcher.
`isr_handlers_init()` must be called in kernel_main **before** `idt_load_minimal()`.

Call order in kernel_main.c:
```c
gdt_init();
mm_init(mbi->mmap_addr, mbi->mmap_length);
isr_handlers_init();   // register ISR 13 + ISR 14 handlers
idt_load_minimal();    // LIDT with those handlers installed
```

---

### Step 4 — paging setup (the critical sequence)
```c
page_dir_t *kernel_dir = paging_create_directory();
paging_identity_map_first4mb(kernel_dir);   // MUST be first
paging_map_kernel(kernel_dir);              // 0xC0000000 → 0x00100000
paging_map_heap(kernel_dir);               // 0xC0400000 → 0x00500000
paging_load_directory((uint32_t)kernel_dir);
paging_enable();                            // CR0.PG = 1 — paging ON
mm_heap_init();                             // AFTER paging_enable()
```

**Common mistake:** calling `mm_heap_init()` before `paging_enable()`.
The heap lives at virtual address 0xC0400000 which doesn't exist
until paging is active. Writing to it pre-paging writes to a random
physical location and silently corrupts memory.

---

### Step 5 — verify with GDB

Start QEMU in debug mode:
```bash
make debug
```

In a second terminal:
```bash
gdb build/kernel.bin
(gdb) target remote :1234
(gdb) break paging_enable
(gdb) continue
(gdb) next                   # step over the CR0 write
(gdb) info registers cr0     # bit 31 must be 1
(gdb) info registers cr3     # should match your kernel_dir phys addr
(gdb) info pg                # QEMU monitor: dump page table walk
```

---

### Step 6 — verify with QEMU monitor
While QEMU is running press **Ctrl+Alt+2** to enter the monitor:
```
(qemu) info pg
# Shows the full page table. Look for:
#   0x00000000–0x003fffff → 0x00000000 (identity map)
#   0xc0000000–0xc03fffff → 0x00100000 (kernel map)
#   0xc0400000–0xc07fffff → 0x00500000 (heap map)
```

---

### Step 7 — trigger intentional page fault
In kernel_main.c, in test_s2(), uncomment TEST 7:
```c
volatile uint32_t *bad = (uint32_t *)0xDEAD0000;
uint32_t val = *bad;
```

Expected serial output:
```
========================================
  PAGE FAULT  (ISR 14)
========================================
  Faulting address : 0xdead0000
  Error code       : 0x0
  Cause            : page not present
  Access type      : read
  Privilege        : kernel (ring 0)
  Kernel halted.
========================================
```
If you see this, ISR 14 is working. Re-comment the line before S3 handoff.

---

### Step 8 — NFR-SEC-01 user isolation test (S5 preview)
Once S3+S4 are done, Jalendhar will run:
```c
// In a user-mode task:
volatile uint32_t *kernel_page = (uint32_t *)0xC0100000;
uint32_t x = *kernel_page;  // must fault with U=1 (user mode)
```
Expected: ISR 14 fires with "user (ring 3)" in the privilege field.
The `[SECURITY]` log line must appear. This satisfies NFR-SEC-01.

---

### Step 9 — S4 handoff checklist
Before handing to Raunak (S3) and completing your part:

- [ ] `paging_create_directory()` exported in include/paging.h
- [ ] `paging_free_directory()` exported in include/paging.h
- [ ] `paging_map_page()` exported (S4 may need to map user pages)
- [ ] All functions have `/* Implements: REQ-XX */` header comments
- [ ] No CR0/CR3 writes outside arch/x86/ (grep check below)
- [ ] No I/O port instructions outside arch/x86/
- [ ] All files use `mm_` or `paging_` prefixes per SRS §7.5
- [ ] Header guards present in all .h files

```bash
# Verify arch isolation
grep -rn "mov.*cr0\|mov.*cr3" --include="*.c" .
# Must return zero results — all CR writes must be in .asm files
grep -rn "outb\|inb"         --include="*.c" arch/x86/paging.c
# Must return zero results
```

---

## Requirement traceability

| Requirement  | File(s)                                      | Function(s)                                      |
|-------------|----------------------------------------------|--------------------------------------------------|
| REQ-MEM-02  | arch/x86/paging.c, arch/x86/paging_asm.asm   | paging_map_page, paging_load_directory, paging_enable |
| REQ-MEM-04  | mm/heap.c                                    | mm_heap_init, kmalloc, kfree                     |
| REQ-MEM-05  | arch/x86/paging.c, arch/x86/isr14.c          | paging_map_kernel (no PAGE_USER), isr_14_handler |
| EXT-SW-01   | arch/x86/paging.c                            | paging_create_directory, paging_free_directory    |
| NFR-REL-01  | arch/x86/isr14.c, arch/x86/isr_handler.c     | isr_14_handler (hlt loop), gpf_handler            |
| NFR-MAINT-01| arch/x86/paging_asm.asm                      | CR3/CR0 writes isolated to arch/x86/             |
| NFR-SEC-01  | arch/x86/paging.c                            | paging_map_kernel (supervisor-only flag)         |
