#ifndef S5_INTEGRATION_H
#define S5_INTEGRATION_H

/*
 * include/s5_integration.h
 * Sprint 5 — Integration & NFR Validation public API.
 *
 * Implements:
 *   NFR-PERF-01 : context switch latency < 50µs
 *   NFR-PERF-02 : PIT stability (~1000 ticks / 10 s)
 *   NFR-SEC-01  : kernel-user page isolation
 *   NFR-REL-01  : GPF + PF handlers installed and tested
 *   NFR-MAINT-01: naming-prefix + arch/ isolation audit
 *   EXT-SW-01   : scheduler ↔ paging interface validation
 *
 * Owner : Palthyavath Jalendhar (B24CS1051)
 * Sprint: S5 — Integration & NFR validation
 *
 * Usage in kernel_main (call AFTER sched_init, BEFORE sched_start):
 *
 *   __asm__ volatile ("sti");
 *   s5_run_all_tests();
 *   __asm__ volatile ("cli");
 *
 * All results go to the COM1 serial log.
 * The VGA summary line is printed by s5_print_vga_summary().
 */

#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Individual test entry points                                        */
/* ------------------------------------------------------------------ */

/**
 * s5_test_ext_sw01()
 *   Validates EXT-SW-01: paging_create_directory() /
 *   paging_free_directory() called correctly by the scheduler path.
 *   Creates a task directory, maps the kernel into it, verifies the
 *   higher-half PDE is present, then frees it cleanly.
 */
void s5_test_ext_sw01(void);

/**
 * s5_test_perf01_context_switch()
 *   Measures round-trip context-switch latency using the PIT tick
 *   counter as a coarse timer.
 *   Reports PASS if measured latency is < 50µs (NFR-PERF-01).
 *
 *   Method: time 1000 forced sched_switch() calls; divide total by
 *   1000.  Because pit_get_ticks() has 10ms resolution the result is
 *   conservative (over-estimates latency).
 */
void s5_test_perf01_context_switch(void);

/**
 * s5_test_perf02_pit_stability()
 *   Busy-waits for exactly 10 seconds (1000 ticks) and counts how
 *   many PIT ticks actually fire.  Reports PASS if within ±1% of
 *   1000 (NFR-PERF-02).
 */
void s5_test_perf02_pit_stability(void);

/**
 * s5_test_sec01_kernel_isolation()
 *   Verifies NFR-SEC-01: a page mapped without PAGE_USER must not be
 *   accessible from ring-3.  Because MiniOS has no ring-3 yet, this
 *   test statically audits the kernel page-table entries and confirms
 *   the U/S bit is 0 for every kernel mapping.
 */
void s5_test_sec01_kernel_isolation(void);

/**
 * s5_test_heap_stress()
 *   Concurrent-pattern heap stress test: allocates and frees blocks
 *   of varying sizes in an interleaved pattern that mimics multi-task
 *   contention.  Verifies no corruption and the heap returns to a
 *   single free block.
 */
void s5_test_heap_stress(void);

/**
 * s5_audit_naming_and_arch()
 *   Static audit: emits a serial checklist confirming:
 *   - mm_ / paging_ / sched_ / isr_ / pit_ / pic_ prefixes are used.
 *   - CR0/CR3/port-I/O code is confined to arch/x86/.
 *   (This is a documentation audit, not a runtime check.)
 */
void s5_audit_naming_and_arch(void);

/* ------------------------------------------------------------------ */
/* Convenience wrapper — runs all S5 tests in order                  */
/* ------------------------------------------------------------------ */

/**
 * s5_run_all_tests()
 *   Calls every test above in the order required by the SRS.
 *   Interrupts must be enabled before calling (PIT needed for timing).
 *   Disables interrupts internally around sensitive measurements.
 */
void s5_run_all_tests(void);

/**
 * s5_print_vga_summary()
 *   Prints a one-line VGA summary ("S5 NFR validation complete") after
 *   all tests have run.  Call from kernel_main after s5_run_all_tests().
 */
void s5_print_vga_summary(void);

#endif /* S5_INTEGRATION_H */