# Project Review Log — Risk Register
## MiniOS Group 31 | IIT Jodhpur | SRS v3.2

Last updated: Sprint 6 (submission)

---

## Risk Register

| Risk ID | Description | Likelihood | Impact | Mitigation | Status |
|---|---|---|---|---|---|
| R-01 | Cross-compiler not available on team members' machines | High | High | Documented install steps in `docs/demo.md`; Raunak hosts shared binary | **RESOLVED** — all members built successfully |
| R-02 | QEMU version incompatibility (x86 emulation differences) | Medium | Medium | Pinned QEMU flags: `-cpu qemu32 -smp 1 -m 64M`; tested on Ubuntu 22.04 | **RESOLVED** — consistent results |
| R-03 | Multiboot header misalignment (magic not at first 8KB) | Low | High | Linker script places `.multiboot` section first; verified with `readelf -S kernel.elf` | **RESOLVED** — boots cleanly |
| R-04 | GDT flush far jump corrupting segment registers | Medium | High | gdt_flush() in NASM; far jump to CS=0x08 flushes segment cache atomically | **RESOLVED** — `[S1] GDT initialized` confirmed |
| R-05 | Physical allocator bitmap overwriting kernel memory | Medium | High | mm_init() marks all frames below kernel end as allocated before scanning Multiboot map | **RESOLVED** — verified via GDB in S1 testing |
| R-06 | Paging enable before identity map causes immediate PF | High | Critical | Identity-map first 4MB before enabling CR0.PG; load CR3 before CR0 set | **RESOLVED** — paging_enable() sequence correct |
| R-07 | Heap overlapping kernel BSS/data | Medium | Critical | Heap starts at 0xC0400000 (above 4MB kernel map ending at 0xC03FFFFF); physically at 0x00500000 (explicitly mapped). **BUG FIX applied:** heap physical frames 0x500000–0x8FFFFF now reserved in mm_phys.c bitmap so mm_alloc_frame() cannot hand them out. | **RESOLVED** — bitmap reservation confirmed |
| R-08 | IDT handler stubs not saving all registers (corrupt task state) | High | High | common_isr_stub saves pushad + segment regs; registers_t struct layout corrected to match NASM PUSHAD order (EDI lowest, EAX highest). **BUG FIX applied:** registers_t field order in isr.h fixed. | **RESOLVED** — struct now matches actual stack layout |
| R-09 | PIC spurious IRQs before remap causing exceptions | Medium | Medium | pic_remap() called before STI; master→0x20, slave→0x28 before any IRQ can fire | **RESOLVED** — no spurious IRQs seen |
| R-10 | PIT divisor rounding error causing drift > 1% | Low | Medium | Divisor 11932 → 99.997 Hz (0.003% error); verified by S5 PERF-02: 1000 ticks in 10s | **RESOLVED** — `[PASS] NFR-PERF-02` |
| R-11 | context_switch() argument offset wrong after compiler stack frame | High | Critical | Verified by disassembling `sched.o` with objdump: `push ebp + sub esp,0x18` + two arg pushes = [esp+52]/[esp+56] after 48-byte save; correct | **RESOLVED** — offsets confirmed via objdump |
| R-12 | Task stack overflow under heavy IRQ nesting | Low | High | Each preemption uses ~188 bytes on 8KB stack; stack fully unwinds on resume; max nesting depth ~42 — well within budget | **MONITORED** — acceptable for kernel tasks |
| R-13 | kmalloc/kfree corruption under concurrent tasks | High | High | CLI/STI guards added to both kmalloc() and kfree() in mm/heap.c; entire free-list walk is atomic w.r.t. IRQ0. **BUG FIX applied.** | **RESOLVED** — S5 heap stress test PASS |
| R-14 | sched_add_task() enabling interrupts unexpectedly early | Medium | Medium | EFLAGS save/restore via pushf+cli / push+popf replaces bare cli/sti; IF is restored to its pre-call state, not unconditionally set. **BUG FIX applied.** | **RESOLVED** — defensive coding applied |
| R-15 | VGA scrolling overwriting kernel data at 0xB8000 | Low | Medium | VGA buffer is at 0xB8000 in physical / identity-mapped region; kernel is at 0x100000+; no overlap | **MONITORED** — not an issue in practice |

---

## Discovered Risk (Post-SRS)

| Risk ID | Description | Root Cause | Impact Observed | Resolution |
|---|---|---|---|---|
| R-16 | `context_switch.asm` `.resume:` label missing `ret` instruction | A previous debugging session incorrectly concluded that `ret` would jump to DS=0x0010 as a return address. The reasoning was wrong — it confused the ESP state after IRET with the ESP state before the save block. | Triple fault (invalid opcode at fall-through address) → QEMU machine reset → infinite reboot loop. Affected entire Sprint 5 integration. | `ret` restored to `.resume:`. Contradictory comment block deleted. Full explanation in `arch/x86/context_switch.asm`. |
| R-17 | Heap physical backing region (0x500000–0x8FFFFF) not reserved in bitmap | `mm_init()` freed all GRUB-marked-available frames above 0x100000, which includes the heap's physical backing region. Kernel frames were re-protected but heap frames were not. | `mm_alloc_frame()` could return frames from inside the heap backing region. Page tables and TCBs allocated during S5 would overwrite live heap data, causing free-list corruption and random task stack corruption — manifests as non-deterministic crashes under multi-task load. | Reserved frames 0x500000–0x8FFFFF in `mm_init()` after kernel protection loop. `mm_phys.c` line 114+. |
| R-18 | `registers_t` general-purpose register field order inverted vs NASM PUSHAD | NASM `PUSHAD` pushes EAX first (highest addr) and EDI last (lowest addr). The struct declared `eax, ecx, edx, ebx...` at the lowest offsets, which is the opposite of actual stack layout. | No current crash: only `int_no`, `err_code`, `eip`, `cs`, `eflags` are read, and those sit above the entire pushad block at correct offsets. Latent: any future syscall handler reading `regs->eax` would silently receive `regs->edi`. | Corrected field order in `include/isr.h` to `edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax`. |
| R-19 | `sched_add_task()` bare STI re-enables interrupts unconditionally | CLI/STI pattern assumed interrupts were enabled at call time. If called from a CLI context, STI would unexpectedly re-enable interrupts mid-setup, allowing IRQ0 → sched_switch() before task is fully enqueued. | Race: a second task could be selected before `task_count` is incremented, causing sched_switch() to see task_count<2 and skip scheduling. | Replaced bare cli/sti with EFLAGS pushf+cli / push+popf save-restore. `scheduler/sched.c`. |

---

## Sprint-by-Sprint Review Notes

### S0 — Environment Setup (Shardul)
- **Completed on time.** Cross-compiler, NASM, QEMU, GRUB tools all installed.
- Linker script places kernel at 0x100000 with 4KB-aligned sections.
- boot.asm Multiboot header verified: magic `0x1BADB002`, flags `0x00000003`.
- VGA driver functional; serial logger functional.
- **No open issues.**

### S1 — Segmentation & Physical Memory (Aman)
- **Completed on time.** GDT: null + kernel code (DPL=0) + kernel data (DPL=0).
- Bitmap allocator: 1 bit per 4KB frame. mm_alloc_frame() / mm_free_frame() verified.
- Multiboot memory map parsed correctly; kernel frames pre-marked allocated.
- **No open issues.**

### S2 — Virtual Memory & Kernel Heap (Aman)
- **Completed on time.** Paging active with identity map + higher-half kernel.
- Page fault handler ISR 14 logs CR2 + error code and halts.
- Heap: 4MB at 0xC0400000, first-fit allocator with forward coalescing.
- **Note:** CLI/STI guards in heap were deferred to S5 (single-threaded in S2). Added in S5 integration.

### S3 — Interrupt Infrastructure (Raunak)
- **Completed on time.** 256-entry IDT, PIC remapped, PIT at 100Hz.
- ISR stubs 0–31 (exceptions) + IRQ stubs 0–15 in isr_stubs.asm.
- GPF (ISR 13) and PF (ISR 14) handlers confirmed via IDT audit.
- **No open issues.**

### S4 — Scheduler, TCB & Context Switch (Raunak)
- **Completed on time in isolation.** Round-robin A→B→C verified in standalone test.
- TCB struct: 24 bytes; offsets verified via `_Static_assert`.
- task_create() builds correct IRET frame (EFLAGS=0x0202 for IF=1).
- **Latent bug:** `.resume: ret` accidentally removed during debugging; manifested only in S5 integration.

### S5 — Integration & NFR Validation (Jalendhar)
- **Completed after debugging.** All 14 NFR tests PASS.
- Root cause of reboot loop identified and fixed (R-16).
- CLI/STI guards added to heap (R-13) and sched_add_task (R-14).
- **Key lesson:** Integration testing revealed bugs that unit testing in isolation could not catch.

### S6 — Documentation & Submission (Shardul)
- RTM complete — all REQ-XX mapped to file + function + evidence.
- Demo script written with expected output and troubleshooting guide.
- Risk register updated with all 15 original + 1 discovered risk.
- Code freeze: all `.o`, `.elf`, `.iso` removed from repo before tagging.