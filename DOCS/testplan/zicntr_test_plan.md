# Zicntr 基础计数器与定时器测试计划

## 概述

本测试计划覆盖 RISC-V Zicntr（Base Counters and Timers）扩展，验证 `cycle`、`time`、`instret` 三个 64 位基础计数器的可访问性、只读属性、计数语义与跨 hart 时间同步行为。

本测试计划依据 `SPEC/riscv-isa-manual/src/unpriv/zicntr.adoc` 中的规范点（norm 标记）编写。

### 本文档覆盖的 SPEC 章节
- Zicntr Extension for Base Counters and Timers（cycle/time/instret CSR 的语义与访问）
- rdcycle/rdtime/rdinstret 伪指令对应的 CSR 行为

### 范围说明
- 本测试框架使用 `riscv64-unknown-linux-elf-gcc` 工具链，XLEN=64，CSR 指令可直接访问完整 64 位计数器。XLEN=32 的高 32 位访问（`cycleh`/`timeh`/`instreth`）规范点在本框架下不可执行，单列于 Group 6 标注为不适用。
- 计数器访问权限控制（`mcounteren`/`scounteren`/`hcounteren`）不属于 zicntr.adoc 的规范内容，由特权规范相关测试计划覆盖（见下节）。

### 由其他测试计划覆盖
- 计数器访问使能控制（mcounteren/scounteren） → `Sm_CSR_test_plan.md` / `Ss_CSR_test_plan.md`
- Hypervisor 计数器访问控制（hcounteren、VU-mode 计数器可见性） → `Hypervisor_CSR_test_plan.md`
- VS/VU-mode 读 `time` 返回 `time + htimedelta` → `Hypervisor_CSR_test_plan.md`（htimedelta 组）
- cycle/instret 计数器的特权级过滤与 inhibit 控制（mcountinhibit、M/S 过滤） → `Smcntrpmf_test_plan.md`
- 计数器溢出中断与 HPM 计数器（mhpmcounter、scountovf） → `Sscofpmf_test_plan.md`
- 性能监控计数器 hpmcounter3-31（Zihpm 扩展） → `zihpm_test_plan.md`
- Sscounterenw（scounteren 可写位约束） → `Sscounterenw_test_plan.md`
- Shcounterenw（hcounteren 可写位约束） → `Shcounterenw_test_plan.md`

---

## 覆盖的规范点

本章节列出本文档所有测试组中引用的规范点（norm ID），已去重并按字母顺序排列。

| Norm ID | 原文 | 中文说明 |
|---------|------|----------|
| `norm:zicntr_rdcycle_op` | The `rdcycle` pseudoinstruction reads the low XLEN bits of the `cycle` CSR which holds a count of the number of clock cycles executed by the processor core on which the hart is running from an arbitrary start time in the past. | `rdcycle` 伪指令读取 `cycle` CSR 的低 XLEN 位，该计数器保存处理器核心从过去任意起点开始执行的时钟周期数。 |
| `norm:zicntr_rdcycleh_op` | `rdcycleh` is only present when XLEN=32 and reads bits 63-32 of the same cycle counter. | `rdcycleh` 仅在 XLEN=32 时存在，读取同一 cycle 计数器的 63-32 位。 |
| `norm:zicntr_rdtime_op` | The `rdtime` pseudoinstruction reads the low XLEN bits of the `time` CSR, which counts wall-clock real time that has passed from an arbitrary start time in the past. | `rdtime` 伪指令读取 `time` CSR 的低 XLEN 位，该计数器统计从过去任意起点开始经过的墙钟实时时间。 |
| `norm:zicntr_rdtimeh_op` | RDTIMEH is only present when XLEN=32 and reads bits 63-32 of the same real-time counter. | RDTIMEH 仅在 XLEN=32 时存在，读取同一实时计数器的 63-32 位。 |
| `norm:zicntr_time_hart_sync` | The real-time clocks of all harts must be synchronized to within one tick of the real-time clock. | 所有 hart 的实时时钟必须同步在实时时钟的一个 tick 以内。 |
| `norm:zicntr_rdinstret_op` | The `rdinstret` pseudoinstruction reads the low XLEN bits of the `instret` CSR, which counts the number of instructions retired by this hart from some arbitrary start point in the past. | `rdinstret` 伪指令读取 `instret` CSR 的低 XLEN 位，该计数器统计本 hart 从过去任意起点开始退休（retire）的指令数。 |
| `norm:zicntr_rdinstreth_op` | `rdinstreth` is only present when XLEN=32 and reads bits 63-32 of the same instruction counter. | `rdinstreth` 仅在 XLEN=32 时存在，读取同一指令计数器的 63-32 位。 |
| `norm:zihpm_op_sz_mode_acc_count` | RISC-V ISAs provide a set of up to thirty-two 64-bit performance counters and timers. | RISC-V ISA 提供最多 32 个 64 位性能计数器与定时器。 |
| `norm:zihpm_op_sz_mode_acc_priv` | These counters are accessible via unprivileged XLEN-bit read-only CSR registers `0xC00`–`0xC1F`. | 这些计数器通过非特权 XLEN 位只读 CSR 寄存器 `0xC00`–`0xC1F` 访问。 |
| `norm:zihpm_op_sz_mode_acc_xlen32` | When XLEN=32, the upper 32 bits are accessed via CSR registers `0xC80`–`0xC9F`. | XLEN=32 时，高 32 位通过 CSR 寄存器 `0xC80`–`0xC9F` 访问。 |
| `norm:zihpm_op_sz_mode_acc_partition` | These counters are divided between the Zicntr and Zihpm extensions. | 这些计数器在 Zicntr 与 Zihpm 扩展之间划分。 |

**非 norm 标记的规范性描述**（正文与 NOTE 中的可测要求）：
- Zicntr 扩展包含 `cycle`、`time`、`instret` 三个计数器，依赖 Zicsr 扩展；计数器伪指令映射到 `csrrs rd, counter, x0` 规范形式，其他只读 CSR 指令形式（基于 csrrc/csrrsi/csrrci）也是合法的读取方式。
- NOTE（instret 退休语义）：引发同步异常的指令（包括 `ecall` 和 `ebreak`）不视为退休，因此不递增 `instret` CSR。

---

## Group 1. 计数器 CSR 可访问性与只读属性

**规范依据**：
- `norm:zihpm_op_sz_mode_acc_count`：最多 32 个 64 位性能计数器与定时器
- `norm:zihpm_op_sz_mode_acc_priv`：通过非特权 XLEN 位只读 CSR 寄存器 0xC00–0xC1F 访问
- `norm:zihpm_op_sz_mode_acc_partition`：计数器在 Zicntr 与 Zihpm 之间划分（cycle/time/instret 属 Zicntr）
- 正文：计数器为只读 CSR，伪指令映射到 `csrrs rd, counter, x0`，csrrc/csrrsi/csrrci 形式同样合法

**测试职责**：验证 `cycle`(0xC00)、`time`(0xC01)、`instret`(0xC02) 在各特权级下可读、不可写，且支持全部只读 CSR 指令形式。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| CNTR-01 | M-mode 读取三个基础计数器 | M-mode 用 csrr 依次读取 cycle/time/instret | 读取成功，不产生 trap |
| CNTR-02 | S-mode 读取三个基础计数器 | 使能 mcounteren.CY/TM/IR，S-mode 用 csrr 读取 cycle/time/instret | 读取成功，不产生 trap |
| CNTR-03 | U-mode 读取三个基础计数器 | 使能 mcounteren 与 scounteren 的 CY/TM/IR 位，U-mode 用 csrr 读取 cycle/time/instret | 读取成功，不产生 trap |
| CNTR-04 | cycle 只读属性 | M-mode 执行 csrw cycle, x / csrs cycle, x / csrc cycle, x | 均触发 illegal-instruction exception (cause=2) |
| CNTR-05 | time 只读属性 | M-mode 执行 csrw/csrs/csrc time | 均触发 illegal-instruction exception (cause=2) |
| CNTR-06 | instret 只读属性 | M-mode 执行 csrw/csrs/csrc instret | 均触发 illegal-instruction exception (cause=2) |
| CNTR-07 | csrrc 读取形式合法 | rd≠x0 且 rs1=x0 时执行 csrrc rd, cycle, x0 | 正常读到 cycle 值，不触发异常 |
| CNTR-08 | csrrsi 读取形式合法 | 执行 csrrsi rd, cycle, 0 | 正常读到 cycle 值，不触发异常 |
| CNTR-09 | csrrci 读取形式合法 | rd≠x0 且 uimm=0 时执行 csrrci rd, time, 0 | 正常读到 time 值，不触发异常 |
| CNTR-10 | 伪指令规范形式 csrrs rd, csr, x0 | 用汇编伪指令 rdcycle/rdtime/rdinstret 读取（即 csrrs rd, csr, x0） | 读取成功，结果与直接 csrr 一致 |
| CNTR-11 | CSR 地址编码验证 | 验证 cycle=0xC00、time=0xC01、instret=0xC02 的地址编码读取 | 按地址编码读取到对应计数器 |

---

## Group 2. cycle 计数器功能

**规范依据**：
- `norm:zicntr_rdcycle_op`：cycle 保存处理器核心从过去任意起点执行的时钟周期数

**测试职责**：验证 cycle 计数器随执行推进、单调非递减。由于计数速率与实现相关，测试只验证相对递增关系，不假设绝对频率。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| CYCLE-01 | cycle 随执行递增 | 先后两次读取 cycle，中间执行一段已知长度的空操作循环 | 第二次读取值大于第一次 |
| CYCLE-02 | cycle 单调非递减 | 连续多次（如 100 次）读取 cycle，记录序列 | 序列单调非递减（每次读取值 ≥ 上一次） |
| CYCLE-03 | cycle 递增量与执行量正相关 | 分别执行短循环与长循环后读取 cycle 差值 | 长循环的 cycle 增量 ≥ 短循环的 cycle 增量 |
| CYCLE-04 | 64 位完整读取（XLEN=64） | XLEN=64 下读取 cycle，检查高 32 位参与返回 | csrr 一次返回完整 64 位值（两次相邻读取差值落在低位的合理范围，无高 32 位截断迹象） |

---

## Group 3. time 计数器功能

**规范依据**：
- `norm:zicntr_rdtime_op`：time 统计从过去任意起点经过的墙钟实时时间
- `norm:zicntr_time_hart_sync`：所有 hart 的实时时钟必须同步在一个 tick 以内

**测试职责**：验证 time 计数器推进、一致性，以及多 hart 间时间同步。注意：简单平台上 time 与 cycle 可能返回相同结果（SPEC NOTE 允许），因此 time==cycle 不构成失败。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| TIME-01 | time 随时间推进 | 先后两次读取 time，中间插入延时（循环或 WFI 等待） | 第二次读取值 ≥ 第一次，且延时后能观测到新值 |
| TIME-02 | time 单调非递减 | 连续多次读取 time | 序列单调非递减 |
| TIME-03 | time 与 mtime 一致 | M-mode 同时读取 `time` CSR 与内存映射 mtime（CLINT/ACLINT） | 两者一致或差异在极小范围（采样先后造成的 1-2 tick 内） |
| TIME-04 | 多 hart 时间同步 | 多 hart 平台：各 hart 在收到同步信号后尽快读取 time，M-mode 汇总比较 | 任意两 hart 读到的 time 差值不超过合理上界（一个 tick 的可观测语义，允许采样开销带来的少量偏移，需与 SPEC 的 "as if" 语义比对判定） |
| TIME-05 | time 周期恒定（误差界内） | 以 cycle 为参照，多次测量 time 相邻采样的增量 | time 增量序列在小的误差界内保持恒定（允许实现相关频率，仅验证恒定周期特性） |

---

## Group 4. instret 计数器功能

**规范依据**：
- `norm:zicntr_rdinstret_op`：instret 统计本 hart 从过去任意起点退休的指令数
- NOTE（退休语义）：引发同步异常的指令（包括 ecall/ebreak）不视为退休，不递增 instret

**测试职责**：验证 instret 的退休计数语义，包括已知指令序列的增量、异常指令不计入退休（规范依据含 `norm:instret_exception`：引发同步异常的指令不视为退休）。测量采用 trap 入口探针 handler（其架构第一条指令读 minstret）与无 trap 对照序列比较，避免 handler 开销污染；中断需全局禁用。

**已知平台缺陷**：QEMU 将引发异常的指令计入 instret，IRET-03/04/05 在 QEMU 上保持 FAIL，详见 `bugs/qemu_zicntr_instret_exception_bug.md`。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| IRET-01 | instret 随执行递增 | 先后两次读取 instret，中间执行固定条数的空操作指令序列 | 第二次读取值大于第一次 |
| IRET-02 | 已知序列增量范围验证 | 读取 instret，执行 N 条已知指令（无异常、无中断），再读取 instret | 增量恰为 N+1：两次读取间的退休流在任一自计数约定下都包含 N 条 nop 加其中一次读取指令本身 |
| IRET-03 | ecall 不计入退休 | 读取 instret → 执行 ecall（触发同步异常）→ handler 中跳过该指令 → 返回后读取 instret | 增量等于测量区间内实际退休指令数，ecall 自身未贡献 1 次计数（与未执行 ecall 的对照序列比较） |
| IRET-04 | 非法指令不计入退休 | 读取 instret → 执行一条非法指令（触发 illegal-instruction exception）→ handler 跳过 → 返回后读取 instret | 该非法指令未贡献退休计数（与对照序列比较） |
| IRET-05 | ebreak 不计入退休 | 读取 instret → 执行 ebreak（触发 breakpoint exception）→ handler 跳过 → 返回后读取 instret | ebreak 自身未贡献 1 次计数（与对照序列比较） |
| IRET-06 | instret 单调非递减 | 连续多次读取 instret | 序列单调非递减 |
| IRET-07 | instret 按 hart 独立计数 | 多 hart 平台：各 hart 分别执行不同数量的指令后读取各自 instret | 各 hart 的 instret 增量反映自身执行的指令数，互不串扰 |

---

## Group 5. 计数器间关系

**规范依据**：
- `norm:zicntr_rdcycle_op` / `norm:zicntr_rdinstret_op`：cycle 计核心时钟周期，instret 计 hart 退休指令数；SPEC NOTE 指出一 hart/core 时 cycle/instret 可用于测量 CPI

**测试职责**：验证 cycle 与 instret 的相对关系符合性能计数语义。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| REL-01 | cycle 与 instret 均推进 | 执行一段指令序列，分别测量 cycle 与 instret 增量 | 两者增量均 > 0 |
| REL-02 | cycle 增量不小于 instret 增量的合理性检查 | 同一段序列分别测量两个增量 | 仅验证两者均为正且可重复测量；不断言固定比例（CPI 为实现相关） |

---

## Group 6. RV32 高半区访问（XLEN=32 专用，本框架不适用）

**规范依据**：
- `norm:zihpm_op_sz_mode_acc_xlen32`：XLEN=32 时高 32 位通过 0xC80–0xC9F 访问
- `norm:zicntr_rdcycleh_op` / `norm:zicntr_rdtimeh_op` / `norm:zicntr_rdinstreth_op`：cycleh/timeh/instreth 仅 XLEN=32 存在
- SPEC 示例代码：XLEN=32 下高低半区读取的一致性算法（先读高半区，再读低半区，再读高半区比对）

**测试职责**：以下 CNTRH-01~04 用例仅在 XLEN=32 环境执行。本测试框架基于 RV64 工具链，`cycleh`/`timeh`/`instreth`（0xC80/0xC81/0xC82）在 XLEN=64 下不存在，故 CNTRH-01~04 在当前框架中标记为不适用，保留条目以便未来 RV32 框架复用。

RV64 侧补充用例 CNTRH-05~07 由同一批 norm 反向推导：高半区 CSR "仅当 XLEN=32 存在"，故 XLEN=64 下访问 0xC80/0xC81/0xC82 必须触发非法指令异常。此部分已在 `Zicntr/tests/test_highhalf.c` 实现。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| CNTRH-01 | cycleh/timeh/instreth 可访问 | XLEN=32 下读取 0xC80/0xC81/0xC82 | 读取成功 |
| CNTRH-02 | 高半区只读属性 | XLEN=32 下写 cycleh/timeh/instreth | illegal-instruction exception (cause=2) |
| CNTRH-03 | 64 位拼接一致性 | XLEN=32 下按 SPEC 示例算法（high-low-high 比对重试）读取 64 位 cycle | 拼接值与两次高半区读取一致，无进位撕裂 |
| CNTRH-04 | 低 32 位与高 32 位拼接推进 | XLEN=32 下延时前后分别拼接读取 64 位 time | 拼接后的 64 位值单调非递减 |
| CNTRH-05 | RV64 下 cycleh(0xC80) 不存在 | XLEN=64 下读取 0xC80 | illegal-instruction exception (cause=2) |
| CNTRH-06 | RV64 下 timeh(0xC81) 不存在 | XLEN=64 下读取 0xC81 | illegal-instruction exception (cause=2) |
| CNTRH-07 | RV64 下 instreth(0xC82) 不存在 | XLEN=64 下读取 0xC82 | illegal-instruction exception (cause=2) |

---

## 测试假设与约束

1. **计数速率实现相关**：SPEC 明确 cycle 计数速率、time 时钟周期均为执行环境相关，测试只验证递增性、单调性与相对关系，不断言绝对频率。
2. **time 可能等于 cycle**：SPEC NOTE 允许简单平台用 cycle 作为 rdtime 的实现，测试不得将 time==cycle 判定为失败。
3. **中断干扰**：instret 增量验证期间需禁用中断（或测量区间避开中断处理），避免中断处理指令污染增量断言；trap 预期验证需遵循框架的 timer 中断防护规范。
4. **精确值断言谨慎**：两次 instret 读取之间的 CSR 指令自身也会计入退休数，区间断言的上界需覆盖测量开销。
5. **多 hart 同步测试**：hart 间同步验证受采样时序影响，判定标准以 "软件无法观测到超过一个 tick 的差异"（as-if 语义）为准，失败时必须与 SPEC 比对后报告平台问题，而不是放宽测试标准。当前框架仅启动 hart 0（entry.S 中其余 hart 原地自旋），TIME-04/IRET-07 以 SKIP 处置。
6. **模拟器行为不作假设**：若 QEMU/Spike/Sail/HW 对只读属性、退休计数、时间同步等行为不符合 SPEC，保持用例失败并记录到 `bugs/` 目录。
7. **QEMU 精确计数依赖 icount**：QEMU 未启用 `-icount` 时 cycle/instret 返回宿主派生值，精确计数类用例（IRET-02/03/04/05、TIME-03/05）无法在该模式下运行；`Zicntr/Makefile` 已按项目惯例附加 `-icount shift=1`。

---

## 平台验证记录

| 平台 | 结果 | 说明 |
|------|------|------|
| QEMU（icount shift=1） | 27 PASS / 3 FAIL / 2 SKIP | FAIL：IRET-03/04/05，QEMU 缺陷（bugs/qemu_zicntr_instret_exception_bug.md）；SKIP：TIME-04/IRET-07（单 hart 框架限制） |
| Spike | 30 PASS / 0 FAIL / 2 SKIP | 全部通过（SPIKE_ISA_EXT=_zicntr） |
| Sail | 30 PASS / 0 FAIL / 2 SKIP | 全部通过 |
