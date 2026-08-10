# Zihpm 硬件性能计数器测试计划

## 概述

本测试计划覆盖 RISC-V Zihpm（Hardware Performance Counters）扩展，验证非特权硬件性能计数器 `hpmcounter3`-`hpmcounter31`（CSR 地址 `0xC03`-`0xC1F`）的可访问性、实现状态发现、只读属性、非特权视图与 M-mode 镜像（`mhpmcounter3`-`mhpmcounter31`）的一致性、S/U-mode 访问以及事件配置无效时的行为。

本测试计划依据 `SPEC/riscv-isa-manual/src/unpriv/zihpm.adoc` 中的规范点（norm 标记）编写。

### 本文档覆盖的 SPEC 章节
- Zihpm Extension for Hardware Performance Counters（hpmcounter3-hpmcounter31 CSR 的语义与访问）
- XLEN=32 时的高 32 位访问（hpmcounter3h-hpmcounter31h）规范点

### 范围说明
- 本测试框架使用 `riscv64-unknown-linux-elf-gcc` 工具链，XLEN=64，CSR 指令可直接访问完整 64 位计数器。XLEN=32 的高 32 位访问（`hpmcounter3h`-`hpmcounter31h`，`0xC83`-`0xC9F`）规范点在本框架下不可执行，单列于 Group 6 标注为不适用，并补充 RV64 下高半区地址不存在的反向验证用例。
- Zihpm 扩展 SPEC 明确实现的数量、宽度与计数事件均为平台相关，本计划的断言只针对 SPEC 强制的行为（访问合法性、只读属性、异常类型、一致性），不假设具体实现数量与事件编码。

### 由其他测试计划覆盖
- 基础计数器 cycle/time/instret（0xC00-0xC02） → `zicntr_test_plan.md`
- 计数器访问使能控制（mcounteren/scounteren 的位域语义与控制矩阵） → `Sm_CSR_test_plan.md` / `Ss_CSR_test_plan.md`
- Hypervisor 计数器访问控制（hcounteren、VU-mode 计数器可见性） → `Hypervisor_CSR_test_plan.md`
- Sscounterenw（scounteren 可写位约束） → `Sscounterenw_test_plan.md`
- Shcounterenw（hcounteren 可写位约束） → `Shcounterenw_test_plan.md`
- M-mode 镜像寄存器 mhpmcounter3-31、事件选择寄存器 mhpmevent3-31、mcountinhibit 自身的寄存器行为 → `Smcntrpmf_test_plan.md`（本计划仅将上述 CSR 作为测试激励使用，不重复验证其字段语义）
- 计数器溢出中断（Sscofpmf，scountovf/OVF） → `Sscofpmf_test_plan.md`

---

## 覆盖的规范点

本章节列出本文档所有测试组中引用的规范点（norm ID），已去重并按字母顺序排列。

| Norm ID | 原文 | 中文说明 |
|---------|------|----------|
| `norm:hpm_counter_op_sz_mode_dependency` | The Zihpm extension depends on the Zicsr extension. | Zihpm 扩展依赖 Zicsr 扩展。 |
| `norm:hpm_counter_op_sz_mode_xlen32` | When XLEN=32, the upper 32 bits of these performance counters are accessible via additional CSRs hpmcounter3h-hpmcounter31h. | XLEN=32 时，这些性能计数器的高 32 位可通过附加 CSR hpmcounter3h-hpmcounter31h 访问。 |
| `norm:hpm_misconfigured_event_behavior` | If the configuration used to select the events counted by a counter is misconfigured, the counter may return a constant value. | 若用于选择计数事件的配置无效，计数器可能返回常数值。 |
| `norm:hpm_platform_specific_impl` | The implemented number and width of these additional counters, and the set of events they count, are platform-specific. | 这些附加计数器的实现数量与宽度、以及所计数的事件集合均为平台相关。 |
| `norm:hpm_unimplemented_counter_access` | Accessing an unimplemented counter may cause an illegal-instruction exception or may return a constant value. | 访问未实现的计数器可能引发非法指令异常，也可能返回常数值。 |
| `norm:hpmcounter_op_sz_mode` | The Zihpm extension comprises up to 29 additional unprivileged 64-bit hardware performance counters, hpmcounter3-hpmcounter31. | Zihpm 扩展包含最多 29 个附加的非特权 64 位硬件性能计数器 hpmcounter3-hpmcounter31。 |
| `norm:zihpm_op_sz_mode_acc_count` | RISC-V ISAs provide a set of up to thirty-two 64-bit performance counters and timers. | RISC-V ISA 提供最多 32 个 64 位性能计数器与定时器。 |
| `norm:zihpm_op_sz_mode_acc_partition` | These counters are divided between the Zicntr and Zihpm extensions. | 这些计数器在 Zicntr 与 Zihpm 扩展之间划分。 |
| `norm:zihpm_op_sz_mode_acc_priv` | These counters are accessible via unprivileged XLEN-bit read-only CSR registers 0xC00–0xC1F. | 这些计数器通过非特权 XLEN 位只读 CSR 寄存器 0xC00–0xC1F 访问。 |
| `norm:zihpm_op_sz_mode_acc_xlen32` | When XLEN=32, the upper 32 bits are accessed via CSR registers 0xC80–0xC9F. | XLEN=32 时，高 32 位通过 CSR 寄存器 0xC80–0xC9F 访问。 |

> 说明：`norm:zihpm_op_sz_mode_acc_*` 四个规范点定义于 `zicntr.adoc` 的计数器总览段落，主验证责任在 `zicntr_test_plan.md`（覆盖 0xC00-0xC02 基础计数器部分）；本计划验证其中 Zihpm 划分部分（0xC03-0xC1F 与 0xC83-0xC9F）。
>
> Zihpm 依赖 Zicsr（`norm:hpm_counter_op_sz_mode_dependency`）通过全部用例以 CSR 指令访问 hpmcounter 隐式覆盖，不单设用例。
>
> SPEC NOTE（同时刻读取多个计数器）为信息性内容（软件可通过 time 计数器前后比对检测上下文切换），不构成规范断言，本计划不设计用例。

---

## Group 1. hpmcounter3-31 可访问性与实现状态发现

**规范依据**：
- `norm:hpmcounter_op_sz_mode`：最多 29 个非特权 64 位计数器 hpmcounter3-hpmcounter31
- `norm:hpm_platform_specific_impl`：实现数量与宽度为平台相关
- `norm:hpm_unimplemented_counter_access`：访问未实现计数器可能触发 illegal-instruction，也可能返回常数值
- `norm:zihpm_op_sz_mode_acc_priv` / `norm:zihpm_op_sz_mode_acc_partition`：0xC03-0xC1F 为 Zihpm 划分的非特权只读 CSR

**测试职责**：验证 hpmcounter3-31 的 CSR 地址编码与计数器编号映射、M-mode 下的可读性，并对全部 29 个计数器按 SPEC 允许的行为逐一分类（可读且镜像保持写入 / 可读但镜像只读零 / illegal-instruction / 常数值），不允许出现非确定性行为。镜像只读零是特权规范 `norm:mhpmcounter_mhpmevent_rdonly0` 明确允许的合法实现（计数器与事件选择器均为只读 0）。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| HPMD-01 | CSR 地址编码与计数器编号映射 | 用数字 CSR 地址 0xC03 与 0xC1F 读取，并和具名 hpmcounter3/hpmcounter31 读取结果比对 | 0xC00+N 对应 hpmcounterN（N=3..31），两种形式读到一致的值 |
| HPMD-02 | M-mode 可读性探测 | M-mode 对 hpmcounter3-31（0xC03-0xC1F）逐一执行 csrr | 每个计数器要么读取成功，要么触发 illegal-instruction (cause=2)，不允许其他行为 |
| HPMD-03 | 实现状态分类与合规判定 | 基于 HPMD-02 结果，对每个计数器分类为"已实现（可读且镜像保持）/ 镜像只读零 / 未实现-trap / 未实现-常数值" | 全部分类落在 SPEC 允许的行为集合内（norm:hpm_unimplemented_counter_access 与 norm:mhpmcounter_mhpmevent_rdonly0） |
| HPMD-04 | trap 路径异常现场正确 | 对探测判定为 trap 的计数器检查 trap 现场 | mcause=2，mepc=故障指令 PC，mtval=故障指令编码 |
| HPMD-05 | 常数值路径两次读取一致 | 对探测判定为"不 trap 但疑似未实现"的计数器连续读取两次，并在一段执行间隔后再读 | 多次读取返回相同常数值（SPEC 允许行为的确定性验证） |
| HPMD-06 | 实现数量上界 | 统计可读计数器数量 | 不超过 29 个（hpmcounter3-31 范围） |

---

## Group 2. 只读属性与 CSR 读指令形式

**规范依据**：
- `norm:zihpm_op_sz_mode_acc_priv`：非特权 XLEN 位只读 CSR 寄存器
- `norm:hpmcounter_op_sz_mode`：hpmcounter3-31 为非特权只读计数器视图
- 正文：计数器经 CSR 指令访问，读形式包含 csrr（csrrs rd,csr,x0）及 csrrc/csrrsi/csrrci 的纯读形式

**测试职责**：验证 hpmcounter3-31 对一切写形式触发 illegal-instruction，对所有合法读形式可正常读取。测试载体选取探测判定为"已实现"的计数器；写形式与纯读用例执行前将 mhpmeventN 置 0（不选择事件）并使 mcountinhibit[N]=1，冻结计数器值以避免断言受计数推进干扰。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| HPMRO-01 | csrw 写 hpmcounter 触发异常 | M-mode 执行 csrw hpmcounter3, rs1 | illegal-instruction exception (cause=2) |
| HPMRO-02 | csrs/csrc 写 hpmcounter 触发异常 | M-mode 执行 csrs hpmcounter3, rs1(非零) 与 csrc hpmcounter3, rs1(非零) | 均触发 illegal-instruction exception (cause=2) |
| HPMRO-03 | csrrw/csrrwi 写 hpmcounter 触发异常 | M-mode 执行 csrrw rd, hpmcounter3, rs1 与 csrrwi rd, hpmcounter3, uimm | 均触发 illegal-instruction exception (cause=2) |
| HPMRO-04 | csrrs rs1=x0 纯读不触发异常 | M-mode 执行 csrrs rd, hpmcounter3, x0（即 csrr） | 不触发异常，读到计数器当前值 |
| HPMRO-05 | csrrc rs1=x0 纯读不触发异常 | M-mode 执行 csrrc rd, hpmcounter3, x0 | 不触发异常，读到计数器当前值 |
| HPMRO-06 | csrrsi/csrrci uimm=0 纯读不触发异常 | M-mode 执行 csrrsi rd, hpmcounter3, 0 与 csrrci rd, hpmcounter3, 0 | 均不触发异常，读到计数器当前值 |
| HPMRO-07 | 各读形式结果一致 | 同一冻结计数器上依次用 csrr/csrrc(rd,csr,x0)/csrrsi(rd,csr,0) 读取 | 各形式读到相同值 |
| HPMRO-08 | hpmcounter31 只读属性 | 对 0xC1F（若实现）重复 HPMRO-01/04 的写与读验证 | 写触发 illegal-instruction (cause=2)，读正常 |

---

## Group 3. mhpmcounter 镜像一致性与值保持宽度

**规范依据**：
- `norm:hpmcounter_op_sz_mode`：hpmcounter3-31 是 64 位计数器的非特权视图
- `norm:hpm_platform_specific_impl`：实现的计数器宽度为平台相关

**测试职责**：以 M-mode 写 mhpmcounterN 为激励，验证非特权视图 hpmcounterN 与 M-mode 镜像的值一致性、写入值的稳定性与实现宽度行为。全部用例仅对 Group 1 判定为"可读"的计数器（已实现或镜像只读零）执行；HPMMIR-05/06 依赖写入保持，需要"已实现"类计数器，镜像只读零（SPEC 合法实现）时跳过。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| HPMMIR-01 | 非特权视图与 M-mode 镜像一致 | M-mode 写 mhpmcounterN=pattern，读回 mhpmcounterN 与 hpmcounterN | 两者一致（hpmcounterN == mhpmcounterN 读回值） |
| HPMMIR-02 | 全 1 模式写入与实现宽度 | 写 mhpmcounterN=0xFFFF_FFFF_FFFF_FFFF，读回 mhpmcounterN 与 hpmcounterN | 读回值稳定（高位可能被实现宽度截断，SPEC 允许），两次读回一致 |
| HPMMIR-03 | 写入值稳定保持 | 写 mhpmcounterN=pattern（mhpmeventN=0、mcountinhibit[N]=1），间隔若干指令后多次读取 hpmcounterN | 各次读值均等于写入值（冻结配置下无漂移） |
| HPMMIR-04 | mcountinhibit 冻结后值不变 | 使 mcountinhibit[N]=1，写 mhpmcounterN 后执行一段指令序列 | 序列前后 hpmcounterN 值不变 |
| HPMMIR-05 | 双模式值切换镜像 | 先后写入两个不同 pattern，每次写入后读 hpmcounterN | 每次读值均与最新写入一致（镜像只读零时 SKIP） |
| HPMMIR-06 | 相邻计数器无串扰 | 写 mhpmcounterN=patternA、mhpmcounterN+1=patternB（两者均已实现） | hpmcounterN/hpmcounterN+1 分别读到 patternA/patternB，互不影响 |

---

## Group 4. S-mode/U-mode 读取访问

**规范依据**：
- `norm:hpmcounter_op_sz_mode`：hpmcounter3-31 为非特权（unprivileged）计数器
- `norm:zihpm_op_sz_mode_acc_priv`：经非特权 CSR 地址访问，低特权级可见性受特权架构 counteren 机制控制

**测试职责**：验证已实现的 hpmcounter 在 counteren 使能链下可被 S-mode/U-mode 读取。测试前置 mcountinhibit[N]=1、mhpmeventN=0 冻结值；counteren 位域的完整控制矩阵由 `Sm_CSR_test_plan.md` / `Ss_CSR_test_plan.md` 覆盖，本组仅验证 Zihpm 视图的基本可达性与禁闭时的异常类型。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| HPMACC-01 | S-mode 读取已实现计数器 | mcounteren[N]=1，S-mode csrr hpmcounterN | 读取成功，值与 M-mode 镜像一致 |
| HPMACC-02 | U-mode 读取已实现计数器 | mcounteren[N]=1 且 scounteren[N]=1，U-mode csrr hpmcounterN | 读取成功，值与 M-mode 镜像一致 |
| HPMACC-03 | mcounteren[N]=0 时 S-mode 访问被禁闭 | mcounteren[N]=0，S-mode csrr hpmcounterN | illegal-instruction exception (cause=2) |
| HPMACC-04 | scounteren[N]=0 时 U-mode 访问被禁闭 | mcounteren[N]=1、scounteren[N]=0，U-mode csrr hpmcounterN | illegal-instruction exception (cause=2) |

---

## Group 5. 事件选择与配置无效行为

**规范依据**：
- `norm:hpm_platform_specific_impl`：计数事件集合为平台相关
- `norm:hpm_misconfigured_event_behavior`：事件选择配置无效时，计数器可能返回常数值

**测试职责**：验证向 mhpmeventN 写入任意（含无效）事件编码不触发 trap，且对应 hpmcounterN 仍可读取，行为为 SPEC 允许的常数值或正常计数之一。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| HPMEVT-01 | mhpmevent 写入不触发 trap | M-mode 向已实现计数器对应的 mhpmeventN 写入 0、全 1 与若干探测值 | 写入过程不触发异常（WARL 字段） |
| HPMEVT-02 | 事件配置无效时计数器可读 | mhpmeventN 置无效事件值，读 hpmcounterN | 读取成功，不触发 trap |
| HPMEVT-03 | 无效配置下计数器返回常数值或计数 | mhpmeventN 置无效值，执行一段指令序列前后读取 hpmcounterN | 返回常数值，或按实现计数推进；两者均为 SPEC 允许行为 |
| HPMEVT-04 | 无效配置下行为稳定 | 重复 HPMEVT-03 多次采样 | 多次测量表现一致（恒为常数值，或恒有推进），不允许随机跳变的不确定行为 |
| HPMEVT-05 | 事件配置不影响相邻计数器 | 修改 mhpmeventN，检查 hpmcounterN+1 的值 | 相邻计数器值不受影响（前置两者均已实现且均被冻结） |

---

## Group 6. RV32 高半区访问（XLEN=32 专用，本框架不适用）

**规范依据**：
- `norm:hpm_counter_op_sz_mode_xlen32`：XLEN=32 时高 32 位经 hpmcounter3h-hpmcounter31h 访问
- `norm:zihpm_op_sz_mode_acc_xlen32`：XLEN=32 时高 32 位经 0xC80-0xC9F 访问（Zihpm 部分为 0xC83-0xC9F）

**测试职责**：以下 HPMH-01~03 用例仅在 XLEN=32 环境执行。本测试框架基于 RV64 工具链，`hpmcounter3h`-`hpmcounter31h`（0xC83-0xC9F）在 XLEN=64 下不存在，故 HPMH-01~03 在当前框架中标记为不适用，保留条目以便未来 RV32 框架复用。

RV64 侧补充用例 HPMH-04~05 由同一批 norm 反向推导：高半区 CSR "仅当 XLEN=32 存在"，故 XLEN=64 下访问 0xC83-0xC9F 必须触发非法指令异常。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| HPMH-01 | XLEN=32 高半区可访问 | XLEN=32 下读取 hpmcounter3h-hpmcounter31h（0xC83-0xC9F，限已实现计数器） | 读取成功 |
| HPMH-02 | XLEN=32 高半区只读属性 | XLEN=32 下写 hpmcounterNh | illegal-instruction exception (cause=2) |
| HPMH-03 | XLEN=32 64 位拼接一致性 | XLEN=32 下按 SPEC 示例算法（high-low-high 比对重试）拼接读取 64 位 hpmcounterN | 拼接值与两次高半区读取一致，无进位撕裂 |
| HPMH-04 | RV64 下 hpmcounter3h(0xC83) 不存在 | XLEN=64 下读取 0xC83 | illegal-instruction exception (cause=2) |
| HPMH-05 | RV64 下 hpmcounter31h(0xC9F) 不存在 | XLEN=64 下读取 0xC9F | illegal-instruction exception (cause=2) |

---

## 测试假设与约束

1. **实现数量、宽度与事件平台相关**：SPEC 明确三者均为 platform-specific，测试不假设任何计数器一定实现，也不假设具体事件编码；全部功能用例基于 Group 1 的运行时探测结果选择目标计数器，对未实现计数器跳过（SKIP）相应功能用例。
2. **镜像只读零是合法实现**：特权规范 `norm:mhpmcounter_mhpmevent_rdonly0` 允许计数器与其事件选择器均为只读 0（写被忽略、读恒为 0，不触发 trap）；分类将其单列为"镜像只读零"类，不得误判为"已实现"或 FAIL；依赖写入保持的用例（HPMMIR-05/06）在该类平台上 SKIP。
3. **未实现计数器的两种合规路径**：illegal-instruction 与常数值均为 SPEC 允许，测试不得将其中任一种判定为失败；但同一平台对同一计数器的行为必须确定（HPMD-05），且不允许出现第三种行为（如随机值、其他异常类型）。
4. **冻结值语义**：镜像一致性与只读属性用例要求计数器值在测量区间内稳定，需将 mhpmeventN 置 0（不选择事件）并使 mcountinhibit[N]=1；两者属 Smcntrpmf 范畴，此处仅作测试激励使用。
5. **mhpmcounter/mhpmevent 仅作激励**：本计划通过 M-mode 镜像寄存器构造已知值，其自身寄存器字段行为（WARL 宽度、事件编码等）由 `Smcntrpmf_test_plan.md` 覆盖，本计划不重复断言。
6. **counteren 门控**：S/U-mode 用例依赖 mcounteren/scounteren 位 N 的配置，位 N 对应 hpmcounterN；counteren 自身的位域语义与完整控制矩阵属特权规范，由 `Sm_CSR_test_plan.md` / `Ss_CSR_test_plan.md` 覆盖。
7. **模拟器行为不作假设**：若 QEMU/Spike/Sail/HW 对只读属性、未实现计数器访问行为、镜像一致性等不符合 SPEC，保持用例失败并记录到 `bugs/` 目录，不修改测试适配错误实现。

---

## 附录 A：规范点覆盖矩阵

| Norm ID | 覆盖用例 | 备注 |
|---------|----------|------|
| `norm:hpmcounter_op_sz_mode` | HPMD-01 ~ HPMD-06, HPMRO-01 ~ HPMRO-08, HPMMIR-01 ~ HPMMIR-06, HPMACC-01 ~ HPMACC-02 | 计数器集合与只读非特权视图属性 |
| `norm:hpm_counter_op_sz_mode_xlen32` | HPMH-01 ~ HPMH-03（RV32 不适用），HPMH-04 ~ HPMH-05（RV64 反向验证） | |
| `norm:hpm_counter_op_sz_mode_dependency` | 全部用例（隐式） | 依赖 Zicsr，经 CSR 指令访问覆盖，不单设用例 |
| `norm:hpm_platform_specific_impl` | HPMD-03, HPMD-06, HPMMIR-02, HPMEVT-01 ~ HPMEVT-05 | 平台相关项以探测与行为集合断言方式验证 |
| `norm:mhpmcounter_mhpmevent_rdonly0`（特权规范） | HPMD-03, HPMMIR-05, HPMMIR-06 | 镜像只读零为合法实现，分类单列并作为写保持用例的 SKIP 条件 |
| `norm:hpm_unimplemented_counter_access` | HPMD-02 ~ HPMD-05 | trap 与常数值两条合规路径均验证 |
| `norm:hpm_misconfigured_event_behavior` | HPMEVT-01 ~ HPMEVT-04 | |
| `norm:zihpm_op_sz_mode_acc_count` | HPMD-06（间接） | 主验证责任在 `zicntr_test_plan.md` |
| `norm:zihpm_op_sz_mode_acc_partition` | HPMD-01 | 本计划验证 0xC03-0xC1F 的 Zihpm 划分部分 |
| `norm:zihpm_op_sz_mode_acc_priv` | HPMRO-01 ~ HPMRO-08, HPMACC-01 ~ HPMACC-04 | 只读属性与非特权访问 |
| `norm:zihpm_op_sz_mode_acc_xlen32` | HPMH-01 ~ HPMH-05 | 与 `norm:hpm_counter_op_sz_mode_xlen32` 联合覆盖 |
| SPEC NOTE：同时刻读取多个计数器 | — | 信息性内容，不设计用例 |

---

## 平台验证记录

| 平台 | 结果 | 说明 |
|------|------|------|
| QEMU | 29 PASS / 0 FAIL / 5 SKIP | 实现 16 个计数器（自 hpmcounter3 起连续，64 位宽），其余 13 个触发 illegal-instruction；SKIP：HPMD-05（无常数值计数器）、HPMRO-08（hpmcounter31 未实现）、HPMH-01~03（RV32 专用） |
| Spike | 26 PASS / 0 FAIL / 8 SKIP | 全部 29 个计数器可读但 mhpmcounter 镜像为只读零（norm:mhpmcounter_mhpmevent_rdonly0 合法实现）；SKIP：HPMD-04/05、HPMMIR-05/06、HPMEVT-05、HPMH-01~03 |
| Sail | 29 PASS / 0 FAIL / 5 SKIP | 全部 29 个计数器已实现且镜像可写；SKIP：HPMD-04/05（无未实现计数器）、HPMH-01~03（RV32 专用） |
