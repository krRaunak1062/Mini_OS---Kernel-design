# MiniOS — Demo Script
## Group 31 | IIT Jodhpur | B.Tech CSE 2024

| Name | Roll No | Sprint |
|---|---|---|
| Aman Yadav | B24CS1006 | S1, S2 — Memory Management |
| Raunak Kumar | B24CS1062 | S3, S4 — Interrupts & Scheduler |
| Palthyavath Jalendhar | B24CS1051 | S5 — Integration & NFR Validation |
| Shardul Diwate | B24CS1028 | S0, S6 — Boot, Build & Docs |

---

## Prerequisites

```bash
i686-elf-gcc --version    # 13.2.0
nasm --version            # 2.x
qemu-system-i386 --version
grub-mkrescue --version
```

---

## Build & Run

```bash
make clean && make iso    # clean build → MiniOS.iso
make run                  # boot in QEMU, serial log in terminal
```

---

## Expected Output — Phase 1: Boot & Init

```
[BOOT] Serial logger initialized
[BOOT] VGA driver initialized
[KERNEL] MiniOS — Sprints 1-5
[KERNEL] Multiboot magic: 0x2BADB002
[S1]  GDT initialized
[mm_init] ready. Kernel: 0x00100000 – 0x0012D000
[S1]  mm_init() complete
[PAGING] Kernel mapped: virt 0xC0000000 – 0xC03FFFFF → phys 0x00100000 – 0x004FFFFF
[KERNEL] Paging ENABLED.
[HEAP] Initialised: 0xC0400000 - 0xC0800000 (4095 KB usable)
[S3]  IDT loaded
[S3]  PIC remapped
[S3]  PIT configured at 100Hz
[S3]  ISR handlers installed
```

---

## Expected Output — Phase 2: Sprint 5 NFR Tests (takes ~12 seconds)

```
========================================
  SPRINT 5 — INTEGRATION & NFR TESTS
  Group 31 | IIT Jodhpur
========================================
  [PASS] paging_create_directory() returned valid pointer
  [PASS] New PD is fully zeroed (all 1024 entries = 0)
  [PASS] paging_map_page() creates PT entry in task PD
  [PASS] paging_free_directory() completed without crash
  [PASS] NFR-PERF-01: context switch latency < 50us (TSC)
  Waiting 10 seconds (1000 ticks) — please wait...
  Ticks in 10s: 1000 (expected 1000)
  [PASS] NFR-PERF-02: PIT fires within 1% of 100 Hz
  [PASS] NFR-SEC-01: all kernel PTEs have U/S=0 (supervisor-only)
  [PASS] NFR-REL-01: ISR 14 (#PF) handler is installed in IDT
  [PASS] Heap stress phase 1: 30 allocations succeeded
  [PASS] Heap stress phase 2: no data corruption detected
  [PASS] Heap stress phase 3: re-allocation after fragmentation OK
  [PASS] Heap stress phase 4: all blocks freed
  [PASS] NFR-MAINT-01: all function prefixes match SRS §7.5
  [PASS] NFR-PORT-01: all CR0/CR3/port I/O code confined to arch/x86/
========================================
  S5 RESULTS: 14 PASS, 0 FAIL
  STATUS: ALL TESTS PASSED
========================================
```

> NFR-PERF-02 waits exactly 10 seconds — this is intentional.

---

## Expected Output — Phase 3: Round-Robin Scheduler

```
[SCHED] Starting first task PID=0x00000001
[TASK A] PID=1 running
[TASK B] PID=2 running      ← each task prints multiple times per
[TASK B] PID=2 running         10ms quantum (normal — loop is fast)
[TASK B] PID=2 running
[TASK C] PID=3 running
[TASK C] PID=3 running
[TASK A] PID=1 running
[TASK A] PID=1 running
... (A-block → B-block → C-block, repeating forever)
```

**Why multiple prints per slot:** Each task runs a `200000`-iteration busy-wait loop (~0.67ms at QEMU speed). The PIT quantum is 10ms, so each task completes the loop ~10-15 times per quantum before being preempted. This is correct preemptive round-robin — every task gets exactly one 10ms slot in sequence.

Press **Ctrl+C** to stop.

---

## What to Point Out During Demo

### 1. VGA window (QEMU display)
Shows the boot banner in green:
```
=======================================
       MiniOS Kernel v0.5
       Group 31 | IIT Jodhpur
       Sprints 1-5 Complete
=======================================
```

### 2. Live page fault demo (optional)
Temporarily add to `task_a()`:
```c
*(volatile uint32_t *)0xDEADBEEF = 0;
```
Rebuild and run. Serial output:
```
  PAGE FAULT  (ISR 14)
  Faulting address : 0xDEADBEEF
  Cause            : page not present
  Kernel halted.
```

### 3. GDB debug mode
```bash
# Terminal 1
make debug              # QEMU pauses, waits for GDB on :1234

# Terminal 2
gdb kernel.elf
(gdb) target remote :1234
(gdb) break kernel_main
(gdb) continue
```

---

## Q&A Reference

| Question | Answer |
|---|---|
| What CPU mode? | 32-bit Protected Mode, set in boot.asm before kernel_main |
| How is paging enabled? | CR3 loaded with PD phys addr, CR0 bit 31 set — both in arch/x86/paging_asm.asm |
| How does context switch work? | PUSHFD + push CS + push EIP(.resume) + push DS + PUSHAD saved to kernel stack; IRET resumes |
| Why IRET not RET? | IRET atomically restores EIP + CS + EFLAGS, re-enabling interrupts in one instruction |
| Where is IRQ0 handled? | irq_common_stub → irq_handler() → pit_irq0_handler() → sched_switch() → context_switch() |
| Why CLI around kmalloc? | Prevents context switch mid-heap-walk causing double-alloc or list corruption |
| How is kernel isolated? | All kernel PTEs have U/S=0; ring-3 access raises #PF (ISR 14) — verified by NFR-SEC-01 test |
| Heap virtual range? | 0xC0400000–0xC07FFFFF (virtual), 0x500000–0x8FFFFF (physical) |
| IDT entries? | 256 — 0–31 CPU exceptions, 32–47 hardware IRQs, rest are default stubs |
| PIT frequency? | 100Hz — divisor 11932 written to ports 0x43/0x40 |
| Why multiple prints per quantum? | 200000-iteration loop finishes in ~0.67ms; 10ms quantum allows ~15 iterations |