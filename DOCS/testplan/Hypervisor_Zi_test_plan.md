**中文 | [English](../testplan_en/Hypervisor_Zi_test_plan_en.md)**

# Hypervisor 与 Z* 扩展交叉测试计划

> 本文档描述 Hypervisor（H）扩展与其他 Z* 系列扩展（含 Zk* 密码学扩展）在交叉场景下的测试计划。本方案从 `Hypervisor_cross_test_plan.md` 拆分而来，仅保留 Hypervisor 与 Z* 扩展交叉的内容。这些测试场景原本在各扩展的独立测试计划中被标记为"由 Hypervisor 测试计划覆盖"或"因缺少 H 扩展而排除"，但经分析发现现有 Hypervisor 测试计划并未完全覆盖。
>
> 生成时间：2026-06-22

---

## 本文档覆盖的 SPEC 章节

本方案依据以下 RISC-V 官方规范（本地路径）：

- `SPEC/riscv-isa-manual/src/priv/hypervisor.adoc` — Hypervisor（H）扩展：VS/VU-mode 访问受控 CSR 的 virtual-instruction 异常机制
- `SPEC/riscv-isa-manual/src/unpriv/zk.adoc` — Zkr 熵源扩展：`seed` CSR、`mseccfg.SSEED/USEED` 访问控制
- `SPEC/riscv-isa-manual/src/unpriv/zihintntl.adoc` — Zihintntl 扩展：NTL HINT 指令的无架构副作用语义与 trap 行为
- `SPEC/riscv-isa-manual/src/unpriv/zcmt.adoc` — Zcmt 扩展：cm.jt/cm.jalt 表跳转指令、jvt CSR 与两次隐式取指语义

官方仓库：

- https://github.com/riscv/riscv-isa-manual （对应仓库内上述路径文件）

---

## 范围

### 覆盖的扩展交叉

- **Hypervisor × Zkr**：`mseccfg.SSEED` 对 VS/VU-mode 访问 `seed` CSR 的控制、VS/VU-mode 下 virtual-instruction 与 illegal-instruction 异常类型区分、HS-mode 访问 seed 的 SSEED 控制、只读访问异常优先于 virtual-instruction
- **Hypervisor × Zihintntl**：HS/VS/VU-mode 下 NTL HINT 的正常执行（不得误触发 virtual-instruction exception）、NTL 作用于 H 扩展虚拟机访存指令（HLV/HSV/HLVX）、VS-mode 下 NTL + CMO 的 virtual-instruction 报告、VS-mode 下 NTL + target 的 G-stage guest-page-fault 报告
- **Hypervisor × Zcmt**：HS/VS/VU-mode 下表跳转指令（cm.jt/cm.jalt）的正常执行、VS/VU-mode 下 jvt CSR 的访问与 Smstateen（JVT 位）门控、VS-mode 下 JVT 表项取指的两阶段翻译与 G-stage guest instruction page fault 报告

### 不在本文档范围

- 已由 `Hypervisor_CSR_test_plan.md`、`Hypervisor_Interrupts_test_plan.md`、`Hypervisor_Exceptions_test_plan.md`、`Hypervisor_2_stage_test_plan.md`、`Hypervisor_gstage_test_plan.md` 覆盖的 Hypervisor 基础功能
- 各扩展在非 Hypervisor 场景下的行为（由各自独立测试计划覆盖）
- Hypervisor 与 Ss\*/Sv\*/Sm\* 扩展的交叉测试（分别由 `Hypervisor_Ss_test_plan.md`、`Hypervisor_Sv_test_plan.md`、`Hypervisor_Sm_test_plan.md` 覆盖）
- Zkr 非 Hypervisor 场景（M/S/U-mode 访问 seed 的基础控制）— 由 `Zkr_test_plan.md` 覆盖
- Zihintntl 非 Hypervisor 场景（M/S/U-mode 基础语义、编码、压缩变体、CMO 交互、LR/SC 前进保证等）— 由 `zihintntl_test_plan.md` 覆盖
- Zcmt 非 Hypervisor 场景（jvt WARL 行为、编码与操作语义、PMP/页表故障处理、表更新可见性与字节序等）— 由 `zcmt_test_plan.md` 覆盖

---

## 覆盖的规范点

下表列出本方案覆盖的规范点。带 `norm:` 前缀的为 SPEC 官方标签；不带前缀的为根据 SPEC 原文自行拆解的规范点。

| 规范 ID | 来源 | 描述（英文） | 描述（中文） |
|---------|------|-------------|-------------|
| `norm:mseccfg_sseed_VSorVU-mode_op` | `zk.adoc` | When the H extension is also implemented, access to the seed CSR from an HS-qualified instruction leads to a virtual-instruction exception in VS and VU modes; all other types of accesses raise an illegal-instruction exception. | 实现 H 扩展时，VS/VU 模式下 HS 限定指令访问 seed 引发虚拟指令异常；其他访问类型引发非法指令异常。 |
| `norm:mseccfg_sseed_SorHS-mode_op` | `zk.adoc` | When SSEED is 0, access to the seed CSR from S-/HS-mode raises an illegal-instruction exception. When SSEED is 1, read-write access to the seed CSR from S-/HS-mode is allowed; all other types of accesses raise an illegal-instruction exception. | SSEED=0 时 S/HS 模式访问 seed 引发非法指令异常；SSEED=1 时允许读写访问，其他访问类型仍引发非法指令异常。 |
| `norm:mseccfg_sseed_useed_op_tbl` | `zk.adoc` | Entropy Source Access Control table: M always available; U controlled by USEED; S/HS controlled by SSEED; VS/VU controlled by SSEED with virtual-instruction exception for HS-qualified read-write. | 熵源访问控制表：M 模式始终可用；U 模式由 USEED 控制；S/HS 由 SSEED 控制；VS/VU 由 SSEED 控制且 HS 限定的读写引发虚拟指令异常。 |
| `norm:seed_ro_illegal` | `zk.adoc` | Attempts to access the seed CSR using a read-only CSR-access instruction (csrrs/csrrc with rs1=x0 or csrrsi/csrrci with uimm=0) raise an illegal-instruction exception; any other CSR-access instruction may be used to access seed. | 使用只读 CSR 访问指令访问 seed 引发非法指令异常；其他 CSR 访问指令可用于访问 seed。 |
| `norm:NTL_target_definition` | `zihintntl.adoc` | The insn:ntl[] instructions do not change architectural state, nor do they alter the architecturally visible effects of the target instruction. | NTL 指令不改变架构状态，也不改变 target 指令的架构可见效果（虚拟化环境下同样适用）。 |
| `norm:NTL_range` | `zihintntl.adoc` | The insn:ntl[] instructions affect all memory-access instructions except the cache-management instructions in the ext:zicbom[] extension. | NTL 指令影响所有内存访问指令（含 H 扩展的 HLV/HSV/HLVX 虚拟机访存指令），Zicbom 的 cache-management 指令除外。 |
| `norm:cm-jt_op` | `zcmt.adoc` | cm.jt reads an entry from the jump vector table in memory and jumps to the address that was read. | cm.jt 从内存中的跳转向量表读取一个表项并跳转到读到的地址。 |
| `norm:cm-jalt_op` | `zcmt.adoc` | cm.jalt reads an entry from the jump vector table in memory and jumps to the address that was read, linking to _ra_. | cm.jalt 从内存中的跳转向量表读取一个表项并跳转到读到的地址，同时将返回地址链接到 ra。 |
| `norm:jvt_base_vm` | `zcmt.adoc` | jvt[base] is a virtual address, whenever virtual memory is enabled. | 虚拟内存启用时（含 VS-mode 的 vsatp 翻译），jvt.base 是虚拟地址。 |
| `norm:Zcmt_fetch` | `zcmt.adoc` | ... the execution of a table jump instruction involves two instruction fetches, the first to read the instruction (cm.jt/cm.jalt) and the second to read from the jump vector table (JVT). Both instruction fetches are _implicit_ reads, and both require execute permission; read permission is irrelevant. | 表跳转涉及两次指令取指：第一次取指令本身，第二次取 JVT 表项；两次均为隐式读且都要求执行权限，读权限无关。 |
| `norm:Zcmt_trap` | `zcmt.adoc` | If an exception occurs on either instruction fetch, xEPC is set to the PC of the table jump instruction, xCAUSE is set as expected for the type of fault and xTVAL (if not set to zero) contains the fetch address which caused the fault. | 任一次取指发生异常时，xEPC 设为表跳转指令的 PC，xCAUSE 按故障类型设置，xTVAL（若实现写非零）为引发故障的取指地址。 |
| `norm:stateen0_jvt_op` | `smstateen.adoc` | The JVT bit controls access to the `jvt` CSR provided by the Zcmt extension. | stateen0 的 JVT 位控制对 Zcmt 提供的 jvt CSR 的访问。 |
| `norm:htval_trapval` | `hypervisor.adoc` | htval trap value reporting for guest-page faults (implementation may write zero or the faulting GPA>>2). | guest-page fault 时 htval 的故障值报告（实现允许写零或故障 GPA>>2）。 |

---

## Group 1. Hypervisor × Zkr 交叉测试

**规范依据**：
- `norm:mseccfg_sseed_VSorVU-mode_op`：实现 H 扩展时，VS/VU 模式下 HS 限定指令访问 seed 引发 virtual-instruction exception；其他访问引发 illegal-instruction exception
- `norm:mseccfg_sseed_useed_op_tbl`：VS/VU + SSEED=0 时任何访问引发 illegal-instruction exception；VS/VU + SSEED=1 时读写访问引发 virtual-instruction exception
- `norm:mseccfg_sseed_SorHS-mode_op`：SSEED=0 时 HS-mode 访问 seed 引发 illegal-instruction exception；SSEED=1 时允许读写访问
- `norm:seed_ro_illegal`：只读访问在任何模式下都引发 illegal-instruction exception

**测试职责**：验证 H 扩展下 VS/VU-mode 和 HS-mode 访问 seed CSR 的异常类型区分与访问控制。

### 1.1 HS-mode 访问控制（SSEED）

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZKR-HYP-01 | SSEED=1 HS-mode csrrw 访问 seed 正常 | 设 mseccfg.SSEED=1（H 扩展下 HS-mode），HS-mode 执行 csrrw rd, seed, x0 | 正常返回 seed 值 |
| ZKR-HYP-02 | SSEED=0 HS-mode csrrw 访问 seed 触发异常 | 设 mseccfg.SSEED=0，HS-mode 执行 csrrw rd, seed, x0 | illegal-instruction exception (cause=2) |
| ZKR-HYP-13 | SSEED=1 HS-mode 只读访问触发异常 | 设 mseccfg.SSEED=1，HS-mode 执行 csrrs rd, seed, x0 | illegal-instruction exception (cause=2) |

### 1.2 VS/VU-mode 访问控制（SSEED + H 扩展）

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZKR-HYP-03 | SSEED=0 VS-mode csrrw 访问 seed 触发 illegal | 设 mseccfg.SSEED=0，VS-mode 执行 csrrw rd, seed, x0 | illegal-instruction exception (cause=2) |
| ZKR-HYP-04 | SSEED=1 VS-mode csrrw 访问 seed 触发 virtual-instruction | 设 mseccfg.SSEED=1，VS-mode 执行 csrrw rd, seed, x0 | virtual-instruction exception (cause=22) |
| ZKR-HYP-05 | SSEED=0 VU-mode csrrw 访问 seed 触发 illegal | 设 mseccfg.SSEED=0，VU-mode 执行 csrrw rd, seed, x0 | illegal-instruction exception (cause=2) |
| ZKR-HYP-06 | SSEED=1 VU-mode csrrw 访问 seed 触发 virtual-instruction | 设 mseccfg.SSEED=1，VU-mode 执行 csrrw rd, seed, x0 | virtual-instruction exception (cause=22) |
| ZKR-HYP-07 | SSEED=1 VS-mode 只读访问触发 illegal（非 virtual） | 设 mseccfg.SSEED=1，VS-mode 执行 csrrs rd, seed, x0 | illegal-instruction exception (cause=2)（只读访问条件优先） |
| ZKR-HYP-08 | SSEED=1 VS-mode csrrsi uimm=0 触发 illegal | 设 mseccfg.SSEED=1，VS-mode 执行 csrrsi rd, seed, 0 | illegal-instruction exception (cause=2) |
| ZKR-HYP-09 | SSEED=1 VS-mode csrrs(rs1≠x0) 触发 virtual-instruction | 设 mseccfg.SSEED=1，VS-mode 执行 csrrs rd, seed, t0 (t0≠0) | virtual-instruction exception (cause=22)（HS 限定的读写） |
| ZKR-HYP-10 | SSEED=0 VS-mode csrrs(rs1≠x0) 触发 illegal | 设 mseccfg.SSEED=0，VS-mode 执行 csrrs rd, seed, t0 | illegal-instruction exception (cause=2) |
| ZKR-HYP-11 | SSEED 不影响 M-mode（VS/VU 场景下） | 设 mseccfg.SSEED=0，M-mode csrrw seed | 正常访问 |
| ZKR-HYP-14 | SSEED=1 VU-mode 只读访问触发 illegal（非 virtual） | 设 mseccfg.SSEED=1，VU-mode 执行 csrrs rd, seed, x0 | illegal-instruction exception (cause=2)（只读访问条件优先） |
| ZKR-HYP-15 | SSEED=1 VU-mode csrrsi uimm=0 触发 illegal | 设 mseccfg.SSEED=1，VU-mode 执行 csrrsi rd, seed, 0 | illegal-instruction exception (cause=2) |
| ZKR-HYP-16 | SSEED=1 VU-mode csrrs(rs1≠x0) 触发 virtual-instruction | 设 mseccfg.SSEED=1，VU-mode 执行 csrrs rd, seed, t0 (t0≠0) | virtual-instruction exception (cause=22)（HS 限定的读写） |
| ZKR-HYP-17 | SSEED=0 VU-mode csrrs(rs1≠x0) 触发 illegal | 设 mseccfg.SSEED=0，VU-mode 执行 csrrs rd, seed, t0 | illegal-instruction exception (cause=2) |
| ZKR-HYP-18 | SSEED=1 VS-mode csrrci uimm=0 触发 illegal | 设 mseccfg.SSEED=1，VS-mode 执行 csrrci rd, seed, 0 | illegal-instruction exception (cause=2) |

### 1.3 异常优先级与组合场景

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZKR-HYP-12 | VU-mode 只读异常优先于 virtual-instruction | SSEED=1，VU-mode 执行 csrrs rd, seed, x0 | illegal-instruction exception (cause=2)（只读条件 → illegal，非 virtual-instruction） |

> [!NOTE]
> - 本组测试验证 Zkr 扩展在 Hypervisor 场景下的行为。所有测试必须在运行时通过 `HAS_H_EXT()` 检测 H 扩展的可用性，不可用时 TEST_SKIP。
> - ZKR-HYP-03~18 从 `Zkr_test_plan.md` 迁移而来，专门针对依赖 H 扩展的用例。核心语义：VS/VU-mode 访问 seed 时，`mseccfg.SSEED` 决定是否放行，且 HS 限定的读写（SSEED=1）触发 **virtual-instruction**（cause=22），而 SSEED=0 或只读访问触发 **illegal-instruction**（cause=2）。
> - **virtual-instruction 与 illegal-instruction 的区分**：测试断言必须使用准确的 cause 常量，区分 SSEED=1 时的 virtual-instruction（cause=22）与只读访问/SSEED=0 时的 illegal-instruction（cause=2）。

---

## Group 2. Hypervisor × Zihintntl 交叉测试

**规范依据**：
- `norm:NTL_target_definition`：NTL 指令不改变架构状态，也不改变 target 指令的架构可见效果；虚拟化环境下 NTL 前缀序列的架构行为必须与非虚拟化时一致
- `norm:NTL_range`：NTL 影响所有内存访问指令，H 扩展的 HLV/HSV/HLVX 虚拟机访存指令亦在作用范围内

**测试职责**：验证 NTL HINT 在虚拟化环境（HS/VS/VU-mode）下的行为与非虚拟化场景一致：正常执行、不误触发 virtual-instruction exception，且 trap 报告信息（cause/epc/tval/GVA/htval）与不带前缀时完全一致。本组用例从 `zihintntl_test_plan.md` 迁移而来（原 NTL-12 虚拟化部分、NTL-RG-07、NTL-CMO-08、NTL-TRAP-08）。

### 2.1 HS/VS/VU-mode 下 NTL 执行

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| NTL-HYP-01 | HS-mode 执行 NTL + load | HS-mode 执行 ntl.all + ld，比对目标寄存器与内存 | 正常执行，无异常，结果与不带前缀一致 |
| NTL-HYP-02 | VS-mode 执行 NTL + load | 切入 VS-mode 执行 ntl.all + ld | 正常执行，无 virtual-instruction exception，结果与不带前缀一致 |
| NTL-HYP-03 | VU-mode 执行 NTL + load | 切入 VU-mode 执行 ntl.all + ld | 正常执行，无异常，结果与不带前缀一致 |

### 2.2 NTL 作用于 H 扩展虚拟机访存指令

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| NTL-HYP-04 | NTL 作用于 HLV/HSV/HLVX | HS-mode 以 ntl.all 前缀执行 HLV/HSV/HLVX，对照不带前缀的相同序列 | 正常执行，访存效果正确，两者架构行为一致 |

### 2.3 VS-mode 下 NTL + CMO 的 trap 报告

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| NTL-HYP-05 | ntl 前缀不改变 CMO 指令的 virtual-instruction 报告 | 若实现 Zicbom：VS-mode 下 henvcfg.CBIE=0 且 CBCFE=0 时执行「ntl.all + cbo.inval/cbo.clean」，对照不带前缀的相同序列 | 与不带前缀一样触发 virtual-instruction exception (cause=22)，cause/epc 指向 CMO 指令而非 NTL |

### 2.4 VS-mode 下 NTL + target 的 G-stage 缺页

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| NTL-HYP-06 | VS-mode 下 NTL + target 的 G-stage 缺页 | VS-mode 执行「ntl.p1 + ld」触发 G-stage guest-page-fault，对照不带前缀的相同序列 | 递送到 HS-mode，load guest-page fault (cause=21)，hstatus.GVA=1，stval/htval 正确，行为与不带前缀一致 |

> [!NOTE]
> - 本组所有测试必须在运行时通过 `HAS_H_EXT()` 检测 H 扩展的可用性，不可用时 TEST_SKIP。
> - Zihintntl 无独立 misa/CSR 探测标志，按平台配置声明启用；NTL 指令以 raw encoding（.word 0x00200033/0x00300033/0x00400033/0x00500033）注入执行流，避免工具链别名干扰。
> - NTL-HYP-05 另需探测 Zicbom；NTL-HYP-06 需构造 VS-stage 有效、G-stage 无效的映射。核心断言策略沿用 `zihintntl_test_plan.md` 的「HINT 无副作用对照法」：带/不带 NTL 前缀的架构可见行为（含异常信息）必须完全一致。

---

## Group 3. Hypervisor × Zcmt 交叉测试

**规范依据**：
- `norm:cm-jt_op` / `norm:cm-jalt_op`：表跳转为普通指令，无特权级限制，HS/VS/VU-mode 下均应正常执行
- `norm:jvt_base_vm`：虚拟内存启用时 jvt.base 为虚拟地址，VS-mode 下经 vsatp 两阶段翻译
- `norm:Zcmt_fetch` / `norm:Zcmt_trap`：第二次取指（JVT 表项）同样经过翻译，故障时 xEPC 指向表跳转指令、xTVAL 为故障取指地址
- `norm:stateen0_jvt_op`：stateen0 的 JVT 位控制 jvt CSR 访问，仅门控 CSR 访问、不门控指令执行
- `norm:htval_trapval`：G-stage 故障时 htval 的故障值报告规则

**测试职责**：验证表跳转指令与 jvt CSR 在虚拟化环境下的行为：HS/VS/VU-mode 正常执行不误触发 virtual-instruction exception；VS-mode 下 JVT 表项取指经两阶段翻译，G-stage 故障按 guest instruction page fault 报告；hstateen0.JVT 门控 VS/VU 的 jvt 访问但不门控指令执行。本组用例从 `zcmt_test_plan.md` 迁移而来（原 ZCMT-27/28、ZACC-03/04/05 虚拟化部分、ZACC-06 虚拟化部分；HZCMT-08 为对照 ZCMT-25 补充的 VS-stage 故障用例）。

### 3.1 HS/VS/VU-mode 表跳转执行

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| HZCMT-01 | HS-mode 执行表跳转 | HS-mode 执行 cm.jt 与 cm.jalt（jvt 指向有效表） | 均正常跳转与链接，无异常 |
| HZCMT-02 | VS-mode 执行表跳转 | VS-mode 执行 cm.jt 与 cm.jalt | 均正常执行，无 virtual-instruction exception |
| HZCMT-03 | VU-mode 执行表跳转 | VU-mode 执行 cm.jt 与 cm.jalt | 均正常执行，无异常 |

### 3.2 VS/VU-mode jvt 访问与 stateen 门控

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| HZCMT-04 | VS/VU-mode 访问 jvt | stateen 使能状态下，VS/VU-mode csrr/csrw jvt | 访问正常（jvt 权限 URW + stateen 使能） |
| HZCMT-05 | hstateen0.JVT 门控 VS/VU 访问 | 实现 Smstateen 时按层级清零 hstateen0/sstateen0 的 JVT 位，VS/VU 访问 jvt | VS/VU 触发 virtual-instruction/illegal-instruction（详细用例见 `Smstateen_test_plan.md` / `Ssstateen_test_plan.md`） |
| HZCMT-06 | stateen 不门控表跳转指令执行 | 实现 Smstateen 时清零各级 stateen 的 JVT 位，VS-mode 执行 cm.jt/cm.jalt | 指令正常执行（state enable 仅门控 jvt CSR 访问，不门控指令本身） |

### 3.3 VS-mode 两阶段翻译下的表跳转

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| HZCMT-07 | VS-mode 翻译路径正常跳转 | VS-mode 启用 vsatp，VS-stage 映射表页 X=1，执行 cm.jt | 跳转成功（VS-stage 翻译生效） |
| HZCMT-08 | VS-mode 表页 VS-stage X=0 触发故障 | 表所在页 VS-stage 映射但 X=0，VS-mode 执行 cm.jt | instruction page fault 按委托路径递送，sepc=cm.jt PC，stval=表项虚拟地址 |
| HZCMT-09 | VS-mode 第二次取指 G-stage 故障 | G-stage 表页映射无效，VS-mode 执行 cm.jt | guest instruction page fault (cause=20) 递送至 HS-mode，hstatus.GVA=1，htval=故障表项 GPA>>2（norm:htval_trapval 允许为零） |

> [!NOTE]
> - 本组所有测试必须在运行时通过 `HAS_H_EXT()` 检测 H 扩展的可用性，不可用时 TEST_SKIP；Zcmt 支持以平台配置 `ZCMT_SUPPORTED` 宏为准，jvt 可写性探测结果（只读实现，`norm:jvt_op` 允许）决定功能用例是否适用。
> - HZCMT-05 另需探测 Smstateen；HZCMT-07~09 需启用 vsatp（及 hgatp）构造两阶段翻译，VS-stage 有效 + G-stage 无效的映射组合用于隔离 G-stage 故障。
> - HZCMT-08/09 验证第二次取指（JVT 表项）的故障路径：vsepc 必须指向 cm.jt 指令本身而非表地址（`norm:Zcmt_trap`），vstval/htval 报告表项取指地址。

---

## 关键注意事项

1. **扩展检测**：所有测试必须在运行时检测所需扩展（H、Zkr、Zihintntl、Zcmt 等）的可用性，不可用时 TEST_SKIP。Zkr 通过 `seed` CSR（0x015）的存在性探测；Zihintntl 无独立探测标志，按平台配置声明启用；Zcmt 以平台配置 `ZCMT_SUPPORTED` 宏与 jvt CSR（0x017）trap-armed 探测为准。

2. **SSEED 控制**：`mseccfg.SSEED` 控制 S/HS/VS/VU-mode 对 seed CSR 的访问。M-mode 访问不受 SSEED 影响（ZKR-HYP-11）。

3. **只读访问优先**：只读 CSR 访问指令（csrrs/csrrc with rs1=x0，或 csrrsi/csrrci with uimm=0）访问 seed 在任何模式下都触发 illegal-instruction（cause=2），该条件优先于 virtual-instruction 判定。

4. **virtual-instruction 与 illegal-instruction 的区分**：VS/VU-mode 访问受控 CSR 时，SSEED=1 的 HS 限定读写触发 virtual-instruction (cause=22)；SSEED=0 或只读访问触发 illegal-instruction (cause=2)。

---

## 参考

- `SPEC/hypervisor.adoc` — RISC-V Hypervisor Extension, Version 1.0
- `SPEC/riscv-isa-manual/src/unpriv/zk.adoc` — Zkr Entropy Source Extension
- `SPEC/riscv-isa-manual/src/unpriv/zihintntl.adoc` — Zihintntl Extension for Non-Temporal Locality Hints
- `SPEC/riscv-isa-manual/src/unpriv/zcmt.adoc` — Zcmt Extension for Compressed Table Jumps
- `DOCS/testplan/Zkr_test_plan.md` — Zkr 独立测试计划
- `DOCS/testplan/zihintntl_test_plan.md` — Zihintntl 独立测试计划
- `DOCS/testplan/zcmt_test_plan.md` — Zcmt 独立测试计划
- `DOCS/testplan/Hypervisor_CSR_test_plan.md` — Hypervisor CSR 子集测试计划
- `DOCS/testplan/Hypervisor_Interrupts_test_plan.md` — Hypervisor 中断子集测试计划
- `DOCS/testplan/Hypervisor_Exceptions_test_plan.md` — Hypervisor 异常与 trap 子集测试计划
- `DOCS/testplan/Hypervisor_2_stage_test_plan.md` — 两阶段翻译测试计划
- `DOCS/testplan/Hypervisor_gstage_test_plan.md` — G-stage 独立测试计划

---

## 附录 A：规范点覆盖矩阵

下表标明"覆盖的规范点"章节中每条规范点被哪些测试用例覆盖。

| Norm ID | 覆盖的测试 ID |
|---------|---------------|
| `norm:mseccfg_sseed_SorHS-mode_op` | ZKR-HYP-01、ZKR-HYP-02、ZKR-HYP-13 |
| `norm:mseccfg_sseed_VSorVU-mode_op` | ZKR-HYP-03~06、ZKR-HYP-09、ZKR-HYP-10、ZKR-HYP-16、ZKR-HYP-17 |
| `norm:mseccfg_sseed_useed_op_tbl` | ZKR-HYP-03~06、ZKR-HYP-11 |
| `norm:seed_ro_illegal` | ZKR-HYP-07、ZKR-HYP-08、ZKR-HYP-12、ZKR-HYP-13、ZKR-HYP-14、ZKR-HYP-15、ZKR-HYP-18 |
| `norm:NTL_target_definition` | NTL-HYP-01 ~ NTL-HYP-06 |
| `norm:NTL_range` | NTL-HYP-04 |
| `norm:cm-jt_op` | HZCMT-01 ~ HZCMT-03、HZCMT-06 ~ HZCMT-09 |
| `norm:cm-jalt_op` | HZCMT-01 ~ HZCMT-03、HZCMT-06 |
| `norm:jvt_base_vm` | HZCMT-07 ~ HZCMT-09 |
| `norm:Zcmt_fetch` | HZCMT-08、HZCMT-09 |
| `norm:Zcmt_trap` | HZCMT-08、HZCMT-09 |
| `norm:stateen0_jvt_op` | HZCMT-04 ~ HZCMT-06 |
| `norm:htval_trapval` | HZCMT-09 |
