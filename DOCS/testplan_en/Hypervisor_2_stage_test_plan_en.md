**[中文](../testplan/Hypervisor_2_stage_test_plan.md) | English**

# RISC-V Two-Stage Address Translation Test Plan

This document defines the test plan for **two-stage address translation** (VS-stage + G-stage) in the RISC-V Hypervisor extension. It covers the joint behavior of VS-stage (controlled by `vsatp`) and G-stage (controlled by `hgatp`) when V=1: the full VA → GPA → SPA chain, G-stage faults caused by implicit accesses, permission intersection, TLB fences, HLV/HLVX/HSV instructions, and two-stage accesses triggered by `mstatus.MPRV+MPV` in M-mode.

> **Scope**: This plan covers the following five categories of scenarios:
> 1. V=1 and hgatp=Bare: VS-stage only (verify that vsatp behaves the same as satp)
> 2. V=1 and vsatp=Bare (GVA=GPA): G-stage only (thoroughly covered by the companion document `Hypervisor_gstage_test_plan.md`; this document only provides a cross reference)
> 3. V=1 with both stages enabled: full VS-stage + G-stage chain
> 4. HLV/HLVX/HSV: two-stage explicitly triggered in HS-mode (or U-mode + HU=1)
> 5. `mstatus.MPRV=1 + MPV=1`: two-stage explicitly triggered in M-mode
>
> The current repository is RV64, covering only Sv39/Sv48/Sv57 and Sv39x4/Sv48x4/Sv57x4.

> **Companion Document**: This plan is paired with `Hypervisor_gstage_test_plan.md`; pure G-stage independent translation behavior is covered by that plan.

---

## Applicable Scope and Combination Matrix

Two-stage translation involves VS-stage (4 MODEs) x G-stage (4 MODEs) = 16 combinations. This plan assigns test responsibilities per the table below:

| VS-stage \ G-stage | Bare | Sv39x4 | Sv48x4 | Sv57x4 |
|-------------------|------|--------|--------|--------|
| **Bare** | ❌ Equivalent to V=0 (not in this plan) | (Group A) → see `Hypervisor_gstage_test_plan.md` | (Group A) → see same document | (Group A) → see same document |
| **Sv39** | Group B | **Group C primary combination** | Group C′ | Group C′ |
| **Sv48** | Group B | Group C′ | **Group C primary combination** | Group C′ |
| **Sv57** | Group B | Group C′ | Group C′ | **Group C primary combination** |

- **Group A**: G-stage only (VS-stage Bare), independently covered by `Hypervisor_gstage_test_plan.md`
- **Group B**: VS-stage only (hgatp=Bare), verifying equivalence between vsatp and regular satp behavior under V=1
- **Group C primary combinations**: 3 same-width matched combinations (Sv39+Sv39x4 / Sv48+Sv48x4 / Sv57+Sv57x4), covering core two-stage behavior
- **Group C′ cross-width combinations**: 6 cross-width combinations, at least 1~2 sanity cases per pair

---

## SPEC Sections Covered by This Document

- Local paths:
  - `SPEC/riscv-isa-manual/src/priv/hypervisor.adoc`
  - `SPEC/riscv-isa-manual/src/priv/supervisor.adoc`
- Official repository: https://github.com/riscv/riscv-isa-manual

Covers the following chapters of `hypervisor.adoc` ("H" Extension for Hypervisor Support, Version 1.0):

- Two-Stage Address Translation
- Guest Physical Address Translation
- Virtual Supervisor Address Translation and Protection (`vsatp`) Register
- Hypervisor Memory-Management Fence Instructions (HFENCE.VVMA / HFENCE.GVMA)
- Hypervisor Virtual-Machine Load and Store Instructions (HLV / HLVX / HSV)
- Memory-Management Fences (with V=0/V=1 SFENCE.VMA semantics)
- Machine Status (`mstatus` and `mstatush`) Registers — MPV / MPRV tables
- Trap Cause Codes
- Hypervisor Trap Value (`htval`) Register / Hypervisor Trap Instruction (`htinst`) Register
- Transformed Instruction or Pseudoinstruction for `mtinst` or `htinst`

Covers the following chapters of `supervisor.adoc`:

- Supervisor Address Translation and Protection (VPN→PPN bit-width definitions of Sv39/Sv48/Sv57)
- SUM / MXR field definitions of `sstatus`

## Covered Specification Points

| Norm ID | Original Text |
|---------|---------------|
| `norm:H_vm_twostage` | Whenever the current virtualization mode V is 1, two-stage address translation and protection is in effect. For any virtual memory access, the original virtual address is converted in the first stage by VS-level address translation, as controlled by `vsatp`, into a guest physical address. The guest physical address is then converted in the second stage by guest physical address translation, as controlled by `hgatp`, into a supervisor physical address. |
| `norm:H_vm_gstagetrans` | When V=1, memory accesses that would normally bypass address translation are subject to G-stage address translation alone. This includes memory accesses made in support of VS-stage address translation, such as reads and writes of VS-level page tables. |
| `norm:hgatp_mode_sv39x4` | For Sv39x4, partitioning is identical to Sv39, except with 2 more bits at the high end in VPN[2]. Address bits 63:41 must all be zeros, or else a guest-page-fault exception occurs. |
| `norm:hgatp_mode_sv48x4` | For Sv48x4, partitioning is identical to Sv48, except with 2 more bits at the high end in VPN[3]. Address bits 63:50 must all be zeros, or else a guest-page-fault exception occurs. |
| `norm:hgatp_mode_sv57x4` | For Sv57x4, partitioning is identical to Sv57, except with 2 more bits at the high end in VPN[4]. Address bits 63:59 must all be zeros, or else a guest-page-fault exception occurs. |
| `norm:H_vm_gpatrans` | The conversion of an Sv32x4, Sv39x4, Sv48x4, or Sv57x4 guest physical address uses the same algorithm as Sv32, Sv39, Sv48, or Sv57, except: `hgatp` substitutes for `satp`; the effective privilege mode must be VS-mode or VU-mode; the current privilege mode is always taken to be U-mode when checking the U bit; and guest-page-fault exceptions are raised instead of regular page-fault exceptions. |
| `norm:H_vm_gpapriv` | For G-stage address translation, all memory accesses are considered to be user-level accesses. Access type permissions are checked during G-stage translation the same as for VS-stage. For memory accesses supporting VS-stage translation, permissions and A/D bit needs are checked as though for an implicit load or store, not for the original access type. However, any exception is always reported for the original access type. |
| `norm:vsstatus_mxr_vm` | The `vsstatus` field MXR, which makes execute-only pages readable by explicit loads, only overrides VS-stage page protection. Setting MXR at VS-level does not override guest-physical page protections. |
| `norm:sstatus_mxr_vm` | Setting MXR at HS-level, however, overrides both VS-stage and G-stage execute-only permissions. |
| `norm:vsatp_sz_acc_op` | The `vsatp` register is a VSXLEN-bit read/write register that is VS-mode's version of supervisor register `satp`. When V=1, `vsatp` substitutes for the usual `satp`. `vsatp` controls VS-stage address translation, the first stage of two-stage translation for guest virtual addresses. |
| `norm:vsatp_v0` | When V=0, `vsatp` does not directly affect the behavior of the machine, unless a virtual-machine load/store (HLV, HLVX, or HSV) or the MPRV feature in the `mstatus` register is used to execute a load or store as though V=1. |
| `norm:vsatp_mode_unsupported_v0` | When V=0, a write to `vsatp` with an unsupported MODE value is either ignored as it is for `satp`, or the fields of `vsatp` are treated as WARL in the normal way. |
| `norm:vsatp_mode_unsupported_v1` | However, when V=1, a write to `satp` with an unsupported MODE value is ignored and no write to `vsatp` is effected. |
| `norm:vs_stage_speculative_a_bit` | When `vsatp` is active, VS-stage page-table entries' A bits must not be set as a result of speculative execution, unless the effective privilege mode is VS or VU. |
| `norm:hlsv_mode` | The hypervisor virtual-machine load and store instructions are valid only in M-mode or HS-mode, or in U-mode when `hstatus`.HU=1. |
| `norm:hlsv_priv` | Each instruction performs an explicit memory access with an effective privilege mode of VS or VU. The effective privilege mode is VU when `hstatus`.SPVP=0, and VS when `hstatus`.SPVP=1. |
| `norm:hlsv_trans` | As usual for VS-mode and VU-mode, two-stage address translation is applied, and the HS-level `sstatus`.SUM is ignored. |
| `norm:hlsv_sstatus_mxr` | HS-level `sstatus`.MXR makes execute-only pages readable by explicit loads for both stages of address translation (VS-stage and G-stage). |
| `norm:hlsv_vsstatus_mxr` | `vsstatus`.MXR affects only the first translation stage (VS-stage). |
| `norm:hlsv_u_op` | Instructions HLVX.HU and HLVX.WU are the same as HLV.HU and HLV.WU, except that execute permission takes the place of read permission during address translation. The supervisor physical memory attributes must grant both execute and read permissions. |
| `norm:hlsv_virtinst` | Attempts to execute a virtual-machine load/store instruction (HLV, HLVX, or HSV) when V=1 cause a virtual-instruction exception. |
| `norm:hlsv_illegalinst` | Attempts to execute one of these same instructions from U-mode when `hstatus`.HU=0 cause an illegal-instruction exception. |
| `norm:hlsv_op` | For every RV32I or RV64I load instruction, there is a corresponding virtual-machine load instruction: HLV.B, HLV.BU, HLV.H, HLV.HU, HLV.W, HLV.WU, and HLV.D. For every store instruction, there is: HSV.B, HSV.H, HSV.W, and HSV.D. Instructions HLV.WU, HLV.D, and HSV.D are not valid for RV32. |
| `norm:hfence-vvma_hfence-gvma_op` | HFENCE.VVMA and HFENCE.GVMA perform a function similar to SFENCE.VMA, except applying to the VS-level memory-management data structures controlled by CSR `vsatp` (HFENCE.VVMA) or the guest-physical memory-management data structures controlled by CSR `hgatp` (HFENCE.GVMA). |
| `norm:hfence-vvma_mode` | HFENCE.VVMA is valid only in M-mode or HS-mode. Executing an HFENCE.VVMA guarantees that any previous stores already visible to the current hart are ordered before all implicit reads by that hart done for VS-stage address translation for subsequent instructions when `hgatp`.VMID has the same setting. |
| `norm:hfence-vvma_limits` | Implicit reads need not be ordered when `hgatp`.VMID is different than at the time HFENCE.VVMA executed. If rs1≠x0, it specifies a single guest virtual address, and if rs2≠x0, it specifies a single guest address-space identifier (ASID). |
| `norm:hfence-vvma_asid` | When rs2≠x0, bits XLEN-1:ASIDMAX of the value held in rs2 are reserved for future standard use. If ASIDLEN < ASIDMAX, the implementation shall ignore bits ASIDMAX-1:ASIDLEN of the value held in rs2. |
| `norm:hfence-vvma_tvm` | Neither `mstatus`.TVM nor `hstatus`.VTVM causes HFENCE.VVMA to trap. |
| `norm:hfence-gvma_op` | HFENCE.GVMA is valid only in HS-mode when `mstatus`.TVM=0, or in M-mode (irrespective of `mstatus`.TVM). Executing an HFENCE.GVMA guarantees that any previous stores already visible to the current hart are ordered before all implicit reads done for G-stage address translation for subsequent instructions. If rs1≠x0, it specifies a single guest physical address, shifted right by 2 bits, and if rs2≠x0, it specifies a single VMID. |
| `norm:hfence-gvma_mode` | If `hgatp`.MODE is changed for a given VMID, an HFENCE.GVMA with rs1=x0 (and rs2 set to either x0 or the VMID) must be executed to order subsequent guest translations with the MODE change—even if the old MODE or new MODE is Bare. |
| `norm:hfence-gvma_vmid` | When rs2≠x0, bits XLEN-1:VMIDMAX of the value held in rs2 are reserved. If VMIDLEN < VMIDMAX, the implementation shall ignore bits VMIDMAX-1:VMIDLEN. |
| `norm:hfence-vvma_hfence-gvma_exceptions` | Attempts to execute HFENCE.VVMA or HFENCE.GVMA when V=1 cause a virtual-instruction exception, while attempts in U-mode cause an illegal-instruction exception. Attempting HFENCE.GVMA in HS-mode when `mstatus`.TVM=1 also causes an illegal-instruction exception. |
| `norm:sfence_vma_v0` | When V=0, the virtual-address argument to SFENCE.VMA is an HS-level virtual address, and the ASID argument is an HS-level ASID. The instruction orders stores only to HS-level address-translation structures with subsequent HS-level address translations. |
| `norm:sfence_vma_v1` | When V=1, the virtual-address argument is a guest virtual address within the current virtual machine, and the ASID argument is a VS-level ASID. The instruction orders stores only to the VS-level address-translation structures with subsequent VS-stage address translations for the same virtual machine. |
| `norm:mstatus_mprv_hypervisor` | The hypervisor extension changes the behavior of MPRV. When MPRV=0, normal translation. When MPRV=1, explicit memory accesses are translated and protected as though the current virtualization mode were set to MPV and the current nominal privilege mode were set to MPP. |
| `norm:mstatus_mprv_hlsv` | MPRV does not affect the virtual-machine load/store instructions, HLV, HLVX, and HSV. The explicit loads and stores of these instructions always act as though V=1 and the nominal privilege mode were `hstatus`.SPVP, overriding MPRV. |
| `norm:H_guest_page_fault` | Guest-page-fault traps may be delegated from M-mode to HS-mode under the control of `medeleg`, but cannot be delegated to other privilege modes. On a guest-page fault, `mtval` or `stval` is written with the faulting guest virtual address, and `mtval2` or `htval` is written either with zero or with the faulting guest physical address, shifted right by 2 bits. |
| `norm:htval_trapval` | When a guest-page-fault trap is taken into HS-mode, `htval` is written with either zero or the guest physical address that faulted, shifted right by 2 bits. For other traps, `htval` is set to zero. |
| `norm:H_trap_xtinst_guestpage` | For guest-page faults, the trap instruction register is written with a special pseudoinstruction value if: (a) the fault is caused by an implicit memory access for VS-stage address translation, and (b) a nonzero value is written to `mtval2` or `htval`. If both conditions are met, zero is not allowed. |
| `norm:H_trap_xtinst_guestpage_rw` | A write pseudoinstruction (0x00002020 or 0x00003020) is used for the case that the machine is attempting automatically to update bits A and/or D in VS-level page tables. All other implicit memory accesses for VS-stage address translation will be reads. |
| `norm:henvcfg_adue_op` | If the Svadu extension is implemented, the ADUE bit controls whether hardware updating of PTE A/D bits is enabled for VS-stage address translation. When ADUE=1, hardware updating is enabled. When ADUE=0, the implementation behaves as though Svade were implemented for VS-stage address translation. If Svadu is not implemented, ADUE is read-only zero. |
| `norm:henvcfg_pbmte_op` | The PBMTE bit controls whether the Svpbmt extension is available for use in VS-stage address translation. When PBMTE=1, Svpbmt is available for VS-stage address translation. When PBMTE=0, the implementation behaves as though Svpbmt were not implemented for VS-stage address translation. If Svpbmt is not implemented, PBMTE is read-only zero. |
| `norm:H_straddle` | When an instruction fetch or a misaligned memory access straddles a page boundary, two different address translations are involved. When a guest-page fault occurs, the faulting virtual address may be a page-boundary address that is higher than the instruction's original virtual address. |
| `norm:mtval2_htval_virtaddr` | When a guest-page fault is not due to an implicit memory access for VS-stage address translation, a nonzero guest physical address written to `mtval2`/`htval` shall correspond to the exact virtual address written to `mtval`/`stval`. |
| `norm:mtval2_trapval_other` | Otherwise, for misaligned loads and stores that cause guest-page faults, a nonzero guest physical address in `mtval2` corresponds to the faulting portion of the access as indicated by the virtual address in `mtval`. For instruction guest-page faults on systems with variable-length instructions, a nonzero `mtval2` corresponds to the faulting portion of the instruction. |
| `norm:H_vm_gpa_g` | The G bit in all G-stage PTEs is currently not used. It should be cleared by software for forward compatibility, and must be ignored by hardware. |
| `norm:H_pmp` | Machine-level physical memory protection applies to supervisor physical addresses and is in effect regardless of virtualization mode. |
| `norm:hgatp_mode_bare_trans` | When the address translation scheme selected by the MODE field of `hgatp` is Bare, guest physical addresses are equal to supervisor physical addresses without modification, and no memory protection applies in the trivial translation of guest physical addresses to supervisor physical addresses. |
| `norm:H_exception_priority` | If an instruction may raise multiple synchronous exceptions, the decreasing priority order indicates which exception is taken and reported in `mcause` or `scause`. |
| `norm:hgatp_ppn_op` | For the paged virtual-memory schemes, the root page table is 16 KiB and must be aligned to a 16-KiB boundary. In these modes, the lowest two bits of the physical page number (PPN) in `hgatp` always read as zeros. |
| `norm:hgatp_mode_warl` | A write to `hgatp` with an unsupported MODE value is not ignored as it is for `satp`. Instead, the fields of `hgatp` are WARL in the normal way, when so indicated. |
| `norm:satp_ppn_sv39_sz` | The 27-bit VPN is translated into a 44-bit PPN via a three-level page table, while the 12-bit page offset is untranslated. |
| `norm:satp_ppn_sv48_sz` | The 36-bit VPN is translated into a 44-bit PPN via a four-level page table, while the 12-bit page offset is untranslated. |
| `norm:satp_ppn_sv57_sz` | The 45-bit VPN is translated into a 44-bit PPN via a five-level page table, while the 12-bit page offset is untranslated. |
| `norm:hstatus_vtvm_op` | When VTVM=1, an attempt in VS-mode to execute SFENCE.VMA or SINVAL.VMA or to access CSR `satp` raises a virtual-instruction exception. |
| `norm:mstatus_tvm_hs` | Setting TVM=1 prevents HS-mode from accessing `hgatp` or executing HFENCE.GVMA or HINVAL.GVMA, but has no effect on accesses to `vsatp` or instructions HFENCE.VVMA or HINVAL.VVMA. |
| `norm:hlvx-wu_valid32` | HLVX.WU is valid for RV32, even though LWU and HLV.WU are not. (For RV32, HLVX.WU can be considered a variant of HLV.W, as sign extension is irrelevant for 32-bit values.) |
| `norm:sstatus_sum` | The SUM bit modifies the privilege with which S-mode loads and stores access virtual memory. When SUM=0, S-mode memory accesses to pages that are accessible by U-mode (U=1) will fault. When SUM=1, these accesses are permitted. SUM has no effect when page-based virtual memory is not in effect, nor when executing in U-mode. |
| `norm:sstatus_mxr` | The MXR bit modifies the privilege with which loads access virtual memory. When MXR=0, only loads from pages marked readable (R=1) will succeed. When MXR=1, loads from pages marked either readable or executable (R=1 or X=1) will succeed. MXR has no effect when page-based virtual memory is not in effect. |

---

## Test Group Definitions

### Group 1: VS-stage Single Stage when V=1 (hgatp = Bare)

**Specification Basis**:
- `norm:H_vm_twostage`: Two-stage is enforced when V=1, but either stage can be "effectively disabled" by setting the corresponding ATP to Bare
- `norm:vsatp_sz_acc_op`: `vsatp` is VS-mode's `satp`, using the same algorithm and PTE format
- When hgatp=Bare, GPA equals SPA directly, and two-stage degenerates to single VS-stage translation

**Test Responsibility**: When hgatp=Bare, VS-stage behavior should be identical to normal S-mode translation controlled by satp. This group uses the core Groups from `vm_test_plan.md` as baseline to verify vsatp equivalence under V=1.

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| TS-VS-01 | Sv39 vsatp 1GB gigapage | hgatp=Bare, vsatp=Sv39, 1GB identity mapping, VS-mode R/W | R/W success |
| TS-VS-02 | Sv39 vsatp 2MB megapage | Same as above, 2MB | R/W success |
| TS-VS-03 | Sv39 vsatp 4KB page | Same as above, 4KB | R/W success |
| TS-VS-04 | Sv48 vsatp 4KB | hgatp=Bare, vsatp=Sv48, 4KB mapping | R/W success |
| TS-VS-05 | Sv57 vsatp 4KB | hgatp=Bare, vsatp=Sv57, 4KB mapping | R/W success |
| TS-VS-06 | VS-stage U-bit: VS accesses U=0 | vsatp=Sv39, PTE U=0, VS-mode (nominal S) access, `vsstatus.SUM=0` | Success (VS-mode is S-level, U=0 PTE denies U-mode but allows VS-mode) |
| TS-VS-07 | VS-stage U-bit: VS accesses U=1 + SUM=0 | vsatp=Sv39, PTE U=1, VS-mode access, `vsstatus.SUM=0` | store/load page-fault (cause 13/15, equivalent to vm_test_plan SUM-02) |
| TS-VS-08 | VS-stage U-bit: VS accesses U=1 + SUM=1 | Same as above but `vsstatus.SUM=1` | Success |
| TS-VS-09 | VS-stage MXR: vsstatus.MXR=1 reads X-only | PTE R=0,X=1, `vsstatus.MXR=1`, VS-mode load | Success |
| TS-VS-10 | VS-stage PTE V=0/RW=01 | vsatp=Sv39, PTE V=0 or R=0,W=1 | page-fault (cause 12/13/15, **not** guest-page-fault) |

> [!NOTE]
> All Group 1 test cases use the corresponding Groups from `vm_test_plan.md` as baseline, but the trap context switches to the HS-mode handler, and the trap source is `hstatus.SPV=1`. The fault cause remains 12/13/15 (regular page-fault) because G-stage Bare does not trigger guest-page-fault.

---

### Group 2: vsatp CSR Behavior

**Specification Basis**:
- `norm:vsatp_sz_acc_op`: When V=1, `satp` actually accesses `vsatp`
- `norm:vsatp_mode_unsupported_v0`: When V=0, writing unsupported MODE to `vsatp` behaves like `satp` (ignored or WARL)
- `norm:vsatp_mode_unsupported_v1`: When V=1, writing unsupported MODE to `satp` is **ignored** and does not write to `vsatp`
- `norm:vs_stage_speculative_a_bit`: VS-stage A bit must not be set by speculative execution (unless actually in VS/VU-mode)
- `norm:vsatp_v0`: When V=0, `vsatp` does not directly affect machine behavior, but HLV/HSV/MPRV can trigger as-if V=1

**Test Responsibility**: Verify vsatp access, MODE WARL/ignore behavior, ASID field, and V=0/V=1 differences.

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| TS-VSATP-01 | satp accesses vsatp when V=1 | HS-mode writes a value to vsatp, switch to VS-mode and read with `csrr satp` | Reads vsatp value |
| TS-VSATP-02 | satp writes to vsatp when V=1 | VS-mode writes a legal value with `csrw satp`, switch back to HS-mode and read with `csrr vsatp` | vsatp has been written |
| TS-VSATP-03 | Writing unsupported MODE ignored when V=1 | VS-mode writes MODE=2 (reserved) via `csrw satp` | vsatp not written (different from V=0 behavior) |
| TS-VSATP-04 | Writing reserved MODE to vsatp when V=0 | HS-mode directly writes MODE=7 (reserved encoding, never supported) with `csrw vsatp` | (a) Entire write ignored (satp semantics), or (b) MODE adjusted to legal value (0/8/9/10) by WARL; reserved encoding must not persist (norm:vsatp_mode_unsupported_v0) |
| TS-VSATP-05 | vsatp ASID field read/write | HS-mode writes vsatp ASID=0xFF, reads back | ASID preserves implementation-supported bits |
| TS-VSATP-06 | vsatp PPN field read/write | HS-mode writes legal PPN, reads back | PPN field reads back correctly (no PPN[1:0] forced to zero like hgatp) |
| TS-VSATP-07 | VS accesses satp when hstatus.VTVM=1 | Set `hstatus.VTVM=1`, VS-mode `csrr satp` | virtual-instruction exception (cause=22) |


---

### Group 3: Full Two-Stage Identity Mapping (Same Width)

**Specification Basis**:
- `norm:H_vm_twostage`: Both stages are effective when V=1
- `norm:H_vm_gstagetrans`: When V=1, even implicit accesses supporting VS-stage translation (reading/writing VS-level page tables) go through G-stage translation

**Test Responsibility**: Verify the core translation path for three same-width two-stage combinations under VA = GPA = SPA identity mapping.

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| TS-MAP-01 | Sv39+Sv39x4 1GB two-stage identity | Two-stage 1GB identity mapping, VS-mode R/W | Success |
| TS-MAP-02 | Sv39+Sv39x4 2MB | Two-stage 2MB | Success |
| TS-MAP-03 | Sv39+Sv39x4 4KB | Two-stage 4KB | Success |
| TS-MAP-04 | Sv39+Sv39x4 superpage mixed | VS-stage uses 1GB, G-stage uses 4KB; and vice versa | Success (effective page size takes the smaller) |
| TS-MAP-05 | Sv48+Sv48x4 1GB | Same-mode 1GB two-stage | Success |
| TS-MAP-06 | Sv48+Sv48x4 2MB | Same-mode 2MB | Success |
| TS-MAP-07 | Sv48+Sv48x4 4KB | Same-mode 4KB | Success |
| TS-MAP-08 | Sv48+Sv48x4 512GB terapage | Large superpage (subject to platform memory limits) | Success (within legal subrange) |
| TS-MAP-09 | Sv57+Sv57x4 1GB | Same-mode 1GB | Success |
| TS-MAP-10 | Sv57+Sv57x4 2MB | Same-mode 2MB | Success |
| TS-MAP-11 | Sv57+Sv57x4 4KB | Same-mode 4KB | Success |
| TS-MAP-12 | VU-mode two-stage access | Sv39+Sv39x4, both VS-stage and G-stage U=1, enter VU-mode access | Success |

---

### Group 4: Cross-Width Two-Stage Combinations

**Specification Basis**:
- VS-stage and G-stage MODE selection are independent; the specification does not require matching widths (typical usage is hypervisor selecting wider or narrower scheme in hgatp than vsatp)
- `norm:hgatp_mode_sv39x4`, `norm:hgatp_mode_sv48x4`, `norm:hgatp_mode_sv57x4`: Define three G-stage modes

**Test Responsibility**: Cover all 9 cross-width combinations (including 3 VS<=G and 3 VS>G normal flows, plus 3 VS>G GPA out-of-bounds fault flows), with each pair verified using multiple page granularity sub-variants (4K/2M/1G) to ensure combinations are usable and translation is correct.

#### VS<=G Normal Flow (VS address space no wider than G)

| Test ID | Test Name | VS-stage | G-stage | Test Description | Expected Result |
|---------|-----------|----------|---------|------------------|-----------------|
| TS-XMODE-01.a | Sv39+Sv48x4 VS=4K G=4K | Sv39 | Sv48x4 | 4KB identity mapping R/W | Success |
| TS-XMODE-01.b | Sv39+Sv48x4 VS=4K G=2M | Sv39 | Sv48x4 | G-stage 2MB superpage | Success |
| TS-XMODE-01.c | Sv39+Sv48x4 VS=4K G=1G | Sv39 | Sv48x4 | G-stage 1GB superpage | Success |
| TS-XMODE-02.a | Sv39+Sv57x4 VS=4K G=4K | Sv39 | Sv57x4 | 4KB identity mapping R/W | Success |
| TS-XMODE-02.b | Sv39+Sv57x4 VS=4K G=2M | Sv39 | Sv57x4 | G-stage 2MB superpage | Success |
| TS-XMODE-02.c | Sv39+Sv57x4 VS=4K G=1G | Sv39 | Sv57x4 | G-stage 1GB superpage | Success |
| TS-XMODE-04.a | Sv48+Sv57x4 VS=4K G=4K | Sv48 | Sv57x4 | 4KB identity mapping R/W | Success |
| TS-XMODE-04.b | Sv48+Sv57x4 VS=4K G=2M | Sv48 | Sv57x4 | G-stage 2MB superpage | Success |
| TS-XMODE-04.c | Sv48+Sv57x4 VS=4K G=1G | Sv48 | Sv57x4 | G-stage 1GB superpage | Success |

#### VS>G Normal Flow (VS address space wider than G, but GPA falls within G-stage addressable range)

| Test ID | Test Name | VS-stage | G-stage | Test Description | Expected Result |
|---------|-----------|----------|---------|------------------|-----------------|
| TS-XMODE-03.a | Sv48+Sv39x4 VS=4K G=4K | Sv48 | Sv39x4 | 4KB identity mapping R/W; GPA falls within [0, 2^41) | Success |
| TS-XMODE-03.b | Sv48+Sv39x4 VS=4K G=2M | Sv48 | Sv39x4 | G-stage 2MB superpage; GPA < 2^41 | Success |
| TS-XMODE-03.c | Sv48+Sv39x4 VS=4K G=1G | Sv48 | Sv39x4 | G-stage 1GB superpage; GPA < 2^41 | Success |
| TS-XMODE-05.a | Sv57+Sv39x4 VS=4K G=4K | Sv57 | Sv39x4 | 4KB identity mapping R/W; GPA < 2^41 | Success |
| TS-XMODE-05.b | Sv57+Sv39x4 VS=4K G=2M | Sv57 | Sv39x4 | G-stage 2MB superpage; GPA < 2^41 | Success |
| TS-XMODE-05.c | Sv57+Sv39x4 VS=4K G=1G | Sv57 | Sv39x4 | G-stage 1GB superpage; GPA < 2^41 | Success |
| TS-XMODE-06.a | Sv57+Sv48x4 VS=4K G=4K | Sv57 | Sv48x4 | 4KB identity mapping R/W; GPA < 2^50 | Success |
| TS-XMODE-06.b | Sv57+Sv48x4 VS=4K G=2M | Sv57 | Sv48x4 | G-stage 2MB superpage; GPA < 2^50 | Success |
| TS-XMODE-06.c | Sv57+Sv48x4 VS=4K G=1G | Sv57 | Sv48x4 | G-stage 1GB superpage; GPA < 2^50 | Success |

#### VS>G Out-of-Bounds Fault Flow (VS translation produces GPA exceeding G-stage addressable range)

| Test ID | Test Name | VS-stage | G-stage | Test Description | Expected Result |
|---------|-----------|----------|---------|------------------|-----------------|
| TS-XMODE-07 | Sv48+Sv39x4 GPA out-of-bounds | Sv48 | Sv39x4 | VS-stage PTE PPN points to GPA >= 2^41 | guest-page-fault (cause=21) |
| TS-XMODE-08 | Sv57+Sv39x4 GPA out-of-bounds | Sv57 | Sv39x4 | VS-stage PTE PPN points to GPA >= 2^41 | guest-page-fault (cause=21) |
| TS-XMODE-09 | Sv57+Sv48x4 GPA out-of-bounds | Sv57 | Sv48x4 | VS-stage PTE PPN points to GPA >= 2^50 | guest-page-fault (cause=21) |

> [!NOTE]
> - TS-XMODE-03/05/06 key scenario: Although VS-stage address space is wider than G-stage, as long as the GPA produced by VS translation falls within G-stage addressable range (low address region), two-stage translation should complete normally. Tests use identity mapping in low memory region (test_data_area < 4GB) to verify this scenario.
> - TS-XMODE-07/08/09 key scenario: When VS-stage translation produces a GPA exceeding what G-stage mode can handle (e.g., Sv48 outputs GPA >= 2^41 but hgatp=Sv39x4 only supports 41-bit GPA), guest-page-fault (cause=21) should be triggered. Implementation tampers with VS leaf PTE PPN to point to out-of-bounds address.

---

### Group 5: Non-Identity Two-Stage Mapping

**Specification Basis**:
- `norm:H_vm_twostage`: Two-stage independent translation, final physical address = composition of two-stage mappings
- Specification does not restrict VA, GPA, SPA to be equal

**Test Responsibility**: Verify end-to-end correctness of two-stage chain under non-identity mapping, and verify that VS-stage output GPA strictly matches G-stage input GPA.

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| TS-NID-01 | VS-stage non-identity | VS-stage maps VA1 -> GPA_X, G-stage maps GPA_X -> SPA_X (GPA_X = SPA_X identity); VS-mode writes VA1, physical SPA_X should be modified | Physical read of SPA_X sees written value |
| TS-NID-02 | G-stage non-identity | VS-stage maps VA1 -> GPA_X identity; G-stage maps GPA_X -> SPA_Y (different address); VS-mode writes VA1, physical SPA_Y should be modified | Physical read of SPA_Y sees written value; physical read of GPA_X unchanged |
| TS-NID-03 | Both stages non-identity | VS-stage VA1 -> GPA_X, G-stage GPA_X -> SPA_Y | VS-mode access to VA1 affects physical SPA_Y |
| TS-NID-04 | Multi-page mapping continuity | VS-stage and G-stage each map 4 consecutive pages to different base addresses, verify all 4 page mappings take effect | Each page R/W succeeds and data is isolated |

---

### Group 6: G-stage Fault Triggered by Implicit Access

**Specification Basis**:
- `norm:H_vm_gstagetrans`: When V=1, implicit accesses supporting VS-stage translation (reading VS-level page tables) are also subject to G-stage translation
- `norm:H_vm_gpapriv`: Implicit accesses check permissions and A/D bits as implicit load/store
- `norm:H_guest_page_fault`: On fault, stval=GVA, htval may be written with GPA of implicit access
- `norm:htval_trapval`: When fault is triggered by implicit VS-stage access, htval is written with **PTE's GPA** (not GPA corresponding to original VA)
- `norm:H_trap_xtinst_guestpage`: In this scenario, htinst must be written with pseudoinstruction (0 not allowed)
- `norm:H_trap_xtinst_guestpage_rw`: read uses `0x00002000`/`0x00003000`, write (A/D auto-update) uses `0x00002020`/`0x00003020`

**Test Responsibility**: Place VS-stage page table itself at a GPA that G-stage does not map or has no permissions, trigger implicit access fault, verify htval/htinst/cause correctness.

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| TS-IMPL-01 | VS-stage PT at unmapped GPA in G-stage | VS-stage root page table allocated at GPA_PT, G-stage does not map GPA_PT; VS-mode load VA | cause=21 (load guest-page-fault), stval=VA, htval=GPA_PT>>2, htinst=`0x00003000` (RV64 read) |
| TS-IMPL-03 | VS-stage PT read-only in G-stage + D=0 store | VS-stage PTE needs hardware D bit update (store access), but G-stage maps corresponding GPA as read-only | cause=23 (HW A/D implementation) or cause=21 (no HW A/D implementation), both compliant |
| TS-IMPL-04 | VS-stage PT G-stage U=0 | VS-stage page table GPA mapped with U=0 in G-stage | Implicit access fails, guest-page-fault |
| TS-IMPL-06 | inst guest-page-fault from implicit | VS-stage translation fetch PT implicit access fails | cause=20, htinst must be pseudoinst (read: `0x00003000`) |

> [!NOTE]
> Key design constraints for cases in this group: the physical address of the VS-level root page table must be explicitly obtainable by the test, so that this GPA alone can be marked invalid in G-stage while keeping normal 4KB-granularity G-stage mappings for the remaining regions such as the code segment, stack, and target data, avoiding the root page table also being identity-mapped due to large superpage coverage; htval should point to the GPA of the VS-level PTE (rather than the GPA corresponding to the original VA).

---

### Group 7: Permission Intersection (VS-stage RWX x G-stage RWX)

**Specification Basis**:
- Two-stage permissions take intersection: any stage denial triggers fault
- Fault cause distinction: VS-stage failure -> page-fault (12/13/15); G-stage failure -> guest-page-fault (20/21/23)

**Test Responsibility**: Cover key intersections of VS-stage and G-stage RWX permissions, verify correspondence between fault cause and stage source.

| Test ID | Test Name | VS-stage PTE | G-stage PTE | Access Type | Expected Result |
|---------|-----------|--------------|-------------|-------------|-----------------|
| TS-PERM-01 | Dual-stage RWX | RWX | RWXU | R/W/X | All success |
| TS-PERM-02 | VS-stage R-only, G-stage RWX | R | RWXU | store | store page-fault (cause=15, **VS-stage source**) |
| TS-PERM-03 | VS-stage RWX, G-stage R-only | RWX | RU | store | store guest-page-fault (cause=23, **G-stage source**) |
| TS-PERM-04 | VS-stage X-only, G-stage RWX | X | RWXU | load (no MXR) | load page-fault (cause=13) |
| TS-PERM-05 | VS-stage RWX, G-stage X-only | RWX | XU | load (no MXR) | load guest-page-fault (cause=21) |
| TS-PERM-06 | VS-stage RX, G-stage RX | RX | RXU | fetch | Success |
| TS-PERM-07 | VS-stage V=0 | V=0 | RWXU | load | page-fault (cause=13) |
| TS-PERM-08 | G-stage V=0 | RWX | V=0 | load | guest-page-fault (cause=21) |
| TS-PERM-09 | G-stage U=0 | RWX | RWX (U=0) | load | guest-page-fault (cause=21) |
| TS-PERM-10 | VS-stage U=0 (VU access) | RWX (U=0) | RWXU | VU-mode load | page-fault (cause=13, U-mode cannot access U=0) |
| TS-PERM-11 | VS-stage U=1 (VS access, SUM=0) | RWX (U=1) | RWXU | VS-mode load with vsstatus.SUM=0 | page-fault (cause=13) |
| TS-PERM-12 | VS-stage U=1 (VS access, SUM=1) | RWX (U=1) | RWXU | VS-mode load with vsstatus.SUM=1 | Success |

---

### Group 8: MXR Differential Effects in Two-Stage

**Specification Basis**:
- `norm:vsstatus_mxr_vm`: "The vsstatus field MXR ... only overrides VS-stage page protection. Setting MXR at VS-level does not override guest-physical page protections."
- `norm:sstatus_mxr_vm`: "Setting MXR at HS-level, however, overrides both VS-stage and G-stage execute-only permissions."

**Test Responsibility**: Verify MXR's dual semantics in two-stage.

| Test ID | Test Name | VS-stage | G-stage | sstatus.MXR | vsstatus.MXR | Access | Expected Result |
|---------|-----------|----------|---------|-------------|--------------|--------|-----------------|
| TS-MXR-01 | No MXR, X-only unreadable | X-only | RWXU | 0 | 0 | load | load page-fault |
| TS-MXR-02 | vsstatus.MXR=1, VS X-only readable | X-only | RWXU | 0 | 1 | load | Success |
| TS-MXR-03 | vsstatus.MXR=1 cannot override G-stage X-only | RWX | X-only U | 0 | 1 | load | guest-page-fault (vsstatus.MXR does not affect G-stage) |
| TS-MXR-04 | sstatus.MXR=1 overrides both stages | X-only | X-only U | 1 | 0 | load | Success |
| TS-MXR-05 | sstatus.MXR=1 + vsstatus.MXR=1 | X-only | X-only U | 1 | 1 | load | Success (equivalent to TS-MXR-04) |

---

### Group 9: SUM Effects in Two-Stage

**Specification Basis**:
- `vsstatus.SUM` controls VS-stage U-bit checking (whether VS-mode can access U=1 PTEs)
- G-stage always treats as U-mode, no SUM concept exists
- `norm:hlsv_trans`: HS-level `sstatus.SUM` is ignored during HLV/HSV (see Group 13)

**Test Responsibility**: Verify vsstatus.SUM only affects VS-stage.

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| TS-SUM-01 | vsstatus.SUM=0, VS accesses U=1 | VS-stage PTE U=1, VS-mode load with SUM=0 | page-fault (cause=13) |
| TS-SUM-02 | vsstatus.SUM=1, VS accesses U=1 | Same as above with SUM=1 | Success |
| TS-SUM-03 | SUM does not affect G-stage U-bit | VS-stage U=1 (VS access OK with SUM=1), G-stage U=0 | guest-page-fault (G-stage not affected by SUM) |

---

### Group 10: HFENCE.VVMA Semantics

**Specification Basis**:
- `norm:hfence-vvma_hfence-gvma_op`: HFENCE.VVMA is similar to executing SFENCE.VMA in VS-mode, but guarantees VS-level page table writes are visible before subsequent VS-stage translations
- `norm:hfence-vvma_mode`: Valid only in M-mode / HS-mode
- `norm:hfence-vvma_limits`: Only affects VM corresponding to current hgatp.VMID; rs1/rs2 select VA/ASID
- `norm:hfence-vvma_tvm`: mstatus.TVM and hstatus.VTVM do not affect HFENCE.VVMA
- `norm:hfence-vvma_hfence-gvma_exceptions`: V=1 execution -> virtual-instruction exception (cause=22); U-mode execution -> illegal-instruction exception

**Test Responsibility**: Verify HFENCE.VVMA flush effectiveness and exception behavior.

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| TS-HV-01 | Global flush (rs1=0, rs2=0) | VS-stage modifies PTE, HS-mode executes `hfence.vvma`, then VS-mode accesses | New PTE takes effect |
| TS-HV-02 | Flush by VA (rs1!=0) | Modify VS-stage PTE, then `hfence.vvma vaddr, x0` | New PTE for that VA takes effect |
| TS-HV-03 | Flush by ASID (rs2!=0) | `hfence.vvma x0, asid` | Specified ASID translation is flushed |
| TS-HV-04 | mstatus.TVM=1 has no effect | HS-mode sets TVM=1, then executes `hfence.vvma` | Executes normally (no exception) |
| TS-HV-05 | hstatus.VTVM=1 has no effect | HS-mode sets VTVM=1, then executes `hfence.vvma` | Executes normally |
| TS-HV-06 | V=1 execution -> virtual-inst exception | VS-mode executes `hfence.vvma` | virtual-instruction exception (cause=22) |

---

### Group 11: HFENCE.GVMA Semantics

**Specification Basis**:
- `norm:hfence-gvma_op`: HFENCE.GVMA flushes G-stage translation cache
- Valid only in HS-mode (mstatus.TVM=0) or M-mode
- `norm:hfence-gvma_mode`: Must execute HFENCE.GVMA (rs1=0) after modifying hgatp.MODE
- `norm:hfence-gvma_vmid`: rs2 selects VMID
- rs1 is GPA>>2
- `norm:hfence-vvma_hfence-gvma_exceptions`: V=1 -> virtual-inst; mstatus.TVM=1 in HS-mode -> illegal-inst; U-mode -> illegal-inst

**Test Responsibility**: Verify HFENCE.GVMA flush effectiveness and exception behavior.

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| TS-HG-01 | Global flush (rs1=0, rs2=0) | HS-mode modifies G-stage PTE, then `hfence.gvma`, VS-mode accesses | New G-stage PTE takes effect |
| TS-HG-02 | Flush by GPA (rs1=GPA>>2) | `hfence.gvma gpa>>2, x0` | Specified GPA translation is flushed |
| TS-HG-03 | Flush by VMID (rs2=vmid) | `hfence.gvma x0, vmid` | Specified VMID translation is flushed |
| TS-HG-04 | Must fence after hgatp.MODE switch | Switch hgatp.MODE Sv39x4<->Sv48x4 | Works normally only after executing `hfence.gvma` post-switch |
| TS-HG-05 | mstatus.TVM=1 triggers illegal | HS-mode sets TVM=1, then `hfence.gvma` | illegal-instruction (cause=2) |
| TS-HG-06 | V=1 execution -> virtual-inst | VS-mode executes `hfence.gvma` | virtual-instruction (cause=22) |

---

### Group 12: SFENCE.VMA Behavior under V=1

**Specification Basis**:
- `norm:sfence_vma_v0`: When V=0, SFENCE.VMA operates on HS-level page tables
- `norm:sfence_vma_v1`: When V=1, SFENCE.VMA's VA is GVA, ASID is VS-level ASID, only affects VM corresponding to hgatp.VMID
- `norm:hstatus_vtvm_op`: When hstatus.VTVM=1, VS-mode executing SFENCE.VMA -> virtual-instruction exception

**Test Responsibility**: Verify SFENCE.VMA semantics switching under V=1.

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| TS-SF-01 | V=1 SFENCE.VMA flushes VS-stage | VS-mode modifies PTE controlled by vsatp, then executes `sfence.vma` | New PTE takes effect |
| TS-SF-02 | V=1 SFENCE.VMA does not flush G-stage | VS-mode `sfence.vma` does not affect G-stage translation cache | G-stage modifications do not take effect immediately (requires HFENCE.GVMA) |
| TS-SF-03 | hstatus.VTVM=1 -> virtual-inst | Set VTVM=1, VS-mode executes `sfence.vma` | virtual-instruction (cause=22) |
| TS-SF-04 | V=0 SFENCE.VMA does not affect VS-stage | HS-mode executes `sfence.vma` | Only flushes HS-level translation cache |

---

### Group 13: HLV/HLVX/HSV Two-Stage Translation

**Specification Basis**:
- `norm:hlsv_mode`: HLV/HLVX/HSV valid only in M-mode or HS-mode; U-mode only when `hstatus.HU=1`
- `norm:hlsv_priv`: Effective privilege determined by `hstatus.SPVP` (0=VU, 1=VS)
- `norm:hlsv_trans`: Always uses two-stage translation (vsatp + hgatp), HS-level `sstatus.SUM` is ignored
- `norm:hlsv_sstatus_mxr`: HS-level `sstatus.MXR` affects both stages
- `norm:hlsv_vsstatus_mxr`: `vsstatus.MXR` affects only VS-stage
- `norm:hlsv_u_op`: HLVX.HU/WU uses execute permission instead of read permission
- `norm:hlvx-wu_valid32`: HLVX.WU is valid on RV32 as well
- `norm:hlsv_virtinst`: V=1 execution of HLV/HLVX/HSV -> virtual-instruction exception
- `norm:hlsv_illegalinst`: U-mode with hstatus.HU=0 -> illegal-instruction exception
- `norm:hlsv_op`: every RV load/store has a corresponding HLV/HSV variant; width and sign/zero-extension semantics (TS-HLV-13/14)

**Test Responsibility**: Verify HLV/HLVX/HSV behavior under two-stage translation, effective privilege control, MXR/SUM differences, exception triggering.

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| TS-HLV-01 | HS-mode HLV.D reads guest memory | Two-stage mapping established, HS-mode executes `hlv.d` to read guest VA | Reads correct value |
| TS-HLV-02 | HS-mode HSV.D writes guest memory | HS-mode executes `hsv.d` to write guest VA | Guest memory is written |
| TS-HLV-03 | SPVP=0 (VU) accesses U=0 -> fault | VS-stage PTE U=0, SPVP=0, HS-mode `hlv.d` | load page-fault (U-mode cannot access U=0) |
| TS-HLV-04 | SPVP=1 (VS) accesses U=0 -> success | VS-stage PTE U=0, SPVP=1, HS-mode `hlv.d` | Success (VS-mode is S-level) |
| TS-HLV-05 | sstatus.SUM is ignored | VS-stage PTE U=1, SPVP=1, sstatus.SUM=0 but vsstatus.SUM=1, HS-mode `hlv.d` | Success (HS sstatus.SUM does not affect, controlled by vsstatus.SUM) |
| TS-HLV-06 | sstatus.MXR=1 affects VS-stage X-only | VS-stage X-only, G-stage RWX, sstatus.MXR=1, HS-mode `hlv.d` | Success |
| TS-HLV-07 | sstatus.MXR=1 affects G-stage X-only | VS-stage RWX, G-stage X-only U, sstatus.MXR=1, HS-mode `hlv.d` | Success (HS-MXR affects both stages) |
| TS-HLV-08 | vsstatus.MXR=1 does not affect G-stage X-only | VS-stage RWX, G-stage X-only U, vsstatus.MXR=1, sstatus.MXR=0, HS-mode `hlv.d` | guest-page-fault |
| TS-HLV-09 | HLVX.WU uses X permission instead of R | VS-stage X-only (R=0,X=1), G-stage X-only U, HS-mode `hlvx.wu` | Success (X permission suffices) |
| TS-HLV-10 | V=1 executes HLV -> virtual-inst | VS-mode executes `hlv.d` | virtual-instruction exception (cause=22) |
| TS-HLV-11 | U-mode + HU=0 -> illegal | hstatus.HU=0, U-mode executes `hlv.d` | illegal-instruction exception (cause=2) |
| TS-HLV-12 | U-mode + HU=1 -> normal | hstatus.HU=1, U-mode executes `hlv.d`, correct two-stage mapping | Success |
| TS-HLV-13 | HLV width variants read and extension | Plant width-discriminating pattern in guest page, execute HLV.B/BU/H/HU/W/WU/D in sequence | Each variant reads the correct width with correct sign/zero extension (`norm:hlsv_op`) |
| TS-HLV-14 | HSV width variants write isolation | Pre-fill guest page with 0xFF, execute HSV.B/H/W in sequence | Only the target-width bytes are modified; adjacent bytes remain 0xFF (`norm:hlsv_op`) |

> [!NOTE]
> TS-HLV-09 (HLVX) test requires test page to have at least X=1 in both VS-stage and G-stage (read permission may be absent). Additionally, SPA corresponding physical memory attributes must satisfy both X+R permissions (`norm:hlsv_u_op`), satisfied in practice through PMP configuration of full RWX.

---

### Group 14: mstatus.MPRV+MPV Triggered Two-Stage

**Specification Basis**:
- `norm:mstatus_mprv_hypervisor`: In M-mode when `mstatus.MPRV=1`, explicit memory accesses are translated according to virtualization mode and privilege level determined by `mstatus.MPV` and `mstatus.MPP`
- h-mprv table: MPRV=1 + MPV=1 + MPP=0 -> VU-level two-stage; MPRV=1 + MPV=1 + MPP=1 -> VS-level two-stage; MPP=3 (M) -> no translation
- `norm:mstatus_mprv_hlsv`: MPRV does not affect HLV/HLVX/HSV (these instructions always act as V=1 + SPVP)

**Test Responsibility**: Verify two-stage translation triggered by MPRV+MPV combinations in M-mode, covering key rows of h-mprv table.

| Test ID | Test Name | MPRV | MPV | MPP | Test Description | Expected Result |
|---------|-----------|------|-----|-----|------------------|-----------------|
| TS-MPRV-01 | MPRV=0 -> no translation | 0 | 1 | 1 | M-mode normal ld access, MPRV=0 | Direct physical access, no two-stage triggered |
| TS-MPRV-02 | MPRV=1 + MPV=1 + MPP=S -> VS-level two-stage | 1 | 1 | S | Two-stage mapping ready, M-mode normal ld accesses guest VA | Translated via two-stage, accesses SPA |
| TS-MPRV-03 | MPRV=1 + MPV=1 + MPP=U -> VU-level two-stage | 1 | 1 | U | Two-stage mapping U=1, M-mode normal ld | Translated via two-stage from VU perspective; U=0 PTE triggers fault |
| TS-MPRV-04 | MPRV=1 + MPP=M -> no translation | 1 | x | M | M-mode normal ld, MPP=M | Direct physical access (independent of MPV) |
| TS-MPRV-05 | MPRV does not affect HLV | 1 | 0 | M | M-mode sets MPRV=1 + MPV=0, but executes HLV.D | HLV still translates as V=1 + SPVP, independent of MPRV/MPV/MPP |

> [!WARNING]
> TS-MPRV series tests require special attention: After M-mode sets MPRV=1, **all** explicit load/store in M-mode itself are translated, including stack accesses and CSR reads/writes within trap handlers. Must enable MPRV within minimal code block and clear immediately after target access to avoid affecting stack/local variable accesses. Recommend using inline asm to strictly control MPRV enable window.

---

### Group 15: A/D Bit Two-Stage Handling

**Specification Basis**:
- `norm:henvcfg_adue_op`: When ADUE=0, VS-stage behaves as Svade (A/D=0 triggers page-fault instead of hardware update); when ADUE=1, hardware can auto-update VS-stage PTE A/D
- `norm:H_vm_gpapriv`: Implicit memory accesses supporting VS-stage translation (including implicit store for hardware A/D update) check G-stage permissions and A/D bits as implicit load/store

**Test Responsibility**: Verify henvcfg.ADUE control effect on VS-stage A/D bit hardware update, and G-stage permission interception behavior for implicit A/D writes.

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| TS-AD-01 | VS-stage A=0 when ADUE=0 | henvcfg.ADUE=0, VS-stage PTE A=0, VS-mode load | page-fault (cause=13), hardware does not auto-update A bit |
| TS-AD-02 | VS-stage A=0 normal when ADUE=1 | henvcfg.ADUE=1, VS-stage PTE A=0, G-stage maps PTE GPA as RWU | Access succeeds, VS-stage PTE A bit auto-set to 1 by hardware |
| TS-AD-03 | A bit update intercepted by G-stage read-only when ADUE=1 | henvcfg.ADUE=1, VS-stage PTE A=0, G-stage maps PTE GPA as read-only U | cause=23 (HW A/D implementation) or cause=21 (no HW A/D implementation), both compliant |
| TS-AD-04 | D bit update intercepted by G-stage read-only when ADUE=1 | henvcfg.ADUE=1, VS-stage PTE A=1,D=0, VS-mode store, G-stage maps PTE GPA as read-only U | guest-page-fault (cause=23), htinst=`0x00003020` (write pseudoinst) |
| TS-AD-05 | G-stage PTE A=0 | G-stage PTE A=0 (other permissions normal RWXU), VS-mode load | guest-page-fault (cause=21) |
| TS-AD-06 | G-stage PTE D=0 + store | G-stage PTE D=0 (R=1,W=1,U=1,A=1), VS-mode store | guest-page-fault (cause=23) |

---

### Group 16: Page Boundary Straddle

**Specification Basis**:
- `norm:H_straddle`: When instruction fetch or misaligned memory access straddles page boundary, two address translations are involved; on guest-page-fault, stval may be page boundary address
- `norm:mtval2_htval_virtaddr`: On non-implicit access guest-page-fault, nonzero GPA in htval must correspond to exact virtual address pointed to by stval
- `norm:mtval2_trapval_other`: On misaligned-access / variable-length-fetch guest-page-fault, nonzero mtval2/htval corresponds to the faulting portion (TS-STRD-01/02)

**Test Responsibility**: Verify fault behavior and htval/stval precision for cross-page-boundary accesses under two-stage translation.

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| TS-STRD-01 | Load crosses page, second page G-stage unmapped | 4-byte load starting at page_end-2, first page two-stage normal, second page G-stage invalid | guest-page-fault (cause=21), stval=second page start GVA, htval=0 or second page GPA>>2 (faulting portion, `norm:mtval2_trapval_other`) |
| TS-STRD-02 | Fetch crosses page, second page G-stage no X | 32-bit instruction starting 2 bytes before the page end, second page G-stage X=0 | inst guest-page-fault (cause=20), stval=second page start GVA, htval=0 or second page GPA>>2 (`norm:mtval2_trapval_other`) |
| TS-STRD-03 | Store crosses page, second page VS-stage no W | 4-byte store crosses page, second page VS-stage PTE W=0 | store page-fault (cause=15), stval=original VA |

---

### Group 17: G-stage PTE G Bit Ignored

**Specification Basis**:
- `norm:H_vm_gpa_g`: G bit in G-stage PTEs is currently unused, software should clear for forward compatibility, hardware must ignore

**Test Responsibility**: Verify G-stage PTE G bit being set does not affect translation behavior.

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| TS-GBIT-01 | G-stage PTE G=1 does not affect translation | G-stage PTE sets G=1, other permissions normal (V=1,R=1,W=1,X=1,U=1,A=1,D=1) | Two-stage translation succeeds normally, G bit ignored by hardware |

---

### Group 18: henvcfg.PBMTE Control over VS-stage

**Specification Basis**:
- `norm:henvcfg_pbmte_op`: When PBMTE=0, behaves as Svpbmt not implemented (VS-stage PTE PBMT field bit[62:61] is reserved, nonzero triggers exception); when PBMTE=1, Svpbmt available for VS-stage address translation

**Test Responsibility**: Verify PBMTE switch effect on VS-stage PTE PBMT field.

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| TS-PBMT-01 | PBMT nonzero triggers fault when PBMTE=0 | henvcfg.PBMTE=0, VS-stage PTE bit[62:61]=01 (NC) | page-fault (reserved bits illegal) |
| TS-PBMT-02 | PBMT=NC translates normally when PBMTE=1 | henvcfg.PBMTE=1, VS-stage PTE bit[62:61]=01 (NC), other permissions normal | Translation succeeds |

---

### Group 19: PMP Interaction with Two-Stage

**Specification Basis**:
- `norm:H_pmp`: Machine-level physical memory protection applies to supervisor physical addresses and is in effect regardless of virtualization mode.

**Test Responsibility**: Verify that after successful two-stage translation, final SPA is still subject to PMP constraints.

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| TS-PMP-01 | SPA falls in PMP denied region | Both two-stage translations succeed, final SPA configured as inaccessible by PMP | access-fault (cause=5 load / 7 store), not page-fault nor guest-page-fault |

---

### Group 20: Exception Priority

**Specification Basis**:
- `norm:H_exception_priority`: When multiple synchronous exceptions may trigger simultaneously, decreasing priority order determines which is reported

**Test Responsibility**: Verify exception priority correctness in two-stage translation scenarios.

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| TS-PRIO-01 | Misaligned + VS-stage fault priority | VS-mode loads from misaligned address, VS-stage PTE V=0 simultaneously | page-fault (cause=13) prioritized over misaligned (per spec priority table) |
| TS-PRIO-02 | G-stage implicit access interception prioritized over VS-stage PTE check | VS-stage root page table GPA marked invalid by G-stage + VS-stage PTE itself also illegal encoding | guest-page-fault (G-stage implicit root table read intercepted first, VS-stage PTE content not checked) |

---

### Group 21: hgatp Root Page Table Alignment and WARL Behavior

**Specification Basis**:
- `norm:hgatp_ppn_op`: G-stage root page table is 16 KiB and must be aligned to 16 KiB boundary; PPN lowest two bits always read as zero
- `norm:hgatp_mode_warl`: Writing unsupported MODE value is not ignored like satp, but handled as WARL

**Test Responsibility**: Verify hgatp CSR alignment constraints and WARL semantics.

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| TS-HGATP-01 | hgatp.PPN[1:0] forced to zero | Write hgatp.PPN lowest 2 bits as 1, read back | PPN[1:0] always reads as 0 (16 KiB alignment WARL enforced constraint, spec explicitly requires; non-compliance is FAIL) |
| TS-HGATP-02 | hgatp MODE WARL | Write hgatp.MODE with unsupported value (e.g., reserved encoding) | WARL behavior (field adjusted, no exception; different from vsatp's "ignore" semantics) |

---

### Group 22: Svinval Instruction Exception Behavior

**Specification Basis**:
- `norm:hstatus_vtvm_op` (original text): "When VTVM=1, an attempt in VS-mode to execute SFENCE.VMA or **SINVAL.VMA** or to access CSR `satp` raises a virtual-instruction exception."
- `norm:mstatus_tvm_hs` (original text): "Setting TVM=1 prevents HS-mode from accessing `hgatp` or executing HFENCE.GVMA or **HINVAL.GVMA**..."

**Test Responsibility**: Verify Svinval extension instruction exception triggering behavior in two-stage scenarios.

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| TS-SINV-01 | VS executes SINVAL.VMA when VTVM=1 | hstatus.VTVM=1, VS-mode executes `sinval.vma` | virtual-instruction exception (cause=22) |
| TS-SINV-02 | HS executes HINVAL.GVMA when TVM=1 | mstatus.TVM=1, HS-mode executes `hinval.gvma` | illegal-instruction exception (cause=2) |

---

### Group 23: Large Page Granularity Two-Stage Combinations

**Specification Basis**:
- `norm:H_vm_twostage`: During two-stage translation, VS-stage and G-stage leaf nodes can be any supported superpage granularity
- Maximum leaf node granularity supported by VS-stage determined by vsatp MODE: Sv39 max 1G (level 2), Sv48 max 512G (level 3), Sv57 max 256T (level 4)
- Maximum leaf node granularity supported by G-stage determined by hgatp MODE: Sv39x4 max 1G (level 2), Sv48x4 max 512G (level 3), Sv57x4 max 256T (level 4)
- Effective page size of two-stage translation takes the smaller of VS-stage and G-stage leaf node granularities

**Test Responsibility**: Verify two-stage translation correctness when VS-stage and G-stage use different large superpage granularities (1G/512G/256T), supplementing Group 3 which only covers up to 4K/2M (and partial 1G).

| Test ID | Test Name | VS Granularity | G Granularity | VS-stage | G-stage | Test Description | Expected Result |
|---------|-----------|----------------|---------------|----------|---------|------------------|-----------------|
| TS-LP-01 | VS=1G G=4K identity mapping | 1G | 4K | Sv39+ | Sv39x4+ | VS-stage 1GB superpage, G-stage 4KB per-page mapping | R/W success |
| TS-LP-02 | VS=1G G=2M identity mapping | 1G | 2M | Sv39+ | Sv39x4+ | VS-stage 1GB superpage, G-stage 2MB superpage | R/W success |
| TS-LP-03 | VS=1G G=1G identity mapping | 1G | 1G | Sv39+ | Sv39x4+ | Both stages 1GB superpage | R/W success |
| TS-LP-04 | VS=4K G=512G identity mapping | 4K | 512G | Sv39+ | Sv48x4+ | G-stage single 512GB superpage covering all physical memory | R/W success |
| TS-LP-05 | VS=2M G=512G identity mapping | 2M | 512G | Sv39+ | Sv48x4+ | VS-stage 2MB + G-stage 512GB superpage | R/W success |
| TS-LP-06 | VS=1G G=512G identity mapping | 1G | 512G | Sv39+ | Sv48x4+ | VS-stage 1GB + G-stage 512GB superpage | R/W success |
| TS-LP-07 | VS=4K G=256T identity mapping | 4K | 256T | Sv39+ | Sv57x4 | G-stage single 256TB superpage covering all physical memory | R/W success |
| TS-LP-08 | VS=2M G=256T identity mapping | 2M | 256T | Sv39+ | Sv57x4 | VS-stage 2MB + G-stage 256TB superpage | R/W success |
| TS-LP-09 | VS=512G G=4K identity mapping | 512G | 4K | Sv48+ | Sv39x4+ | VS-stage 512GB superpage, G-stage 4KB per-page mapping | R/W success |
| TS-LP-10 | VS=256T G=4K identity mapping | 256T | 4K | Sv57 | Sv39x4+ | VS-stage 256TB superpage, G-stage 4KB per-page mapping | R/W success |

> [!NOTE]
> - TS-LP-04~08 (G=512G/256T): G-stage establishes a single superpage identity mapping covering [0, 512G) or [0, 256T). Since the platform physical memory base addresses all fall within this range, the mapping is feasible.
> - TS-LP-09/10 (VS=512G/256T): VS-stage likewise establishes a single 512GB/256TB superpage identity mapping.
> - All test cases use identity mapping (VA=GPA=SPA), success criterion is VS-mode read/write to `test_data_area` returning correct values.
> - 512G/256T superpage requires corresponding hgatp/vsatp MODE support: 512G requires Sv48x4+ or Sv48+ vsatp; 256T requires Sv57x4 or Sv57 vsatp. Cases auto-SKIP when the corresponding mode is not supported.

---

### Group 24: VS-stage PPN Width (44-bit) Verification

**Specification Basis**:
- `norm:satp_ppn_sv39_sz` / `norm:satp_ppn_sv48_sz` / `norm:satp_ppn_sv57_sz`: Sv39/Sv48/Sv57 translate VPN into a **44-bit PPN**, i.e. the PPN field of a VS-stage (`vsatp`) leaf PTE is 44 bits wide, so the GPA produced by VS-stage translation can reach up to bit 55 (44+12)
- `norm:H_vm_twostage`: The GPA produced by VS-stage feeds G-stage translation; as long as the GPA falls within the addressable range of the G-stage MODE (Sv39x4 -> 41-bit, Sv48x4 -> 50-bit, Sv57x4 -> 59-bit GPA space), translation must complete normally

**Test Responsibility**: Verify that VS-stage PTEs retain the full 44-bit PPN and that high GPAs (>=2^41 / >=2^50 / bit55) complete two-stage translation successfully under a wide G-stage. Under single-stage translation the SPA is bounded by real physical memory so high-address scenarios cannot be constructed; two-stage translation can remap a high GPA back to real low-address memory via G-stage, making this property testable.

| Test ID | Test Name | VS-stage | G-stage | Test Description | Expected Result |
|---------|-----------|---------|--------|------------------|-----------------|
| TS-PPNW-01 | Sv39 VS-stage outputs GPA >= 2^41 | Sv39 | Sv48x4 | VS-stage maps test VA -> GPA=2^41 (PPN bit29, beyond the Sv39x4 41-bit boundary), G-stage remaps that GPA to a real low-address SPA, VS-mode R/W | R/W success (norm:satp_ppn_sv39_sz) |
| TS-PPNW-02 | Sv39 VS-stage outputs GPA bit55 | Sv39 | Sv57x4 | VS-stage maps test VA -> GPA=2^55 (PPN bit43, top bit of the 44-bit PPN), G-stage remaps to a real low-address SPA | R/W success (norm:satp_ppn_sv39_sz) |
| TS-PPNW-03 | Sv48 VS-stage outputs GPA >= 2^50 | Sv48 | Sv57x4 | VS-stage maps test VA -> GPA=2^50 (PPN bit38), G-stage remaps to a real low-address SPA | R/W success (norm:satp_ppn_sv48_sz) |
| TS-PPNW-04 | Sv57 VS-stage outputs GPA bit55 | Sv57 | Sv57x4 | VS-stage maps test VA -> GPA=2^55 (PPN bit43), G-stage remaps to a real low-address SPA | R/W success (norm:satp_ppn_sv57_sz) |

> [!NOTE]
> - The negative counterpart -- "a GPA produced by VS-stage beyond a narrow G-stage's addressable range (e.g. Sv48+Sv39x4 with GPA >= 2^41) must raise guest-page-fault (cause=21)" -- is already covered by Group 4's TS-XMODE-07/08/09; this group is the complementary success-path verification.
> - All chosen GPAs are below the corresponding G-stage GPA space limit (Sv48x4 -> 2^50, Sv57x4 -> 2^59) while above the narrower mode's limit, keeping each case both legal and discriminating.
> - High GPA regions do not correspond to any real memory; G-stage remaps them onto the physical page of the test data area. The tests verify translation-path correctness only, independent of platform memory size.
> - Cases are gated by the VS/G mode combination of the current test suite; they only run under matching combinations and auto-SKIP for other combinations.

---

### Group 25: hgatp=Bare Joint Behavior (G-stage Trivial Translation x VS-stage/Joint Mechanisms)

**Specification Basis**:
- `norm:hgatp_mode_bare_trans`: When `hgatp`.MODE=Bare, guest physical addresses equal supervisor physical addresses without modification, and no memory protection applies in the trivial translation (a guest-page-fault can never occur)
- `norm:H_vm_twostage`: With V=1 either stage can be "effectively disabled" by setting its ATP to Bare; with an active VS-stage plus Bare G-stage, faults still originate from the VS-stage (cause 12/13/15)
- `norm:H_vm_gpatrans`: guest-page-faults are only produced by G-stage translation failures -- unreachable when the G-stage is Bare
- `norm:hlsv_op`: HLV/HLVX/HSV always perform two-stage accesses with V=1 and nominal privilege `hstatus.SPVP`; with a Bare G-stage they likewise pass through the trivial translation
- `norm:mstatus_mprv_hypervisor`: With MPRV=1 explicit accesses are translated per MPV/MPP; with a Bare G-stage they likewise pass through
- `norm:hfence-gvma_mode`: After `hgatp`.MODE changes, an HFENCE.GVMA with rs1=x0 must be executed to order subsequent guest translations -- even if the old or new MODE is Bare
- `norm:sstatus_sum` / `norm:sstatus_mxr`: SUM/MXR only take effect when page-based virtual memory is active; with a Bare G-stage they have no G-stage effect

**Test Responsibility**: With an active VS-stage or joint mechanisms (HLV/HSV, MPRV+MPV, HFENCE.GVMA) involved, verify the trivial-translation semantics of a Bare G-stage: VS-stage fault-code discrimination, pass-through behavior, mode-switch ordering, and the absence of G-stage SUM/MXR effects.

| Test ID | Test Name | Test Description | Expected Result |
|---------|-----------|------------------|-----------------|
| TS-BARE-01 | VS-stage fault code discrimination | Enable VS-stage (current suite mode) + hgatp=Bare, target VS-stage page PTE V=0, VS-mode load | cause=13 (load page-fault), and assert cause is NOT in {20,21,23} (a Bare G-stage can never produce a guest-page-fault) |
| TS-BARE-02 | HLV/HSV pass-through | Both stages Bare, after enabling the two-stage configuration HS-mode executes an HLV read and an HSV write on the test data area | No trap; read-back value matches the written value (GPA=SPA under the trivial G-stage translation) |
| TS-BARE-03 | MPRV+MPV pass-through | Both stages Bare, M-mode sets MPV=1 and MPP=VS, then performs ld/sd inside an MPRV=1 window | No trap; access passes through as a V=1 trivial translation, value correct |
| TS-BARE-04 | Bare->Sv*x4 switch | Switch hgatp from Bare to the current suite's G-stage mode (identity G-stage page tables pre-built), execute a global HFENCE.GVMA flush after the switch, VS-mode R/W | Access succeeds (`norm:hfence-gvma_mode`: a MODE change must be ordered by HFENCE.GVMA, new MODE takes effect) |
| TS-BARE-05 | Sv*x4->Bare switch | First access successfully under the current suite's G-stage mode, then switch back to Bare + global HFENCE.GVMA flush, VS-mode accesses the same GPA | Pass-through succeeds, no residual G-stage translation interference |
| TS-BARE-06 | SUM/MXR ineffective under dual Bare | Both stages Bare, set `vsstatus.SUM`=1 and `vsstatus.MXR`=1, then VS-mode accesses a PMP-allowed address | Access succeeds, behavior identical to SUM/MXR=0 (SUM/MXR have no effect without page-based translation) |

> [!NOTE]
> - This group complements Group 1 (TS-VS, hgatp=Bare baseline): Group 1 verifies VS-stage translation parity under a Bare G-stage, while this group verifies the trivial-translation semantics of the Bare G-stage itself and its intersections with joint mechanisms.
> - Pure G-stage Bare standalone behavior (pass-through/fetch/VU/PMP fallback/htval/GVA) is covered by Group 14 (GBARE-01~05) of the G-stage test plan.
> - Dual-Bare scenarios need no page tables at all; case setup only performs state resets and CSR programming.

---

## Test Priority

| Priority | Test Groups | Covered Test IDs | Rationale |
|----------|-------------|------------------|-----------|
| P0 (Must) | Group 1, Group 3, Group 6, Group 7, Group 15 | TS-VS-01~10, TS-MAP-01~12, TS-IMPL-01~06, TS-PERM-01~12, TS-AD-01~06 | VS-stage baseline + same-width two-stage + implicit G-stage fault + permission intersection + A/D bit handling |
| P1 (Important) | Group 8, Group 9, Group 10, Group 11, Group 13, Group 16, Group 19, Group 25 | TS-MXR-01~05, TS-SUM-01~03, TS-HV-01~06, TS-HG-01~06, TS-HLV-01~12, TS-STRD-01~03, TS-PMP-01~02, TS-BARE-01~06 | MXR/SUM dual-stage semantics, HFENCE flush, HLV/HSV, page boundary straddle, PMP interaction, G-stage Bare trivial translation joint behavior |
| P2 (Recommended) | Group 2, Group 5, Group 12, Group 14, Group 17, Group 18, Group 20, Group 21, Group 22, Group 24 | TS-VSATP-01~07, TS-NID-01~04, TS-SF-01~04, TS-MPRV-01~05, TS-GBIT-01, TS-PBMT-01~02, TS-PRIO-01~02, TS-HGATP-01~02, TS-SINV-01~02, TS-PPNW-01~04 | vsatp CSR, non-identity mapping, SFENCE V=1, MPRV, G bit, PBMTE, exception priority, hgatp WARL, Svinval exceptions, VS-stage 44-bit PPN width verification |
| P3 (Optional) | Group 4, Group 23 | TS-XMODE-01~09, TS-LP-01~10 | Cross-width combinations + large page granularity combinations (compatibility verification) |

---

## Test Design Key Points

1. **fault cause distinguishes stage source**: VS-stage failure → cause 12/13/15 (regular page-fault); G-stage failure → cause 20/21/23 (guest-page-fault). Test assertions must use accurate cause constants; no fuzzy handling.

2. **htval dual meaning** (`norm:htval_trapval`): explicit access fault: htval = original GPA >> 2, corresponding to the same access as stval; implicit VS-stage access fault: htval = VS-level PTE's GPA >> 2, not corresponding to the same address as stval; whether the access is implicit can be determined via htinst.

3. **htinst pseudoinstruction encoding** (`norm:H_trap_xtinst_guestpage_rw`): RV64 implicit read is `0x00003000`, implicit write (A/D auto-update) is `0x00003020`; when mtval2/htval is nonzero and the access is an implicit VS-stage access, a pseudoinst **must** be written (0 not allowed).

4. **MXR dual semantics** (`norm:vsstatus_mxr_vm`, `norm:sstatus_mxr_vm`): HS-level `sstatus.MXR` affects both VS-stage and G-stage; `vsstatus.MXR` affects only VS-stage. Tests should set/clear the two MXR fields separately to verify the difference.

5. **SUM only affects VS-stage**: `vsstatus.SUM` controls VS-stage U-bit checking; G-stage is always from the U-mode perspective with no SUM concept; during HLV/HSV the HS-level `sstatus.SUM` is ignored (`norm:hlsv_trans`).

6. **VMID switch ordering**: when switching VMID, strictly follow the order vsatp=0 → write hgatp → write vsatp, to avoid speculative execution polluting TLB tags.

7. **MPRV+MPV enable-window safety**: after M-mode sets MPRV=1, all load/stores (including stack accesses) are translated; MPRV must be enabled within a minimal window and cleared immediately after the target access completes (see Group 14 notes for details).

8. **Platform capability probing**: 512GB/256TB large superpage tests are limited by platform physical memory size; some implementations may not support modes such as Sv57/Sv57x4; mode support should be probed before case execution, auto-SKIP when unsupported.

## Combination Coverage Strategy

- Two-stage test cases are run in independent test suites per the (G-mode, VS-mode) Cartesian product (9 combinations in total); each suite independently tests one combination and fully traverses all page granularities supported under that combination; cases not matching the current suite's combination auto-SKIP.
- Granularity matrix cases traverse the VS x G page-granularity Cartesian product (the ranges supported by each mode among 4K/2M/1G/512G/256T): the VS granularity set is determined by vsatp MODE (Sv39 → {4K, 2M, 1G}; Sv48 → {4K, 2M, 1G, 512G}; Sv57 → {4K, 2M, 1G, 512G, 256T}), and the G granularity set is likewise determined by hgatp MODE; granularity combinations beyond the support boundaries of the current combination auto-SKIP.
- Test suites for pure G-stage independent translation are covered by `Hypervisor_gstage_test_plan.md` and do not carry two-stage tests.

---

## References

- `hypervisor.adoc` — RISC-V Hypervisor Extension, Version 1.0
- `supervisor.adoc` — Sv39/Sv48/Sv57 address translation and `sstatus` field definitions
- `Hypervisor_gstage_test_plan.md` — G-stage independent translation test plan (companion)
- `vm_test_plan.md` — VS-stage / regular VM test plan (behavior baseline)

---

## Appendix A: Specification Point Coverage Matrix

| Norm ID | Covered Test Cases | Notes |
|---------|--------------------|-------|
| `norm:H_vm_twostage` | TS-VS-01~10, TS-MAP-01~12, all TS-XMODE, TS-NID-01~04, TS-LP-01~10, granularity matrix cases, TS-BARE-01 | V=1 two-stage chain and single-stage degeneration |
| `norm:H_vm_gstagetrans` | TS-IMPL-01/03/04/06, TS-PRIO-02 | VS-stage implicit accesses subject to G-stage translation |
| `norm:H_vm_gpatrans` | TS-MAP, TS-PERM-03/05/08/09, TS-BARE-01 | G-stage algorithm and guest-page-fault source |
| `norm:H_vm_gpapriv` | TS-IMPL-04, TS-AD-03~06, TS-PERM-09 | Implicit accesses checked as implicit load/store |
| `norm:vsstatus_mxr_vm` | TS-MXR-02/03, TS-HLV-08 | vsstatus.MXR only overrides VS-stage |
| `norm:sstatus_mxr_vm` | TS-MXR-04/05, TS-HLV-06/07 | HS-level MXR overrides both stages |
| `norm:vsatp_sz_acc_op` | TS-VSATP-01/02, TS-VS-01~10 | satp accesses vsatp when V=1 |
| `norm:vsatp_v0` | TS-VSATP-04, TS-HLV-01/02, TS-MPRV-02/03 | vsatp only effective via HLV/HSV/MPRV when V=0 |
| `norm:vsatp_mode_unsupported_v0` | TS-VSATP-04 | Writing unsupported MODE at V=0 ignored or WARL |
| `norm:vsatp_mode_unsupported_v1` | TS-VSATP-03 | Writing unsupported MODE at V=1 ignored |
| `norm:vs_stage_speculative_a_bit` | No direct case | See untestable notes at the end |
| `norm:hlsv_mode` | TS-HLV-10~12 | HLV/HSV effective modes and HU control |
| `norm:hlsv_priv` | TS-HLV-03/04 | SPVP determines effective privilege VS/VU |
| `norm:hlsv_trans` | TS-HLV-01/02/05 | HLV/HSV two-stage translation and HS-level SUM ignored |
| `norm:hlsv_sstatus_mxr` | TS-HLV-06/07 | HS-level MXR effective for HLV two-stage |
| `norm:hlsv_vsstatus_mxr` | TS-HLV-08 | vsstatus.MXR only affects HLV's VS-stage |
| `norm:hlsv_u_op` | TS-HLV-09 | HLVX substitutes X permission for R permission |
| `norm:hlsv_virtinst` | TS-HLV-10 | Executing HLV/HSV at V=1 -> virtual-instruction |
| `norm:hlsv_illegalinst` | TS-HLV-11 | U-mode + HU=0 -> illegal-instruction |
| `norm:hlsv_op` | TS-HLV-13/14 | Read/write and extension semantics of width variants |
| `norm:hfence-vvma_hfence-gvma_op` | TS-HV-01~03, TS-HG-01~03 | HFENCE semantics similar to SFENCE.VMA |
| `norm:hfence-vvma_mode` | TS-HV-01~03 | HFENCE.VVMA ordering guarantees and valid modes |
| `norm:hfence-vvma_limits` | TS-HV-02/03 | rs1/rs2 select VA/ASID and VMID limits |
| `norm:hfence-vvma_asid` | TS-HV-03 | ASID high-bit ignore rules |
| `norm:hfence-vvma_tvm` | TS-HV-04/05 | TVM/VTVM do not affect HFENCE.VVMA |
| `norm:hfence-gvma_op` | TS-HG-01~03 | HFENCE.GVMA ordering guarantees and rs1=GPA>>2 |
| `norm:hfence-gvma_mode` | TS-HG-04, TS-BARE-04/05 | HFENCE.GVMA required after MODE change (including Bare) |
| `norm:hfence-gvma_vmid` | TS-HG-03 | rs2 selects VMID and high-bit ignore |
| `norm:hfence-vvma_hfence-gvma_exceptions` | TS-HV-06, TS-HG-05/06 | Exception triggering for V=1/U-mode/TVM |
| `norm:sfence_vma_v0` | TS-SF-04 | V=0 SFENCE.VMA only flushes HS level |
| `norm:sfence_vma_v1` | TS-SF-01/02 | V=1 SFENCE.VMA only flushes VS-stage |
| `norm:mstatus_mprv_hypervisor` | TS-MPRV-01~04, TS-BARE-03 | Two-stage behavior of MPRV+MPV/MPP combinations |
| `norm:mstatus_mprv_hlsv` | TS-MPRV-05 | MPRV does not affect HLV/HSV |
| `norm:H_guest_page_fault` | TS-IMPL, TS-PERM-03/05/08/09, TS-AD-03~06, TS-XMODE-07~09 | guest-page-fault delegation and trap value writes |
| `norm:htval_trapval` | TS-IMPL-01, TS-STRD-01/02 | htval written with GPA>>2 or 0 |
| `norm:H_trap_xtinst_guestpage` | TS-IMPL-01/06, TS-AD-04 | Implicit access + nonzero htval must write pseudoinst |
| `norm:H_trap_xtinst_guestpage_rw` | TS-IMPL-01/06 (read), TS-AD-04 (write) | read/write pseudoinstruction encoding distinction |
| `norm:henvcfg_adue_op` | TS-AD-01~04 | ADUE controls VS-stage A/D hardware update |
| `norm:henvcfg_pbmte_op` | TS-PBMT-01/02 | PBMTE controls VS-stage Svpbmt availability |
| `norm:H_straddle` | TS-STRD-01~03 | Cross-page access faults and stval page-boundary address |
| `norm:mtval2_htval_virtaddr` | TS-STRD-01/02, TS-XMODE-07~09 | htval corresponds to stval for non-implicit faults |
| `norm:mtval2_trapval_other` | TS-STRD-01/02 | Faulting portion address for misaligned/straddle faults |
| `norm:H_vm_gpa_g` | TS-GBIT-01 | G-stage PTE G bit ignored |
| `norm:H_pmp` | TS-PMP-01 | SPA still subject to PMP after two-stage translation |
| `norm:hgatp_mode_bare_trans` | TS-VS-01~10, TS-BARE-01~06 | hgatp=Bare trivial translation and joint behavior |
| `norm:H_exception_priority` | TS-PRIO-01/02 | Multi-exception priority verification |
| `norm:hgatp_ppn_op` | TS-HGATP-01 | PPN[1:0] forced read-zero (16KB alignment) |
| `norm:hgatp_mode_warl` | TS-HGATP-02 | Unsupported MODE handled per WARL |
| `norm:satp_ppn_sv39_sz` | TS-PPNW-01/02 | Sv39 VS-stage outputs 44-bit PPN |
| `norm:satp_ppn_sv48_sz` | TS-PPNW-03 | Sv48 VS-stage outputs 44-bit PPN |
| `norm:satp_ppn_sv57_sz` | TS-PPNW-04 | Sv57 VS-stage outputs 44-bit PPN |
| `norm:hgatp_mode_sv39x4` | TS-MAP-01~04, TS-XMODE-01/02/03/05/07/08, GH series two-stage combinations | Sv39x4 GPA width and must-be-zero high-bit check |
| `norm:hgatp_mode_sv48x4` | TS-MAP-05~08, TS-XMODE-01/04/06/09 | Sv48x4 GPA width and must-be-zero high-bit check |
| `norm:hgatp_mode_sv57x4` | TS-MAP-09~11, TS-XMODE-02/04/06 | Sv57x4 GPA width and must-be-zero high-bit check |
| `norm:hstatus_vtvm_op` | TS-VSATP-07, TS-SF-03, TS-SINV-01 | VS-mode exception triggering when VTVM=1 |
| `norm:mstatus_tvm_hs` | TS-HG-05, TS-SINV-02 | TVM=1 prevents HS-mode access to hgatp/HFENCE.GVMA |
| `norm:hlvx-wu_valid32` | TS-HLV-09 (partial) | See untestable notes at the end |
| `norm:sstatus_sum` | TS-SUM-01~03, TS-BARE-06 | SUM semantics (verified at VS-stage via vsstatus mirror) |
| `norm:sstatus_mxr` | TS-MXR-01~05 | MXR basic semantics (HS level and VS level) |

Notes on uncovered/untestable items:

- `norm:vs_stage_speculative_a_bit`: The requirement that the VS-stage A bit must not be set by speculative execution is a microarchitectural behavior difficult to observe directly, and cannot be observably verified via functional cases; this plan contains no direct verification case and only declares the constraint here.
- `norm:hlvx-wu_valid32`: The current repository is RV64, and its RV32 validity portion is untestable; TS-HLV-09 covers the semantics of HLVX.WU substituting execute permission for read permission.
