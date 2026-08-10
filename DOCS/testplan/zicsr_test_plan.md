**中文 | [English](../testplan_en/zicsr_test_plan_en.md)**

# Zicsr 扩展测试计划（CSR 指令）

## 概述

本测试计划覆盖 RISC-V Zicsr 扩展定义的 CSR 指令集行为，包括六条 CSR 指令（csrrw/csrrs/csrrc/csrrwi/csrrsi/csrrci）的原子读-改-写语义、x0 操作数与零立即数的读写条件、对只读 CSR 的访问规则、副作用触发条件、汇编伪指令编码、指令级访问权限与异常，以及 CSR 访问排序语义。各具体 CSR 的寄存器字段行为（WARL/WLRL、字段语义）不在本计划范围内，由特权级 CSR 专项测试计划覆盖。

本测试计划依据 `SPEC/riscv-isa-manual/src/unpriv/zicsr.adoc` 中的规范点（norm 标记）编写。

### 本文档覆盖的 SPEC 章节
- CSR Instructions（csrrw/csrrs/csrrc 及立即数形式 csrrwi/csrrsi/csrrci 的操作语义）
- CSR 读写条件表（Table: Conditions determining whether a CSR instruction reads or writes the specified CSR）
- CSR 读写副作用与字段级副作用规则
- CSR 被指令执行隐式修改时的显式读/写语义（instret 等）
- 汇编伪指令（csrr/csrw/csrwi/csrs/csrc/csrsi/csrci）
- CSR Access Ordering（程序序、读返回前值、写覆盖隐式更新、副作用同步、弱序/强序）

### 由其他测试计划覆盖
- Machine 级 CSR 寄存器字段行为（mstatus/misa/mideleg/mepc 等 WARL/WLRL 约束） → `Sm_CSR_test_plan.md`
- Supervisor 级 CSR 寄存器字段行为（sstatus/satp 等） → `Ss_CSR_test_plan.md`
- Hypervisor CSR 与 VS CSR 字段行为 → `Hypervisor_CSR_test_plan.md`
- 计数器 CSR（cycle/time/instret 及其高位寄存器）的 Zicntr/Zihpm 行为 → `zicntr_test_plan.md`
- FENCE 指令本身的内存序语义 → `zifencei_test_plan.md`
- 浮点状态 CSR（fcsr/frm/fflags）字段行为 → FP 相关测试计划

---

## 覆盖的规范点

本章节列出本文档所有测试组中引用的规范点（norm ID），已去重并按字母顺序排列。

| Norm ID | 原文 | 中文说明 |
|---------|------|----------|
| `norm:csr_access_order` | A CSR access is performed after the execution of any prior instructions in program order whose behavior modifies or is modified by the CSR state and before the execution of any subsequent instructions in program order whose behavior modifies or is modified by the CSR state. | CSR 访问在程序序上位于所有行为受该 CSR 状态影响或影响该 CSR 状态的先前指令之后、后续指令之前执行。 |
| `norm:csr_fence_ordering` | To enforce ordering in all other cases, software should execute a fence instruction between the relevant accesses. For the purposes of the fence instruction, CSR read accesses are classified as device input (I), and CSR write accesses are classified as device output (O). | 需要排序时软件应在相关访问间插入 fence；对 fence 而言 CSR 读访问归类为设备输入 (I)，CSR 写访问归类为设备输出 (O)。 |
| `norm:csr_memory_access_ordering` | CSR accesses are not ordered with respect to explicit memory accesses, unless a CSR access modifies the execution behavior of the instruction that performs the explicit memory access or unless a CSR access and an explicit memory access are ordered by either the syntactic dependencies defined by the memory model or the ordering requirements defined in the PMA chapter. | CSR 访问与显式内存访问之间默认无顺序约束，除非 CSR 访问改变该内存访问指令的执行行为，或两者被内存模型语法依赖/PMA 排序要求约束。 |
| `norm:csr_rs1_uimm_side_effect` | csrrs, csrrsi, csrrc, and csrrci only action side effects for fields for which the rs1 or uimm argument has at least one bit set corresponding to that field. | csrrs/csrrsi/csrrc/csrrci 仅对 rs1 或 uimm 中至少有一位对应该字段的字段触发写副作用。 |
| `norm:csr_side_effects` | The table summarizes the behavior of the CSR instructions with respect to whether they read and/or write the CSR. | 读写条件表总结了 CSR 指令是否读和/或写 CSR 的行为。 |
| `norm:csr_side_effects_synchronous` | Any side effects from an explicit CSR access are normally observed to occur synchronously in program order. Unless specified otherwise, the full consequences of any such side effects are observable by the very next instruction, and no consequences may be observed out-of-order by preceding instructions. | 显式 CSR 访问的副作用通常按程序序同步发生；除非另有说明，其全部后果对紧随其后的下一条指令可观察，且不得被先前指令乱序观察到。 |
| `norm:csr_strongly_ordered` | The hardware platform may define that accesses to certain CSRs are strongly ordered. Accesses to strongly ordered CSRs have stronger ordering constraints with respect to accesses to both weakly ordered CSRs and accesses to memory-mapped I/O regions. | 硬件平台可定义某些 CSR 的访问为强序；强序 CSR 相对弱序 CSR 与 MMIO 区域访问具有更强的排序约束。 |
| `norm:csr_weakly_ordered` | For the RVWMO memory consistency model, CSR accesses are weakly ordered by default, so other harts or devices may observe CSR accesses in an order different from program order. | 在 RVWMO 内存一致性模型下，CSR 访问默认弱序，其他 hart 或设备可能观察到与程序序不同的 CSR 访问顺序。 |
| `norm:csrr_order` | An explicit CSR read returns the CSR state before the execution of the instruction. | 显式 CSR 读返回该指令执行前的 CSR 状态。 |
| `norm:csrrc_op` | The csrrc (Atomic Read and Clear Bits in CSR) instruction reads the value of the CSR, zero-extends the value to XLEN bits, and writes it to integer register rd. The initial value in integer register rs1 is treated as a bit mask that specifies bit positions to be cleared in the CSR. Any bit that is high in rs1 will cause the corresponding bit to be cleared in the CSR, if that CSR bit is writable. | csrrc 原子读并清位：CSR 旧值零扩展写入 rd；rs1 作为位掩码，为 1 的位对应的 CSR 可写位被清零。 |
| `norm:csrrs_csrrc_rs1_x0` | For both csrrs and csrrc, if rs1=x0, then the instruction will not write to the CSR at all, and so shall not cause any of the side effects that might otherwise occur on a CSR write, nor raise illegal-instruction exceptions on accesses to read-only CSRs. Both csrrs and csrrc always read the addressed CSR and cause any read side effects regardless of rs1 and rd fields. Note that if rs1 specifies a register other than x0, and that register holds a zero value, the instruction will not action any attendant per-field side effects, but will action any side effects caused by writing to the entire CSR. | csrrs/csrrc 当 rs1=x0 时完全不写 CSR，不触发写副作用，也不因访问只读 CSR 触发非法指令异常；两条指令总是读 CSR 并触发读副作用，与 rs1/rd 无关。若 rs1 为非 x0 但持有零值，则不触发逐字段副作用，但仍触发整 CSR 写副作用。 |
| `norm:csrrs_op` | The csrrs (Atomic Read and Set Bits in CSR) instruction reads the value of the CSR, zero-extends the value to XLEN bits, and writes it to integer register rd. The initial value in integer register rs1 is treated as a bit mask that specifies bit positions to be set in the CSR. Any bit that is high in rs1 will cause the corresponding bit to be set in the CSR, if that CSR bit is writable. | csrrs 原子读并置位：CSR 旧值零扩展写入 rd；rs1 作为位掩码，为 1 的位对应的 CSR 可写位被置 1。 |
| `norm:csrrw_op` | The csrrw (Atomic Read/Write CSR) instruction atomically swaps values in the CSRs and integer registers. csrrw reads the old value of the CSR, zero-extends the value to XLEN bits, then writes it to integer register rd. The initial value in rs1 is written to the CSR. If rd=x0, then the instruction shall not read the CSR and shall not cause any of the side effects that might occur on a CSR read. | csrrw 原子交换：CSR 旧值零扩展写入 rd，rs1 值写入 CSR；rd=x0 时不读 CSR 且不触发读副作用。 |
| `norm:csrrw_rs1_x0` | A csrrw with rs1=x0 will attempt to write zero to the destination CSR. | csrrw 当 rs1=x0 时尝试向目标 CSR 写零。 |
| `norm:csrrwi_csrrsi_csrrci_ops` | The csrrwi, csrrsi, and csrrci variants are similar to csrrw, csrrs, and csrrc respectively, except they update the CSR using an XLEN-bit value obtained by zero-extending a 5-bit unsigned immediate (uimm[4:0]) field encoded in the rs1 field instead of a value from an integer register. For csrrsi and csrrci, if the uimm[4:0] field is zero, then these instructions will not write to the CSR, and shall not cause any of the side effects that might otherwise occur on a CSR write, nor raise illegal-instruction exceptions on accesses to read-only CSRs. For csrrwi, if rd=x0, then the instruction shall not read the CSR and shall not cause any of the side effects that might occur on a CSR read. Both csrrsi and csrrci will always read the CSR and cause any read side effects regardless of rd and rs1 fields. | csrrwi/csrrsi/csrrci 与 csrrw/csrrs/csrrc 类似，但用 rs1 字段编码的 5 位无符号立即数零扩展后更新 CSR。csrrsi/csrrci 当 uimm=0 时不写 CSR、不触发写副作用、不因访问只读 CSR 触发异常；csrrwi 当 rd=x0 时不读 CSR 不触发读副作用；csrrsi/csrrci 总是读 CSR。 |
| `norm:csrw_order` | An explicit CSR write suppresses and overrides any implicit writes or modifications to the same CSR by the same instruction. | 显式 CSR 写会抑制并覆盖同一指令对该 CSR 的任何隐式写或修改。 |

> 说明：本章还定义了 CSR 指令的编码格式（12 位 csr 域位于 bits 31-20，立即数形式使用 rs1 字段编码的 5 位零扩展立即数）、读-改-写的回写语义（csrrs/csrrc 将读到的值整体回写，可能改变"读值与底层值不同"的位）、以及汇编伪指令编码（csrr→csrrs rd,csr,x0；csrw→csrrw x0,csr,rs1；csrwi→csrrwi x0,csr,uimm；csrs/csrc/csrsi/csrci）。这些规范点未标注 norm 锚点，分别由 Group 1、Group 4 的用例覆盖。

---

## Group 1. CSR 指令基本操作

**规范依据**：
- `norm:csrrw_op`：csrrw 原子交换 CSR 与整数寄存器值
- `norm:csrrw_rs1_x0`：csrrw 当 rs1=x0 时写零
- `norm:csrrs_op`：csrrs 按 rs1 位掩码置位
- `norm:csrrc_op`：csrrc 按 rs1 位掩码清位
- 编码格式规范：CSR 地址编码于指令 bits 31-20；CSR 旧值零扩展到 XLEN 后写 rd
- 读-改-写回写语义：csrrs/csrrc 将读到的值整体回写，读值与底层值不同的位可能被更新

**测试职责**：验证六条 CSR 指令中寄存器形式三条的基本原子读-改-写语义、零扩展行为与读-改-写回写特性。测试载体使用 mscratch/sscratch（读写 CSR）。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZCSR-01 | csrrw 原子交换基本功能 | M-mode 预置 mscratch=A，执行 csrrw rd, mscratch, rs1(B)，再用 csrr 读 mscratch | rd=A（旧值），mscratch=B（新值） |
| ZCSR-02 | csrrw rd=x0 不读 CSR 但写入生效 | 预置 mscratch=A，执行 csrrw x0, mscratch, rs1(B)，读 mscratch | mscratch=B，无任何读副作用 |
| ZCSR-03 | csrrw rs1=x0 写零 | 预置 mscratch=A（非零），执行 csrrw rd, mscratch, x0 | rd=A，mscratch=0 |
| ZCSR-04 | csrrs 置位且未选择位不变 | 预置 mscratch=V，csrrs rd, mscratch, rs1(mask) | rd=V；mscratch=(V \| mask)（限可写位），mask 为 0 的位不受影响 |
| ZCSR-05 | csrrc 清位且未选择位不变 | 预置 mscratch=V，csrrc rd, mscratch, rs1(mask) | rd=V；mscratch=(V & ~mask)（限可写位），mask 为 0 的位不受影响 |
| ZCSR-06 | csrrs/csrrc 读-改-写回写语义 | 选择一个读值与底层值可能不同的 CSR 位（如 pmpaddr 粒度位）或模拟等价场景，用 csrrc/csrrs 修改其他位后检查该位底层值 | 被读出的值整体回写，读值与底层值不同的位以读值为准更新（SPEC NOTE 语义） |
| ZCSR-07 | CSR 旧值零扩展到 XLEN | RV64 下 csrrw/csrrs 读窄有效域 CSR（如低 32 位有效的 CSR），检查 rd 高 32 位 | rd 高 32 位为零扩展（全 0），非符号扩展 |
| ZCSR-08 | sscratch 上 csrrw/csrrs/csrrc 一致性 | S-mode 重复 Group 1 核心场景于 sscratch | 行为与 M-mode/mscratch 一致 |
| ZCSR-09 | csrrs 非 x0 寄存器持零值时整 CSR 写语义 | 预置 mscratch=V，用持有 0 的非 x0 寄存器执行 csrrs rd, mscratch, rs1 | rd=V；无逐字段副作用，但整 CSR 写副作用按 SPEC 应触发（通过可观察行为验证） |

---

## Group 2. CSR 立即数形式指令

**规范依据**：
- `norm:csrrwi_csrrsi_csrrci_ops`：立即数形式用 rs1 字段编码的 5 位无符号立即数零扩展后更新 CSR；csrrsi/csrrci 当 uimm=0 时不写 CSR；csrrwi 当 rd=x0 时不读 CSR；csrrsi/csrrci 总是读 CSR
- `norm:csr_side_effects`：读写条件表立即数操作数部分

**测试职责**：验证三条立即数形式 CSR 指令的立即数零扩展语义及与寄存器形式的等价性，uimm=0 与 rd=x0 的特殊行为。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZIMM-01 | csrrwi 写立即数并返回旧值 | 预置 mscratch=A，csrrwi rd, mscratch, 0x1F | rd=A，mscratch=0x1F |
| ZIMM-02 | csrrwi 立即数零扩展 | csrrwi rd, mscratch, 0x1F，读回 mscratch | mscratch=0x000000000000001F（5 位立即数零扩展到 XLEN，高位全 0） |
| ZIMM-03 | csrrsi 置位立即数 | 预置 mscratch=V，csrrsi rd, mscratch, uimm | rd=V；mscratch=(V \| zext(uimm))，未覆盖位不变 |
| ZIMM-04 | csrrci 清位立即数 | 预置 mscratch=V，csrrci rd, mscratch, uimm | rd=V；mscratch=(V & ~zext(uimm))，未覆盖位不变 |
| ZIMM-05 | csrrsi uimm=0 不写 CSR | 对只读 CSR（mvendorid）执行 csrrsi rd, csr, 0 | 不触发异常，读到正确值，CSR 值不变 |
| ZIMM-06 | csrrci uimm=0 不写 CSR | 对只读 CSR（mvendorid）执行 csrrci rd, csr, 0 | 不触发异常，读到正确值，CSR 值不变 |
| ZIMM-07 | csrrwi uimm=0 仍写零 | 预置 mscratch=A，csrrwi rd, mscratch, 0 | rd=A，mscratch=0（区别于 csrrsi/csrrci 的 uimm=0 语义） |
| ZIMM-08 | csrrwi rd=x0 不读 CSR 但写入生效 | 预置 mscratch=A，csrrwi x0, mscratch, uimm，读 mscratch | mscratch=uimm，无读副作用 |
| ZIMM-09 | csrrsi/csrrci 总是读 CSR | 无论 rd/uimm 取值，对可读 CSR 执行 csrrsi/csrrci | rd 始终返回 CSR 当前值 |
| ZIMM-10 | 立即数形式与寄存器形式等价性 | 同一 CSR 上分别用 csrrs/csrrsi（相同 5 位掩码）与 csrrc/csrrci 执行并比较结果 | 两种形式结果完全一致 |

---

## Group 3. CSR 读写条件与副作用

**规范依据**：
- `norm:csr_side_effects`：读写条件表（rd=x0/rs1=x0/uimm=0 组合下是否读写 CSR）
- `norm:csrrw_op`：csrrw rd=x0 时不读 CSR、不触发读副作用
- `norm:csrrs_csrrc_rs1_x0`：csrrs/csrrc rs1=x0 时不写 CSR、不触发写副作用、不因只读 CSR 触发异常
- `norm:csrrwi_csrrsi_csrrci_ops`：立即数形式对应的读写条件
- `norm:csr_rs1_uimm_side_effect`：csrrs/csrrsi/csrrc/csrrci 仅对掩码覆盖的字段触发副作用

**测试职责**：按 SPEC 读写条件表逐项验证各操作数组合下指令是否真正读/写 CSR，以及对只读 CSR 的访问边界。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZSIDE-01 | csrrw rd=x0 写只读 CSR 仍触发异常 | M-mode 执行 csrrw x0, mvendorid, rs1 | illegal-instruction exception（写仍然执行，只读 CSR 写触发异常） |
| ZSIDE-02 | csrrw rd≠x0 读只读 CSR 合法 | M-mode 执行 csrrw rd, mvendorid, rs1 | 读旧值合法，随后写触发 illegal-instruction exception（cause=2，tval 为该指令编码） |
| ZSIDE-03 | csrrs rs1=x0 读只读 CSR 不触发异常 | M-mode 执行 csrrs rd, mvendorid, x0 | 不触发异常，rd=mvendorid 当前值（纯读，无写） |
| ZSIDE-04 | csrrc rs1=x0 读只读 CSR 不触发异常 | M-mode 执行 csrrc rd, cycle, x0（若 Zicntr 实现，否则回退 mvendorid） | 不触发异常，读到 cycle 值 |
| ZSIDE-05 | csrrs rs1≠x0 写只读 CSR 触发异常 | M-mode 执行 csrrs rd, mvendorid, rs1(非零) | illegal-instruction exception（cause=2） |
| ZSIDE-06 | csrrc rs1≠x0 写只读 CSR 触发异常 | M-mode 执行 csrrc rd, mvendorid, rs1(非零) | illegal-instruction exception（cause=2） |
| ZSIDE-07 | csrrwi rd=x0 写只读 CSR 仍触发异常 | csrrwi x0, mvendorid, uimm | illegal-instruction exception（写仍然执行） |
| ZSIDE-08 | csrrsi uimm=0 读只读 CSR 不触发异常 | csrrsi rd, mvendorid, 0 | 不触发异常，rd=mvendorid 当前值 |
| ZSIDE-09 | csrrci uimm=0 读只读 CSR 不触发异常 | csrrci rd, mvendorid, 0 | 不触发异常，rd=mvendorid 当前值 |
| ZSIDE-10 | csrrsi uimm≠0 写只读 CSR 触发异常 | csrrsi rd, mvendorid, 非零 uimm | illegal-instruction exception（cause=2） |
| ZSIDE-11 | 读写条件表逐项核对 | 按 SPEC 表逐一执行 8 种操作数组合（寄存器 4 种 + 立即数 4 种），用可写 CSR 观察实际读写发生情况 | 每种组合的读/写行为与 SPEC 表完全一致 |
| ZSIDE-12 | 字段级副作用掩码约束（探测性） | 对标准 CSR 验证 csrrs/csrrc 仅掩码覆盖字段的行为；由于 SPEC 声明当前标准 CSR 无字段写副作用，本用例以"不产生异常、不改变未覆盖字段"作为符合性判据 | 未覆盖字段值不变；无意外副作用（平台如提供自定义副作用 CSR 可增强） |

---

## Group 4. 汇编伪指令编码

**规范依据**：
- 伪指令编码规范：csrr rd,csr = csrrs rd,csr,x0；csrw csr,rs1 = csrrw x0,csr,rs1；csrwi csr,uimm = csrrwi x0,csr,uimm；csrs/csrc/csrsi/csrci 为对应的不关心旧值形式

**测试职责**：验证汇编伪指令展开后的指令编码与功能行为符合 SPEC 定义。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZPSEU-01 | csrr 展开为 csrrs rd,csr,x0 | 检查 csrr rd, mscratch 的机器码编码（rs1 域=x0，opcode=csrrs），并执行验证 | 编码正确；功能为纯读，不写 CSR |
| ZPSEU-02 | csrw 展开为 csrrw x0,csr,rs1 | 检查 csrw mscratch, rs1 的机器码编码（rd 域=x0，opcode=csrrw），并执行验证 | 编码正确；功能为纯写，不读 CSR |
| ZPSEU-03 | csrwi 展开为 csrrwi x0,csr,uimm | 检查 csrwi mscratch, uimm 的机器码编码，并执行验证 | 编码正确；写入零扩展立即数 |
| ZPSEU-04 | csrs/csrc 不读旧值到寄存器 | 执行 csrs mscratch, rs1 与 csrc mscratch, rs1（rd=x0 编码），验证置位/清位生效 | 置位/清位正确，无 rd 输出 |
| ZPSEU-05 | csrsi/csrci 立即数置位/清位 | 执行 csrsi/csrci mscratch, uimm | 低 5 位范围内置位/清位正确 |
| ZPSEU-06 | csrr 对只读 CSR 不触发异常 | csrr rd, mvendorid（即 csrrs rd,mvendorid,x0） | 不触发异常（rs1=x0 不写），rd=mvendorid |

---

## Group 5. CSR 指令访问权限与异常

**规范依据**：
- CSR 地址编码规范：csr 域 bits 31-20 编码 12 位 CSR 地址，其中高位编码最低访问特权级与读/写属性
- 异常语义：访问未实现 CSR 或违反访问权限的 CSR 指令触发 illegal-instruction exception（具体 CSR 地址的访问规则由特权架构定义，本组仅验证 Zicsr 指令层面的异常递送）

**测试职责**：验证 CSR 指令在地址未实现、特权级不足、读写属性冲突场景下的 illegal-instruction exception 行为与 trap 现场（mcause/mepc/mtval）。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZACC-01 | 访问未实现 CSR 地址触发异常 | M-mode 对一个运行时探测确定的未实现 CSR 地址（标准未分配槽位候选列表）执行 csrr | illegal-instruction exception（cause=2） |
| ZACC-02 | 异常现场信息正确 | 承接 ZACC-01，检查 trap 现场 | mcause=2，mepc=故障指令 PC，mtval=故障指令编码（按 Sstvala/特权架构要求） |
| ZACC-03 | U-mode 访问 M 级 CSR 触发异常 | U-mode 执行 csrr mscratch | illegal-instruction exception（cause=2） |
| ZACC-04 | U-mode 访问 S 级 CSR 触发异常 | U-mode 执行 csrr sscratch | illegal-instruction exception（cause=2） |
| ZACC-05 | S-mode 访问 M 级 CSR 触发异常 | S-mode 执行 csrr mscratch 与 csrr misa | illegal-instruction exception（cause=2） |
| ZACC-06 | M-mode 访问全部级别 CSR 合法 | M-mode 访问 mscratch/sscratch（及 U 级可读 CSR） | 全部正常访问 |
| ZACC-07 | U-mode 访问被 scounteren 禁止的计数器 | 配置 scounteren 相应位为 0，U-mode 执行 csrr cycle | illegal-instruction exception（cause=2） |
| ZACC-08 | U-mode 访问被 scounteren 允许的计数器 | 配置 scounteren 相应位为 1，U-mode 执行 csrr cycle | 正常读取 |
| ZACC-09 | 写只读 CSR 触发异常（各级指令形式） | 分别用 csrrw/csrrwi/csrrs(rs1≠x0)/csrrsi(uimm≠0) 写只读 CSR（mvendorid） | 全部触发 illegal-instruction exception（cause=2） |
| ZACC-10 | 非对齐异常委托时异常递送正确 | 前置 medeleg[2]=1，在 S-mode 访问 M 级 CSR | trap 递送到 S-mode：scause=2，sepc/stval 正确 |
| ZACC-11 | mstatus.FS=0 时访问 fcsr 触发异常 | 前置 F 扩展实现；设 mstatus.FS=0，执行 csrr fcsr | illegal-instruction exception（cause=2） |
| ZACC-12 | mstatus.FS≠0 时访问 fcsr 正常 | 前置 F 扩展实现；设 mstatus.FS=Initial，执行 csrr/csrw fcsr | 正常读写 |

---

## Group 6. CSR 访问排序语义

**规范依据**：
- `norm:csr_access_order`：CSR 访问相对相关指令按程序序执行
- `norm:csrr_order`：显式 CSR 读返回指令执行前的 CSR 状态
- `norm:csrw_order`：显式 CSR 写抑制并覆盖同一指令对该 CSR 的隐式写
- `norm:csr_side_effects_synchronous`：副作用按程序序同步发生，下一条指令可观察全部后果
- `norm:csr_weakly_ordered` / `norm:csr_memory_access_ordering` / `norm:csr_fence_ordering` / `norm:csr_strongly_ordered`：跨 hart/设备的弱序语义与 fence I/O 归类（平台相关，见附录覆盖矩阵）

**测试职责**：以 instret（指令执行隐式修改的 CSR）为载体，验证显式 CSR 读/写与隐式更新之间的顺序语义。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZORD-01 | 显式读返回指令执行前的值 | M-mode（mcounteren 使能或 M-mode 直接）执行 csrr rd, instret | rd 为 csrr 指令执行前的 instret 值（不含 csrr 自身计数） |
| ZORD-02 | 显式写抑制隐式更新 | csrrw 向 instret 写入 N，紧随其后 csrr 读 instret | 读到 N（显式写抑制并覆盖 csrrw 自身的隐式自增；SPEC："a value written to instret by one instruction will be the value read by the following instruction"） |
| ZORD-03 | 写入值被后续指令精确观察 | csrrw 写 instret=N，其后两条 nop，再 csrr 读 instret | 读到 N + 2（写后每退休一条指令自增 1，验证写值作为后续隐式自增基准） |
| ZORD-04 | 副作用同步可观察 | csrrw 写 mscratch 后立即（下一条指令）csrr 读回 | 紧随的下一条指令即可观察到写入的全部后果 |
| ZORD-05 | 程序序 CSR 依赖序列 | 构造 CSR 读写依赖序列（写 A → 读 A → 写 B 依赖 A 读值 → 读 B） | 观察到的值序列严格符合程序序，无乱序 |

---

## 附录 A：规范点覆盖矩阵

| Norm ID | 覆盖用例 | 备注 |
|---------|----------|------|
| `norm:csrrw_op` | ZCSR-01, ZCSR-02, ZCSR-07, ZSIDE-01, ZSIDE-02 | |
| `norm:csrrw_rs1_x0` | ZCSR-03 | |
| `norm:csrrs_op` | ZCSR-04, ZCSR-06, ZCSR-09 | |
| `norm:csrrc_op` | ZCSR-05, ZCSR-06 | |
| `norm:csrrs_csrrc_rs1_x0` | ZSIDE-03, ZSIDE-04, ZSIDE-05, ZSIDE-06, ZPSEU-06, ZCSR-09 | rs1 非 x0 持零值语义见 ZCSR-09 |
| `norm:csrrwi_csrrsi_csrrci_ops` | ZIMM-01 ~ ZIMM-10, ZSIDE-07 ~ ZSIDE-10 | |
| `norm:csr_side_effects` | ZSIDE-11, ZIMM-10 | 读写条件表逐项核对 |
| `norm:csr_rs1_uimm_side_effect` | ZSIDE-12, ZCSR-04, ZCSR-05, ZIMM-03, ZIMM-04 | 标准 CSR 当前无字段写副作用，副作用触发本身为探测性验证 |
| 编码格式（12 位 csr 域、5 位零扩展立即数） | ZIMM-02, ZPSEU-01 ~ ZPSEU-05 | 非 norm 锚点规范 |
| 读-改-写回写语义 | ZCSR-06 | SPEC NOTE 语义 |
| 汇编伪指令编码 | ZPSEU-01 ~ ZPSEU-06 | 非 norm 锚点规范 |
| CSR 地址访问权限与异常 | ZACC-01 ~ ZACC-12 | 具体 CSR 地址的权限定义属特权架构，本组验证指令层异常递送 |
| `norm:csr_access_order` | ZORD-01 ~ ZORD-05 | |
| `norm:csrr_order` | ZORD-01 | |
| `norm:csrw_order` | ZORD-02, ZORD-03 | |
| `norm:csr_side_effects_synchronous` | ZORD-04 | |
| `norm:csr_weakly_ordered` | — | 跨 hart/设备全局内存序观察，确定性验证需多核同步基础设施，暂不设计用例 |
| `norm:csr_memory_access_ordering` | — | 同上，且依赖 MMIO/PMA 平台定义 |
| `norm:csr_fence_ordering` | — | CSR 读=I/写=O 的 fence 归类需设备侧观察点，平台相关，暂不设计用例 |
| `norm:csr_strongly_ordered` | — | 平台相关（hardware platform may define），无标准强序 CSR 定义，暂不设计用例 |
