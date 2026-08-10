# Zifencei 测试计划（Instruction-Fetch Fence 扩展）

## 概述

本测试计划覆盖 RISC-V Zifencei 扩展，即 `fence.i` 指令。`fence.i` 提供同一 hart 上指令内存写入与指令取指之间的显式同步：RISC-V 不保证对指令内存的 store 对本 hart 的取指可见，直到该 hart 执行一条 `fence.i`。测试重点是验证指令编码、取指同步语义（store → fence.i → fetch 排序）、保留字段的前向兼容行为，以及各特权级下的可执行性。

本测试计划依据 `SPEC/riscv-isa-manual/src/unpriv/zifencei.adoc` 中的规范点（norm 标记）编写，并引用 `rv-32-64g.adoc` 中的指令编码表。

### 本文档覆盖的 SPEC 章节
- Zifencei Extension for Instruction-Fetch Fence（`unpriv/zifencei.adoc`）
- RV32I/RV64I 指令编码表中的 FENCE.I 编码行（`unpriv/rv-32-64g.adoc`，RV32/RV64 Zifencei Standard Extension 表）

### 由其他测试计划覆盖
- FENCE 指令本身的内存排序语义（pred/succ 位） → 基础 ISA 内存模型测试（不在本特权级测试框架范围内）
- 未执行 fence.i 时并发修改指令内存的取指原子性规则（Ziccif） → 属 NOTE/软件约定，不设用例
- Zcmt 跳转向量表写入后的 fence.i 要求 → `Zcmt` 相关测试（如未来纳入）
- SFENCE.VMA 与 TLB 同步 → `Sv*_test_plan.md` 系列

---

## 覆盖的规范点

本章节列出本文档所有测试组中引用的规范点（norm ID），已去重并按字母顺序排列。

| Norm ID | 原文 | 中文说明 |
|---------|------|----------|
| `norm:fence_i_op` | A `fence.i` instruction orders all explicit memory accesses that precede the `fence.i` in program order before all instruction fetches that follow the `fence.i` in program order. | `fence.i` 指令将程序序在其之前的所有显式内存访问，排序在程序序在其之后的所有指令取指之前。 |
| `norm:fence_i_rsv` | The unused fields in the `fence.i` instruction, _funct12_, _rs1_, and _rd_, are reserved for finer-grain fences in future extensions. For forward compatibility, base implementations shall ignore these fields, and standard software shall zero these fields. | `fence.i` 的未用字段 _funct12_、_rs1_、_rd_ 为未来细粒度 fence 保留。为前向兼容，基础实现必须忽略这些字段；标准软件必须将这些字段清零。 |

章节正文中的规范性陈述（未带 norm 标记，同样作为用例依据）：

| 陈述 | 原文摘录 | 中文说明 |
|------|----------|----------|
| 取指同步语义 | A `fence.i` instruction ensures that a subsequent instruction fetch on a RISC-V hart will see any previous data stores already visible to the same RISC-V hart. | `fence.i` 保证同一 hart 后续的指令取指能看到该 hart 此前已可见的所有数据 store。 |
| 多 hart 局限 | `fence.i` does *not* ensure that other RISC-V harts' instruction fetches will observe the local hart's stores in a multiprocessor system. | `fence.i` 不保证其他 hart 的取指能观察本 hart 的 store（属"不保证"条款，非硬件义务）。 |
| 取指与派生访问排序 | An instruction fetch is always ordered before any explicit memory accesses that instruction gives rise to. | 指令取指总是排序在该指令所派生的任何显式内存访问之前。 |

---

## Group 1. FENCE.I 编码验证

**规范依据**：
- `rv-32-64g.adoc` Zifencei 编码表：FENCE.I = imm[11:0] / rs1 / funct3=`001` / rd / opcode=`0001111`
- `norm:fence_i_rsv`：标准软件必须将 funct12、rs1、rd 清零（标准编码 = 0x0000100F）

**测试职责**：验证 FENCE.I 的机器码与 SPEC 编码表完全一致。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| FENI-01 | FENCE.I 指令字编码验证 | 取 `fence.i` 指令的 32 位机器码（汇编器生成或 `.insn`/`.word` 构造），逐字段比对 | 机器码 = 0x0000100F：opcode=0001111, funct3=001, rd=00000, rs1=00000, imm[11:0]=0 |
| FENI-02 | 汇编助记符与构造编码一致 | 比对汇编器对 `fence.i` 助记符生成的编码与手动构造值（`.word 0x0000100F`） | 两者编码一致 |

---

## Group 2. 取指同步基本语义

**规范依据**：
- `norm:fence_i_op`：fence.i 之前的显式内存访问排序在其之后的取指之前
- 章节正文：fence.i 保证后续取指看到此前对本 hart 可见的 store

**测试职责**：以自修改代码（self-modifying code）方式验证 store → fence.i → fetch 的同步效果。测试模式：在可执行缓冲区预置"旧指令"并先执行一次（使其进入取指路径），再 store 写入"新指令"，执行 fence.i 后跳转执行，通过新指令的架构可见副作用判定取指是否看到新内容。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| FENI-03 | fence.i 后取指看到新指令（单条 patch） | 缓冲区预置 `li a0, 0; ret-seq`，先执行一次；再 store 新指令 `li a0, 1`，fence.i 后跳转缓冲区执行 | a0=1，取指看到 patch 后的指令 |
| FENI-04 | 连续两次 patch 均生效 | 在 FENI-03 基础上再次 store 第三个指令（`li a0, 2`），再次 fence.i 后执行 | a0=2，重复同步仍有效 |
| FENI-05 | 多条 store 在单条 fence.i 后全部可见 | 分多次 store（如 sd + sw + sh 拼接一条 32 位指令）写入缓冲区，仅执行一条 fence.i 后跳转执行 | 完整的新指令被执行（所有 store 对取指可见） |
| FENI-06 | patch RV32/RVC 混合宽度指令 | 先 patch 一条 16 位压缩指令（如 `c.li a0, 1`）并执行；再 patch 为 `c.li a0, 2`，fence.i 后执行（若实现 C 扩展） | 两次均执行 patch 后的压缩指令 |
| FENI-07 | fence.i 后跳转目标为新代码 | store patch 后立即 jalr 到缓冲区（jalr 位于 fence.i 之后） | 跳转后执行的是新指令，控制流正常返回 |
| FENI-08 | 取指派生访问的排序 | patch 后的新指令为一条 store（写入标志变量），执行后检查标志 | 标志被写入（取指排序在其派生的显式访问之前，指令完整执行） |
| FENI-09 | 无 fence.i 时不做强断言（行为记录） | store patch 后不经 fence.i 直接执行缓冲区 | SPEC 不保证取指看到新值，新旧均合法；用例仅记录实际行为，不作 PASS/FAIL 判定（信息性用例） |

---

## Group 3. 保留字段前向兼容

**规范依据**：
- `norm:fence_i_rsv`：基础实现必须忽略 funct12、rs1、rd 字段（为未来细粒度 fence 保留）

**测试职责**：验证实现忽略非零保留字段、仍按 fence.i 语义执行，不得因保留字段非零而引发非法指令异常。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| FENI-10 | 全 1 保留字段编码可执行 | 用 `.word 0xFFF99F8F`（funct12=0xFFF, rs1=11111, rd=11111, funct3=001）构造非标准编码，trap-armed 执行 | 无 trap，指令正常退休（实现忽略保留字段） |
| FENI-11 | 非零保留字段编码保持同步语义 | 先 patch 缓冲区为旧指令并执行，store 新指令后用 FENI-10 的非标准编码作为 fence.i 执行，再跳转缓冲区 | 取指看到新指令（非零保留字段不影响同步功能） |
| FENI-12 | rs1 非零不影响行为 | 构造 rs1 指向含已知值的寄存器（非零）的编码执行 | 正常执行，rs1 值不被消费也不影响同步效果 |

---

## Group 4. 各特权级下的 FENCE.I 执行

**规范依据**：
- Zifencei 章节未对 fence.i 设置任何特权级限制或 stateen 门控；fence.i 与 FENCE 同属 opcode=0001111 空间，为普通非特权指令
- `norm:fence_i_op`：同步语义在所有特权级下一致适用

**测试职责**：验证 fence.i 在 M/S/U 及（若实现 H 扩展）HS/VS/VU 各特权级下均可正常执行，同步功能不因特权级改变。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| FENI-13 | M-mode 执行 fence.i 并同步 | M-mode 执行 FENI-03 的 patch + fence.i + 执行序列 | 正常同步，无异常 |
| FENI-14 | S-mode 执行 fence.i 并同步 | S-mode 执行同样的 patch + fence.i + 执行序列 | 正常同步，无异常 |
| FENI-15 | U-mode 执行 fence.i 并同步 | U-mode 执行同样的 patch + fence.i + 执行序列 | 正常同步，无异常（fence.i 未被任何机制从 U 级移除） |
| FENI-16 | HS-mode 执行 fence.i（H 扩展） | 实现 H 扩展时，HS-mode 执行 patch + fence.i 序列 | 正常同步，无异常 |
| FENI-17 | VS-mode 执行 fence.i（H 扩展） | 实现 H 扩展时，切入 VS-mode 执行 patch + fence.i 序列 | 正常同步，无 virtual-instruction exception |
| FENI-18 | VU-mode 执行 fence.i（H 扩展） | 实现 H 扩展时，切入 VU-mode 执行 patch + fence.i 序列 | 正常同步，无异常 |
| FENI-19 | mstatus.FIOM 不影响 fence.i | 设 mstatus.FIOM=1 后执行 patch + fence.i 序列 | 同步正常（FIOM 仅修改含 I/O 排序位的 FENCE，fence.i 无 pred/succ 字段） |
| FENI-20 | Smstateen 不门控 fence.i | 若实现 Smstateen，清零 mstateen0/hstateen0/sstateen0 后在各级执行 fence.i | fence.i 正常执行（基础 ISA 指令不受 stateen 门控） |

---

## Group 5. 多 hart 场景（条件性，需 SMP 支持）

**规范依据**：
- 章节正文：fence.i 仅同步本地 hart；跨 hart 可见需写方额外执行数据 fence + 各远端 hart 各自执行 fence.i（软件约定）
- SPEC 中的 producer/consumer litmus 示例（NOTE，仅作行为说明）

**测试职责**：当前公共框架为单 hart 结构（无 secondary hart 启动支持），本组用例标记为**条件性**：仅在框架具备 SMP 能力后实现。注意"fence.i 不保证远端 hart 可见"属"不保证"条款，硬件做出任何行为（立即或延迟可见）均合法，故只验证软件约定序列的正确性（正例），不设反例断言。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| FENI-21 | producer/consumer 跨 hart 同步（正例） | Hart0：store patch 指令 → `fence w,w` → 置 flag；Hart1：轮询 flag=1 → 执行 fence.i → 跳转 patch 区执行 | Hart1 执行 patch 后的新指令（a0/a1 结果与 litmus 正例一致） |
| FENI-22 | 远端 hart 无 fence.i 时不做断言 | Hart0 完成 patch 与 flag 置位，Hart1 不经 fence.i 直接执行 patch 区 | SPEC 不保证可见，新旧均合法；仅记录行为，不作 PASS/FAIL 判定（信息性用例） |

---

## 测试实现说明

1. **缓冲区要求**：自修改代码缓冲区必须位于可读写且可执行的内存区域（M-mode 用 PMP 配置 RWX；S/U 级用页表映射 RWX，注意 G-stage/VS-stage 均需开放执行权限）。缓冲区地址应避免与取指路径上的只读代码段重叠。
2. **缓存预置**：为最大化暴露同步缺陷，patch 前先执行一次旧指令使其进入指令缓存/取指流水线；部分实现（如 QEMU）取指始终穿透，不能据此豁免硬件验证——任何平台若 FENI-03/04 失败即属违反 `norm:fence_i_op`，保持 FAIL 并记录至 `bugs/`。
3. **信息性用例**：FENI-09/FENI-22 验证的是 SPEC "不保证" 的行为域，任何结果均合法，仅打印记录，不计入通过率，严禁将特定结果作为判据。
4. **NOTE 条款不设用例**：实现建议（flush icache/pipeline、snoop 方案）、Linux ABI 移除用户态 fence.i 的历史背景、未来按地址 fence 的讨论等均为 NOTE（非规范性说明），不产生强制用例。
5. **指令构造**：patch 指令以 `.word` 常量形式写入缓冲区；FENI-10~12 的非标准编码用 `.word` 直接构造，避免汇编器拒绝或自动改写保留字段。
6. **平台差异处置**：若任一平台（QEMU/Spike/Sail/硬件）对标准编码 fence.i 产生异常、忽略保留字段时产生异常、或 fence.i 后取指未看到已 store 的新指令，均属 SPEC 违反，用例保持 FAIL 并记录至 `bugs/` 目录，不做跳过或 workaround。
