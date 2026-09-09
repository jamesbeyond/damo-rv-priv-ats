**中文 | [English](../testplan_en/Hypervisor_Zc_test_plan_en.md)**

# Hypervisor 与 Zc* 压缩指令扩展交叉测试计划

> 本文档描述 Hypervisor（H）扩展与 Zc* 系列压缩指令扩展（Zca、Zcb、Zcmp、Zcmt、Zcmop、Zcd、Zcf 等）在交叉场景下的测试计划。每个 Zc* 扩展与 Hypervisor 的交集独占一个 Group；当前已覆盖 Hypervisor × Zca（Group 1）与 Hypervisor × Zcmt（Group 2，自 `Hypervisor_Zi_test_plan.md` 迁入），其余 Zc* 扩展陆续补全。

---

## 本文档覆盖的 SPEC 章节

本方案依据以下 RISC-V 官方规范（本地路径）：

- `SPEC/riscv-isa-manual/src/unpriv/zc.adoc` — Zc* 系列压缩指令扩展总览：Zca/Zcb/Zcd/Zcf/Zcmp/Zcmt/Zcmop 等子扩展的组成与依赖关系
- `SPEC/riscv-isa-manual/src/unpriv/zca.adoc` — Zca 扩展：压缩 load/store 指令（`c.lw`/`c.sw`/`c.ld`/`c.sd`/`c.lwsp`/`c.swsp`/`c.ldsp`/`c.sdsp`）到 32 位等效指令的展开规则、IALIGN=16 与 instruction-address-misaligned 异常的排除、控制转移与整数计算指令
- `SPEC/riscv-isa-manual/src/unpriv/zcmt.adoc` — Zcmt 扩展：`cm.jt`/`cm.jalt` 表跳转指令、`jvt` CSR 与两次隐式取指语义、JVT 表项取指的翻译与故障报告
- `SPEC/riscv-isa-manual/src/priv/smstateen.adoc` — Smstateen 扩展：`stateen0.JVT` 位对 Zcmt `jvt` CSR 访问的门控
- `SPEC/riscv-isa-manual/src/priv/hypervisor.adoc` — Hypervisor（H）扩展：`htinst` 寄存器与 trap instruction CSR 的 transformed/伪指令写入规则、压缩指令的三步转换（展开→转换→bit1 清 0，bits[1:0]=01）、transformed load/store 格式与 Addr. Offset、tinst-values 各异常可写值表、隐式 VS-stage 遍历的伪指令强制性、两阶段翻译与 guest-page fault
- `SPEC/riscv-isa-manual/src/priv/machine.adoc` — `mtinst` 寄存器与 M-mode trap instruction CSR 写入规则（与 `htinst` 共享同一套 transformed/伪指令取值约束）

（随其余 Zc* 扩展的交叉用例补入，本节将增列对应 SPEC 文件，如 `zcb.adoc`、`zcmp.adoc`、`zcmop.adoc` 等。）

官方仓库：

- https://github.com/riscv/riscv-isa-manual （对应仓库内上述路径文件）

---

## 范围

### 覆盖的扩展交叉

本文档面向 Hypervisor 与全部 Zc* 压缩指令扩展的交叉场景，**每个扩展独占一个 Group**，当前覆盖情况：

- **Group 1 — Hypervisor × Zca**（已覆盖）：Zca 全部 8 条压缩 load/store（寄存器基址 `c.lw`/`c.sw`/`c.ld`/`c.sd` 与栈指针基址 `c.lwsp`/`c.swsp`/`c.ldsp`/`c.sdsp`）在 VS/VU-mode 触发递送 HS-mode 或陷入 M-mode 的 load/store guest-page fault 时 `htinst`/`mtinst` 的压缩 transformed 值（展开为 32 位等效指令→标准转换→bit1 清 0，bits[1:0]=01）、transformed load 与 store 两种格式的字段保留差异、Addr. Offset 语义、IALIGN=16 与 instruction-address-misaligned 排除在虚拟化取指下的体现、取指类异常 `htinst` 不写 transformed、VS/VU-mode 压缩指令正常执行与语义一致性
- **Group 2 — Hypervisor × Zcmt**（已覆盖，自 `Hypervisor_Zi_test_plan.md` 迁入，用例编号 HZCMT-01~09 保持不变）：HS/VS/VU-mode 下表跳转指令（`cm.jt`/`cm.jalt`）的正常执行与 virtual-instruction 排除、VS/VU-mode 下 `jvt` CSR 的访问与 Smstateen（JVT 位）门控、VS-mode 下 JVT 表项第二次取指的两阶段翻译与 G-stage guest instruction page fault 报告
- **Group 3+ — Hypervisor × Zcb / Zcmp / Zcmop / Zcd / Zcf 等**（陆续补全）：其余 Zc* 扩展与 Hypervisor 的交集将各自独占一个 Group 逐步补入

### 不在本文档范围

- **压缩指令 `htinst` transformed 机制的存在性验证**（以单一 `c.lw` 样例证明 bits[1:0]=01 转换规则生效）— 由 `Hypervisor_Exceptions_test_plan.md` Group 4 的 TINST-08 覆盖；本文档 Group 1 从 Zca 扩展视角系统遍历全部压缩 load/store 变体（尤其 store 与栈指针基址变体）验证每条展开的正确性，与 TINST-08 的机制存在性验证互补而不重复
- **非压缩 load/store 的 `htinst` transformed（bits[1:0]=11）、隐式 VS-stage 遍历伪指令、illegal-instruction/ecall/中断写零** — 由 `Hypervisor_Exceptions_test_plan.md` Group 4（TINST-01~10）覆盖
- **原子指令（LR/SC/AMO/amocas/load-acquire/store-release）的 `htinst` transformed atomic 格式** — 由 `Hypervisor_Za_test_plan.md` 覆盖（Zca 不含任何原子指令，压缩原子指令在 RISC-V 中不存在）
- **guest 取指跨页边界的两阶段翻译、隐式遍历取指故障的 `htval`/`mtval2` 与伪指令** — 由 `Hypervisor_2_stage_test_plan.md`（TS-STRD-02、TS-IMPL-06）覆盖；本文档 Group 1.8/1.9 仅覆盖 Zca 特有的 IALIGN=16 使能与 instruction-address-misaligned 排除，以及"取指类异常绝不写 transformed"与 load/store 类的对比消歧
- **受约束 LR/SC 循环中允许压缩指令**（`norm:constrained_lrsc_compressed_allowed`）— 非虚拟化由 `Zalrsc_test_plan.md`（ZLRSC-45）覆盖，虚拟化由 `Hypervisor_Za_test_plan.md` Group 1 覆盖
- **压缩编码 load/store 在 misaligned atomicity granule（MAG）范围内的未对齐放宽** — 非虚拟化由 `Zama16b_test_plan.md`（ZAMA16B-25/48）覆盖；本文档 HZCA-20 仅验证未对齐压缩访存陷入 HS-mode 时 `htinst` 的 Addr. Offset 语义，不重复 MAG 粒度判定
- **Zcmt 非 Hypervisor 场景**（`jvt` WARL 行为、编码与操作语义、PMP/页表故障处理、表更新可见性与字节序等）— 由 `Zcmt_test_plan.md` 覆盖；Zcmt 与 Hypervisor 的交叉（原 `Hypervisor_Zi_test_plan.md` Group 3，HZCMT-01~09）已整体迁入本文档 Group 2
- **各 Zc* 扩展的非 Hypervisor 场景**（压缩指令编码与保留编码、HINT 空间、全零非法指令、整数计算/控制转移指令基础语义、各特权级基础可执行性等）— 由各自独立测试计划覆盖

---

## 覆盖的规范点

下表列出本方案覆盖的规范点。带 `norm:` 前缀的为 SPEC 官方标签；不带前缀的为根据 SPEC 原文拆解的规范点。随其余 Zc* 扩展补全，本节将按扩展增设子章节。

### Zca 相关（`zca.adoc`）

| 规范 ID | 来源 | 描述（英文） | 描述（中文） |
|---------|------|-------------|-------------|
| `norm:Zca_align16` | `zca.adoc` | The Zca extension allows 16-bit instructions to be freely intermixed with 32-bit instructions, with the latter now able to start on any 16-bit boundary, i.e., IALIGN=16. | Zca 允许 16 位指令与 32 位指令自由混合，32 位指令可起始于任意 16 位边界，即 IALIGN=16；虚拟化取指下该对齐放宽同样成立。 |
| `norm:Zca_no_misaligned` | `zca.adoc` | With the addition of the Zca extension, no instructions can raise instruction-address-misaligned exceptions. | 加入 Zca 后，任何指令都不会引发 instruction-address-misaligned 异常（cause=0）；故 VS/VU-mode 跳转到 2 字节对齐地址执行不得报 cause=0。 |
| `norm:c-lw_op` | `zca.adoc` | c.lw loads a 32-bit value from memory into register rd', computing the effective address by adding the zero-extended offset scaled by 4 to rs1'. It expands to lw rd',offset(rs1'). | `c.lw` 展开为 `lw rd',offset(rs1')`（funct3=010）；VS/VU-mode 下语义不变。 |
| `norm:c-sw_op` | `zca.adoc` | c.sw stores a 32-bit value in register rs2' to memory... It expands to sw rs2',offset(rs1'). | `c.sw` 展开为 `sw rs2',offset(rs1')`（funct3=010）；VS/VU-mode 下语义不变。 |
| `norm:c-ld_op` | `zca.adoc` | c.ld is an XLEN=64-only instruction that loads a 64-bit value... It expands to ld rd',offset(rs1'). | `c.ld`（仅 RV64）展开为 `ld rd',offset(rs1')`（funct3=011）。 |
| `norm:c-sd_op` | `zca.adoc` | c.sd is an XLEN=64-only instruction that stores a 64-bit value... It expands to sd rs2',offset(rs1'). | `c.sd`（仅 RV64）展开为 `sd rs2',offset(rs1')`（funct3=011）。 |
| `norm:c-lwsp_op` | `zca.adoc` | c.lwsp loads a 32-bit value... adding the zero-extended offset scaled by 4 to the stack pointer x2. It expands to lw rd,offset(x2). | `c.lwsp` 展开为 `lw rd,offset(x2)`，base 固定为栈指针 x2；transformed 时 rs1 域（=x2）被 Addr. Offset 替换。 |
| `norm:c-swsp_op` | `zca.adoc` | c.swsp stores a 32-bit value in register rs2... adding the zero-extended offset scaled by 4 to the stack pointer x2. It expands to sw rs2,offset(x2). | `c.swsp` 展开为 `sw rs2,offset(x2)`，base 固定为 x2。 |
| `norm:c-ldsp_op` | `zca.adoc` | c.ldsp is an XLEN=64-only instruction that loads a 64-bit value... adding the zero-extended offset scaled by 8 to x2. It expands to ld rd,offset(x2). | `c.ldsp`（仅 RV64）展开为 `ld rd,offset(x2)`（funct3=011，base=x2）。 |
| `norm:c-sdsp_op` | `zca.adoc` | c.sdsp is an XLEN=64-only instruction that stores a 64-bit value... adding the zero-extended offset scaled by 8 to x2. It expands to sd rs2,offset(x2). | `c.sdsp`（仅 RV64）展开为 `sd rs2,offset(x2)`（funct3=011，base=x2）。 |

### Hypervisor trap instruction 相关（`hypervisor.adoc` / `machine.adoc`）

| 规范 ID | 来源 | 描述（英文） | 描述（中文） |
|---------|------|-------------|-------------|
| `norm:htinst_sz_acc_op` | `hypervisor.adoc` | The htinst register is an HSXLEN-bit read/write register. When a trap is taken into HS-mode, htinst is written with a value that, if nonzero, provides information about the instruction that trapped. | trap 进入 HS-mode 时 `htinst` 写入非零值以提供陷入指令信息，辅助 HS-mode 软件模拟。 |
| `norm:H_trap_xtinst` | `hypervisor.adoc` | On any trap into M-mode or HS-mode, one of these values is written automatically into the appropriate trap instruction CSR, mtinst or htinst: zero; a transformation of the trapping instruction; a custom value; or a special pseudoinstruction. | 任何进入 M-mode/HS-mode 的 trap，`mtinst`/`htinst` 自动写入四类值之一；`mtinst` 与 `htinst` 共享同一套取值规则（Group 1.6/1.7 对称路径依据）。 |
| `norm:H_trap_xtinst_exception_lead-in` | `hypervisor.adoc` | On a synchronous exception, if a nonzero value is written, one of the following shall be true about the value. | 同步异常写入非零值时必须满足合法形式之一；否则软件可安全视同零。 |
| `norm:H_trap_xtinst_exception_list` | `hypervisor.adoc` | Bit 0 is 1, and replacing bit 1 with 1 makes the value into a valid encoding of a standard instruction... the register value is the transformation of the trapping instruction. | bit0=1 且把 bit1 替换为 1 后成为有效标准指令编码——压缩 transformed 值 bits[1:0]=01 正满足此约束（bit1 补 1 得 11 即 32 位标准编码）。 |
| `norm:H_trap_xtinst_val` | `hypervisor.adoc` | tinst-values shows the values that may be automatically written for each standard exception cause. For exceptions that prevent the fetching of an instruction, only zero or a pseudoinstruction value may be written. | tinst-values 表规定各异常可写值：load/store（含 guest-page fault）允许 transformed；取指类（instruction page/guest-page fault）Transformed=No，仅零或伪指令（Group 1.9 取指对比依据）。 |
| `norm:H_trap_xtinst_interrupt` | `hypervisor.adoc` | On an interrupt, the value written to the trap instruction register is always zero. | 中断时 trap instruction 寄存器恒写零（Group 1.7 对照用例依据）。 |
| `norm:H_trap_xtinst_guestpage` | `hypervisor.adoc` | For guest-page faults, the trap instruction register is written with a special pseudoinstruction value if the fault is caused by an implicit memory access for VS-stage address translation and a nonzero value is written to mtval2/htval; zero is not allowed. | 隐式 VS-stage 遍历引发 guest-page fault 且 htval/mtval2 非零时必须写伪指令、不允许零；本文档用作压缩访存显式故障（可写零/transformed）与隐式遍历（强制伪指令）的边界对照。 |
| `htinst_transformed_compressed` | `hypervisor.adoc` | For a standard compressed instruction (16-bit size), the transformed instruction is found as follows: (1) Expand the compressed instruction to its 32-bit equivalent; (2) Transform the 32-bit equivalent instruction; (3) Replace bit 1 with a 0. Bits 1:0 will be binary 01 if the trapping instruction is compressed and 11 if not. | 压缩指令 transformed 值的三步构造：展开为 32 位等效指令→按标准规则转换→bit1 清 0；bits[1:0]=01 标记压缩来源、11 标记非压缩。本方案核心断言，适用于全部 Zc* 压缩访存指令。 |
| `htinst_transformed_load` | `hypervisor.adoc` | For a standard load instruction that is not a compressed instruction (LB..LD/FLW..FLQ), the transformed instruction keeps funct3, rd, and opcode the same as the trapping load instruction; the immediate offset is replaced with zero and bits 19:15 (rs1) with Addr. Offset. | transformed load 格式：保留 funct3/rd/opcode，imm 清零，bits19:15←Addr. Offset；压缩 load 先展开为此 32 位形式再 bit1 清 0。 |
| `htinst_transformed_store` | `hypervisor.adoc` | For a standard store instruction that is not a compressed instruction (SB..SD/FSW..FSQ), the transformed instruction keeps rs2, funct3, and opcode the same as the trapping store instruction; both immediate halves are replaced with zero and bits 19:15 (rs1) with Addr. Offset. | transformed store 格式：保留 rs2/funct3/opcode，两段 imm 清零，bits19:15←Addr. Offset；压缩 store 先展开为此形式再 bit1 清 0（TINST-08 未覆盖 store 分支）。 |
| `htinst_addr_offset` | `hypervisor.adoc` | The Addr. Offset field that replaces rs1 in bits 19:15 is the positive difference between the faulting virtual address (written to mtval/stval) and the original virtual address; this difference can be nonzero only for a misaligned memory access. | Addr. Offset = 故障 VA − 原始 VA，仅未对齐拆分访问可非零；对齐的压缩 load/store 恒为 0。 |

### Zcmt 相关（`zcmt.adoc` / `smstateen.adoc`）

| 规范 ID | 来源 | 描述（英文） | 描述（中文） |
|---------|------|-------------|-------------|
| `norm:cm-jt_op` | `zcmt.adoc` | cm.jt reads an entry from the jump vector table in memory and jumps to the address that was read. | cm.jt 从内存中的跳转向量表读取一个表项并跳转到读到的地址。 |
| `norm:cm-jalt_op` | `zcmt.adoc` | cm.jalt reads an entry from the jump vector table in memory and jumps to the address that was read, linking to _ra_. | cm.jalt 从表中读取一个表项并跳转到读到的地址，同时将返回地址链接到 ra。 |
| `norm:jvt_base_vm` | `zcmt.adoc` | jvt[base] is a virtual address, whenever virtual memory is enabled. | 虚拟内存启用时（含 VS-mode 的 vsatp 翻译），jvt.base 是虚拟地址。 |
| `norm:Zcmt_fetch` | `zcmt.adoc` | ... the execution of a table jump instruction involves two instruction fetches, the first to read the instruction (cm.jt/cm.jalt) and the second to read from the jump vector table (JVT). Both instruction fetches are _implicit_ reads, and both require execute permission; read permission is irrelevant. | 表跳转涉及两次指令取指：第一次取指令本身，第二次取 JVT 表项；两次均为隐式读且都要求执行权限，读权限无关。 |
| `norm:Zcmt_trap` | `zcmt.adoc` | If an exception occurs on either instruction fetch, xEPC is set to the PC of the table jump instruction, xCAUSE is set as expected for the type of fault and xTVAL (if not set to zero) contains the fetch address which caused the fault. | 任一次取指发生异常时，xEPC 设为表跳转指令的 PC，xCAUSE 按故障类型设置，xTVAL（若实现写非零）为引发故障的取指地址。 |
| `norm:stateen0_jvt_op` | `smstateen.adoc` | The JVT bit controls access to the `jvt` CSR provided by the Zcmt extension. | stateen0 的 JVT 位控制对 Zcmt 提供的 jvt CSR 的访问；仅门控 jvt CSR 访问，不门控 cm.jt/cm.jalt 指令执行。 |
| `norm:htval_trapval` | `hypervisor.adoc` | htval trap value reporting for guest-page faults (implementation may write zero or the faulting GPA>>2). | guest-page fault 时 htval 的故障值报告（实现允许写零或故障 GPA>>2）；HZCMT-09 第二次取指 G-stage 故障时报告表项 GPA>>2。 |

---

## Group 1. Hypervisor × Zca 交叉测试

**与 Hypervisor 的交集点**：
1. **无 virtual-instruction 门控**：`hypervisor.adoc` 全部 virtual-instruction 条款（VTVM/VTW/VTSR、HLV/HSV/HLVX、CBO 门控、受控 CSR 访问等）无一涵盖 Zca 压缩指令 → guest 压缩指令绝不报 cause=22，与普通 32 位整数指令一致
2. **虚拟化不改变 Zca 语义**：`norm:c-lw_op` 等展开规则在 VS/VU-mode 下不变，压缩 load/store 的加载值、存储效果、栈指针基址计算与 HS-mode 逐位一致
3. **压缩 transformed 三步规则**：guest 压缩 load/store 触发递送 HS-mode 的 load/store guest-page fault 时，`htinst` 非零则必须为"展开为 32 位等效指令→标准转换→bit1 清 0"的结果，bits[1:0]=01（`htinst_transformed_compressed`）
4. **load 与 store 两种 transformed 格式**：压缩 load 展开后走 `htinst_transformed_load`（保留 funct3/rd/opcode），压缩 store 展开后走 `htinst_transformed_store`（保留 rs2/funct3/opcode，清零两段 imm）——store 分支为 TINST-08（仅 `c.lw`）未覆盖的空白
5. **栈指针基址变体的展开**：`c.lwsp`/`c.swsp`/`c.ldsp`/`c.sdsp` 展开后 base=x2，transformed 时 bits19:15（原 rs1=x2 域）被 Addr. Offset 替换，funct3/rd(rs2)/opcode 仍须与展开后的 32 位指令一致
6. **`htinst` 允许为零**：显式访问故障下实现允许写零（减少硬件努力），非零时必须精确匹配 golden
7. **mtinst 与 htinst 共享规则**：`norm:H_trap_xtinst` 明确"the appropriate trap instruction CSR, mtinst or htinst"共用同一套 transformed/伪指令取值约束；guest 压缩 load/store 异常未委托而陷入 M-mode 时，`mtinst` 写入与 `htinst` 相同的压缩 transformed 值
8. **委托路径对照**：同一压缩访存故障，委托到 HS-mode 读 `htinst`、不委托陷入 M-mode 读 `mtinst`，两者 transformed 值必须一致
9. **IALIGN=16 在 guest 取指下成立**：`norm:Zca_align16` 使 32 位指令可起始于任意 16 位边界，VS/VU-mode 取指经 VS-stage + G-stage 两阶段翻译时该对齐放宽同样适用
10. **instruction-address-misaligned 排除**：`norm:Zca_no_misaligned` 规定加入 Zca 后任何指令都不引发 instruction-address-misaligned（cause=0）；故 guest 跳转到 2 字节对齐地址执行压缩指令、或跳转到 2 字节对齐的 32 位指令目标，均不得报 cause=0
11. **取指类异常 htinst 不写 transformed**：`norm:H_trap_xtinst_val`（tinst-values 表）规定 instruction guest-page fault（cause=20）的 Transformed=No，仅可写零或伪指令——与压缩 load/store 写 transformed 形成关键对比

**规范依据**：
- `norm:c-lw_op` / `norm:c-sw_op` / `norm:c-ld_op` / `norm:c-sd_op` / `norm:c-lwsp_op` / `norm:c-swsp_op` / `norm:c-ldsp_op` / `norm:c-sdsp_op`：各压缩 load/store 到 32 位等效指令的展开映射，VS/VU-mode 下语义不变
- `norm:Zca_align16` / `norm:Zca_no_misaligned`：IALIGN=16 允许 16/32 位指令自由混合、任何指令不引发 instruction-address-misaligned
- `norm:H_trap_xtinst` / `norm:H_trap_xtinst_exception_lead-in` / `norm:H_trap_xtinst_exception_list`：trap instruction 写入值类型与压缩 transformed 值 bits[1:0]=01 的合法性约束（mtinst/htinst 共享）
- `norm:H_trap_xtinst_val`：load/store guest-page fault 允许写 transformed，取指类仅零或伪指令
- `norm:H_trap_xtinst_interrupt`：中断时 trap instruction 寄存器恒写零
- `htinst_transformed_compressed` / `htinst_transformed_load` / `htinst_transformed_store` / `htinst_addr_offset`：压缩三步转换、load/store 两种展开格式与 Addr. Offset 语义
- `norm:H_trap_xtinst_guestpage`：隐式 VS-stage 遍历强制伪指令，作为显式访问故障（可写零/transformed）的边界对照

**测试职责**：验证 Zca 在 V=1、两阶段翻译、HS-mode 与 M-mode 视角下的交叉行为：VS/VU-mode 压缩指令正常执行与 cause=22 排除、压缩 load/store 全 8 变体陷入时 `htinst`/`mtinst` transformed 值的逐字段正确性、IALIGN=16 与 instruction-address-misaligned 排除、取指类异常不写 transformed。压缩指令以 `.option rvc` 显式启用后注入（或 raw `.half` 编码注入固定 16 位编码），trap handler 的 xEPC 推进须按 2 字节适配压缩指令长度；golden 值由框架 `hyp_transform_mem_inst()` 对"展开后的 32 位等效编码"计算后清 bit1 得到。与 TINST-08 的划分：TINST-08 以单一 `c.lw` 验证压缩转换机制存在性，本 Group 遍历 Zca 全部压缩 load/store 变体（含 store 与栈指针基址变体）验证每条展开映射的正确性。

### 1.1 HS/VS/VU-mode 压缩指令可执行性与 cause=22 排除

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| HZCA-01 | HS/VS/VU-mode 执行压缩计算与跳转指令 | 三个特权级分别执行 `c.addi`/`c.add`/`c.mv`/`c.j`/`c.beqz` 等压缩指令（目标映射两阶段均有效） | 均正常执行，无异常；VS/VU-mode **绝不**触发 virtual-instruction (cause=22) |
| HZCA-02 | VS-mode 压缩 load/store 语义与非虚拟化一致 | 同一 `c.lw`/`c.sw` 序列分别在 HS-mode 与 VS-mode 对相同数据执行，比对加载值与内存终值 | 架构可见语义完全一致（虚拟化不改变 Zca 语义，`norm:c-lw_op`/`norm:c-sw_op`） |
| HZCA-03 | VU-mode 压缩 load/store 正常执行 | VU-mode 执行 `c.lw`/`c.sw`（VS-stage/G-stage 均有效映射） | 正常执行，无异常，绝不报 cause=22 |
| HZCA-04 | VS-mode 栈指针基址压缩访存正常 | VS-mode 执行 `c.addi4spn`/`c.lwsp`/`c.swsp`/`c.addi16sp`（base=x2） | 正常执行，栈指针基址计算正确，无异常 |
| HZCA-05 | RV64 双字压缩 load/store 正常 | RV64 平台 VS-mode 执行 `c.ld`/`c.sd`/`c.ldsp`/`c.sdsp` | 正常执行，64 位加载/存储语义正确（`norm:c-ld_op`/`norm:c-sd_op`/`norm:c-ldsp_op`/`norm:c-sdsp_op`） |
| HZCA-06 | VS-mode 压缩与 32 位指令混合序列正常执行 | VS-mode 执行一段 16 位压缩指令与 32 位指令自由混合的序列（两阶段翻译均有效），比对执行结果与非虚拟化一致 | 正常执行、结果正确（`norm:Zca_align16`：IALIGN=16 允许 16/32 位指令自由混合）；作为 Group 1.2~1.5 故障注入的正向基线对照 |

### 1.2 寄存器基址压缩 load/store 的 htinst transformed（`c.lw`/`c.sw`/`c.ld`/`c.sd`）

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| HZCA-07 | c.lw 触发 guest-page-fault 的 htinst | 构造 G-stage 目标 GPA 无效，VS-mode 执行单条 `c.lw`（如 `c.lw a0,0(a0)`）触发 load guest-page fault (cause=21) | 递送 HS-mode；`htinst`=0 或压缩 transformed 值（展开为 `lw`，bits[1:0]=01，funct3=010/rd/opcode 保留、imm 清零、bits19:15←Addr. Offset） |
| HZCA-08 | c.sw 触发 guest-page-fault 的 htinst（store 分支） | G-stage 目标 GPA 无写权限，VS-mode 执行单条 `c.sw` 触发 store/AMO guest-page fault (cause=23) | `htinst`=0 或压缩 transformed **store** 值（展开为 `sw`，保留 rs2/funct3=010/opcode，两段 imm 清零，bits[1:0]=01）——覆盖 TINST-08 未含的 store 分支 |
| HZCA-09 | c.ld 触发 guest-page-fault 的 htinst（RV64） | RV64 平台，G-stage 目标 GPA 无效，VS-mode 执行 `c.ld` 触发 cause=21 | `htinst`=0 或压缩 transformed 值（展开为 `ld`，funct3=011，bits[1:0]=01） |
| HZCA-10 | c.sd 触发 guest-page-fault 的 htinst（RV64 store） | RV64 平台，G-stage 目标 GPA 无写权限，VS-mode 执行 `c.sd` 触发 cause=23 | `htinst`=0 或压缩 transformed store 值（展开为 `sd`，funct3=011，保留 rs2/opcode，bits[1:0]=01） |

### 1.3 栈指针基址压缩 load/store 的 htinst transformed（`c.lwsp`/`c.swsp`/`c.ldsp`/`c.sdsp`）

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| HZCA-11 | c.lwsp 触发 guest-page-fault 的 htinst | VS-mode 设 x2 指向 G-stage 无效 GPA，执行 `c.lwsp rd,off(x2)` 触发 cause=21 | `htinst`=0 或压缩 transformed 值（展开为 `lw rd,off(x2)`，funct3=010/rd/opcode 保留；bits19:15 原为 x2 编码，被 Addr. Offset 替换） |
| HZCA-12 | c.swsp 触发 guest-page-fault 的 htinst | VS-mode 设 x2 指向 G-stage 无写权限 GPA，执行 `c.swsp rs2,off(x2)` 触发 cause=23 | `htinst`=0 或压缩 transformed store 值（展开为 `sw rs2,off(x2)`，保留 rs2/funct3=010/opcode） |
| HZCA-13 | c.ldsp 触发 guest-page-fault 的 htinst（RV64） | RV64 平台，VS-mode 执行 `c.ldsp` 触发 cause=21 | `htinst`=0 或压缩 transformed 值（展开为 `ld rd,off(x2)`，funct3=011） |
| HZCA-14 | c.sdsp 触发 guest-page-fault 的 htinst（RV64） | RV64 平台，VS-mode 执行 `c.sdsp` 触发 cause=23 | `htinst`=0 或压缩 transformed store 值（展开为 `sd rs2,off(x2)`，funct3=011） |

### 1.4 transformed load/store 格式区分与字段保留

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| HZCA-15 | load 与 store transformed 格式区分 | 分别对 `c.lw`（cause=21）与 `c.sw`（cause=23）取 `htinst` 非零值，逐字段比对 | load 保留 funct3/rd/opcode（bits14:0）、store 保留 rs2/funct3/opcode（`htinst_transformed_load` vs `htinst_transformed_store`）；两者 bits[1:0] 均为 01 |
| HZCA-16 | 压缩 transformed 字段与展开后 32 位指令一致 | 对每条压缩 load/store，将 `htinst`（非零）的 bit1 补 1 还原为 32 位编码，与陷入指令的 32 位等效展开比对 funct3/rd(rs2)/opcode | 逐字段一致，imm 已清零（`norm:H_trap_xtinst_exception_list`：bit1 补 1 后为有效标准指令编码） |
| HZCA-17 | bits[1:0]=01 压缩标记与非压缩对比 | 同一 load 语义分别以压缩 `c.lw` 与非压缩 `lw` 触发 cause=21，比对两者 `htinst` 的 bits[1:0] | 压缩来源 bits[1:0]=01、非压缩来源 bits[1:0]=11（`htinst_transformed_compressed`；非压缩侧对照 TINST-07） |

### 1.5 Addr. Offset 与 htinst 允许为零

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| HZCA-18 | 对齐压缩 load/store 的 Addr. Offset 恒 0 | 承接 HZCA-07~14（目标自然对齐），检查 `htinst` 非零值的 bits19:15 | Addr. Offset=0（对齐访问 faulting VA 等于 original VA，`htinst_addr_offset`） |
| HZCA-19 | htinst 允许为零（显式访问故障） | 对任一压缩 load/store 的显式 guest-page fault，接受 `htinst`=0 | `htinst`=0 合法（实现可减少努力，`norm:H_trap_xtinst`）；非零时须精确匹配 golden，不接受任意非零值 |
| HZCA-20 | （记录型）未对齐压缩访存的 Addr. Offset | 若平台支持未对齐拆分访问，构造未对齐 `c.lw`/`c.sw` 触发 guest-page fault 使 faulting VA≠original VA，读 `htinst` bits19:15 | Addr. Offset 可非零（=faulting VA − original VA）；MAG 粒度内是否拆分由 `Zama16b_test_plan.md` 判定，本用例仅记录 `htinst` 的 Addr. Offset 语义，不做 MAG 断言 |


### 1.6 M-mode trap 的 mtinst 压缩 transformed

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| HZCA-21 | guest 压缩 load 陷入 M-mode 的 mtinst | 相关 guest-page fault 不委托（陷入 M-mode），VS-mode 执行 `c.lw` 触发 cause=21 | `mtinst`=0 或压缩 transformed 值（展开为 `lw`，bits[1:0]=01），规则同 `htinst` |
| HZCA-22 | guest 压缩 store 陷入 M-mode 的 mtinst | 同上，VS-mode 执行 `c.sw` 触发 cause=23 | `mtinst`=0 或压缩 transformed store 值（保留 rs2/funct3/opcode，bits[1:0]=01） |
| HZCA-23 | mtinst 与 htinst transformed 值一致 | 同一压缩访存故障，分别在委托到 HS-mode 与陷入 M-mode 两种配置下取 trap instruction 值比对 | 两者非零时逐位一致（`norm:H_trap_xtinst` 统一 mtinst/htinst 规则） |

### 1.7 对照：中断写零

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| HZCA-24 | M-mode 中断时 mtinst=0 | 注入一个陷入 M-mode 的中断（V=1 上下文），读 `mtinst` | `mtinst`=0（严格，`norm:H_trap_xtinst_interrupt`）——与 HZCA-21/22 的同步异常 transformed 形成对比 |

### 1.8 IALIGN=16 与 instruction-address-misaligned 排除

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| HZCA-25 | VS-mode 跳转 2 字节对齐地址执行压缩指令 | VS-mode 通过 `c.j`/`c.jr` 跳转到 2 字节对齐（非 4 字节对齐）地址处的压缩指令序列 | 正常执行，**不得**报 instruction-address-misaligned (cause=0)（`norm:Zca_no_misaligned`） |
| HZCA-26 | VU-mode 跳转 2 字节对齐地址 | 同配置，VU-mode 跳转到 2 字节对齐地址执行 | 正常执行，不报 cause=0，不报 virtual-instruction (cause=22) |
| HZCA-27 | IALIGN=16 使 32 位指令起始于 16 位边界 | VS-mode 执行一条起始于奇数 16 位边界（2 字节对齐、非 4 字节对齐）的 32 位指令，两阶段翻译有效 | 正常取指执行，不报 instruction-address-misaligned（`norm:Zca_align16`：32 位指令可起始于任意 16 位边界） |

### 1.9 取指类异常 htinst 不写 transformed

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| HZCA-28 | guest 压缩取指 instruction guest-page-fault 的 htinst | 构造 G-stage 中目标指令页无 X 权限，VS-mode 跳转到该页执行压缩指令触发 instruction guest-page fault (cause=20) | 递送 HS-mode；`htinst`=0 或伪指令，**绝不**为 transformed 值（tinst-values 表 Instruction guest-page fault 的 Transformed=No，`norm:H_trap_xtinst_val`） |
| HZCA-29 | 取指类与访存类 htinst 的对比消歧 | 对同一 guest，分别构造压缩**取指**故障（cause=20）与压缩 **load** 故障（cause=21），比对 `htinst` 可写值空间 | 取指类 `htinst` 不含 transformed（仅 0/伪指令）；访存类 `htinst` 可为压缩 transformed（bits[1:0]=01）——确立按异常类别区分的写入规则 |

> [!NOTE]
> - 本 Group 所有测试必须在运行时通过 `HAS_H_EXT()` 检测 H 扩展；Zca 支持以平台配置声明的压缩指令支持为准，并以 trap-armed 探测确认指令可执行，不满足时全组 TEST_SKIP。RV64 专属指令（`c.ld`/`c.sd`/`c.ldsp`/`c.sdsp`）在 RV32 平台条件排除。
> - **cause=22 为强制负向断言**：HZCA-01/03 中 guest 压缩指令若报 virtual-instruction 即违反 SPEC（`hypervisor.adoc` 无任何条款对 Zca 指令施加 virtual-instruction 门控），应保持 FAIL 并记录至 `bugs/` 目录，不得放宽断言。
> - **golden 值计算**：框架 `hyp_transform_mem_inst()` 已实现 load（opcode 0x03：保留 bits14:0、清零 imm、bits19:15←Addr. Offset）与 store（opcode 0x23：保留 rs2/funct3/opcode、清零两段 imm）分支；压缩指令须先按 `norm:c-lw_op` 等展开为 32 位等效编码传入，再对结果清 bit1（`& ~2`）得到 bits[1:0]=01 的 golden。栈指针基址变体展开后 rs1=x2，其 bits19:15 同样被 Addr. Offset 覆盖。
> - **零或精确匹配**：显式访问故障下 `htinst`/`mtinst`=0 合法；非零必须逐位等于 golden，不得以"任意非零值"通过（严格验证原则，同 TINST Group）。
> - **与隐式遍历的边界**：HZCA-07~20 均为压缩访存指令**自身数据访问**在 G-stage 失败的显式故障（`htinst` 可 0/transformed）；若故障源于 VS-stage 页表**隐式遍历**，则适用 `norm:H_trap_xtinst_guestpage`（htval 非零时强制伪指令、不允许零），该场景由 `Hypervisor_Exceptions_test_plan.md` TINST-05/06 覆盖，本 Group 不重复。
> - **压缩指令注入与 sepc 推进**：每条故障用例须保证陷入指令恰为单条 16 位压缩指令，注入区以 `.option rvc` 包裹或用 raw `.half` 编码；trap handler 依据 xEPC 处指令低两位判定为压缩后按 +2 推进，避免误跳。1.1 的正常执行用例不触发 trap，长度适配主要服务于 1.2~1.9 的故障注入。
> - **mtinst 对称路径（1.6/1.7）**：需具备将 guest-page fault 路由至 M-mode 的委托配置能力（`medeleg`/`hedeleg` 相应位清零）；`mtinst` 的可写/可读实现范围可能窄于 `htinst`（`norm:H_trap_xtinst` NOTE 允许 trap instruction 寄存器最小仅支持 0 与伪指令），`mtinst`=0 恒合法。HZCA-23 的一致性断言仅在两次运行均写入非零 transformed 值时逐位比对，任一侧写零属合法实现，记录而不判失败。
> - **cause=0 为强制负向断言**：HZCA-25~27 中若 guest 取指报 instruction-address-misaligned (cause=0)，即违反 `norm:Zca_no_misaligned`，应保持 FAIL 并记录至 `bugs/` 目录，不得放宽。
> - **与 `Hypervisor_2_stage_test_plan.md` 的划分**：跨页取指（32 位指令跨页边界）的 `htval`/`mtval2` 故障部分报告（TS-STRD-02）与隐式遍历取指故障的伪指令（TS-IMPL-06）由该方案覆盖；本 Group HZCA-28 仅验证"取指类异常 htinst 绝不写 transformed"这一与访存类的对比消歧，HZCA-25~27 仅验证 Zca 特有的 IALIGN=16 与 misaligned 排除，均不重复 2_stage 的跨页/隐式遍历断言。
> - HZCA-20 为记录型用例：是否触发未对齐拆分取决于平台 MAG 与 Zicclsm 支持，Addr. Offset 非零与恒零均为合法观测，仅记录不强制判定；MAG 粒度断言归属 `Zama16b_test_plan.md`。

---

## Group 2. Hypervisor × Zcmt 交叉测试

**与 Hypervisor 的交集点**：
1. **表跳转指令无特权级门控**：`cm.jt`/`cm.jalt` 为普通指令，`hypervisor.adoc` 无任何 virtual-instruction 条款涵盖它们 → HS/VS/VU-mode 均正常执行，绝不报 cause=22
2. **jvt.base 虚拟地址与两阶段翻译**：`norm:jvt_base_vm` 规定虚拟内存启用时 jvt.base 为虚拟地址；VS-mode 下 JVT 表位于 guest 虚拟地址空间，经 vsatp（VS-stage）+ hgatp（G-stage）两阶段翻译
3. **两次隐式取指**：`norm:Zcmt_fetch` 规定表跳转涉及两次取指（指令本身 + JVT 表项），第二次取指同样经两阶段翻译，其 G-stage 故障按 guest instruction page fault（cause=20）报告
4. **stateen 门控 jvt CSR 但不门控指令**：`norm:stateen0_jvt_op` 规定 hstateen0.JVT 门控 VS/VU 对 jvt CSR 的访问，但 cm.jt/cm.jalt 指令执行不受 stateen 门控
5. **trap 现场指向表跳转指令**：`norm:Zcmt_trap` 规定任一次取指故障时 xEPC 指向表跳转指令本身（非 JVT 表地址），xTVAL/htval 报告故障取指地址；`norm:htval_trapval` 规定 G-stage 故障 htval 写故障 GPA>>2（或零）

**规范依据**：
- `norm:cm-jt_op` / `norm:cm-jalt_op`：表跳转为普通指令，无特权级限制，HS/VS/VU-mode 下均应正常执行
- `norm:jvt_base_vm`：虚拟内存启用时 jvt.base 为虚拟地址，VS-mode 下经 vsatp 两阶段翻译
- `norm:Zcmt_fetch` / `norm:Zcmt_trap`：第二次取指（JVT 表项）同样经过翻译，故障时 xEPC 指向表跳转指令、xTVAL 为故障取指地址
- `norm:stateen0_jvt_op`：stateen0 的 JVT 位控制 jvt CSR 访问，仅门控 CSR 访问、不门控指令执行
- `norm:htval_trapval`：G-stage 故障时 htval 的故障值报告规则

**测试职责**：验证表跳转指令与 jvt CSR 在虚拟化环境下的行为：HS/VS/VU-mode 正常执行不误触发 virtual-instruction exception；VS-mode 下 JVT 表项取指经两阶段翻译，G-stage 故障按 guest instruction page fault 报告；hstateen0.JVT 门控 VS/VU 的 jvt 访问但不门控指令执行。本 Group 用例自 `Hypervisor_Zi_test_plan.md` Group 3 整体迁入（用例编号 HZCMT-01~09 保持不变），原始来源为 `Zcmt_test_plan.md`（原 ZCMT-27/28、ZACC-03/04/05/06 虚拟化部分；HZCMT-08 为对照 ZCMT-25 补充的 VS-stage 故障用例）。

### 2.1 HS/VS/VU-mode 表跳转执行

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| HZCMT-01 | HS-mode 执行表跳转 | HS-mode 执行 cm.jt 与 cm.jalt（jvt 指向有效表） | 均正常跳转与链接，无异常 |
| HZCMT-02 | VS-mode 执行表跳转 | VS-mode 执行 cm.jt 与 cm.jalt | 均正常执行，无 virtual-instruction exception |
| HZCMT-03 | VU-mode 执行表跳转 | VU-mode 执行 cm.jt 与 cm.jalt | 均正常执行，无异常 |

### 2.2 VS/VU-mode jvt 访问与 stateen 门控

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| HZCMT-04 | VS/VU-mode 访问 jvt | stateen 使能状态下，VS/VU-mode csrr/csrw jvt | 访问正常（jvt 权限 URW + stateen 使能） |
| HZCMT-05 | hstateen0.JVT 门控 VS/VU 访问 | 实现 Smstateen 时按层级清零 hstateen0/sstateen0 的 JVT 位，VS/VU 访问 jvt | VS/VU 触发 virtual-instruction/illegal-instruction（详细用例见 `Smstateen_test_plan.md` / `Ssstateen_test_plan.md`） |
| HZCMT-06 | stateen 不门控表跳转指令执行 | 实现 Smstateen 时清零各级 stateen 的 JVT 位，VS-mode 执行 cm.jt/cm.jalt | 指令正常执行（state enable 仅门控 jvt CSR 访问，不门控指令本身） |

### 2.3 VS-mode 两阶段翻译下的表跳转

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| HZCMT-07 | VS-mode 翻译路径正常跳转 | VS-mode 启用 vsatp，VS-stage 映射表页 X=1，执行 cm.jt | 跳转成功（VS-stage 翻译生效） |
| HZCMT-08 | VS-mode 表页 VS-stage X=0 触发故障 | 表所在页 VS-stage 映射但 X=0，VS-mode 执行 cm.jt | instruction page fault 按委托路径递送，sepc=cm.jt PC，stval=表项虚拟地址 |
| HZCMT-09 | VS-mode 第二次取指 G-stage 故障 | G-stage 表页映射无效，VS-mode 执行 cm.jt | guest instruction page fault (cause=20) 递送至 HS-mode，hstatus.GVA=1，htval=故障表项 GPA>>2（norm:htval_trapval 允许为零） |

> [!NOTE]
> - 本 Group 所有测试必须在运行时通过 `HAS_H_EXT()` 检测 H 扩展的可用性，不可用时 TEST_SKIP；Zcmt 支持以平台配置 `ZCMT_SUPPORTED` 宏为准，jvt 可写性探测结果（只读实现，`norm:jvt_op` 允许）决定功能用例是否适用。
> - HZCMT-05 另需探测 Smstateen；HZCMT-07~09 需启用 vsatp（及 hgatp）构造两阶段翻译，VS-stage 有效 + G-stage 无效的映射组合用于隔离 G-stage 故障。
> - HZCMT-08/09 验证第二次取指（JVT 表项）的故障路径：vsepc 必须指向 cm.jt 指令本身而非表地址（`norm:Zcmt_trap`），vstval/htval 报告表项取指地址。

---

## 关键注意事项

> 以下注意事项中第 1~2 条为全文档通用；第 3~8 条主要针对 Group 1（Zca）的压缩访存 trap instruction 转换，Group 2（Zcmt）的专属要点见其 Group 内 NOTE；其余 Zc* 扩展补入时各自在其 Group 内补充相应说明。

1. **文档组织**：本文档面向全部 Zc* 压缩指令扩展与 Hypervisor 的交叉，**每个扩展独占一个 Group**（Group 1=Zca，Group 2=Zcmt，Group 3+ 陆续补入 Zcb/Zcmp/Zcmop/Zcd/Zcf 等）。各扩展的用例编号采用独立前缀（Zca 为 `HZCA-`、Zcmt 为 `HZCMT-`），补入新扩展时不得复用或重排既有编号。

2. **扩展检测**：所有测试必须在运行时通过 `HAS_H_EXT()` 检测 H 扩展，并以平台配置声明与 trap-armed 探测确认对应 Zc* 扩展指令可执行；不满足时相应组/用例 TEST_SKIP。RV64 专属指令（`c.ld`/`c.sd`/`c.ldsp`/`c.sdsp`）在 RV32 平台条件排除。Zcmt 支持以平台配置 `ZCMT_SUPPORTED` 宏与 `jvt` CSR（0x017）trap-armed 探测为准，HZCMT-05 另需探测 Smstateen。

3. **Zca 与 A 扩展无交集**：Zca 为整数压缩指令扩展，不含任何原子指令（RISC-V 无压缩形式的 LR/SC/AMO）；原子指令的 `htinst` transformed atomic 格式由 `Hypervisor_Za_test_plan.md` 覆盖，本文档不涉及 opcode 0x2F 的 transformed 分支。

4. **压缩 transformed 三步规则为核心断言**：`htinst`/`mtinst` 非零时必须等于"展开为 32 位等效指令→按 load/store 标准转换→bit1 清 0"的 golden，bits[1:0]=01 标记压缩来源（`htinst_transformed_compressed`）；load 与 store 走不同格式（funct3/rd/opcode vs rs2/funct3/opcode），栈指针基址变体展开后 base=x2。该规则对全部 Zc* 压缩访存指令通用，是后续扩展 Group 的公共基础。

5. **零或精确匹配原则**：显式访问故障下 trap instruction 寄存器允许写零（实现可减少努力），但写入非零值时必须逐位精确匹配 golden，不接受任意非零值；隐式 VS-stage 遍历且 htval 非零时强制伪指令、不允许零（该场景归属 TINST-05/06，本文档仅作边界对照）。

6. **取指类与访存类的写入差异**：instruction guest-page fault（cause=20）的 `htinst` 绝不写 transformed（仅 0/伪指令），load/store guest-page fault（cause=21/23）可写压缩 transformed；断言须按异常类别区分，不得混淆（`norm:H_trap_xtinst_val`）。

7. **压缩指令注入与 sepc 推进**：故障用例须保证陷入指令恰为单条 16 位压缩指令，注入区以 `.option rvc` 包裹或用 raw `.half` 固定编码；trap handler 依据 xEPC 处指令低两位判定长度，压缩指令按 +2 推进 xEPC，避免误跳导致后续断言失真。

8. **合规处置原则**：任一平台若违反上述 SPEC 强制断言（cause=22 误报、cause=0 误报、`htinst`/`mtinst` 非零值偏离 golden、取指类误写 transformed），用例保持 FAIL 并记录至 `bugs/` 目录，不得为通过测试而跳过或做变通；记录型用例（HZCA-20 未对齐 Addr. Offset、HZCA-23 一致性观测）仅记录实现行为，合法结果不判失败。

---

## 参考

- `zc.adoc` — RISC-V Compressed Instructions（Zc* 系列总览：Zca/Zcb/Zcd/Zcf/Zcmp/Zcmt/Zcmop 组成与依赖）
- `zca.adoc` — Zca Extension for Integer Compressed Instructions（压缩 load/store 展开、IALIGN=16、instruction-address-misaligned 排除）
- `zcmt.adoc` — Zcmt Extension for Compressed Table Jumps（cm.jt/cm.jalt 表跳转、jvt CSR、两次隐式取指）
- `smstateen.adoc` — Smstateen Extension（stateen0.JVT 位对 jvt CSR 访问的门控）
- `hypervisor.adoc` — RISC-V Hypervisor Extension（`htinst`/`mtinst` transformed 与伪指令规则、两阶段翻译与 guest-page fault）
- `machine.adoc` — Machine-Level ISA（`mtinst` 寄存器与 trap instruction 写入规则）
- `DOCS/testplan/Hypervisor_Exceptions_test_plan.md` — Hypervisor 异常与 trap 测试计划（Group 4 TINST-01~10：htinst/mtinst 转换指令机制，含 TINST-08 压缩 `c.lw` 机制存在性验证）
- `DOCS/testplan/Hypervisor_Za_test_plan.md` — Hypervisor 与 Za 原子扩展交叉测试计划（原子指令 transformed atomic 格式）
- `DOCS/testplan/Hypervisor_2_stage_test_plan.md` — 两阶段翻译测试计划（TS-STRD-02 跨页取指、TS-IMPL-06 隐式遍历取指伪指令）
- `DOCS/testplan/Hypervisor_Zi_test_plan.md` — Hypervisor 与 Z* 扩展交叉测试计划（Zcmt 交叉已迁出至本文档 Group 2）
- `DOCS/testplan/Zcmt_test_plan.md` — Zcmt 独立测试计划（非 Hypervisor 场景）
- `DOCS/testplan/Smstateen_test_plan.md` / `DOCS/testplan/Ssstateen_test_plan.md` — Smstateen/Ssstateen 测试计划（stateen0.JVT 门控矩阵）
- `DOCS/testplan/Zalrsc_test_plan.md` — Zalrsc 测试计划（ZLRSC-45 受约束循环允许压缩指令，非虚拟化）
- `DOCS/testplan/Zama16b_test_plan.md` — Zama16b 测试计划（ZAMA16B-25/48 压缩编码在 MAG 范围内的未对齐放宽，非虚拟化）

---

## 附录 A：规范点覆盖矩阵

下表标明"覆盖的规范点"章节中每条规范点被哪些测试用例覆盖。当前含 Group 1（Zca）与 Group 2（Zcmt）规范点；其余 Zc* 扩展补入时同步扩充本矩阵。

| Norm ID | 覆盖的测试 ID |
|---------|---------------|
| `norm:Zca_align16` | HZCA-06、HZCA-27 |
| `norm:Zca_no_misaligned` | HZCA-25、HZCA-26、HZCA-27 |
| `norm:c-lw_op` | HZCA-02、HZCA-07、HZCA-15、HZCA-16、HZCA-17、HZCA-21 |
| `norm:c-sw_op` | HZCA-02、HZCA-08、HZCA-15、HZCA-16、HZCA-22 |
| `norm:c-ld_op` | HZCA-05、HZCA-09 |
| `norm:c-sd_op` | HZCA-05、HZCA-10 |
| `norm:c-lwsp_op` | HZCA-04、HZCA-11、HZCA-16 |
| `norm:c-swsp_op` | HZCA-04、HZCA-12、HZCA-16 |
| `norm:c-ldsp_op` | HZCA-05、HZCA-13 |
| `norm:c-sdsp_op` | HZCA-05、HZCA-14 |
| `norm:htinst_sz_acc_op` | HZCA-07 ~ HZCA-19 |
| `norm:H_trap_xtinst` | HZCA-07 ~ HZCA-19、HZCA-21 ~ HZCA-23 |
| `norm:H_trap_xtinst_exception_lead-in` | HZCA-07 ~ HZCA-17、HZCA-21 ~ HZCA-22 |
| `norm:H_trap_xtinst_exception_list` | HZCA-16、HZCA-17 |
| `norm:H_trap_xtinst_val` | HZCA-07 ~ HZCA-10、HZCA-28、HZCA-29 |
| `norm:H_trap_xtinst_interrupt` | HZCA-24 |
| `norm:H_trap_xtinst_guestpage` | HZCA-19（边界对照：显式故障可写零，区别于隐式遍历强制伪指令） |
| `htinst_transformed_compressed` | HZCA-07 ~ HZCA-17、HZCA-21 ~ HZCA-23、HZCA-29 |
| `htinst_transformed_load` | HZCA-07、HZCA-09、HZCA-11、HZCA-13、HZCA-15、HZCA-16 |
| `htinst_transformed_store` | HZCA-08、HZCA-10、HZCA-12、HZCA-14、HZCA-15、HZCA-16、HZCA-22 |
| `htinst_addr_offset` | HZCA-18、HZCA-20 |
| `norm:cm-jt_op` | HZCMT-01 ~ HZCMT-03、HZCMT-06 ~ HZCMT-09 |
| `norm:cm-jalt_op` | HZCMT-01 ~ HZCMT-03、HZCMT-06 |
| `norm:jvt_base_vm` | HZCMT-07 ~ HZCMT-09 |
| `norm:Zcmt_fetch` | HZCMT-08、HZCMT-09 |
| `norm:Zcmt_trap` | HZCMT-08、HZCMT-09 |
| `norm:stateen0_jvt_op` | HZCMT-04 ~ HZCMT-06 |
| `norm:htval_trapval` | HZCMT-09 |
