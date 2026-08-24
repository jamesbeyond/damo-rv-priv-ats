**[中文](../testplan/Shcounterenw_test_plan.md) | English**

# Shcounterenw Extension Test Plan

This document describes the test plan for the Shcounterenw (Counter-Enable Writability for Hypervisor, Version 1.0) extension. The Shcounterenw extension specifies: if the extension is implemented, then for any `hpmcounter` that is not read-only zero, the corresponding bit in `hcounteren` must be writable.

---

## Overview

In the RISC-V Hypervisor extension, the `hcounteren` register controls VS/VU-mode (V=1) access to hardware performance counters (`cycle`, `time`, `instret`, `hpmcounter3`–`hpmcounter31`). When a bit in `hcounteren` is 0, VS-mode access to the corresponding counter triggers a virtual-instruction exception (cause=22), provided the same bit in `mcounteren` is 1.

However, the base H-extension specification (`norm:hcounteren_warl`) allows any bit of `hcounteren` to be read-only zero. This prevents hypervisor software from reliably granting guests access to counters.

**Core constraint of the Shcounterenw extension**:

> If an `hpmcounter` is not read-only zero (i.e., the counter is implemented and meaningful), the corresponding bit in `hcounteren` **must** be writable (i.e., the hypervisor can set it to 0 or 1).

This constraint ensures:
1. The hypervisor can precisely control guest (VS/VU-mode) access to implemented counters
2. The hypervisor can grant or revoke guest read access to performance counters as needed

---

## Test Scope

### SPEC Sections Covered by This Document

This plan is based on the RISC-V Privileged Architecture specification (the Shcounterenw extension chapter and the Hypervisor extension chapters related to hcounteren):

- Local SPEC paths:
  - `SPEC/riscv-isa-manual/src/priv/shcounterenw.adoc` — Shcounterenw Extension for Counter-Enable Writability, Version 1.0
  - `SPEC/riscv-isa-manual/src/priv/hypervisor.adoc` — `hcounteren` register definition (lines 846–877), conditions under which VS/VU-mode counter access triggers a virtual-instruction exception (lines 2348–2365)
- Official GitHub repository: https://github.com/riscv/riscv-isa-manual (mapped via `SPEC/riscv-isa-manual` in `.gitmodules`)

### Key Reference Files

| Path | Description |
|------|-------------|
| `shcounterenw.adoc` | Full Shcounterenw specification (6 lines total) |
| `hypervisor.adoc:846-877` | `hcounteren` register specification (32-bit RW WARL, access control logic) |
| `hypervisor.adoc:2348-2365` | Exact conditions under which VS/VU-mode counter access triggers a virtual-instruction exception |
| `common/encoding.h:275` | `CSR_HCOUNTEREN = 0x606` |
| `common/encoding.h:15` | `CSR_MCOUNTEREN = 0x306` |
| `common/encoding.h:391-450` | `CSR_MHPMCOUNTER3`–`CSR_MHPMCOUNTER31` / `CSR_HPMCOUNTER3`–`CSR_HPMCOUNTER31` |
| `common/encoding.h:305` | `CAUSE_VIRTUAL_INSTRUCTION = 22` |
| `common/hyp/hyp_priv.h:21,24` | `run_in_vs_mode(fn, arg)` / `run_in_vu_mode(fn, arg)` |
| `common/hyp/hyp_csr.h:64-66` | `hcounteren_write()` / `hcounteren_read()` / `hcounteren_set()` / `hcounteren_clear()` |
| `common/hyp/hyp_test.h:79-88` | `EXPECT_VIRTUAL_INST(stmt)` — expect virtual-instruction exception |
| `common/hyp/hyp_test.h:95-100` | `VS_EXPECT_NO_TRAP(stmt)` — expect no exception |
| `DOCS/framework/hypervisor_framework.md` | Hypervisor test framework overview |
| `DOCS/testplan/sscounterenw_test_plan.md` | Sibling S-mode scounteren test plan, used as structural reference |

### Covered Specification Points

| Norm ID | Original Text |
|---------|---------------|
| `norm:shcounterenw_hpmcounter_hcounteren` | If the Shcounterenw extension is implemented, then for any `hpmcounter` that is not read-only zero, the corresponding bit in `hcounteren` must be writable. |
| `norm:hcounteren_sz` | The counter-enable register `hcounteren` is a 32-bit register that controls the availability of the hardware performance monitoring counters to the guest virtual machine. |
| `norm:hcounteren_op` | When the CY, TM, IR, or HPMn bit in the `hcounteren` register is clear, attempts to read the corresponding counter while V=1 will cause a virtual-instruction exception if the same bit in `mcounteren` is 1. |
| `norm:hcounteren_warl` | `hcounteren` must be implemented. However, any of the bits may be read-only zero, indicating reads to the corresponding counter will cause an exception when V=1. Hence, they are effectively WARL fields. |
| `norm:H_virtinst_vs_nonhighctr_h0_m1` | In VS-mode, attempts to access a non-high-half counter CSR when the corresponding bit in `hcounteren` is 0 and the same bit in `mcounteren` is 1. |
| `norm:H_virtinst_vu_nonhighctr_h0_s0_m1` | In VU-mode, attempts to access a non-high-half counter CSR when the corresponding bit in either `hcounteren` or `scounteren` is 0 and the same bit in `mcounteren` is 1. |
| `norm:hcounteren_acc` | In addition, when the TM bit in the `hcounteren` register is clear, attempts to access the `vstimecmp` register (via `stimecmp`) while executing in VS-mode will cause a virtual-instruction exception if the same bit in `mcounteren` is set. |

### Out of Scope

- **Correctness of counter event counting**: Shcounterenw only constrains hcounteren writability; it does not cover whether counters count correctly
- **scounteren writability**: covered by the Sscounterenw test plan (`DOCS/testplan/sscounterenw_test_plan.md`)
- **mcounteren writability**: mcounteren writability is defined by the base privileged specification and is not specific to Shcounterenw
- **Sscofpmf / Smcntrpmf (counter overflow and mode filtering)**: covered by separate test plans
- **hcounteren.TM gating of vstimecmp**: this behavior is defined by `norm:hcounteren_acc` and is not a Shcounterenw-specific constraint
- **RV32 / high-half CSRs (hpmcounterNh)**: only RV64 is covered
- **Multi-hart scenarios**: the project is a single-core test environment
- **G-stage address translation**: this test only uses identity mapping to establish a basic execution environment

---

## Prerequisites and Constraints

> [!IMPORTANT]
> Verification of Shcounterenw first requires determining which `hpmcounter`s are "implemented" (not read-only zero). The verification strategy is: in M-mode, attempt to write a non-zero value to `mhpmcounter` and read it back; if the readback value is non-zero, the counter is considered implemented. For implemented counters, verify the writability of the corresponding `hcounteren` bit.

### Counter Implementation Probing Strategy

```
for each counter index i (0..31):
    1. For i=0 (cycle): nearly all implementations provide cycle; treat as implemented directly
    2. For i=1 (time): nearly all implementations provide time; treat as implemented directly
    3. For i=2 (instret): nearly all implementations provide instret; treat as implemented directly
    4. For i=3..31 (hpmcounter3-31):
       - In M-mode, write a non-zero value (e.g., 0xDEADBEEF) to mhpmcounter[i]
       - Read back mhpmcounter[i]; if the readback value is non-zero, it is implemented
       - Restore the original value
    5. Record a bitmap of implemented counters for use by subsequent tests
```

---

## Design Key Points

### 1. Hierarchical Relationship Between hcounteren and mcounteren

The access control hierarchy chain is `mcounteren → hcounteren → scounteren`:

- **mcounteren** gates HS-mode access to counters and also gates VS/VU-mode (when an mcounteren bit=0, all lower privilege levels cannot access)
- **hcounteren** gates VS-mode access to counters (provided the corresponding mcounteren bit=1)
- **scounteren** gates VU-mode access to counters (provided the corresponding hcounteren bit=1)

Shcounterenw constrains the writability of the **hcounteren** level.

### 2. virtual-instruction exception vs illegal-instruction exception

- `hcounteren bit=0, mcounteren bit=1`: VS-mode access triggers **virtual-instruction exception (cause=22)**
- `mcounteren bit=0`: VS-mode access triggers **illegal-instruction exception (cause=2)**

Tests must ensure the corresponding `mcounteren` bit=1 so that the control effect of `hcounteren` can be observed (triggering cause=22 rather than cause=2).

### 3. Framework Support for VS-mode Access

Use `run_in_vs_mode(fn, arg)` to enter VS-mode and perform counter reads. When the `hcounteren` bit=0:
- VS-mode execution of `csrr cycle/time/instret/hpmcounterN` triggers a virtual-instruction exception
- The trap escalates to HS-mode (since virtual-instruction exceptions are not delegated to VS via hedeleg)
- Verify using the framework's `trap_expect_begin/end` + `EXPECT_VIRTUAL_INST` macro

### 4. Isolation from hyp_reset_state

`common/hyp/hyp_csr.h` provides the `hcounteren_write()`, `hcounteren_set()`, and `hcounteren_clear()` APIs. Save `hcounteren` before the test starts and restore it at the end. `hyp_reset_state()` calls `hcounteren_write(0)` to reset to all zeros (all counters invisible to guests).

### 5. Additional Conditions for VU-mode End-to-End Verification

Counter access in VU-mode requires all of the following:
- `mcounteren` bit=1
- `hcounteren` bit=1
- `scounteren` bit=1

Only when all three levels are 1 can VU-mode access without an exception. When Group 4 verifies the control effect of the hcounteren level, it must ensure mcounteren=1 and scounteren=1, varying only hcounteren.

---

## Test Groups

> [!IMPORTANT]
> 5 test groups, 21 test cases in total. Group 1 verifies the basic writability of hcounteren bits in M-mode; Group 2 performs end-to-end verification of hcounteren access control in VS-mode; Group 3 verifies bit toggle consistency; Group 4 verifies hierarchical interaction in VU-mode; Group 5 records hcounteren bit behavior for read-only zero counters.

---

### Group 1: hcounteren Writability Verification (M-mode Read-Write Loopback)

**Spec Reference**:
- `norm:shcounterenw_hpmcounter_hcounteren` (`shcounterenw.adoc:4-6`): the hcounteren bit corresponding to an implemented counter must be writable

**Test Scope**: In M-mode, for each hcounteren bit corresponding to an implemented hpmcounter, perform write 1/readback and write 0/readback verification.

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| SHCNTW-WR-01 | hcounteren[0] for cycle is writable | Probe whether cycle is implemented; if so, verify hcounteren[0] can be written to 1 and 0 | Readback matches written value |
| SHCNTW-WR-02 | hcounteren[1] for time is writable | Probe whether time is implemented; if so, verify hcounteren[1] can be written to 1 and 0 | Readback matches written value |
| SHCNTW-WR-03 | hcounteren[2] for instret is writable | Probe whether instret is implemented; if so, verify hcounteren[2] can be written to 1 and 0 | Readback matches written value |
| SHCNTW-WR-04 | hcounteren[3:31] for hpmcounter3–31 are writable | Probe hpmcounter3–31 one by one; for implemented ones, verify the corresponding hcounteren bit is writable | Readback matches written value for bits of implemented counters |
| SHCNTW-WR-05 | hcounteren is a 32-bit register | On RV64, write all 1s to hcounteren and check the upper 32 bits on readback | bits[63:32] are read-only zero (`norm:hcounteren_sz`) |

---

### Group 2: hcounteren Controls VS-mode Access (End-to-End Verification)

**Spec Reference**:
- `norm:hcounteren_op` (`hypervisor.adoc:856-864`): when hcounteren bit=0 + mcounteren bit=1, VS-mode access triggers a virtual-instruction exception
- `norm:H_virtinst_vs_nonhighctr_h0_m1` (`hypervisor.adoc:2351-2353`): exact trigger condition

**Test Scope**: For implemented hpmcounters, verify that VS-mode reads succeed when the hcounteren bit is set to 1, and that VS-mode reads trigger a virtual-instruction exception (cause=22) when set to 0.

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| SHCNTW-ACCESS-01 | VS-mode reads cycle successfully when hcounteren[0]=1 | Set mcounteren[0]=1, hcounteren[0]=1, VS-mode reads cycle | No exception |
| SHCNTW-ACCESS-02 | VS-mode reading cycle when hcounteren[0]=0 triggers exception | Set mcounteren[0]=1, hcounteren[0]=0, VS-mode reads cycle | Triggers virtual-instruction exception (cause=22) |
| SHCNTW-ACCESS-03 | VS-mode reads time successfully when hcounteren[1]=1 | Set mcounteren[1]=1, hcounteren[1]=1, VS-mode reads time | No exception |
| SHCNTW-ACCESS-04 | VS-mode reading time when hcounteren[1]=0 triggers exception | Set mcounteren[1]=1, hcounteren[1]=0, VS-mode reads time | Triggers virtual-instruction exception (cause=22) |
| SHCNTW-ACCESS-05 | VS-mode reads instret successfully when hcounteren[2]=1 | Set mcounteren[2]=1, hcounteren[2]=1, VS-mode reads instret | No exception |
| SHCNTW-ACCESS-06 | VS-mode reading instret when hcounteren[2]=0 triggers exception | Set mcounteren[2]=1, hcounteren[2]=0, VS-mode reads instret | Triggers virtual-instruction exception (cause=22) |
| SHCNTW-ACCESS-07 | VS-mode reads hpmcounterN successfully when hcounteren[N]=1 | For an implemented hpmcounterN, set the corresponding bit=1, VS-mode reads | No exception |
| SHCNTW-ACCESS-08 | VS-mode reading hpmcounterN when hcounteren[N]=0 triggers exception | For an implemented hpmcounterN, set the corresponding bit=0, VS-mode reads | Triggers virtual-instruction exception (cause=22) |

---

### Group 3: hcounteren Bit Toggle Consistency

**Spec Reference**:
- `norm:shcounterenw_hpmcounter_hcounteren`: writability implies bits can be toggled between 0 and 1 repeatedly

**Test Scope**: For implemented counters, repeatedly set and clear hcounteren bits, verifying VS-mode access behavior is consistent after each toggle.

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| SHCNTW-TOGGLE-01 | Repeated toggling of the cycle bit | Toggle hcounteren[0] 1→0→1→0, verify VS-mode behavior each time | Behavior matches the current bit value after each toggle |
| SHCNTW-TOGGLE-02 | Repeated toggling of the instret bit | Toggle hcounteren[2] 1→0→1→0, verify VS-mode behavior each time | Behavior matches the current bit value after each toggle |
| SHCNTW-TOGGLE-03 | Repeated toggling of the hpmcounterN bit | Perform toggle verification for implemented hpmcounterN | Behavior matches the current bit value after each toggle |

---

### Group 4: mcounteren / hcounteren / scounteren Hierarchical Interaction

**Spec Reference**:
- `norm:hcounteren_op` (`hypervisor.adoc:856-864`): mcounteren still gates VS/VU-mode access
- `norm:H_virtinst_vu_nonhighctr_h0_s0_m1` (`hypervisor.adoc:2359-2361`): VU-mode requires both hcounteren and scounteren to be 1

**Test Scope**: Verify the hierarchical gating relationship of mcounteren/hcounteren/scounteren: when mcounteren=0, access is impossible even with hcounteren=1; VU-mode requires all three levels to be 1 simultaneously.

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| SHCNTW-HIER-01 | VS-mode reading cycle when mcounteren[0]=0 triggers exception | mcounteren[0]=0, hcounteren[0]=1, VS-mode reads cycle | Triggers illegal-instruction exception (cause=2) |
| SHCNTW-HIER-02 | VS-mode reads cycle successfully when mcounteren[0]=1, hcounteren[0]=1 | Both levels enabled, VS-mode reads cycle | No exception |
| SHCNTW-HIER-03 | VU-mode reads cycle successfully when mcounteren=1, hcounteren=1, scounteren=1 | All three levels enabled, VU-mode reads cycle | No exception |
| SHCNTW-HIER-04 | VU-mode reading cycle when mcounteren=1, hcounteren=0, scounteren=1 triggers exception | hcounteren blocks, VU-mode reads cycle | Triggers virtual-instruction exception (cause=22) |
| SHCNTW-HIER-05 | VU-mode reading cycle when mcounteren=1, hcounteren=1, scounteren=0 triggers exception | scounteren blocks, VU-mode reads cycle | Triggers virtual-instruction exception (cause=22) |

> [!NOTE]
> **Exception type of SHCNTW-HIER-01**: When the `mcounteren` bit=0, regardless of the `hcounteren` setting, VS-mode counter access triggers **illegal-instruction exception (cause=2)** rather than a virtual-instruction exception. This is because mcounteren=0 means "the counter is invisible below S-mode", and the H extension no longer intervenes (not treated as hypervisor-level virtualization interception).

> [!NOTE]
> **Exception type of SHCNTW-HIER-04/05**: During VU-mode access, if the corresponding bit of `hcounteren` or `scounteren` is 0 (with mcounteren=1), both trigger a virtual-instruction exception (cause=22), because the trap semantics are "the hypervisor intercepted the guest's counter access".

> [!IMPORTANT]
> **Prerequisite for SHCNTW-HIER-04/05: scounteren writability verification**
> SHCNTW-HIER-05 depends on `scounteren` bit 0 being clearable. However, Shcounterenw only guarantees **hcounteren** writability; `scounteren` writability is determined by Smcntrpmf or platform policy. The test implementation must, before executing HIER-04/05:
> 1. Attempt to write `scounteren[0] = 0` and verify via readback
> 2. If `scounteren[0]` cannot be cleared (read-only 1), SHCNTW-HIER-05 should execute `TEST_SKIP("scounteren[0] is not writable on this platform")`
> 3. SHCNTW-HIER-04 is unaffected (it relies on hcounteren=0 blocking; scounteren=1 remains at its default value)
>
> Probing flow: first attempt to clear `scounteren[0]` and read back; if it cannot be cleared (read-only 1), SHCNTW-HIER-05 executes `TEST_SKIP("scounteren[0] not writable")`; restore the original `scounteren` value after probing.

---

### Group 5: hcounteren Bit Behavior for Read-Only Zero Counters

**Spec Reference**:
- `norm:hcounteren_warl` (`hypervisor.adoc:873-876`): any bit of hcounteren may be read-only zero
- Shcounterenw **does not constrain** hcounteren bits corresponding to read-only zero counters

**Test Scope**: For hpmcounters probed as read-only zero, record their hcounteren bit behavior (which may be read-only 0 or writable), without pass/fail judgment, for information collection only.

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| SHCNTW-RO-01 | Probe hcounteren bits of read-only zero counters | For all read-only zero hpmcounters, attempt to write the hcounteren bits and report the results | Informational output, no pass/fail |

---

## Appendix: Related Constants and API Reference

### CSR and Bit Definitions

| Name | Value | Description |
|------|-------|-------------|
| `CSR_HCOUNTEREN` | `0x606` | Hypervisor counter-enable register, see `common/encoding.h:275` |
| `CSR_MCOUNTEREN` | `0x306` | Machine counter-enable register, see `common/encoding.h:15` |
| `CSR_SCOUNTEREN` | `0x106` | Supervisor counter-enable register, see `common/encoding.h:139` |
| `CSR_CYCLE` | `0xC00` | Read-only cycle counter (user-level) |
| `CSR_TIME` | `0xC01` | Read-only time counter (user-level) |
| `CSR_INSTRET` | `0xC02` | Read-only instret counter (user-level) |
| `CSR_HPMCOUNTER3`–`31` | `0xC03`–`0xC1F` | Read-only HPM counters (user-level) |
| `CSR_MHPMCOUNTER3`–`31` | `0xB03`–`0xB1F` | Writable HPM counters (machine-level) |
| `CAUSE_VIRTUAL_INSTRUCTION` | `22` | Virtual-instruction exception |
| `CAUSE_ILLEGAL_INSTRUCTION` | `2` | Illegal-instruction exception |

### hcounteren Bit Index Mapping

| Bit | Name | Corresponding Counter |
|-----|------|----------------------|
| 0 | CY | cycle |
| 1 | TM | time |
| 2 | IR | instret |
| 3–31 | HPM3–HPM31 | hpmcounter3–hpmcounter31 |

### Test Framework API

- `hcounteren_read()` / `hcounteren_write(value)`: hcounteren read/write
- `hcounteren_set(mask)` / `hcounteren_clear(mask)`: hcounteren set/clear bits
- `run_in_vs_mode(fn, arg)`: execute fn(arg) in VS-mode (V=1)
- `run_in_vu_mode(fn, arg)`: execute fn(arg) in VU-mode (V=1, U)
- `trap_expect_begin()` / `trap_expect_end()`: trap capture window
- `trap_was_triggered()` / `trap_get_cause()`: trap status query
- `EXPECT_VIRTUAL_INST(stmt)`: expect virtual-instruction exception macro
- `VS_EXPECT_NO_TRAP(stmt)`: expect no exception in VS-mode macro
- `HYP_TEST_END()`: test end macro (includes hyp_reset_state)
- `csr_read_dynamic(addr)` / `csr_write_dynamic(addr, val)`: dynamic CSR read/write by address

---

## Test Statistics

| Group | Test Count | Description |
|-------|------------|-------------|
| Group 1: Writability Verification | 4 | M-mode basic read-write loopback |
| Group 2: VS-mode Access Control | 8 | End-to-end permission verification |
| Group 3: Toggle Consistency | 3 | Repeated toggle verification |
| Group 4: Hierarchical Interaction | 5 | mcounteren/hcounteren/scounteren gating |
| Group 5: Read-Only Zero Report | 1 | Information collection |
| **Total** | **21** | |

---

## Test Execution Notes

### Runtime Environment

- Group 1: M-mode directly operates hcounteren (CSR 0x606), no need to enter VS-mode
- Group 2/3: enter VS-mode via `run_in_vs_mode` to verify counter access
- Group 4: involves hierarchical verification of VS-mode and VU-mode
- Group 5: M-mode probing and reporting
- Single-core environment, no IPI required

### Failure Diagnosis Guide

| Symptom | Possible Cause |
|---------|----------------|
| SHCNTW-WR-01/02/03 failure | Shcounterenw not enabled, hcounteren bits remain read-only zero |
| Some bits fail in SHCNTW-WR-04 | Corresponding hpmcounter implementation probing error (misidentified as implemented), or that bit is indeed read-only |
| SHCNTW-ACCESS-01 failure (exception occurs) | hcounteren_write did not take effect, or mcounteren not properly set |
| SHCNTW-ACCESS-02 failure (no exception) | hcounteren bit=0 did not take effect, VS-mode can still access the counter |
| SHCNTW-ACCESS-02 failure (cause≠22) | mcounteren may be 0, causing cause=2 instead of cause=22 |
| SHCNTW-HIER-01 triggers cause=22 instead of cause=2 | mcounteren clearing did not take effect; the implementation may still take the virtual-inst path when mcounteren=0 (non-compliant) |
| SHCNTW-HIER-04/05 failure (no exception) | hideleg/hedeleg configuration issue, or scounteren/hcounteren writes did not take effect |
| SHCNTW-TOGGLE failure | Race condition or caching issue in hcounteren bit writes (theoretically should not occur in single-core) |

---

## Appendix: Specification Point Coverage Matrix

| Norm ID | Covering Test Cases | Notes |
|---------|---------------------|-------|
| `norm:shcounterenw_hpmcounter_hcounteren` | SHCNTW-WR-01 ~ SHCNTW-WR-04, SHCNTW-TOGGLE-01 ~ SHCNTW-TOGGLE-03 | Core constraint: hcounteren bits of implemented counters are writable and can be toggled repeatedly |
| `norm:hcounteren_sz` | SHCNTW-WR-05 | 32-bit width verification |
| `norm:hcounteren_op` | SHCNTW-ACCESS-01 ~ SHCNTW-ACCESS-08, SHCNTW-HIER-01, SHCNTW-HIER-02 | VS access triggers cause=22 when bit=0 + mcounteren=1 |
| `norm:hcounteren_warl` | SHCNTW-RO-01 | Information collection only: bit behavior of read-only zero counters is not constrained by Shcounterenw |
| `norm:H_virtinst_vs_nonhighctr_h0_m1` | SHCNTW-ACCESS-02, SHCNTW-ACCESS-04, SHCNTW-ACCESS-06, SHCNTW-ACCESS-08, SHCNTW-TOGGLE-01 ~ SHCNTW-TOGGLE-03 | Exact verification of VS-mode trigger conditions |
| `norm:H_virtinst_vu_nonhighctr_h0_s0_m1` | SHCNTW-HIER-03 ~ SHCNTW-HIER-05 | VU-mode three-level gating verification |
| `norm:hcounteren_acc` | — | Not covered: not a Shcounterenw-specific constraint, see "Out of Scope" |
