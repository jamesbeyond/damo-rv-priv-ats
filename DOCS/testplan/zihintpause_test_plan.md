**中文 | [English](../testplan_en/zihintpause_test_plan_en.md)**

# Zihintpause 测试计划（Pause Hint 扩展）

## 概述

本测试计划覆盖 RISC-V Zihintpause（v2.0）扩展，即 PAUSE 指令提示。PAUSE 是一条 HINT 指令，提示当前 hart 暂时降低或暂停指令退休速率，主要用于自旋等待（spin-wait）代码序列以降低能耗。由于 PAUSE 是 HINT，测试重点是验证其编码正确性、可执行性（任何特权级下不得引发异常）、以及无架构可见副作用。

本测试计划依据 `SPEC/riscv-isa-manual/src/unpriv/zihintpause.adoc` 中的规范点（norm 标记）编写，并引用 `rv-32-64g.adoc` 中的指令编码规范点与 `rv64.adoc` 中的 HINT 编码空间表。

### 本文档覆盖的 SPEC 章节
- Zihintpause Extension for Pause Hint（`unpriv/zihintpause.adoc`）
- RV32I/RV64I 指令编码表中的 PAUSE 编码行（`unpriv/rv-32-64g.adoc`，`norm:pause_enc`）
- HINT 编码空间表中的 fence-PAUSE 行（`unpriv/rv64.adoc`）

### 由其他测试计划覆盖
- FENCE 指令本身的内存排序语义 → 基础 ISA 内存模型测试（不在本特权级测试框架范围内）
- LR/SC 前向保证中 PAUSE 的禁用说明 → 属 NOTE（非规范性），仅作背景说明，不设用例
- Hypervisor 相关 trap/委托机制 → `Hypervisor_Exceptions_test_plan.md` 等

---

## 覆盖的规范点

本章节列出本文档所有测试组中引用的规范点（norm ID），已去重并按字母顺序排列。

| Norm ID | 原文 | 中文说明 |
|---------|------|----------|
| `norm:fence_enc` | FENCE 指令编码：`fm` / `pred` / `succ` / `rs1` / funct3=`000` / `rd` / opcode=`0001111`。 | FENCE 指令编码格式，PAUSE 复用该编码空间。 |
| `norm:pause_enc` | PAUSE 编码为 `fm`=0000、`pred`=0001(W)、`succ`=0000、`rs1`=00000、funct3=`000`、`rd`=00000、opcode=`0001111`（即 32 位字 0x0100000F）。 | PAUSE 的完整机器码编码。 |
| `norm:pause_enc_fence` | `pause` is encoded as a `fence` instruction with _pred_=`W`, _succ_=`0`, _fm_=`0`, _rd_=`x0`, and _rs1_=`x0`. | PAUSE 编码为一条 pred=W、succ=0、fm=0、rd=x0、rs1=x0 的 fence 指令。 |
| `norm:pause_op` | The `pause` instruction is a HINT that indicates the current hart's rate of instruction retirement should be temporarily reduced or paused. The duration of its effect must be bounded and may be zero. | PAUSE 是一条 HINT，指示当前 hart 的指令退休速率应暂时降低或暂停。其效果的持续时间必须有上界，且可以为零。 |

---

## Group 1. PAUSE 编码验证

**规范依据**：
- `norm:pause_enc_fence`：PAUSE 编码为 pred=W、succ=0、fm=0、rd=x0、rs1=x0 的 fence 指令
- `norm:pause_enc`：完整编码字段（imm[11:0]=0000_0001_0000，opcode=0001111）
- `norm:fence_enc`：PAUSE 位于 FENCE 编码空间内

**测试职责**：验证 PAUSE 的机器码与 SPEC 编码表完全一致。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| PAUSE-01 | PAUSE 指令字编码验证 | 取 PAUSE 指令的 32 位机器码（由汇编器生成或 `.word`/`.insn` 构造），逐字段比对 | 机器码 = 0x0100000F：opcode=0001111, funct3=000, rd=00000, rs1=00000, succ=0000, pred=0001(W), fm=0000 |
| PAUSE-02 | PAUSE 两种构造方式编码一致 | 比对 `.4byte 0x0100000F` 常量构造与 `.insn i 0x0F, 0, x0, x0, 0x010` 构造（不依赖 march 含 zihintpause）生成的指令字 | 两者编码一致且等于 0x0100000F |
| PAUSE-03 | PAUSE 属于 fence HINT 编码空间 | 将 PAUSE 编码按 fence 字段解析：pred=W、succ=0 | succ 集为空，PAUSE 不强制任何内存排序（HINT 属性成立） |

---

## Group 2. PAUSE HINT 语义与副作用

**规范依据**：
- `norm:pause_op`：PAUSE 是 HINT，效果持续时间必须有上界且可为零

**测试职责**：验证 PAUSE 作为 HINT 的架构可见行为：不改变任何架构状态、必然退休（效果有界）、可连续执行。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| PAUSE-04 | PAUSE 执行不触发异常 | M-mode 执行 PAUSE，trap-armed 观察 | 无 trap，指令正常退休，PC 前进 4 字节 |
| PAUSE-05 | PAUSE 不修改通用寄存器 | 执行前将全部可保存通用寄存器置为已知模式值，执行 PAUSE 后逐一比对 | 所有寄存器值不变（x0 恒零，其余保持模式值） |
| PAUSE-06 | PAUSE 不修改 CSR 状态 | 执行 PAUSE 前后读取 mstatus/mcause/mtval/mepc 等 CSR | CSR 值不变 |
| PAUSE-07 | PAUSE 效果有界（必然退休） | 执行单条 PAUSE，用有界超时机制观察其退休 | 指令在有限时间内退休，hart 继续执行后续指令（效果持续时间有界） |
| PAUSE-08 | 连续多条 PAUSE 累积执行 | 连续执行 N 条 PAUSE（如 16 条），trap-armed 观察 | 全部正常退休，无 trap；累计延迟实现相关，不作定量断言 |
| PAUSE-09 | PAUSE 后程序流正确继续 | PAUSE 后紧跟一条可观测指令（如写内存/寄存器标记） | 标记被正确写入，证明控制流从 PAUSE+4 继续 |

---

## Group 3. 各特权级下的 PAUSE 执行

**规范依据**：
- `norm:pause_op`：PAUSE 是 HINT。HINT 按定义在所有特权级下正常执行（如同其基础编码指令 fence），不引发异常
- `norm:pause_enc_fence`：PAUSE 即一条 fence 指令，fence 可在任意特权级执行

**测试职责**：验证 PAUSE 在 M/S/U 及（若实现 H 扩展）VS/VU 各特权级下均可正常执行，不受特权级限制。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| PAUSE-10 | M-mode 执行 PAUSE | M-mode 执行 PAUSE | 正常执行，无异常 |
| PAUSE-11 | S-mode 执行 PAUSE | 切换至 S-mode 执行 PAUSE | 正常执行，无异常 |
| PAUSE-12 | U-mode 执行 PAUSE | 切换至 U-mode 执行 PAUSE | 正常执行，无异常 |
| PAUSE-13 | HS-mode 执行 PAUSE（H 扩展） | 实现 H 扩展时，HS-mode 执行 PAUSE | 正常执行，无异常 |
| PAUSE-14 | VS-mode 执行 PAUSE（H 扩展） | 实现 H 扩展时，切入 VS-mode 执行 PAUSE | 正常执行，无 virtual-instruction exception |
| PAUSE-15 | VU-mode 执行 PAUSE（H 扩展） | 实现 H 扩展时，切入 VU-mode 执行 PAUSE | 正常执行，无异常 |
| PAUSE-16 | envcfg.FIOM 不影响 PAUSE | 设 menvcfg.FIOM=1 后 S-mode 执行 PAUSE；实现 H 扩展时另设 henvcfg.FIOM=1 后 VS-mode 执行 PAUSE | PAUSE 均正常执行（FIOM 仅修改含 I/O 排序位的 fence 的解释，不影响 succ=0 的 PAUSE；且 FIOM 位于 envcfg 而非 mstatus） |
| PAUSE-17 | Smstateen 不门控 PAUSE | 若实现 Smstateen，清零 mstateen0/hstateen0/sstateen0 后在各级执行 PAUSE | PAUSE 正常执行（基础 ISA HINT 不受 stateen 门控） |

---

## 测试实现说明

1. **指令构造**：框架构建 march 默认不含 zihintpause，统一用 `.4byte 0x0100000F` 与 `.insn i 0x0F, 0, x0, x0, 0x010` 两种与 march 无关的方式构造，两者一致性由 PAUSE-02 验证。
2. **效果不观测原则**：SPEC 仅要求 PAUSE 效果持续时间"有界且可为零"，微架构实际暂停周期数属实现相关且不可架构观测，测试不做定量断言；仅验证必然退休（PAUSE-07 用有界超时防挂死）。
3. **NOTE 条款不设用例**：能耗建议、典型持续时间量级、自旋循环中 PAUSE 数量建议、LR/SC 序列中 PAUSE 破坏前向保证等均为 NOTE（非规范性说明），不产生强制用例。
4. **平台差异**：若任一平台（QEMU/Spike/Sail/硬件）对 PAUSE 产生异常或修改架构状态，属于违反 `norm:pause_op` 的实现缺陷，用例保持 FAIL 并记录至 `bugs/` 目录，不做跳过或 workaround。
