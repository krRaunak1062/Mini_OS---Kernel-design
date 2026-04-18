# Requirement Traceability Matrix (RTM)
## MiniOS — Group 31 | IIT Jodhpur | SRS v3.2

---

## Functional Requirements

| Req ID | Description | Sprint | Source File(s) | Function(s) | Test/Evidence |
|---|---|---|---|---|---|
| REQ-MEM-01 | GDT with null + kernel code (DPL=0) + kernel data (DPL=0) | S1 | `arch/x86/gdt.c`, `arch/x86/gdt_flush.asm` | `gdt_init()`, `gdt_flush()` | Serial: `[S1] GDT initialized` |
| REQ-MEM-02 | Virtual memory via paging (4KB pages, PD+PT) | S2 | `arch/x86/paging.c`, `arch/x86/paging_asm.asm` | `paging_map_page()`, `paging_enable()` | Serial: `[KERNEL] Paging ENABLED` |
| REQ-MEM-03 | Physical frame bitmap allocator | S1 | `mm/mm_phys.c` | `mm_init()`, `mm_alloc_frame()`, `mm_free_frame()` | Serial: `[mm_init] ready` |
| REQ-MEM-04 | Kernel heap: kmalloc / kfree (4MB at 0xC0400000) | S2 | `mm/heap.c` | `mm_heap_init()`, `kmalloc()`, `kfree()` | Serial: `[HEAP] Initialised` + S5 stress test PASS |
| REQ-MEM-05 | Page fault handler (ISR 14): log CR2 + error code, halt | S2 | `arch/x86/isr14.c`, `arch/x86/isr_stubs.asm` | `isr_14()`, `isr14_handler()` | S5: `[PASS] NFR-REL-01: ISR 14 handler installed` |
| REQ-TASK-01 | 256-entry IDT, 32-bit interrupt gates, DPL=0 | S3 | `arch/x86/idt.c`, `arch/x86/idt_flush.asm` | `idt_init()`, `idt_flush()` | Serial: `[S3] IDT loaded` |
| REQ-TASK-02 | PIC remap (IRQs 0–15 → 0x20–0x2F), PIT at 100Hz | S3 | `arch/x86/pic.c`, `arch/x86/pit.c` | `pic_remap()`, `pit_init()`, `pit_irq0_handler()` | S5: `[PASS] NFR-PERF-02: PIT fires within 1%` |
| REQ-TASK-03 | TCB struct: pid, state, kernel_esp, page_dir, next, kernel_stack | S4 | `include/task.h`, `scheduler/task.c` | `task_create()`, `task_destroy()` | Serial: `[TASK] Created PID=...` |
| REQ-TASK-04 | Round-robin scheduler with circular ready queue | S4 | `scheduler/sched.c` | `sched_init()`, `sched_add_task()`, `sched_next()`, `sched_switch()`, `sched_start()` | Serial: `[TASK A/B/C] PID=N running` cycling |
| REQ-TASK-05 | Context switch: full register save/restore, CR3 switch, IRET | S4 | `arch/x86/context_switch.asm` | `context_switch()` | S5: `[PASS] NFR-PERF-01: latency < 50us` |

---

## Non-Functional Requirements

| Req ID | Description | Sprint | Verified By | Evidence |
|---|---|---|---|---|
| NFR-PERF-01 | Context switch latency < 50µs | S4/S5 | `kernel/s5_integration.c: s5_test_perf01_context_switch()` | TSC measurement: avg ~300 cycles @ ~1GHz = ~0.3µs << 50µs |
| NFR-PERF-02 | PIT fires at 100Hz ±1% (1000 ticks in 10s) | S3/S5 | `kernel/s5_integration.c: s5_test_perf02_pit_stability()` | Serial: `Ticks in 10s: 1000 (expected 1000)` |
| NFR-SEC-01 | All kernel PTEs have U/S=0 (supervisor-only) | S2/S5 | `kernel/s5_integration.c: s5_test_sec01_kernel_isolation()` | Serial: `U/S violations found: 0` |
| NFR-REL-01 | GPF (ISR 13) and PF (ISR 14) handlers installed and active | S3/S5 | `kernel/s5_integration.c: s5_test_sec01_kernel_isolation()` | Serial: `IDT[14] handler addr: 0x001035XX` |
| NFR-MAINT-01 | Function naming: mm\_ paging\_ sched\_ isr\_ pit\_ pic\_ prefixes | S0–S4 | `kernel/s5_integration.c: s5_audit_naming_and_arch()` | Serial: `[PASS] NFR-MAINT-01` |
| NFR-PORT-01 | All CR0/CR3/port I/O confined to arch/x86/ | S0–S4 | `kernel/s5_integration.c: s5_audit_naming_and_arch()` | Serial: `[PASS] NFR-PORT-01` |

---

## External Interface Requirements

| Req ID | Description | Sprint | Source File(s) | Function(s) | Evidence |
|---|---|---|---|---|---|
| EXT-SW-01 | Scheduler calls paging_create/free_directory correctly | S5 | `kernel/s5_integration.c` | `s5_test_ext_sw01()` | Serial: `EXT-SW-01 complete. 4 PASS` |
| EXT-COM-01 | COM1 serial logger at 9600 baud, 8N1 | S0 | `kernel/serial.c` | `serial_init()`, `serial_puts()`, `serial_putchar()` | All serial output throughout boot |
| EXT-HW-01 | Multiboot-compliant boot (magic 0x1BADB002) | S0 | `boot/boot.asm` | Multiboot header | Serial: `Multiboot magic: 0x2BADB002` |
| EXT-HW-02 | VGA text mode console (0xB8000, 80×25) | S0 | `kernel/vga.c` | `vga_init()`, `vga_puts()`, `vga_set_color()` | VGA screen output on QEMU display |

---

## S5 Deviation Log (SRS v3.1 → v3.2)

| Item | SRS v3.1 Spec | Actual Implementation | Reason |
|---|---|---|---|
| NFR-PERF-01 measurement | No method specified | RDTSC-based TSC cycle count for 1000 `sched_next()` calls | PIT resolution (10ms) too coarse for single-switch measurement |
| EXT-SW-01 task isolation | Per-task page directories | All kernel tasks share single CR3 (kernel page dir) | Kernel-space tasks don't require separate address spaces; paging_create/free_directory tested standalone |
| context_switch `.resume` | `ret` assumed by design | `ret` was accidentally omitted; restored in S5 integration | Triple fault bug; see Project Review Log risk R-16 |
| heap thread safety | "Raunak adds locking in S4" | CLI/STI guards added in S5 integration (both kmalloc + kfree) | S4 was single-threaded in isolation; guards added when scheduler went live. R-13. |
| heap physical reservation | Not specified in SRS | Heap physical frames 0x500000–0x8FFFFF marked allocated in mm_init() | mm_alloc_frame() could return frames overlapping heap backing region. R-17. |
| registers_t layout | Not explicitly specified | GP register field order corrected to match NASM PUSHAD (EDI lowest, EAX highest) | Original order was inverted — latent bug for any handler reading regs->eax. R-18. |
| sched_add_task interrupt guard | "CLI around queue mutations" | EFLAGS pushf+cli / push+popf replaces bare cli+sti | Bare STI would re-enable interrupts even if caller had them disabled. R-19. |