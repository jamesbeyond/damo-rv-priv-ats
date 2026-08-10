# Zihintntl 测试计划（Non-Temporal Locality Hints）

## 概述

本测试计划覆盖 RISC-V Zihintntl 扩展（Non-Temporal Locality Hints）的功能点，依据 `SPEC/riscv-isa-manual/src/unpriv/zihintntl.adoc` 中的规范点（norm 标记）编写。

Zihintntl 提供 4 条 HINT 指令（`ntl.p1` / `ntl.pall` / `ntl.s1` / `ntl.all`），用于提示紧随其后的指令（target instruction）的显式内存访问缺乏时间局部性。由于 HINT 可被实现自由忽略，本计划的核心测试策略是：**验证 NTL 指令不改变任何架构状态、不改变 target 指令的架构可见行为**（即架构行为与不带 NTL 前缀时完全一致），以及编码正确性、trap/中断交互与 LR/SC 前进保证等规范约束。

### 本文档覆盖的 SPEC 章节
- Zihintntl Extension for Non-Temporal Locality Hints（ntl.p1/ntl.pall/ntl.s1/ntl.all 语义与编码）
- 压缩变体（c.ntl.p1/c.ntl.pall/c.ntl.s1/c.ntl.all，依赖 C/Zca）
- NTL 作用范围（覆盖所有内存访问指令，Zicbom 除外）
- NTL 与 Zicbop prefetch 的交互
- NTL 的 trap/中断交互行为
- NTL 在 LR/SC 循环中的前进保证

### 由其他测试计划覆盖
- Zicbom/Zicboz/Zicbop 指令本身的功能语义、CSR 控制与 trap 行为 → `CMO_test_plan.md`
- CMO × Hypervisor 交互 → `Hypervisor_CMO_test_plan.md`
- HINT 指令的通用编码框架（rd=x0 编码空间的 HINT 判定）→ 相应 base ISA / Zca 测试计划
- LR/SC 前进保证本身的验证（不带 NTL 的基线场景）→ 相应 A 扩展测试计划

### 测试目录
- 新建测试目录 `zihintntl/`，结构参考现有 unpriv 类测试目录（如 `cmo.Zicbom/`）。
- 检测策略：运行前探测 Zihintntl 支持（该扩展无独立 misa/CSR 标志，按平台配置声明启用）；C/Zca、A、Zicbom、Zicboz、Zicbop 相关 Group 按各扩展探测结果条件执行。

### 关键测试方法说明
1. **HINT 无副作用对照法**：对同一 target 指令分别在「带 NTL 前缀」与「不带 NTL 前缀」两种序列下执行，比对架构状态（寄存器、内存、CSR、异常行为）。两者必须完全一致。
2. **raw encoding 注入法**：NTL 指令以 `.word`/`.hword` 原始编码注入执行流，避免汇编器/工具链别名干扰，确保被测编码与 SPEC 一致。
3. **trap 精确断言**：所有涉及异常的用例显式断言 cause、epc、tval，符合框架「ASSERT 显性失败并继续执行」规范。

---

## 覆盖的规范点

本章节列出本文档所有测试组中引用的规范点（norm ID），已去重并按字母顺序排列。

| Norm ID | 原文 | 中文说明 |
|---------|------|----------|
| `norm:NTL_target_definition` | The insn:ntl[] instructions do not change architectural state, nor do they alter the architecturally visible effects of the target instruction. | NTL 指令不改变架构状态，也不改变 target 指令的架构可见效果。 |
| `norm:NTL-P1_op` | The insn:ntl.p1[] instruction indicates that the target instruction does not exhibit temporal locality within the capacity of the innermost level of private cache in the memory hierarchy. | `ntl.p1` 提示 target 指令在内存层次最内层私有 cache 容量范围内无时间局部性。 |
| `norm:NTL-P1_enc` | insn:ntl.p1[] is encoded as insn:add[x0,x0,x2]. | `ntl.p1` 编码为 `add x0, x0, x2`。 |
| `norm:NTL-PALL_op` | The insn:ntl.pall[] instruction indicates that the target instruction does not exhibit temporal locality within the capacity of any level of private cache in the memory hierarchy. | `ntl.pall` 提示 target 指令在任何层级私有 cache 容量范围内无时间局部性。 |
| `norm:NTL-PALL_enc` | insn:ntl.pall[] is encoded as insn:add[x0,x0,x3]. | `ntl.pall` 编码为 `add x0, x0, x3`。 |
| `norm:NTL-S1_op` | The insn:ntl.s1[] instruction indicates that the target instruction does not exhibit temporal locality within the capacity of the innermost level of shared cache in the memory hierarchy. | `ntl.s1` 提示 target 指令在最内层共享 cache 容量范围内无时间局部性。 |
| `norm:NTL-S1_enc` | insn:ntl.s1[] is encoded as insn:add[x0,x0,x4]. | `ntl.s1` 编码为 `add x0, x0, x4`。 |
| `norm:NTL-ALL_op` | The insn:ntl.all[] instruction indicates that the target instruction does not exhibit temporal locality within the capacity of any level of cache in the memory hierarchy. | `ntl.all` 提示 target 指令在任何层级 cache 容量范围内无时间局部性。 |
| `norm:NTL-ALL_enc` | insn:ntl.all[] is encoded as insn:add[x0,x0,x5]. | `ntl.all` 编码为 `add x0, x0, x5`。 |
| `norm:NTL-compressed_variants` | If the ext:c[] or ext:zca[] extension is provided, compressed variants of these HINTs are also provided: insn:c.ntl.p1[] is encoded as insn:c.add[x0,x2]; insn:c.ntl.pall[] is encoded as insn:c.add[x0,x3]; insn:c.ntl.s1[] is encoded as insn:c.add[x0,x4]; and insn:c.ntl.all[] is encoded as insn:c.add[x0,x5]. | 若实现了 C 或 Zca 扩展，则提供压缩变体：`c.ntl.p1`/`c.ntl.pall`/`c.ntl.s1`/`c.ntl.all` 分别编码为 `c.add x0, x2/x3/x4/x5`。 |
| `norm:NTL_range` | The insn:ntl[] instructions affect all memory-access instructions except the cache-management instructions in the ext:zicbom[] extension. | NTL 指令影响所有内存访问指令，但 Zicbom 的 cache-management 指令除外。 |
| `norm:NTL_Zicob_prefetch_outer` | When an insn:ntl[] instruction is applied to a prefetch hint in the ext:zicbop[] extension, it indicates that a cache line should be prefetched into a cache that is outer from the level specified by the insn:ntl[]. | NTL 作用于 Zicbop prefetch 提示时，指示 cache line 应预取到 NTL 指定层级之外（更外层）的 cache。 |
| `norm:NTL_trap_behavior` | In the event that a trap is taken on the target instruction, implementations are discouraged from applying the insn:ntl[] to the first instruction in the trap handler. Instead, implementations are recommended to ignore the HINT in this case. | target 指令发生 trap 时，规范不鼓励实现把 NTL 应用到 trap handler 的第一条指令，建议此时忽略该 HINT。 |
| `norm:NTL-LR_SC_exception` | Since the insn:ntl[] instructions are encoded as insn:add[]s, they can be used within LR/SC loops without voiding the forward-progress guarantee. | NTL 指令编码为 `add`，可在 LR/SC 循环内使用而不破坏前进保证（forward-progress guarantee）。 |

---

## Group 1. NTL 指令核心语义 — 无架构副作用

**规范依据**：
- `norm:NTL_target_definition`：NTL 指令不改变架构状态，也不改变 target 指令的架构可见效果

**测试职责**：验证 4 条 NTL 指令自身的无副作用性，以及带 NTL 前缀的 target 指令与不带前缀时架构行为完全一致。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| NTL-01 | NTL 指令不改变架构状态 | 依次执行 ntl.p1/ntl.pall/ntl.s1/ntl.all（raw encoding），执行前后快照全部通用寄存器与关键 CSR | 所有架构状态不变，x0 恒为 0，不触发任何异常 |
| NTL-02 | ntl.p1 前缀不改变 load 结果 | 对照执行「ntl.p1 + ld」与「ld」，比对目标寄存器值与后续内存读回值 | 两者结果完全一致 |
| NTL-03 | ntl.pall 前缀不改变 load 结果 | 对照执行「ntl.pall + ld」与「ld」 | 两者结果完全一致 |
| NTL-04 | ntl.s1 前缀不改变 load 结果 | 对照执行「ntl.s1 + ld」与「ld」 | 两者结果完全一致 |
| NTL-05 | ntl.all 前缀不改变 load 结果 | 对照执行「ntl.all + ld」与「ld」 | 两者结果完全一致 |
| NTL-06 | NTL 前缀不改变 store 效果 | 对照执行「ntl.all + sd」与「sd」，回读内存比对 | 内存写入值一致 |
| NTL-07 | NTL 前缀不改变 AMO 效果 | 对照执行「ntl.p1 + amoswap.w」与「amoswap.w」，比对返回值与内存 | 返回值与内存效果一致 |
| NTL-08 | NTL 前缀不改变 FP load/store 效果 | 若实现 F/D：对照执行「ntl.all + fld/fsd」与「fld/fsd」 | 浮点寄存器与内存效果一致，fs/FP dirty 行为一致 |
| NTL-09 | NTL 前缀不改变非内存指令语义 | 执行「ntl.p1 + addi a0,a0,1」，比对 a0 | a0 正确 +1，无异常（规范不鼓励此用法但无架构可见影响） |
| NTL-10 | 连续两条 NTL 仅最后一条生效且无副作用 | 执行「ntl.p1 + ntl.all + ld」，验证 load 正常且无异常 | load 正常完成，架构行为与「ntl.all + ld」一致 |
| NTL-11 | NTL 前缀不改变原子指令 aq/rl 语义 | 对照执行「ntl.pall + amoswap.w.aq」与「amoswap.w.aq」 | 结果一致（排序属性不可架构观测时仅验证功能正确） |
| NTL-12 | 各特权级执行 NTL 均合法 | 分别在 M/HS/U-mode（若实现 H 扩展再加 VS/VU-mode）执行 ntl.all + ld | 各特权级均正常执行，无异常 |

---

## Group 2. NTL 指令编码验证

**规范依据**：
- `norm:NTL-P1_enc`：ntl.p1 编码为 add x0, x0, x2
- `norm:NTL-PALL_enc`：ntl.pall 编码为 add x0, x0, x3
- `norm:NTL-S1_enc`：ntl.s1 编码为 add x0, x0, x4
- `norm:NTL-ALL_enc`：ntl.all 编码为 add x0, x0, x5
- `norm:NTL-P1_op` / `norm:NTL-PALL_op` / `norm:NTL-S1_op` / `norm:NTL-ALL_op`：各变体的提示语义

**测试职责**：验证 NTL 指令的机器编码与 SPEC 一致，且该编码可被合法执行（作为 HINT 不触发异常）。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| NTL-ENC-01 | ntl.p1 编码为 0x00200033 | 执行 .word 0x00200033（add x0,x0,x2），验证执行成功且无副作用 | 正常执行，无异常，架构状态不变 |
| NTL-ENC-02 | ntl.pall 编码为 0x00300033 | 执行 .word 0x00300033（add x0,x0,x3） | 正常执行，无异常 |
| NTL-ENC-03 | ntl.s1 编码为 0x00400033 | 执行 .word 0x00400033（add x0,x0,x4） | 正常执行，无异常 |
| NTL-ENC-04 | ntl.all 编码为 0x00500033 | 执行 .word 0x00500033（add x0,x0,x5） | 正常执行，无异常 |
| NTL-ENC-05 | raw encoding 与汇编助记符一致 | 对比工具链生成的 ntl.p1/ntl.pall/ntl.s1/ntl.all 指令字与 SPEC 编码（若工具链支持助记符） | 指令字一致（工具链不支持时以 .word 注入为准并记录） |
| NTL-ENC-06 | raw encoding 作为 HINT 前缀功能等价 | 执行「.word 0x00200033 + ld」，对照「ntl.p1 + ld」 | 行为一致，无异常 |
| NTL-ENC-07 | 非 NTL 的 add x0,x0,rs 仍为合法 HINT | 执行 .word 编码的 add x0,x0,x6（非 NTL 编码）后接 ld | 正常执行，无异常（rd=x0 编码空间均为 HINT） |

---

## Group 3. 压缩变体（C/Zca 条件执行）

**规范依据**：
- `norm:NTL-compressed_variants`：实现 C 或 Zca 时提供压缩变体，c.ntl.p1/pall/s1/all 分别编码为 c.add x0, x2/x3/x4/x5

**测试职责**：验证 4 条压缩 NTL 变体的编码正确性与无副作用性。本 Group 仅在探测到 C/Zca 扩展时执行。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| NTL-C-01 | c.ntl.p1 编码为 0x900A | 执行 .hword 0x900A（c.add x0,x2），验证无副作用 | 正常执行，无异常，架构状态不变 |
| NTL-C-02 | c.ntl.pall 编码为 0x900E | 执行 .hword 0x900E（c.add x0,x3） | 正常执行，无异常 |
| NTL-C-03 | c.ntl.s1 编码为 0x9012 | 执行 .hword 0x9012（c.add x0,x4） | 正常执行，无异常 |
| NTL-C-04 | c.ntl.all 编码为 0x9016 | 执行 .hword 0x9016（c.add x0,x5） | 正常执行，无异常 |
| NTL-C-05 | 压缩 NTL 前缀功能等价 | 对照执行「c.ntl.all + ld」与「ld」 | 结果完全一致，无异常 |
| NTL-C-06 | 压缩 NTL 后接压缩指令 | 执行「c.ntl.p1 + c.ld」序列，验证混合长度指令流正常 | 正常执行，load 结果正确 |
| NTL-C-07 | 压缩与非压缩 NTL 混用 | 执行「ntl.p1 + c.ntl.all + ld」混合序列 | 正常执行，无异常，架构行为与「c.ntl.all + ld」一致 |

---

## Group 4. NTL 作用范围 — 内存访问指令覆盖

**规范依据**：
- `norm:NTL_range`：NTL 指令影响所有内存访问指令，Zicbom 的 cache-management 指令除外

**测试职责**：验证 NTL 前缀对各类内存访问指令（base ISA、A、F/D、Hypervisor 虚拟机访存指令）均不产生架构可见的异常或语义变化。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| NTL-RG-01 | NTL 作用于全部 load 宽度 | 分别以 ntl.all 为前缀执行 lb/lh/lw/ld/lbu/lhu/lwu | 全部正常执行，读值正确 |
| NTL-RG-02 | NTL 作用于全部 store 宽度 | 分别以 ntl.all 为前缀执行 sb/sh/sw/sd | 全部正常执行，内存写入正确 |
| NTL-RG-03 | NTL 作用于全部 AMO 指令 | 以 ntl.p1 为前缀执行 amoswap/amoadd/amoand/amoor/amoxor/amomin/amomax 的 .w/.d 变体 | 全部正常执行，返回值与内存效果正确 |
| NTL-RG-04 | NTL 作用于 LR/SC | 以 ntl.p1 前缀 LR、ntl.all 前缀 SC 执行成功的 LR/SC 对 | SC 成功返回 0，内存写入正确 |
| NTL-RG-05 | NTL 作用于 FP load/store（F/D） | 若实现 F/D：ntl.all 前缀执行 flw/fsw/fld/fsd | 正常执行，浮点值正确 |
| NTL-RG-06 | NTL 作用于向量访存指令（V） | 若实现 V：ntl.all 前缀执行 vle/vse | 正常执行，向量寄存器内容正确 |
| NTL-RG-07 | NTL 作用于 Hypervisor 虚拟机访存（H） | 若实现 H：HS-mode 以 ntl.all 前缀执行 HLV/HSV/HLVX | 正常执行，访存效果正确 |
| NTL-RG-08 | NTL 前缀下未对齐访存行为不变 | 若平台禁止未对齐访问：对照「ntl.all + 未对齐 lw」与「未对齐 lw」的异常行为 | 两者触发相同异常（cause/epc/tval 一致）；若平台支持未对齐则均正常执行 |

---

## Group 5. NTL 与 CMO 指令交互（Zicbom/Zicboz/Zicbop 条件执行）

**规范依据**：
- `norm:NTL_range`：Zicbom 的 cache-management 指令是 NTL 作用范围的唯一例外
- `norm:NTL_Zicob_prefetch_outer`：NTL 作用于 Zicbop prefetch 时指示预取到更外层 cache

**测试职责**：验证 NTL 前缀不改变 Zicbom/Zicboz 指令的架构行为（Zicbom 例外条款仅意味着提示语义不适用于 Zicbom，架构行为仍不得异常），以及 NTL + Zicbop prefetch 组合的架构正确性。预取目标 cache 层级属微架构行为、不可架构观测，仅验证其架构可见约束。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| NTL-CMO-01 | ntl + cbo.clean 无架构副作用 | 若实现 Zicbom：执行「ntl.p1 + cbo.clean」，对照不带前缀的 cbo.clean | 两者架构行为一致，无异常，内存内容不变 |
| NTL-CMO-02 | ntl + cbo.flush 无架构副作用 | 若实现 Zicbom：执行「ntl.pall + cbo.flush」 | 正常执行，内存内容不变 |
| NTL-CMO-03 | ntl + cbo.inval 不改变异常行为 | 若实现 Zicbom：对照「ntl.s1 + cbo.inval」与「cbo.inval」（合法地址） | 两者行为一致（执行成功或触发相同 illegal/virtual-instruction 异常） |
| NTL-CMO-04 | ntl + cbo.zero 效果不变 | 若实现 Zicboz：对照「ntl.all + cbo.zero」与「cbo.zero」 | 两者均将 cache block 全零化，效果一致 |
| NTL-CMO-05 | ntl + prefetch.r 正常执行 | 若实现 Zicbop：执行「ntl.p1 + prefetch.r」 | 正常执行，无异常（预取层级不可架构观测） |
| NTL-CMO-06 | ntl + prefetch.w / prefetch.i 正常执行 | 若实现 Zicbop：执行「ntl.all + prefetch.w」与「ntl.all + prefetch.i」 | 正常执行，无异常 |
| NTL-CMO-07 | ntl + prefetch 对不可访问地址无异常 | 若实现 Zicbop：对无权限地址执行「ntl.all + prefetch.r」 | 不触发任何异常，不产生架构可见副作用（prefetch 无权限时静默） |
| NTL-CMO-08 | ntl 前缀不改变 CMO 指令的 trap 报告 | 若实现 Zicbom：VS-mode 下 CBIE/CBCFE 关闭时执行「ntl.all + cbo.inval/cbo.clean」 | 与不带前缀一样触发 virtual-instruction exception，cause/epc 指向 CMO 指令而非 NTL |

---

## Group 6. Trap 与中断交互

**规范依据**：
- `norm:NTL_trap_behavior`：target 指令发生 trap 时，实现应忽略该 HINT（不鼓励将其应用到 handler 第一条指令）
- `norm:NTL_target_definition`：NTL 不改变 target 指令的架构可见效果（包括异常行为）
- SPEC NOTE：NTL 与 target 之间发生中断时，恢复执行于 target 指令，不重执行 NTL 也不改变程序语义

**测试职责**：验证 NTL 前缀下 target 指令的 trap 精确性、trap handler 正常执行、以及 NTL 与 target 之间中断的恢复正确性。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| NTL-TRAP-01 | target load 缺页时 trap 信息正确 | 执行「ntl.p1 + ld（未映射地址）」 | 触发 load page-fault，sepc/mtval 指向 ld 指令及其地址（而非 NTL 指令） |
| NTL-TRAP-02 | target store 访问错误时 trap 信息正确 | 执行「ntl.all + sd（PMP/权限禁止地址）」 | 触发 store access-fault，cause/tval 正确，epc 指向 sd |
| NTL-TRAP-03 | trap 后 handler 第一条访存指令正常 | NTL-TRAP-01 场景中，trap handler 第一条指令为 load（读取保存区） | handler 正常执行完成（验证 NTL 未被实现应用到 handler 首条指令造成异常——任何因此产生的异常/崩溃均判为实现问题） |
| NTL-TRAP-04 | trap 恢复后重试成功 | handler 修复映射/权限后 sret 返回 target 指令重试 | ld 重试成功，读到正确值，无二次异常 |
| NTL-TRAP-05 | target 指令非法指令异常 | 执行「ntl.p1 + 非法指令编码」 | 触发 illegal-instruction exception，epc/tval 指向非法指令而非 NTL |
| NTL-TRAP-06 | NTL 与 target 之间发生中断 | 在 ntl 与 target 指令之间使能并触发一次 timer/软件中断，handler 仅记录并返回 | 中断正常递送，恢复后 target 指令正常执行，最终结果与无中断时一致 |
| NTL-TRAP-07 | ecall 作为 target 指令 | 执行「ntl.all + ecall」 | 触发 environment-call 异常，cause 正确，NTL 无副作用 |
| NTL-TRAP-08 | VS-mode 下 NTL + target 的 G-stage 缺页 | 若实现 H：VS-mode 执行「ntl.p1 + ld」触发 G-stage guest-page-fault | 递送到 HS-mode，hstatus.GVA=1，stval/htval 正确，行为与不带前缀一致 |

---

## Group 7. LR/SC 前进保证

**规范依据**：
- `norm:NTL-LR_SC_exception`：NTL 指令编码为 add，可在 LR/SC 循环内使用而不破坏前进保证

**测试职责**：验证在 LR/SC 序列与循环中插入 NTL 指令后，保留的 LR/SC 对仍能成功，前进保证不被破坏。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| NTL-LRSC-01 | NTL 修饰 LR 的 LR/SC 对成功 | 执行「lr.w → ntl.p1 + lr.w(重取) → sc.w」或「ntl.p1 + lr.w → sc.w」序列 | SC 成功（返回 0），内存写入正确 |
| NTL-LRSC-02 | NTL 修饰 SC 的 LR/SC 对成功 | 执行「lr.w → ntl.all + sc.w」 | SC 成功（返回 0），内存写入正确 |
| NTL-LRSC-03 | NTL 位于 LR/SC 循环体内不破坏前进保证 | 在带重试的 LR/SC 循环体内（LR 与 SC 之间）插入 NTL 指令，循环执行至成功 | 循环在有限迭代内完成，不死锁；若实现声称支持 LR/SC 则必须最终成功 |
| NTL-LRSC-04 | 无 NTL 基线对照 | 不带 NTL 的同一 LR/SC 循环 | 循环成功，作为 NTL-LRSC-03 的基线对照 |
| NTL-LRSC-05 | NTL 不豁免其他破坏前进保证的指令 | LR/SC 之间插入普通 load/store（非 NTL）后重试 | 行为按 A 扩展规范（前进保证可被破坏），本用例仅验证 NTL 未错误改变该语义的判定基准 |

---

## 附录：测试用例与规范点覆盖矩阵

| Norm ID | 覆盖用例 |
|---------|----------|
| `norm:NTL_target_definition` | NTL-01 ~ NTL-12, NTL-C-05, NTL-C-07, NTL-RG-*, NTL-CMO-*, NTL-TRAP-01/02/05/07/08 |
| `norm:NTL-P1_op` / `norm:NTL-P1_enc` | NTL-02, NTL-ENC-01/05/06 |
| `norm:NTL-PALL_op` / `norm:NTL-PALL_enc` | NTL-03, NTL-ENC-02/05 |
| `norm:NTL-S1_op` / `norm:NTL-S1_enc` | NTL-04, NTL-ENC-03/05 |
| `norm:NTL-ALL_op` / `norm:NTL-ALL_enc` | NTL-05, NTL-ENC-04/05 |
| `norm:NTL-compressed_variants` | NTL-C-01 ~ NTL-C-07 |
| `norm:NTL_range` | NTL-RG-01 ~ NTL-RG-08, NTL-CMO-01 ~ NTL-CMO-04/08 |
| `norm:NTL_Zicob_prefetch_outer` | NTL-CMO-05 ~ NTL-CMO-07 |
| `norm:NTL_trap_behavior` | NTL-TRAP-01 ~ NTL-TRAP-04 |
| `norm:NTL-LR_SC_exception` | NTL-LRSC-01 ~ NTL-LRSC-05 |

**可测性说明**：
- NTL 对 cache 分配/替换策略的实际影响（如不分配进 L1）属微架构行为，无架构可见观测手段，本计划不做断言，仅通过「无架构副作用对照」保证实现不得引入 SPEC 之外的行为。
- `norm:NTL_trap_behavior` 为 "discouraged/recommended" 级约束：若实现将 NTL 应用到 handler 首条指令并产生异常或错误行为，判定为实现缺陷并记录，测试保持 FAIL。
- 各扩展条件 Group（C/Zca、A、F/D、V、H、Zicbom/Zicboz/Zicbop）未实现时整体 SKIP 并在报告中注明，不属于降级跳过。
