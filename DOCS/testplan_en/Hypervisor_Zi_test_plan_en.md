**[中文](../testplan/Hypervisor_Zi_test_plan.md) | English**

# Hypervisor × Z* Extensions Cross Test Plan

> This document describes the test plan for cross-scenarios between the Hypervisor (H) extension and other Z* extension families (including Zk* cryptography extensions). This plan was split out from `Hypervisor_cross_test_plan.md` and retains only the content at the intersection of the Hypervisor and Z* extensions. These test scenarios were originally marked in the standalone test plans of the respective extensions as "covered by the Hypervisor test plan" or "excluded due to the absence of the H extension", but analysis showed that the existing Hypervisor test plans do not fully cover them.
>
> Generation date: 2026-06-22

---

## SPEC Sections Covered by This Document

This plan is based on the following official RISC-V specifications (local paths):

- `SPEC/riscv-isa-manual/src/priv/hypervisor.adoc` — Hypervisor (H) extension: virtual-instruction exception mechanism for VS/VU-mode access to controlled CSRs
- `SPEC/riscv-isa-manual/src/unpriv/zk.adoc` — Zkr entropy source extension: `seed` CSR, `mseccfg.SSEED/USEED` access control
- `SPEC/riscv-isa-manual/src/unpriv/zihintntl.adoc` — Zihintntl extension: no-architectural-side-effect semantics and trap behavior of NTL HINT instructions
- `SPEC/riscv-isa-manual/src/unpriv/zcmt.adoc` — Zcmt extension: cm.jt/cm.jalt table jump instructions, jvt CSR, and two-implicit-fetch semantics

Official repository:

- https://github.com/riscv/riscv-isa-manual (files at the above paths within the repository)

---

## Scope

### Covered Extension Intersections

- **Hypervisor × Zkr**: `mseccfg.SSEED` control over VS/VU-mode access to the `seed` CSR; distinguishing virtual-instruction from illegal-instruction exception types in VS/VU-mode; SSEED control of HS-mode access to seed; read-only access exceptions taking precedence over virtual-instruction
- **Hypervisor × Zihintntl**: normal execution of NTL HINTs in HS/VS/VU-mode (must not spuriously raise a virtual-instruction exception); NTL applied to H-extension virtual-machine memory access instructions (HLV/HSV/HLVX); virtual-instruction reporting of NTL + CMO in VS-mode; G-stage guest-page-fault reporting of NTL + target in VS-mode
- **Hypervisor × Zcmt**: normal execution of table jump instructions (cm.jt/cm.jalt) in HS/VS/VU-mode; jvt CSR access and Smstateen (JVT bit) gating in VS/VU-mode; two-stage translation of JVT entry fetches in VS-mode and G-stage guest instruction page fault reporting

### Out of Scope for This Document

- Hypervisor basic functionality already covered by `Hypervisor_CSR_test_plan.md`, `Hypervisor_Interrupts_test_plan.md`, `Hypervisor_Exceptions_test_plan.md`, `Hypervisor_2_stage_test_plan.md`, `Hypervisor_gstage_test_plan.md`
- Behavior of each extension in non-Hypervisor scenarios (covered by their respective standalone test plans)
- Cross tests between the Hypervisor and Ss\*/Sv\*/Sm\* extensions (covered by `Hypervisor_Ss_test_plan.md`, `Hypervisor_Sv_test_plan.md`, `Hypervisor_Sm_test_plan.md` respectively)
- Zkr non-Hypervisor scenarios (basic control of seed access in M/S/U-mode) — covered by `Zkr_test_plan.md`
- Zihintntl non-Hypervisor scenarios (basic semantics in M/S/U-mode, encodings, compressed variants, CMO interaction, LR/SC forward progress guarantees, etc.) — covered by `zihintntl_test_plan.md`
- Zcmt non-Hypervisor scenarios (jvt WARL behavior, encoding and operational semantics, PMP/page-table fault handling, table update visibility and endianness, etc.) — covered by `zcmt_test_plan.md`

---

## Covered Specification Points

The following table lists the specification points covered by this plan. Entries with the `norm:` prefix are official SPEC labels; entries without the prefix are specification points decomposed from the SPEC text.

| Norm ID | Source | Description |
|---------|--------|-------------|
| `norm:mseccfg_sseed_VSorVU-mode_op` | `zk.adoc` | When the H extension is also implemented, access to the seed CSR from an HS-qualified instruction leads to a virtual-instruction exception in VS and VU modes; all other types of accesses raise an illegal-instruction exception. |
| `norm:mseccfg_sseed_SorHS-mode_op` | `zk.adoc` | When SSEED is 0, access to the seed CSR from S-/HS-mode raises an illegal-instruction exception. When SSEED is 1, read-write access to the seed CSR from S-/HS-mode is allowed; all other types of accesses raise an illegal-instruction exception. |
| `norm:mseccfg_sseed_useed_op_tbl` | `zk.adoc` | Entropy Source Access Control table: M always available; U controlled by USEED; S/HS controlled by SSEED; VS/VU controlled by SSEED with virtual-instruction exception for HS-qualified read-write. |
| `norm:seed_ro_illegal` | `zk.adoc` | Attempts to access the seed CSR using a read-only CSR-access instruction (csrrs/csrrc with rs1=x0 or csrrsi/csrrci with uimm=0) raise an illegal-instruction exception; any other CSR-access instruction may be used to access seed. |
| `norm:NTL_target_definition` | `zihintntl.adoc` | The insn:ntl[] instructions do not change architectural state, nor do they alter the architecturally visible effects of the target instruction. |
| `norm:NTL_range` | `zihintntl.adoc` | The insn:ntl[] instructions affect all memory-access instructions except the cache-management instructions in the ext:zicbom[] extension. |
| `norm:cm-jt_op` | `zcmt.adoc` | cm.jt reads an entry from the jump vector table in memory and jumps to the address that was read. |
| `norm:cm-jalt_op` | `zcmt.adoc` | cm.jalt reads an entry from the jump vector table in memory and jumps to the address that was read, linking to _ra_. |
| `norm:jvt_base_vm` | `zcmt.adoc` | jvt[base] is a virtual address, whenever virtual memory is enabled. |
| `norm:Zcmt_fetch` | `zcmt.adoc` | ... the execution of a table jump instruction involves two instruction fetches, the first to read the instruction (cm.jt/cm.jalt) and the second to read from the jump vector table (JVT). Both instruction fetches are _implicit_ reads, and both require execute permission; read permission is irrelevant. |
| `norm:Zcmt_trap` | `zcmt.adoc` | If an exception occurs on either instruction fetch, xEPC is set to the PC of the table jump instruction, xCAUSE is set as expected for the type of fault and xTVAL (if not set to zero) contains the fetch address which caused the fault. |
| `norm:stateen0_jvt_op` | `smstateen.adoc` | The JVT bit controls access to the `jvt` CSR provided by the Zcmt extension. |
| `norm:htval_trapval` | `hypervisor.adoc` | htval trap value reporting for guest-page faults (implementation may write zero or the faulting GPA>>2). |

---

## Group 1. Hypervisor × Zkr Cross Tests

**Spec Reference**:
- `norm:mseccfg_sseed_VSorVU-mode_op`: When the H extension is implemented, HS-qualified instruction access to seed in VS/VU-mode raises a virtual-instruction exception; other access types raise an illegal-instruction exception
- `norm:mseccfg_sseed_useed_op_tbl`: With VS/VU + SSEED=0, any access raises an illegal-instruction exception; with VS/VU + SSEED=1, read-write access raises a virtual-instruction exception
- `norm:mseccfg_sseed_SorHS-mode_op`: With SSEED=0, HS-mode access to seed raises an illegal-instruction exception; with SSEED=1, read-write access is allowed
- `norm:seed_ro_illegal`: read-only access raises an illegal-instruction exception in any mode

**Test Scope**: Verify the exception type distinction and access control of VS/VU-mode and HS-mode access to the seed CSR under the H extension.

### 1.1 HS-mode Access Control (SSEED)

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| ZKR-HYP-01 | SSEED=1 HS-mode csrrw access to seed succeeds | Set mseccfg.SSEED=1 (HS-mode under the H extension), HS-mode executes csrrw rd, seed, x0 | returns the seed value normally |
| ZKR-HYP-02 | SSEED=0 HS-mode csrrw access to seed raises an exception | Set mseccfg.SSEED=0, HS-mode executes csrrw rd, seed, x0 | illegal-instruction exception (cause=2) |
| ZKR-HYP-13 | SSEED=1 HS-mode read-only access raises an exception | Set mseccfg.SSEED=1, HS-mode executes csrrs rd, seed, x0 | illegal-instruction exception (cause=2) |

### 1.2 VS/VU-mode Access Control (SSEED + H Extension)

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| ZKR-HYP-03 | SSEED=0 VS-mode csrrw access to seed raises illegal | Set mseccfg.SSEED=0, VS-mode executes csrrw rd, seed, x0 | illegal-instruction exception (cause=2) |
| ZKR-HYP-04 | SSEED=1 VS-mode csrrw access to seed raises virtual-instruction | Set mseccfg.SSEED=1, VS-mode executes csrrw rd, seed, x0 | virtual-instruction exception (cause=22) |
| ZKR-HYP-05 | SSEED=0 VU-mode csrrw access to seed raises illegal | Set mseccfg.SSEED=0, VU-mode executes csrrw rd, seed, x0 | illegal-instruction exception (cause=2) |
| ZKR-HYP-06 | SSEED=1 VU-mode csrrw access to seed raises virtual-instruction | Set mseccfg.SSEED=1, VU-mode executes csrrw rd, seed, x0 | virtual-instruction exception (cause=22) |
| ZKR-HYP-07 | SSEED=1 VS-mode read-only access raises illegal (not virtual) | Set mseccfg.SSEED=1, VS-mode executes csrrs rd, seed, x0 | illegal-instruction exception (cause=2) (read-only access condition takes precedence) |
| ZKR-HYP-08 | SSEED=1 VS-mode csrrsi uimm=0 raises illegal | Set mseccfg.SSEED=1, VS-mode executes csrrsi rd, seed, 0 | illegal-instruction exception (cause=2) |
| ZKR-HYP-09 | SSEED=1 VS-mode csrrs(rs1≠x0) raises virtual-instruction | Set mseccfg.SSEED=1, VS-mode executes csrrs rd, seed, t0 (t0≠0) | virtual-instruction exception (cause=22) (HS-qualified read-write) |
| ZKR-HYP-10 | SSEED=0 VS-mode csrrs(rs1≠x0) raises illegal | Set mseccfg.SSEED=0, VS-mode executes csrrs rd, seed, t0 | illegal-instruction exception (cause=2) |
| ZKR-HYP-11 | SSEED does not affect M-mode (in VS/VU scenarios) | Set mseccfg.SSEED=0, M-mode csrrw seed | access succeeds |
| ZKR-HYP-14 | SSEED=1 VU-mode read-only access raises illegal (not virtual) | Set mseccfg.SSEED=1, VU-mode executes csrrs rd, seed, x0 | illegal-instruction exception (cause=2) (read-only access condition takes precedence) |
| ZKR-HYP-15 | SSEED=1 VU-mode csrrsi uimm=0 raises illegal | Set mseccfg.SSEED=1, VU-mode executes csrrsi rd, seed, 0 | illegal-instruction exception (cause=2) |
| ZKR-HYP-16 | SSEED=1 VU-mode csrrs(rs1≠x0) raises virtual-instruction | Set mseccfg.SSEED=1, VU-mode executes csrrs rd, seed, t0 (t0≠0) | virtual-instruction exception (cause=22) (HS-qualified read-write) |
| ZKR-HYP-17 | SSEED=0 VU-mode csrrs(rs1≠x0) raises illegal | Set mseccfg.SSEED=0, VU-mode executes csrrs rd, seed, t0 | illegal-instruction exception (cause=2) |
| ZKR-HYP-18 | SSEED=1 VS-mode csrrci uimm=0 raises illegal | Set mseccfg.SSEED=1, VS-mode executes csrrci rd, seed, 0 | illegal-instruction exception (cause=2) |

### 1.3 Exception Priority and Combined Scenarios

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| ZKR-HYP-12 | VU-mode read-only exception takes precedence over virtual-instruction | SSEED=1, VU-mode executes csrrs rd, seed, x0 | illegal-instruction exception (cause=2) (read-only condition → illegal, not virtual-instruction) |

> [!NOTE]
> - The tests in this group verify the behavior of the Zkr extension under Hypervisor scenarios. All tests must detect the availability of the H extension at runtime via `HAS_H_EXT()`; if unavailable, TEST_SKIP.
> - ZKR-HYP-03~18 are migrated from `Zkr_test_plan.md` and specifically target cases that depend on the H extension. Core semantics: when VS/VU-mode accesses seed, `mseccfg.SSEED` determines whether access is granted, and an HS-qualified read-write (SSEED=1) raises **virtual-instruction** (cause=22), while SSEED=0 or read-only access raises **illegal-instruction** (cause=2).
> - **Distinguishing virtual-instruction from illegal-instruction**: Test assertions must use accurate cause constants, distinguishing virtual-instruction (cause=22) with SSEED=1 from illegal-instruction (cause=2) for read-only access / SSEED=0.

---

## Group 2. Hypervisor × Zihintntl Cross Tests

**Spec Reference**:
- `norm:NTL_target_definition`: NTL instructions do not change architectural state, nor do they alter the architecturally visible effects of the target instruction; in virtualization environments, the architectural behavior of NTL prefix sequences must be identical to the non-virtualized case
- `norm:NTL_range`: NTL affects all memory-access instructions, and the H extension's HLV/HSV/HLVX virtual-machine memory access instructions are also within its scope

**Test Scope**: Verify that the behavior of NTL HINTs in virtualization environments (HS/VS/VU-mode) is consistent with non-virtualized scenarios: normal execution, no spurious virtual-instruction exception, and trap reporting information (cause/epc/tval/GVA/htval) identical to the case without the prefix. The cases in this group are migrated from `zihintntl_test_plan.md` (the virtualization portion of the original NTL-12, NTL-RG-07, NTL-CMO-08, NTL-TRAP-08).

### 2.1 NTL Execution in HS/VS/VU-mode

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| NTL-HYP-01 | HS-mode executes NTL + load | HS-mode executes ntl.all + ld, compare the destination register with memory | executes normally, no exception, result identical to the case without the prefix |
| NTL-HYP-02 | VS-mode executes NTL + load | Enter VS-mode and execute ntl.all + ld | executes normally, no virtual-instruction exception, result identical to the case without the prefix |
| NTL-HYP-03 | VU-mode executes NTL + load | Enter VU-mode and execute ntl.all + ld | executes normally, no exception, result identical to the case without the prefix |

### 2.2 NTL Applied to H-extension Virtual-Machine Memory Access Instructions

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| NTL-HYP-04 | NTL applied to HLV/HSV/HLVX | HS-mode executes HLV/HSV/HLVX with an ntl.all prefix, compared with the same sequence without the prefix | executes normally, memory-access effects correct, architectural behavior of both identical |

### 2.3 Trap Reporting of NTL + CMO in VS-mode

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| NTL-HYP-05 | ntl prefix does not change the virtual-instruction reporting of CMO instructions | If Zicbom is implemented: in VS-mode with henvcfg.CBIE=0 and CBCFE=0, execute "ntl.all + cbo.inval/cbo.clean", compared with the same sequence without the prefix | raises virtual-instruction exception (cause=22) just like without the prefix; cause/epc point to the CMO instruction, not the NTL |

### 2.4 G-stage Page Fault of NTL + target in VS-mode

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| NTL-HYP-06 | G-stage page fault of NTL + target in VS-mode | VS-mode executes "ntl.p1 + ld" triggering a G-stage guest-page-fault, compared with the same sequence without the prefix | delivered to HS-mode, load guest-page fault (cause=21), hstatus.GVA=1, stval/htval correct, behavior identical to the case without the prefix |

> [!NOTE]
> - All tests in this group must detect the availability of the H extension at runtime via `HAS_H_EXT()`; if unavailable, TEST_SKIP.
> - Zihintntl has no independent misa/CSR detection flag; it is enabled by platform configuration declaration. NTL instructions are injected into the execution stream with raw encoding (.word 0x00200033/0x00300033/0x00400033/0x00500033) to avoid toolchain alias interference.
> - NTL-HYP-05 additionally needs to probe Zicbom; NTL-HYP-06 needs to construct a mapping that is valid in VS-stage but invalid in G-stage. The core assertion strategy follows the "HINT no-side-effect comparison method" of `zihintntl_test_plan.md`: the architecturally visible behavior (including exception information) with and without the NTL prefix must be completely identical.

---

## Group 3. Hypervisor × Zcmt Cross Tests

**Spec Reference**:
- `norm:cm-jt_op` / `norm:cm-jalt_op`: table jumps are ordinary instructions with no privilege-level restrictions; they should execute normally in HS/VS/VU-mode
- `norm:jvt_base_vm`: when virtual memory is enabled, jvt.base is a virtual address; in VS-mode it undergoes two-stage translation via vsatp
- `norm:Zcmt_fetch` / `norm:Zcmt_trap`: the second fetch (JVT entry) is also translated; on a fault, xEPC points to the table jump instruction and xTVAL is the faulting fetch address
- `norm:stateen0_jvt_op`: the JVT bit of stateen0 controls jvt CSR access; it gates only CSR access, not instruction execution
- `norm:htval_trapval`: htval trap value reporting rules on G-stage faults

**Test Scope**: Verify the behavior of table jump instructions and the jvt CSR in virtualization environments: normal execution in HS/VS/VU-mode without spuriously raising a virtual-instruction exception; in VS-mode, JVT entry fetches undergo two-stage translation and G-stage faults are reported as guest instruction page fault; hstateen0.JVT gates VS/VU jvt access but not instruction execution. The cases in this group are migrated from `zcmt_test_plan.md` (the virtualization portion of the original ZCMT-27/28, ZACC-03/04/05, and the virtualization portion of ZACC-06; HZCMT-08 is a VS-stage fault case supplemented as a counterpart to ZCMT-25).

### 3.1 Table Jump Execution in HS/VS/VU-mode

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| HZCMT-01 | HS-mode executes table jumps | HS-mode executes cm.jt and cm.jalt (jvt points to a valid table) | both jump and link normally, no exception |
| HZCMT-02 | VS-mode executes table jumps | VS-mode executes cm.jt and cm.jalt | both execute normally, no virtual-instruction exception |
| HZCMT-03 | VU-mode executes table jumps | VU-mode executes cm.jt and cm.jalt | both execute normally, no exception |

### 3.2 jvt Access and stateen Gating in VS/VU-mode

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| HZCMT-04 | VS/VU-mode accesses jvt | With stateen enabled, VS/VU-mode csrr/csrw jvt | access succeeds (jvt permission URW + stateen enabled) |
| HZCMT-05 | hstateen0.JVT gates VS/VU access | When Smstateen is implemented, zero out the JVT bits of hstateen0/sstateen0 per hierarchy, VS/VU accesses jvt | VS/VU raises virtual-instruction/illegal-instruction (see `Smstateen_test_plan.md` / `Ssstateen_test_plan.md` for detailed cases) |
| HZCMT-06 | stateen does not gate table jump instruction execution | When Smstateen is implemented, zero out the JVT bits of stateen at all levels, VS-mode executes cm.jt/cm.jalt | instructions execute normally (state enable gates only jvt CSR access, not the instruction itself) |

### 3.3 Table Jumps under Two-Stage Translation in VS-mode

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| HZCMT-07 | VS-mode translation path jumps normally | VS-mode with vsatp enabled, VS-stage maps the table page with X=1, execute cm.jt | jump succeeds (VS-stage translation effective) |
| HZCMT-08 | VS-mode table page with VS-stage X=0 raises a fault | The table page is mapped in VS-stage but X=0, VS-mode executes cm.jt | instruction page fault delivered per the delegation path, sepc=cm.jt PC, stval=table entry virtual address |
| HZCMT-09 | VS-mode second fetch G-stage fault | The G-stage mapping of the table page is invalid, VS-mode executes cm.jt | guest instruction page fault (cause=20) delivered to HS-mode, hstatus.GVA=1, htval=faulting table entry GPA>>2 (norm:htval_trapval allows zero) |

> [!NOTE]
> - All tests in this group must detect the availability of the H extension at runtime via `HAS_H_EXT()`; if unavailable, TEST_SKIP. Zcmt support follows the platform configuration macro `ZCMT_SUPPORTED`; the jvt writability probe result (read-only implementation, allowed by `norm:jvt_op`) determines whether functional cases are applicable.
> - HZCMT-05 additionally needs to probe Smstateen; HZCMT-07~09 need to enable vsatp (and hgatp) to construct two-stage translation, using a mapping combination of VS-stage valid + G-stage invalid to isolate G-stage faults.
> - HZCMT-08/09 verify the fault path of the second fetch (JVT entry): vsepc must point to the cm.jt instruction itself rather than the table address (`norm:Zcmt_trap`), and vstval/htval report the table entry fetch address.

---

## Key Considerations

1. **Extension detection**: All tests must detect the availability of the required extensions (H, Zkr, Zihintntl, Zcmt, etc.) at runtime; if unavailable, TEST_SKIP. Zkr is detected by probing the presence of the `seed` CSR (0x015); Zihintntl has no independent probe flag and is enabled by platform configuration declaration; Zcmt follows the platform configuration macro `ZCMT_SUPPORTED` and trap-armed probing of the jvt CSR (0x017).

2. **SSEED control**: `mseccfg.SSEED` controls S/HS/VS/VU-mode access to the seed CSR. M-mode access is not affected by SSEED (ZKR-HYP-11).

3. **Read-only access precedence**: read-only CSR-access instructions (csrrs/csrrc with rs1=x0, or csrrsi/csrrci with uimm=0) accessing seed raise illegal-instruction (cause=2) in any mode; this condition takes precedence over the virtual-instruction determination.

4. **Distinguishing virtual-instruction from illegal-instruction**: When VS/VU-mode accesses a controlled CSR, HS-qualified read-write with SSEED=1 raises virtual-instruction (cause=22); SSEED=0 or read-only access raises illegal-instruction (cause=2).

---

## References

- `SPEC/hypervisor.adoc` — RISC-V Hypervisor Extension, Version 1.0
- `SPEC/riscv-isa-manual/src/unpriv/zk.adoc` — Zkr Entropy Source Extension
- `SPEC/riscv-isa-manual/src/unpriv/zihintntl.adoc` — Zihintntl Extension for Non-Temporal Locality Hints
- `SPEC/riscv-isa-manual/src/unpriv/zcmt.adoc` — Zcmt Extension for Compressed Table Jumps
- `DOCS/testplan/Zkr_test_plan.md` — Zkr standalone test plan
- `DOCS/testplan/zihintntl_test_plan.md` — Zihintntl standalone test plan
- `DOCS/testplan/zcmt_test_plan.md` — Zcmt standalone test plan
- `DOCS/testplan/Hypervisor_CSR_test_plan.md` — Hypervisor CSR subset test plan
- `DOCS/testplan/Hypervisor_Interrupts_test_plan.md` — Hypervisor interrupts subset test plan
- `DOCS/testplan/Hypervisor_Exceptions_test_plan.md` — Hypervisor exceptions and trap subset test plan
- `DOCS/testplan/Hypervisor_2_stage_test_plan.md` — Two-stage translation test plan
- `DOCS/testplan/Hypervisor_gstage_test_plan.md` — G-stage standalone test plan

---

## Appendix A: Specification Point Coverage Matrix

The following table indicates which test cases cover each specification point in the "Covered Specification Points" section.

| Norm ID | Covered Test IDs |
|---------|------------------|
| `norm:mseccfg_sseed_SorHS-mode_op` | ZKR-HYP-01, ZKR-HYP-02, ZKR-HYP-13 |
| `norm:mseccfg_sseed_VSorVU-mode_op` | ZKR-HYP-03~06, ZKR-HYP-09, ZKR-HYP-10, ZKR-HYP-16, ZKR-HYP-17 |
| `norm:mseccfg_sseed_useed_op_tbl` | ZKR-HYP-03~06, ZKR-HYP-11 |
| `norm:seed_ro_illegal` | ZKR-HYP-07, ZKR-HYP-08, ZKR-HYP-12, ZKR-HYP-13, ZKR-HYP-14, ZKR-HYP-15, ZKR-HYP-18 |
| `norm:NTL_target_definition` | NTL-HYP-01 ~ NTL-HYP-06 |
| `norm:NTL_range` | NTL-HYP-04 |
| `norm:cm-jt_op` | HZCMT-01 ~ HZCMT-03, HZCMT-06 ~ HZCMT-09 |
| `norm:cm-jalt_op` | HZCMT-01 ~ HZCMT-03, HZCMT-06 |
| `norm:jvt_base_vm` | HZCMT-07 ~ HZCMT-09 |
| `norm:Zcmt_fetch` | HZCMT-08, HZCMT-09 |
| `norm:Zcmt_trap` | HZCMT-08, HZCMT-09 |
| `norm:stateen0_jvt_op` | HZCMT-04 ~ HZCMT-06 |
| `norm:htval_trapval` | HZCMT-09 |
