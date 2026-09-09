**[中文](../testplan/Hypervisor_Zc_test_plan.md) | English**

# Hypervisor × Zc* Compressed Instruction Extensions Cross Test Plan

> This document describes the test plan for cross-scenarios between the Hypervisor (H) extension and the Zc* family of compressed instruction extensions (Zca, Zcb, Zcmp, Zcmt, Zcmop, Zcd, Zcf, etc.). The intersection of each Zc* extension with the Hypervisor occupies its own Group; currently Hypervisor × Zca (Group 1) and Hypervisor × Zcmt (Group 2, migrated in from `Hypervisor_Zi_test_plan.md`) are covered, and the remaining Zc* extensions will be added progressively.

---

## SPEC Sections Covered by This Document

This plan is based on the following official RISC-V specifications (local paths):

- `SPEC/riscv-isa-manual/src/unpriv/zc.adoc` — Zc* family of compressed instruction extensions overview: composition and dependencies of the Zca/Zcb/Zcd/Zcf/Zcmp/Zcmt/Zcmop sub-extensions
- `SPEC/riscv-isa-manual/src/unpriv/zca.adoc` — Zca extension: expansion rules of the compressed load/store instructions (`c.lw`/`c.sw`/`c.ld`/`c.sd`/`c.lwsp`/`c.swsp`/`c.ldsp`/`c.sdsp`) to their 32-bit equivalents, IALIGN=16 and the exclusion of instruction-address-misaligned exceptions, control transfer and integer computational instructions
- `SPEC/riscv-isa-manual/src/unpriv/zcmt.adoc` — Zcmt extension: `cm.jt`/`cm.jalt` table jump instructions, the `jvt` CSR and two-implicit-fetch semantics, translation and fault reporting of JVT entry fetches
- `SPEC/riscv-isa-manual/src/priv/smstateen.adoc` — Smstateen extension: gating of Zcmt `jvt` CSR access by the `stateen0.JVT` bit
- `SPEC/riscv-isa-manual/src/priv/hypervisor.adoc` — Hypervisor (H) extension: the `htinst` register and the transformed/pseudoinstruction write rules of the trap instruction CSR, the three-step transformation of compressed instructions (expand → transform → clear bit 1, bits[1:0]=01), transformed load/store formats and Addr. Offset, the tinst-values table of writable values per exception, the mandatory pseudoinstruction for implicit VS-stage walks, two-stage translation and guest-page faults
- `SPEC/riscv-isa-manual/src/priv/machine.adoc` — the `mtinst` register and M-mode trap instruction CSR write rules (sharing the same set of transformed/pseudoinstruction value constraints as `htinst`)

(As cross cases for the remaining Zc* extensions are added, this section will list the corresponding SPEC files, such as `zcb.adoc`, `zcmp.adoc`, `zcmop.adoc`, etc.)

Official repository:

- https://github.com/riscv/riscv-isa-manual (files at the above paths within the repository)

---

## Scope

### Covered Extension Intersections

This document targets the cross-scenarios between the Hypervisor and all Zc* compressed instruction extensions, with **each extension occupying its own Group**. Current coverage:

- **Group 1 — Hypervisor × Zca** (covered): the compressed transformed values of `htinst`/`mtinst` when all 8 Zca compressed load/store instructions (register-based `c.lw`/`c.sw`/`c.ld`/`c.sd` and stack-pointer-based `c.lwsp`/`c.swsp`/`c.ldsp`/`c.sdsp`) trigger load/store guest-page faults delivered to HS-mode or trapped into M-mode in VS/VU-mode (expand to 32-bit equivalent → standard transform → clear bit 1, bits[1:0]=01), the field-retention differences between the transformed load and store formats, Addr. Offset semantics, the manifestation of IALIGN=16 and instruction-address-misaligned exclusion in virtualized instruction fetch, that fetch-class exceptions do not write a transformed `htinst`, and normal execution and semantic consistency of compressed instructions in VS/VU-mode
- **Group 2 — Hypervisor × Zcmt** (covered, migrated in from `Hypervisor_Zi_test_plan.md`, case IDs HZCMT-01~09 unchanged): normal execution of table jump instructions (`cm.jt`/`cm.jalt`) in HS/VS/VU-mode and virtual-instruction exclusion, `jvt` CSR access and Smstateen (JVT bit) gating in VS/VU-mode, two-stage translation of the second JVT entry fetch in VS-mode and G-stage guest instruction page fault reporting
- **Group 3+ — Hypervisor × Zcb / Zcmp / Zcmop / Zcd / Zcf etc.** (to be added): the intersections of the remaining Zc* extensions with the Hypervisor will each occupy their own Group and be added progressively

### Out of Scope for This Document

- **Existence verification of the compressed-instruction `htinst` transformed mechanism** (proving the bits[1:0]=01 transformation rule takes effect with a single `c.lw` sample) — covered by TINST-08 in Group 4 of `Hypervisor_Exceptions_test_plan.md`; Group 1 of this document systematically traverses all compressed load/store variants (especially store and stack-pointer-based variants) from the Zca extension perspective to verify the correctness of each expansion, complementing rather than duplicating the mechanism-existence verification of TINST-08
- **Transformed `htinst` of non-compressed load/store (bits[1:0]=11), implicit VS-stage walk pseudoinstructions, illegal-instruction/ecall/interrupt writing zero** — covered by Group 4 (TINST-01~10) of `Hypervisor_Exceptions_test_plan.md`
- **Transformed atomic `htinst` format of atomic instructions (LR/SC/AMO/amocas/load-acquire/store-release)** — covered by `Hypervisor_Za_test_plan.md` (Zca contains no atomic instructions; compressed atomic instructions do not exist in RISC-V)
- **Two-stage translation of guest instruction fetches straddling page boundaries, and `htval`/`mtval2` and pseudoinstructions of implicit-walk fetch faults** — covered by `Hypervisor_2_stage_test_plan.md` (TS-STRD-02, TS-IMPL-06); Group 1.8/1.9 of this document only covers the Zca-specific IALIGN=16 enablement and instruction-address-misaligned exclusion, and the contrast/disambiguation that "fetch-class exceptions never write a transformed value" versus load/store classes
- **Compressed instructions allowed in constrained LR/SC loops** (`norm:constrained_lrsc_compressed_allowed`) — covered by `Zalrsc_test_plan.md` (ZLRSC-45) for the non-virtualized case and by Group 1 of `Hypervisor_Za_test_plan.md` for the virtualized case
- **Misaligned relaxation of compressed load/store within the misaligned atomicity granule (MAG)** — covered by `Zama16b_test_plan.md` (ZAMA16B-25/48) for the non-virtualized case; HZCA-20 of this document only verifies the Addr. Offset semantics of `htinst` when a misaligned compressed memory access traps into HS-mode, without duplicating MAG granularity determination
- **Zcmt non-Hypervisor scenarios** (`jvt` WARL behavior, encoding and operational semantics, PMP/page-table fault handling, table update visibility and endianness, etc.) — covered by `Zcmt_test_plan.md`; the Zcmt × Hypervisor intersection (formerly Group 3 of `Hypervisor_Zi_test_plan.md`, HZCMT-01~09) has been migrated entirely into Group 2 of this document
- **Non-Hypervisor scenarios of each Zc* extension** (compressed instruction encodings and reserved encodings, HINT space, all-zero illegal instruction, basic semantics of integer computational/control transfer instructions, basic executability at each privilege level, etc.) — covered by their respective standalone test plans

---

## Covered Specification Points

The following table lists the specification points covered by this plan. Entries with the `norm:` prefix are official SPEC labels; entries without the prefix are specification points decomposed from the SPEC text. As the remaining Zc* extensions are added, this section will add per-extension subsections.

### Zca Related (`zca.adoc`)

| Norm ID | Source | Description |
|---------|--------|-------------|
| `norm:Zca_align16` | `zca.adoc` | The Zca extension allows 16-bit instructions to be freely intermixed with 32-bit instructions, with the latter now able to start on any 16-bit boundary, i.e., IALIGN=16. This alignment relaxation also holds under virtualized instruction fetch. |
| `norm:Zca_no_misaligned` | `zca.adoc` | With the addition of the Zca extension, no instructions can raise instruction-address-misaligned exceptions (cause=0); thus jumping to a 2-byte-aligned address for execution in VS/VU-mode must not report cause=0. |
| `norm:c-lw_op` | `zca.adoc` | c.lw loads a 32-bit value from memory into register rd', computing the effective address by adding the zero-extended offset scaled by 4 to rs1'. It expands to lw rd',offset(rs1') (funct3=010); semantics are unchanged in VS/VU-mode. |
| `norm:c-sw_op` | `zca.adoc` | c.sw stores a 32-bit value in register rs2' to memory... It expands to sw rs2',offset(rs1') (funct3=010); semantics are unchanged in VS/VU-mode. |
| `norm:c-ld_op` | `zca.adoc` | c.ld is an XLEN=64-only instruction that loads a 64-bit value... It expands to ld rd',offset(rs1') (funct3=011). |
| `norm:c-sd_op` | `zca.adoc` | c.sd is an XLEN=64-only instruction that stores a 64-bit value... It expands to sd rs2',offset(rs1') (funct3=011). |
| `norm:c-lwsp_op` | `zca.adoc` | c.lwsp loads a 32-bit value... adding the zero-extended offset scaled by 4 to the stack pointer x2. It expands to lw rd,offset(x2), with base fixed to stack pointer x2; when transformed, the rs1 field (=x2) is replaced by Addr. Offset. |
| `norm:c-swsp_op` | `zca.adoc` | c.swsp stores a 32-bit value in register rs2... adding the zero-extended offset scaled by 4 to the stack pointer x2. It expands to sw rs2,offset(x2), with base fixed to x2. |
| `norm:c-ldsp_op` | `zca.adoc` | c.ldsp is an XLEN=64-only instruction that loads a 64-bit value... adding the zero-extended offset scaled by 8 to x2. It expands to ld rd,offset(x2) (funct3=011, base=x2). |
| `norm:c-sdsp_op` | `zca.adoc` | c.sdsp is an XLEN=64-only instruction that stores a 64-bit value... adding the zero-extended offset scaled by 8 to x2. It expands to sd rs2,offset(x2) (funct3=011, base=x2). |

### Hypervisor Trap Instruction Related (`hypervisor.adoc` / `machine.adoc`)

| Norm ID | Source | Description |
|---------|--------|-------------|
| `norm:htinst_sz_acc_op` | `hypervisor.adoc` | The htinst register is an HSXLEN-bit read/write register. When a trap is taken into HS-mode, htinst is written with a value that, if nonzero, provides information about the instruction that trapped, to assist HS-mode software emulation. |
| `norm:H_trap_xtinst` | `hypervisor.adoc` | On any trap into M-mode or HS-mode, one of these values is written automatically into the appropriate trap instruction CSR, mtinst or htinst: zero; a transformation of the trapping instruction; a custom value; or a special pseudoinstruction. `mtinst` and `htinst` share the same set of value rules (basis for the symmetric path in Group 1.6/1.7). |
| `norm:H_trap_xtinst_exception_lead-in` | `hypervisor.adoc` | On a synchronous exception, if a nonzero value is written, one of the following shall be true about the value; otherwise software may safely treat it as zero. |
| `norm:H_trap_xtinst_exception_list` | `hypervisor.adoc` | Bit 0 is 1, and replacing bit 1 with 1 makes the value into a valid encoding of a standard instruction... the register value is the transformation of the trapping instruction. A compressed transformed value with bits[1:0]=01 satisfies exactly this constraint (setting bit 1 to 1 yields 11, i.e., a 32-bit standard encoding). |
| `norm:H_trap_xtinst_val` | `hypervisor.adoc` | tinst-values shows the values that may be automatically written for each standard exception cause. For exceptions that prevent the fetching of an instruction, only zero or a pseudoinstruction value may be written. Load/store (including guest-page fault) allow transformed; fetch classes (instruction page/guest-page fault) have Transformed=No, only zero or pseudoinstruction (basis for the fetch contrast in Group 1.9). |
| `norm:H_trap_xtinst_interrupt` | `hypervisor.adoc` | On an interrupt, the value written to the trap instruction register is always zero (basis for the control case in Group 1.7). |
| `norm:H_trap_xtinst_guestpage` | `hypervisor.adoc` | For guest-page faults, the trap instruction register is written with a special pseudoinstruction value if the fault is caused by an implicit memory access for VS-stage address translation and a nonzero value is written to mtval2/htval; zero is not allowed. Used in this document as the boundary contrast between explicit compressed memory-access faults (may write zero/transformed) and implicit walks (mandatory pseudoinstruction). |
| `htinst_transformed_compressed` | `hypervisor.adoc` | For a standard compressed instruction (16-bit size), the transformed instruction is found as follows: (1) Expand the compressed instruction to its 32-bit equivalent; (2) Transform the 32-bit equivalent instruction; (3) Replace bit 1 with a 0. Bits 1:0 will be binary 01 if the trapping instruction is compressed and 11 if not. This is the core assertion of this plan and applies to all Zc* compressed memory-access instructions. |
| `htinst_transformed_load` | `hypervisor.adoc` | For a standard load instruction that is not a compressed instruction (LB..LD/FLW..FLQ), the transformed instruction keeps funct3, rd, and opcode the same as the trapping load instruction; the immediate offset is replaced with zero and bits 19:15 (rs1) with Addr. Offset. A compressed load is first expanded to this 32-bit form, then bit 1 is cleared. |
| `htinst_transformed_store` | `hypervisor.adoc` | For a standard store instruction that is not a compressed instruction (SB..SD/FSW..FSQ), the transformed instruction keeps rs2, funct3, and opcode the same as the trapping store instruction; both immediate halves are replaced with zero and bits 19:15 (rs1) with Addr. Offset. A compressed store is first expanded to this form, then bit 1 is cleared (the store branch is not covered by TINST-08). |
| `htinst_addr_offset` | `hypervisor.adoc` | The Addr. Offset field that replaces rs1 in bits 19:15 is the positive difference between the faulting virtual address (written to mtval/stval) and the original virtual address; this difference can be nonzero only for a misaligned memory access. For aligned compressed load/store it is always 0. |

### Zcmt Related (`zcmt.adoc` / `smstateen.adoc`)

| Norm ID | Source | Description |
|---------|--------|-------------|
| `norm:cm-jt_op` | `zcmt.adoc` | cm.jt reads an entry from the jump vector table in memory and jumps to the address that was read. |
| `norm:cm-jalt_op` | `zcmt.adoc` | cm.jalt reads an entry from the jump vector table in memory and jumps to the address that was read, linking to _ra_. |
| `norm:jvt_base_vm` | `zcmt.adoc` | jvt[base] is a virtual address, whenever virtual memory is enabled (including vsatp translation in VS-mode). |
| `norm:Zcmt_fetch` | `zcmt.adoc` | ... the execution of a table jump instruction involves two instruction fetches, the first to read the instruction (cm.jt/cm.jalt) and the second to read from the jump vector table (JVT). Both instruction fetches are _implicit_ reads, and both require execute permission; read permission is irrelevant. |
| `norm:Zcmt_trap` | `zcmt.adoc` | If an exception occurs on either instruction fetch, xEPC is set to the PC of the table jump instruction, xCAUSE is set as expected for the type of fault and xTVAL (if not set to zero) contains the fetch address which caused the fault. |
| `norm:stateen0_jvt_op` | `smstateen.adoc` | The JVT bit controls access to the `jvt` CSR provided by the Zcmt extension; it gates only jvt CSR access, not cm.jt/cm.jalt instruction execution. |
| `norm:htval_trapval` | `hypervisor.adoc` | htval trap value reporting for guest-page faults (implementation may write zero or the faulting GPA>>2); HZCMT-09 reports the table entry GPA>>2 on a second-fetch G-stage fault. |

---

## Group 1. Hypervisor × Zca Cross Tests

**Intersection points with the Hypervisor**:
1. **No virtual-instruction gating**: none of the virtual-instruction clauses in `hypervisor.adoc` (VTVM/VTW/VTSR, HLV/HSV/HLVX, CBO gating, controlled CSR access, etc.) covers Zca compressed instructions → guest compressed instructions never report cause=22, consistent with ordinary 32-bit integer instructions
2. **Virtualization does not change Zca semantics**: expansion rules such as `norm:c-lw_op` are unchanged in VS/VU-mode; the loaded value, store effect, and stack-pointer-based address computation of compressed load/store are bit-for-bit identical to HS-mode
3. **Three-step compressed transformation rule**: when a guest compressed load/store triggers a load/store guest-page fault delivered to HS-mode, a nonzero `htinst` must be the result of "expand to 32-bit equivalent → standard transform → clear bit 1", bits[1:0]=01 (`htinst_transformed_compressed`)
4. **Two transformed formats for load and store**: a compressed load expands to `htinst_transformed_load` (keeping funct3/rd/opcode), a compressed store expands to `htinst_transformed_store` (keeping rs2/funct3/opcode, clearing both immediate halves) — the store branch is a gap not covered by TINST-08 (only `c.lw`)
5. **Expansion of stack-pointer-based variants**: `c.lwsp`/`c.swsp`/`c.ldsp`/`c.sdsp` expand with base=x2; when transformed, bits19:15 (the original rs1=x2 field) is replaced by Addr. Offset, while funct3/rd(rs2)/opcode must still match the expanded 32-bit instruction
6. **`htinst` may be zero**: under explicit access faults the implementation may write zero (reducing hardware effort); when nonzero it must exactly match the golden value
7. **mtinst and htinst share rules**: `norm:H_trap_xtinst` states "the appropriate trap instruction CSR, mtinst or htinst" share the same transformed/pseudoinstruction value constraints; when a guest compressed load/store exception is not delegated and traps into M-mode, `mtinst` is written with the same compressed transformed value as `htinst`
8. **Delegation path contrast**: for the same compressed memory-access fault, delegating to HS-mode reads `htinst` while not delegating and trapping into M-mode reads `mtinst`; the two transformed values must be identical
9. **IALIGN=16 holds under guest instruction fetch**: `norm:Zca_align16` allows 32-bit instructions to start on any 16-bit boundary; this alignment relaxation also applies when VS/VU-mode instruction fetch undergoes VS-stage + G-stage two-stage translation
10. **instruction-address-misaligned exclusion**: `norm:Zca_no_misaligned` specifies that with Zca no instruction raises instruction-address-misaligned (cause=0); thus a guest jumping to a 2-byte-aligned address to execute a compressed instruction, or to a 2-byte-aligned 32-bit instruction target, must not report cause=0
11. **Fetch-class exceptions do not write transformed htinst**: `norm:H_trap_xtinst_val` (tinst-values table) specifies Transformed=No for instruction guest-page fault (cause=20), only zero or pseudoinstruction — forming a key contrast with compressed load/store writing transformed values

**Spec Reference**:
- `norm:c-lw_op` / `norm:c-sw_op` / `norm:c-ld_op` / `norm:c-sd_op` / `norm:c-lwsp_op` / `norm:c-swsp_op` / `norm:c-ldsp_op` / `norm:c-sdsp_op`: the expansion mapping of each compressed load/store to its 32-bit equivalent, semantics unchanged in VS/VU-mode
- `norm:Zca_align16` / `norm:Zca_no_misaligned`: IALIGN=16 allows free mixing of 16/32-bit instructions; no instruction raises instruction-address-misaligned
- `norm:H_trap_xtinst` / `norm:H_trap_xtinst_exception_lead-in` / `norm:H_trap_xtinst_exception_list`: trap instruction write value types and the legality constraint of the compressed transformed value bits[1:0]=01 (shared by mtinst/htinst)
- `norm:H_trap_xtinst_val`: load/store guest-page fault allows writing a transformed value; fetch classes only zero or pseudoinstruction
- `norm:H_trap_xtinst_interrupt`: on an interrupt the trap instruction register always writes zero
- `htinst_transformed_compressed` / `htinst_transformed_load` / `htinst_transformed_store` / `htinst_addr_offset`: the three-step compressed transformation, the two load/store expansion formats, and Addr. Offset semantics
- `norm:H_trap_xtinst_guestpage`: implicit VS-stage walks mandate a pseudoinstruction, as the boundary contrast to explicit access faults (may write zero/transformed)

**Test Scope**: Verify the cross behavior of Zca under V=1, two-stage translation, and the HS-mode and M-mode perspectives: normal execution of compressed instructions in VS/VU-mode and cause=22 exclusion, field-by-field correctness of the `htinst`/`mtinst` transformed values when all 8 compressed load/store variants trap, IALIGN=16 and instruction-address-misaligned exclusion, and that fetch-class exceptions do not write a transformed value. Compressed instructions are injected after explicitly enabling `.option rvc` (or injected with raw `.half` encoding of the fixed 16-bit encoding); the trap handler's xEPC advance must adapt to the 2-byte length of compressed instructions; the golden value is computed by the framework's `hyp_transform_mem_inst()` on the "expanded 32-bit equivalent encoding" then clearing bit 1. Division from TINST-08: TINST-08 verifies the existence of the compressed transformation mechanism with a single `c.lw`, while this Group traverses all Zca compressed load/store variants (including store and stack-pointer-based variants) to verify the correctness of each expansion mapping.

### 1.1 HS/VS/VU-mode Compressed Instruction Executability and cause=22 Exclusion

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| HZCA-01 | HS/VS/VU-mode executes compressed computational and jump instructions | The three privilege levels each execute compressed instructions such as `c.addi`/`c.add`/`c.mv`/`c.j`/`c.beqz` (target mappings valid at both stages) | All execute normally, no exception; VS/VU-mode **never** triggers virtual-instruction (cause=22) |
| HZCA-02 | VS-mode compressed load/store semantics identical to non-virtualized | The same `c.lw`/`c.sw` sequence executes in HS-mode and VS-mode on the same data, comparing loaded value and final memory value | Architecturally visible semantics completely identical (virtualization does not change Zca semantics, `norm:c-lw_op`/`norm:c-sw_op`) |
| HZCA-03 | VU-mode compressed load/store executes normally | VU-mode executes `c.lw`/`c.sw` (VS-stage/G-stage both valid mappings) | Executes normally, no exception, never reports cause=22 |
| HZCA-04 | VS-mode stack-pointer-based compressed memory access normal | VS-mode executes `c.addi4spn`/`c.lwsp`/`c.swsp`/`c.addi16sp` (base=x2) | Executes normally, stack-pointer-based address computation correct, no exception |
| HZCA-05 | RV64 doubleword compressed load/store normal | On an RV64 platform, VS-mode executes `c.ld`/`c.sd`/`c.ldsp`/`c.sdsp` | Executes normally, 64-bit load/store semantics correct (`norm:c-ld_op`/`norm:c-sd_op`/`norm:c-ldsp_op`/`norm:c-sdsp_op`) |
| HZCA-06 | VS-mode mixed compressed and 32-bit instruction sequence executes normally | VS-mode executes a sequence freely mixing 16-bit compressed and 32-bit instructions (two-stage translation both valid), comparing the result with non-virtualized | Executes normally, result correct (`norm:Zca_align16`: IALIGN=16 allows free mixing of 16/32-bit instructions); serves as the positive baseline control for the fault injection in Group 1.2~1.5 |

### 1.2 htinst transformed of Register-Based Compressed load/store (`c.lw`/`c.sw`/`c.ld`/`c.sd`)

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| HZCA-07 | htinst of c.lw triggering guest-page-fault | Construct an invalid G-stage target GPA, VS-mode executes a single `c.lw` (e.g., `c.lw a0,0(a0)`) triggering a load guest-page fault (cause=21) | Delivered to HS-mode; `htinst`=0 or the compressed transformed value (expanded to `lw`, bits[1:0]=01, funct3=010/rd/opcode retained, imm cleared, bits19:15←Addr. Offset) |
| HZCA-08 | htinst of c.sw triggering guest-page-fault (store branch) | G-stage target GPA has no write permission, VS-mode executes a single `c.sw` triggering a store/AMO guest-page fault (cause=23) | `htinst`=0 or the compressed transformed **store** value (expanded to `sw`, retaining rs2/funct3=010/opcode, both immediate halves cleared, bits[1:0]=01) — covers the store branch not included in TINST-08 |
| HZCA-09 | htinst of c.ld triggering guest-page-fault (RV64) | On an RV64 platform, invalid G-stage target GPA, VS-mode executes `c.ld` triggering cause=21 | `htinst`=0 or the compressed transformed value (expanded to `ld`, funct3=011, bits[1:0]=01) |
| HZCA-10 | htinst of c.sd triggering guest-page-fault (RV64 store) | On an RV64 platform, G-stage target GPA has no write permission, VS-mode executes `c.sd` triggering cause=23 | `htinst`=0 or the compressed transformed store value (expanded to `sd`, funct3=011, retaining rs2/opcode, bits[1:0]=01) |

### 1.3 htinst transformed of Stack-Pointer-Based Compressed load/store (`c.lwsp`/`c.swsp`/`c.ldsp`/`c.sdsp`)

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| HZCA-11 | htinst of c.lwsp triggering guest-page-fault | VS-mode sets x2 to an invalid G-stage GPA, executes `c.lwsp rd,off(x2)` triggering cause=21 | `htinst`=0 or the compressed transformed value (expanded to `lw rd,off(x2)`, funct3=010/rd/opcode retained; bits19:15 originally the x2 encoding, replaced by Addr. Offset) |
| HZCA-12 | htinst of c.swsp triggering guest-page-fault | VS-mode sets x2 to a G-stage GPA with no write permission, executes `c.swsp rs2,off(x2)` triggering cause=23 | `htinst`=0 or the compressed transformed store value (expanded to `sw rs2,off(x2)`, retaining rs2/funct3=010/opcode) |
| HZCA-13 | htinst of c.ldsp triggering guest-page-fault (RV64) | On an RV64 platform, VS-mode executes `c.ldsp` triggering cause=21 | `htinst`=0 or the compressed transformed value (expanded to `ld rd,off(x2)`, funct3=011) |
| HZCA-14 | htinst of c.sdsp triggering guest-page-fault (RV64) | On an RV64 platform, VS-mode executes `c.sdsp` triggering cause=23 | `htinst`=0 or the compressed transformed store value (expanded to `sd rs2,off(x2)`, funct3=011) |

### 1.4 Transformed load/store Format Distinction and Field Retention

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| HZCA-15 | load vs store transformed format distinction | Take the nonzero `htinst` values for `c.lw` (cause=21) and `c.sw` (cause=23) respectively, comparing field by field | load retains funct3/rd/opcode (bits14:0), store retains rs2/funct3/opcode (`htinst_transformed_load` vs `htinst_transformed_store`); both have bits[1:0]=01 |
| HZCA-16 | Compressed transformed fields match the expanded 32-bit instruction | For each compressed load/store, set bit 1 of the (nonzero) `htinst` back to 1 to restore the 32-bit encoding, comparing funct3/rd(rs2)/opcode with the 32-bit equivalent expansion of the trapping instruction | Field-by-field identical, imm already cleared (`norm:H_trap_xtinst_exception_list`: setting bit 1 to 1 yields a valid standard instruction encoding) |
| HZCA-17 | bits[1:0]=01 compressed marker vs non-compressed contrast | Trigger cause=21 with compressed `c.lw` and non-compressed `lw` respectively for the same load semantics, comparing bits[1:0] of the two `htinst` values | Compressed source bits[1:0]=01, non-compressed source bits[1:0]=11 (`htinst_transformed_compressed`; the non-compressed side contrasts with TINST-07) |

### 1.5 Addr. Offset and htinst May Be Zero

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| HZCA-18 | Addr. Offset of aligned compressed load/store is always 0 | Following HZCA-07~14 (naturally aligned targets), check bits19:15 of the nonzero `htinst` value | Addr. Offset=0 (for aligned access the faulting VA equals the original VA, `htinst_addr_offset`) |
| HZCA-19 | htinst may be zero (explicit access fault) | For any explicit guest-page fault of a compressed load/store, accept `htinst`=0 | `htinst`=0 is legal (the implementation may reduce effort, `norm:H_trap_xtinst`); when nonzero it must exactly match the golden value, not accepting an arbitrary nonzero value |
| HZCA-20 | (Recording type) Addr. Offset of misaligned compressed memory access | If the platform supports misaligned split access, construct a misaligned `c.lw`/`c.sw` triggering a guest-page fault so that faulting VA≠original VA, read `htinst` bits19:15 | Addr. Offset may be nonzero (=faulting VA − original VA); whether splitting occurs within the MAG granularity is determined by `Zama16b_test_plan.md`; this case only records the Addr. Offset semantics of `htinst` without making a MAG assertion |

### 1.6 mtinst Compressed Transformed on M-mode Trap

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| HZCA-21 | mtinst of guest compressed load trapping into M-mode | The relevant guest-page fault is not delegated (traps into M-mode), VS-mode executes `c.lw` triggering cause=21 | `mtinst`=0 or the compressed transformed value (expanded to `lw`, bits[1:0]=01), same rules as `htinst` |
| HZCA-22 | mtinst of guest compressed store trapping into M-mode | Same as above, VS-mode executes `c.sw` triggering cause=23 | `mtinst`=0 or the compressed transformed store value (retaining rs2/funct3/opcode, bits[1:0]=01) |
| HZCA-23 | mtinst and htinst transformed values are consistent | For the same compressed memory-access fault, take the trap instruction value under two configurations (delegated to HS-mode, and trapped into M-mode) and compare | The two are bit-for-bit identical when nonzero (`norm:H_trap_xtinst` unifies mtinst/htinst rules) |

### 1.7 Control: Interrupt Writes Zero

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| HZCA-24 | mtinst=0 on M-mode interrupt | Inject an interrupt that traps into M-mode (V=1 context), read `mtinst` | `mtinst`=0 (strict, `norm:H_trap_xtinst_interrupt`) — forming a contrast with the synchronous-exception transformed values of HZCA-21/22 |

### 1.8 IALIGN=16 and instruction-address-misaligned Exclusion

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| HZCA-25 | VS-mode jumps to a 2-byte-aligned address to execute compressed instructions | VS-mode jumps via `c.j`/`c.jr` to a compressed instruction sequence at a 2-byte-aligned (not 4-byte-aligned) address | Executes normally, **must not** report instruction-address-misaligned (cause=0) (`norm:Zca_no_misaligned`) |
| HZCA-26 | VU-mode jumps to a 2-byte-aligned address | Same configuration, VU-mode jumps to a 2-byte-aligned address to execute | Executes normally, does not report cause=0, does not report virtual-instruction (cause=22) |
| HZCA-27 | IALIGN=16 lets a 32-bit instruction start on a 16-bit boundary | VS-mode executes a 32-bit instruction starting on an odd 16-bit boundary (2-byte-aligned, not 4-byte-aligned), two-stage translation valid | Fetches and executes normally, does not report instruction-address-misaligned (`norm:Zca_align16`: 32-bit instructions may start on any 16-bit boundary) |

### 1.9 Fetch-Class Exceptions Do Not Write Transformed htinst

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| HZCA-28 | htinst of guest compressed fetch instruction guest-page-fault | Construct a G-stage target instruction page with no X permission, VS-mode jumps to that page to execute a compressed instruction triggering an instruction guest-page fault (cause=20) | Delivered to HS-mode; `htinst`=0 or a pseudoinstruction, **never** a transformed value (tinst-values table: Transformed=No for Instruction guest-page fault, `norm:H_trap_xtinst_val`) |
| HZCA-29 | Contrast/disambiguation of fetch-class vs memory-access-class htinst | For the same guest, construct a compressed **fetch** fault (cause=20) and a compressed **load** fault (cause=21) respectively, comparing the writable value space of `htinst` | Fetch-class `htinst` contains no transformed value (only 0/pseudoinstruction); memory-access-class `htinst` may be a compressed transformed value (bits[1:0]=01) — establishing the write rule distinguished by exception class |

> [!NOTE]
> - All tests in this Group must detect the H extension at runtime via `HAS_H_EXT()`; Zca support follows the compressed-instruction support declared by the platform configuration, with trap-armed probing confirming instruction executability; if not satisfied, the whole group TEST_SKIP. RV64-only instructions (`c.ld`/`c.sd`/`c.ldsp`/`c.sdsp`) are conditionally excluded on RV32 platforms.
> - **cause=22 is a mandatory negative assertion**: if a guest compressed instruction in HZCA-01/03 reports virtual-instruction, it violates the SPEC (no clause in `hypervisor.adoc` imposes virtual-instruction gating on Zca instructions); it must remain FAIL and be recorded in the `bugs/` directory, without relaxing the assertion.
> - **Golden value computation**: the framework's `hyp_transform_mem_inst()` already implements the load (opcode 0x03: retain bits14:0, clear imm, bits19:15←Addr. Offset) and store (opcode 0x23: retain rs2/funct3/opcode, clear both immediate halves) branches; a compressed instruction must first be expanded to its 32-bit equivalent encoding per `norm:c-lw_op` etc. and passed in, then bit 1 is cleared (`& ~2`) on the result to obtain the bits[1:0]=01 golden value. Stack-pointer-based variants expand with rs1=x2, whose bits19:15 is likewise overwritten by Addr. Offset.
> - **Zero or exact match**: under explicit access faults `htinst`/`mtinst`=0 is legal; when nonzero it must be bit-for-bit equal to the golden value, and must not pass with "an arbitrary nonzero value" (strict verification principle, same as the TINST Group).
> - **Boundary with implicit walks**: HZCA-07~20 are all explicit faults where the compressed memory-access instruction's **own data access** fails at G-stage (`htinst` may be 0/transformed); if the fault originates from an **implicit walk** of the VS-stage page table, then `norm:H_trap_xtinst_guestpage` applies (mandatory pseudoinstruction when htval is nonzero, zero not allowed), a scenario covered by TINST-05/06 of `Hypervisor_Exceptions_test_plan.md` and not duplicated in this Group.
> - **Compressed instruction injection and sepc advance**: each fault case must ensure the trapping instruction is exactly a single 16-bit compressed instruction, with the injection region wrapped in `.option rvc` or using raw `.half` encoding; the trap handler determines it is compressed from the low two bits of the instruction at xEPC and advances by +2, avoiding a mis-jump. The normal-execution cases in 1.1 do not trigger traps; length adaptation mainly serves the fault injection in 1.2~1.9.
> - **mtinst symmetric path (1.6/1.7)**: requires the delegation-configuration capability to route guest-page faults to M-mode (`medeleg`/`hedeleg` corresponding bits cleared); the writable/readable implementation range of `mtinst` may be narrower than `htinst` (the `norm:H_trap_xtinst` NOTE allows a trap instruction register to minimally support only 0 and pseudoinstructions), and `mtinst`=0 is always legal. The consistency assertion of HZCA-23 performs a bit-for-bit comparison only when both runs write nonzero transformed values; either side writing zero is a legal implementation, recorded without judging failure.
> - **cause=0 is a mandatory negative assertion**: if guest instruction fetch in HZCA-25~27 reports instruction-address-misaligned (cause=0), it violates `norm:Zca_no_misaligned` and must remain FAIL and be recorded in the `bugs/` directory, without relaxation.
> - **Division from `Hypervisor_2_stage_test_plan.md`**: the `htval`/`mtval2` faulting-portion reporting of cross-page fetches (a 32-bit instruction straddling a page boundary) (TS-STRD-02) and the pseudoinstruction of implicit-walk fetch faults (TS-IMPL-06) are covered by that plan; HZCA-28 of this Group only verifies the contrast/disambiguation that "fetch-class exceptions never write a transformed htinst" versus memory-access classes, and HZCA-25~27 only verify the Zca-specific IALIGN=16 and misaligned exclusion, neither duplicating the cross-page/implicit-walk assertions of 2_stage.
> - HZCA-20 is a recording-type case: whether misaligned splitting is triggered depends on platform MAG and Zicclsm support; both a nonzero and an always-zero Addr. Offset are legal observations, recorded without a mandatory verdict; MAG granularity assertions belong to `Zama16b_test_plan.md`.

---

## Group 2. Hypervisor × Zcmt Cross Tests

**Intersection points with the Hypervisor**:
1. **Table jump instructions have no privilege-level gating**: `cm.jt`/`cm.jalt` are ordinary instructions; no virtual-instruction clause in `hypervisor.adoc` covers them → they execute normally in HS/VS/VU-mode and never report cause=22
2. **jvt.base virtual address and two-stage translation**: `norm:jvt_base_vm` specifies jvt.base is a virtual address when virtual memory is enabled; in VS-mode the JVT table resides in the guest virtual address space, undergoing vsatp (VS-stage) + hgatp (G-stage) two-stage translation
3. **Two implicit fetches**: `norm:Zcmt_fetch` specifies a table jump involves two fetches (the instruction itself + the JVT entry); the second fetch also undergoes two-stage translation, and its G-stage fault is reported as a guest instruction page fault (cause=20)
4. **stateen gates the jvt CSR but not the instruction**: `norm:stateen0_jvt_op` specifies hstateen0.JVT gates VS/VU access to the jvt CSR, but cm.jt/cm.jalt instruction execution is not gated by stateen
5. **trap context points to the table jump instruction**: `norm:Zcmt_trap` specifies that on either fetch fault xEPC points to the table jump instruction itself (not the JVT table address), and xTVAL/htval report the faulting fetch address; `norm:htval_trapval` specifies htval writes the faulting GPA>>2 (or zero) on a G-stage fault

**Spec Reference**:
- `norm:cm-jt_op` / `norm:cm-jalt_op`: table jumps are ordinary instructions with no privilege-level restrictions; they should execute normally in HS/VS/VU-mode
- `norm:jvt_base_vm`: when virtual memory is enabled, jvt.base is a virtual address; in VS-mode it undergoes two-stage translation via vsatp
- `norm:Zcmt_fetch` / `norm:Zcmt_trap`: the second fetch (JVT entry) is also translated; on a fault, xEPC points to the table jump instruction and xTVAL is the faulting fetch address
- `norm:stateen0_jvt_op`: the JVT bit of stateen0 controls jvt CSR access; it gates only CSR access, not instruction execution
- `norm:htval_trapval`: htval trap value reporting rules on G-stage faults

**Test Scope**: Verify the behavior of table jump instructions and the jvt CSR in virtualization environments: normal execution in HS/VS/VU-mode without spuriously raising a virtual-instruction exception; in VS-mode, JVT entry fetches undergo two-stage translation and G-stage faults are reported as guest instruction page fault; hstateen0.JVT gates VS/VU jvt access but not instruction execution. The cases in this Group are migrated entirely from Group 3 of `Hypervisor_Zi_test_plan.md` (case IDs HZCMT-01~09 unchanged), originally sourced from `Zcmt_test_plan.md` (the virtualization portion of the original ZCMT-27/28, ZACC-03/04/05/06; HZCMT-08 is a VS-stage fault case supplemented as a counterpart to ZCMT-25).

### 2.1 Table Jump Execution in HS/VS/VU-mode

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| HZCMT-01 | HS-mode executes table jumps | HS-mode executes cm.jt and cm.jalt (jvt points to a valid table) | both jump and link normally, no exception |
| HZCMT-02 | VS-mode executes table jumps | VS-mode executes cm.jt and cm.jalt | both execute normally, no virtual-instruction exception |
| HZCMT-03 | VU-mode executes table jumps | VU-mode executes cm.jt and cm.jalt | both execute normally, no exception |

### 2.2 jvt Access and stateen Gating in VS/VU-mode

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| HZCMT-04 | VS/VU-mode accesses jvt | With stateen enabled, VS/VU-mode csrr/csrw jvt | access succeeds (jvt permission URW + stateen enabled) |
| HZCMT-05 | hstateen0.JVT gates VS/VU access | When Smstateen is implemented, zero out the JVT bits of hstateen0/sstateen0 per hierarchy, VS/VU accesses jvt | VS/VU raises virtual-instruction/illegal-instruction (see `Smstateen_test_plan.md` / `Ssstateen_test_plan.md` for detailed cases) |
| HZCMT-06 | stateen does not gate table jump instruction execution | When Smstateen is implemented, zero out the JVT bits of stateen at all levels, VS-mode executes cm.jt/cm.jalt | instructions execute normally (state enable gates only jvt CSR access, not the instruction itself) |

### 2.3 Table Jumps under Two-Stage Translation in VS-mode

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| HZCMT-07 | VS-mode translation path jumps normally | VS-mode with vsatp enabled, VS-stage maps the table page with X=1, execute cm.jt | jump succeeds (VS-stage translation effective) |
| HZCMT-08 | VS-mode table page with VS-stage X=0 raises a fault | The table page is mapped in VS-stage but X=0, VS-mode executes cm.jt | instruction page fault delivered per the delegation path, sepc=cm.jt PC, stval=table entry virtual address |
| HZCMT-09 | VS-mode second fetch G-stage fault | The G-stage mapping of the table page is invalid, VS-mode executes cm.jt | guest instruction page fault (cause=20) delivered to HS-mode, hstatus.GVA=1, htval=faulting table entry GPA>>2 (norm:htval_trapval allows zero) |

> [!NOTE]
> - All tests in this Group must detect the availability of the H extension at runtime via `HAS_H_EXT()`; if unavailable, TEST_SKIP. Zcmt support follows the platform configuration macro `ZCMT_SUPPORTED`; the jvt writability probe result (read-only implementation, allowed by `norm:jvt_op`) determines whether functional cases are applicable.
> - HZCMT-05 additionally needs to probe Smstateen; HZCMT-07~09 need to enable vsatp (and hgatp) to construct two-stage translation, using a mapping combination of VS-stage valid + G-stage invalid to isolate G-stage faults.
> - HZCMT-08/09 verify the fault path of the second fetch (JVT entry): vsepc must point to the cm.jt instruction itself rather than the table address (`norm:Zcmt_trap`), and vstval/htval report the table entry fetch address.

---

## Key Considerations

> Among the following considerations, items 1~2 are common to the whole document; items 3~8 mainly target the compressed memory-access trap instruction transformation of Group 1 (Zca), while the specific points of Group 2 (Zcmt) are in its Group NOTE; other Zc* extensions add their own within their Group when included.

1. **Document organization**: This document targets the intersection of all Zc* compressed instruction extensions with the Hypervisor, with **each extension occupying its own Group** (Group 1=Zca, Group 2=Zcmt, Group 3+ progressively adding Zcb/Zcmp/Zcmop/Zcd/Zcf etc.). Each extension's case IDs use an independent prefix (Zca is `HZCA-`, Zcmt is `HZCMT-`); when adding a new extension, existing IDs must not be reused or renumbered.

2. **Extension detection**: All tests must detect the H extension at runtime via `HAS_H_EXT()`, and confirm the corresponding Zc* extension instructions are executable via platform configuration declaration and trap-armed probing; if not satisfied, the corresponding group/cases TEST_SKIP. RV64-only instructions (`c.ld`/`c.sd`/`c.ldsp`/`c.sdsp`) are conditionally excluded on RV32 platforms. Zcmt support follows the platform configuration macro `ZCMT_SUPPORTED` and trap-armed probing of the `jvt` CSR (0x017); HZCMT-05 additionally needs to probe Smstateen.

3. **Zca has no intersection with the A extension**: Zca is an integer compressed instruction extension containing no atomic instructions (RISC-V has no compressed forms of LR/SC/AMO); the transformed atomic `htinst` format of atomic instructions is covered by `Hypervisor_Za_test_plan.md`, and this document does not involve the opcode 0x2F transformed branch.

4. **The three-step compressed transformation rule is the core assertion**: when `htinst`/`mtinst` is nonzero it must equal the golden value of "expand to 32-bit equivalent → standard load/store transform → clear bit 1", with bits[1:0]=01 marking a compressed source (`htinst_transformed_compressed`); load and store use different formats (funct3/rd/opcode vs rs2/funct3/opcode), and stack-pointer-based variants expand with base=x2. This rule is common to all Zc* compressed memory-access instructions and is the shared basis for subsequent extension Groups.

5. **Zero or exact match principle**: under explicit access faults the trap instruction register may write zero (the implementation may reduce effort), but a nonzero value must bit-for-bit exactly match the golden value, not accepting an arbitrary nonzero value; on an implicit VS-stage walk with nonzero htval a pseudoinstruction is mandatory and zero is not allowed (that scenario belongs to TINST-05/06, and this document only serves as a boundary contrast).

6. **Write difference between fetch-class and memory-access-class**: the `htinst` of an instruction guest-page fault (cause=20) never writes a transformed value (only 0/pseudoinstruction), while a load/store guest-page fault (cause=21/23) may write a compressed transformed value; assertions must distinguish by exception class and must not confuse them (`norm:H_trap_xtinst_val`).

7. **Compressed instruction injection and sepc advance**: a fault case must ensure the trapping instruction is exactly a single 16-bit compressed instruction, with the injection region wrapped in `.option rvc` or using raw `.half` fixed encoding; the trap handler determines the length from the low two bits of the instruction at xEPC, advancing xEPC by +2 for compressed instructions to avoid a mis-jump that would distort subsequent assertions.

8. **Compliance handling principle**: if any platform violates the above mandatory SPEC assertions (spurious cause=22, spurious cause=0, `htinst`/`mtinst` nonzero value deviating from the golden value, or a fetch class erroneously writing a transformed value), the case remains FAIL and is recorded in the `bugs/` directory, and must not be skipped or worked around to pass the test; recording-type cases (HZCA-20 misaligned Addr. Offset, HZCA-23 consistency observation) only record the implementation behavior, and legal results are not judged as failures.

---

## References

- `zc.adoc` — RISC-V Compressed Instructions (Zc* family overview: Zca/Zcb/Zcd/Zcf/Zcmp/Zcmt/Zcmop composition and dependencies)
- `zca.adoc` — Zca Extension for Integer Compressed Instructions (compressed load/store expansion, IALIGN=16, instruction-address-misaligned exclusion)
- `zcmt.adoc` — Zcmt Extension for Compressed Table Jumps (cm.jt/cm.jalt table jumps, jvt CSR, two implicit fetches)
- `smstateen.adoc` — Smstateen Extension (gating of jvt CSR access by the stateen0.JVT bit)
- `hypervisor.adoc` — RISC-V Hypervisor Extension (`htinst`/`mtinst` transformed and pseudoinstruction rules, two-stage translation and guest-page faults)
- `machine.adoc` — Machine-Level ISA (`mtinst` register and trap instruction write rules)
- `DOCS/testplan/Hypervisor_Exceptions_test_plan.md` — Hypervisor exceptions and trap test plan (Group 4 TINST-01~10: htinst/mtinst transformed instruction mechanism, including TINST-08 compressed `c.lw` mechanism-existence verification)
- `DOCS/testplan/Hypervisor_Za_test_plan.md` — Hypervisor × Za atomic extension cross test plan (transformed atomic format of atomic instructions)
- `DOCS/testplan/Hypervisor_2_stage_test_plan.md` — Two-stage translation test plan (TS-STRD-02 cross-page fetch, TS-IMPL-06 implicit-walk fetch pseudoinstruction)
- `DOCS/testplan/Hypervisor_Zi_test_plan.md` — Hypervisor × Z* extension cross test plan (the Zcmt intersection has been migrated out to Group 2 of this document)
- `DOCS/testplan/Zcmt_test_plan.md` — Zcmt standalone test plan (non-Hypervisor scenarios)
- `DOCS/testplan/Smstateen_test_plan.md` / `DOCS/testplan/Ssstateen_test_plan.md` — Smstateen/Ssstateen test plans (stateen0.JVT gating matrix)
- `DOCS/testplan/Zalrsc_test_plan.md` — Zalrsc test plan (ZLRSC-45 compressed instructions allowed in constrained loops, non-virtualized)
- `DOCS/testplan/Zama16b_test_plan.md` — Zama16b test plan (ZAMA16B-25/48 compressed encodings within the MAG for misaligned relaxation, non-virtualized)

---

## Appendix A: Specification Point Coverage Matrix

The following table indicates which test cases cover each specification point in the "Covered Specification Points" section. It currently contains the specification points of Group 1 (Zca) and Group 2 (Zcmt); it will be expanded as the remaining Zc* extensions are added.

| Norm ID | Covered Test IDs |
|---------|------------------|
| `norm:Zca_align16` | HZCA-06, HZCA-27 |
| `norm:Zca_no_misaligned` | HZCA-25, HZCA-26, HZCA-27 |
| `norm:c-lw_op` | HZCA-02, HZCA-07, HZCA-15, HZCA-16, HZCA-17, HZCA-21 |
| `norm:c-sw_op` | HZCA-02, HZCA-08, HZCA-15, HZCA-16, HZCA-22 |
| `norm:c-ld_op` | HZCA-05, HZCA-09 |
| `norm:c-sd_op` | HZCA-05, HZCA-10 |
| `norm:c-lwsp_op` | HZCA-04, HZCA-11, HZCA-16 |
| `norm:c-swsp_op` | HZCA-04, HZCA-12, HZCA-16 |
| `norm:c-ldsp_op` | HZCA-05, HZCA-13 |
| `norm:c-sdsp_op` | HZCA-05, HZCA-14 |
| `norm:htinst_sz_acc_op` | HZCA-07 ~ HZCA-19 |
| `norm:H_trap_xtinst` | HZCA-07 ~ HZCA-19, HZCA-21 ~ HZCA-23 |
| `norm:H_trap_xtinst_exception_lead-in` | HZCA-07 ~ HZCA-17, HZCA-21 ~ HZCA-22 |
| `norm:H_trap_xtinst_exception_list` | HZCA-16, HZCA-17 |
| `norm:H_trap_xtinst_val` | HZCA-07 ~ HZCA-10, HZCA-28, HZCA-29 |
| `norm:H_trap_xtinst_interrupt` | HZCA-24 |
| `norm:H_trap_xtinst_guestpage` | HZCA-19 (boundary contrast: explicit faults may write zero, distinguished from the mandatory pseudoinstruction of implicit walks) |
| `htinst_transformed_compressed` | HZCA-07 ~ HZCA-17, HZCA-21 ~ HZCA-23, HZCA-29 |
| `htinst_transformed_load` | HZCA-07, HZCA-09, HZCA-11, HZCA-13, HZCA-15, HZCA-16 |
| `htinst_transformed_store` | HZCA-08, HZCA-10, HZCA-12, HZCA-14, HZCA-15, HZCA-16, HZCA-22 |
| `htinst_addr_offset` | HZCA-18, HZCA-20 |
| `norm:cm-jt_op` | HZCMT-01 ~ HZCMT-03, HZCMT-06 ~ HZCMT-09 |
| `norm:cm-jalt_op` | HZCMT-01 ~ HZCMT-03, HZCMT-06 |
| `norm:jvt_base_vm` | HZCMT-07 ~ HZCMT-09 |
| `norm:Zcmt_fetch` | HZCMT-08, HZCMT-09 |
| `norm:Zcmt_trap` | HZCMT-08, HZCMT-09 |
| `norm:stateen0_jvt_op` | HZCMT-04 ~ HZCMT-06 |
| `norm:htval_trapval` | HZCMT-09 |
