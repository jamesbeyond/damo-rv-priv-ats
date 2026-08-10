# T-SBI（ACT）适配方案

**状态：** 草案 v1.2（基于 ACT 最新文档从头起草，取代此前项目内的 T-SBI 相关草稿文档；v1.1 新增 §2.7~§2.10：T-SBI 边界定义、实现责任与 rvtest_trap_handler.h 复用策略；v1.2 新增 §4.7：trap handler 职责切分模型、典型路径与 T-SBI server 依赖关系）

**依据文档（riscv-arch-test 仓库，ACT4 演进版）：**

| 文档 | 路径 | 内容 |
|------|------|------|
| Abstraction Layer | `docs/ctp/src/abstraction.adoc` | T-SBI 调用约定、操作码表、RVMODEL 宏、boot 序列 |
| RVA23 CRD rev 0.2 | `docs/crd/src/rva23_crd.adoc` | RVA23 认证对执行环境（T-SBI）的要求 |
| Trap Handler 参考实现 | `tests/env/rvtest_trap_handler.h` | T-SBI dispatch、静态指令表、GOTO_xMODE 实现 |

---

## 1. 背景与目标

ACT 框架正向 RVA23 认证体系（ACT4）演进。其核心变化是：**认证测试不再假设被测对象实现标准 M-mode**，而是要求存在一个"Supervisor Execution Environment（SEE）"，测试以 S-mode 为中心运行，通过轻量级的 **T-SBI（Test Supervisor Binary Interface）** 向执行环境请求服务（特权切换、M-mode CSR 访问、内存映射 I/O 访问等）。

RVA23 CRD 明确：

- 认证**不测试** M-mode trap 行为（除 S-mode ecall 外），几乎所有异常委托给 S-mode；
- M-mode 中断（MEI/MTI/MSI）**OUT-OF-SCOPE**；
- 执行环境必须实现 T-SBI，且对每个"会影响低特权级行为的可写 M-mode CSR 字段"提供 emulate 能力（以支持 custom M-mode）。

本项目（C 语言特权测试框架）目前以 **M-mode 为中心**：测试主要在 M-mode 编排，trap 记录全部落在 M-mode handler，特权切换用内存传参的私有 ecall 约定。若要与 ACT/RVA23 认证体系对齐（复用其测试思想、未来向认证测试套件靠拢，或让我们的用例能在"custom M-mode + T-SBI"模型下运行），需要对框架做系统性适配。

**本方案目标：**

1. 在 `common/` 中实现与 ACT 完全兼容的 T-SBI 服务端（M-mode ecall 服务）与客户端 API；
2. 将框架 ecall 约定从"内存传参"迁移到 ACT 的"寄存器传参（a0/a1）"约定；
3. 提供 RVA23 profile 引导模式（boot 到 S-mode、按 CRD 配置委托），使 trap 记录与测试编排可在 S-mode 为中心的模式下工作；
4. 保持现有 M-mode 为中心的测试套件在迁移期间可持续运行（分阶段迁移，不搞一次性大爆炸切换）。

---

## 2. T-SBI 规格要点（摘自 ACT 文档）

### 2.1 调用约定

- 通过 `ecall` 发起；**操作码放在 a0**，可选参数放 a1（个别操作可用 a2），返回值在 a0；
- handler **不得**把 a0/a1 用作内部临时寄存器（唯一允许被改写的返回寄存器是 a0）；
- 与已批准 SBI（OpenSBI）**不兼容**，专为本测试框架设计（无 1MB firmware、兼容 RV32E 不用 a6/a7）。

### 2.2 T-SBI 基础调用（CTP 定义，必须实现）

| 函数名 | 操作码（a0） | 语义 |
|--------|--------------|------|
| TSBI_ECALL_TEST | `0x00000073` | 测试 ecall 路径，返回 xEPC 于 a0（证明进入了 handler） |
| TSBI_CSR_SET | `0x<CSR>5a073` | 执行 CSRS 语义，a1 = 置位值 |
| TSBI_CSR_CLEAR | `0x<CSR>5b073` | 执行 CSRC 语义，a1 = 清除值 |
| TSBI_CSR_WRITE | `0x<CSR>59073` | 执行 CSRW 语义，a1 = 写入值 |
| TSBI_CSR_READ | `0x<CSR>02573` | 执行 CSRR 语义，结果返回 a0 |
| TSBI_LW | `0x0005a503` | `lw a0, 0(a1)`，M 权限读内存映射 I/O（如 mtime） |
| TSBI_LW+4 | `0x0045a503` | `lw a0, 4(a1)` |
| TSBI_LD | `0x0005b503` | `ld a0, 0(a1)`（RV64） |
| TSBI_SW | `0x00a5a023` | `sw a0, 0(a1)`，M 权限写内存映射 I/O |
| TSBI_SW+4 | `0x00a5a223` | `sw a0, 4(a1)` |
| TSBI_SD | `0x00a5b023` | `sd a0, 0(a1)`（RV64） |

即：**CSR 访问与内存访问的操作码就是目标指令的完整机器码编码**（自描述，handler 无需查语义表，直接执行该编码）。

### 2.3 ACT 附加系统调用（ACT trap handler 提供）

| 函数名 | 操作码（a0） | 语义 |
|--------|--------------|------|
| TSBI_GOTO_MMODE | `0x1` | 从任意特权级切换到 M-mode |
| TSBI_GOTO_SMODE | `0x2` | 切换到 S/HS-mode |
| TSBI_GOTO_UMODE | `0x3` | 切换到 U-mode |
| TSBI_GOTO_VSMODE | `0x4` | 切换到 VS-mode |
| TSBI_GOTO_VUMODE | `0x5` | 切换到 VU-mode |

**分层 dispatch 语义**（S/HS、VS handler 实现同样的调用）：
- 能在当前特权级完成的请求就地处理（如 S-mode handler 处理 S-CSR 访问、GOTO_S/U）；
- 不能完成的（GOTO_MMODE、M-mode CSR 访问、GOTO_VS/VU）**通过再次 ecall 沿特权链上传**，参数 a0/a1 保持不变。

**Dispatch 顺序**（handler 内）：
1. `(a0-1) < 5` → GOTO_xMODE；
2. `a0 == 0x73` → ECALL_TEST；
3. `a0[6:0] == 0x73 && a0[14:12] != 0` → CSR_ACCESS（SYSTEM opcode 且 funct3≠0）；
4. `a0[6:0]` 匹配 load/store 编码 → 内存访问；
5. 其它 → 返回 `-1`（TSBI_RESERVED_RET），或按 ACT 参考实现打印错误码并 HALT_FAIL。

### 2.4 ACT 参考实现的 CSR 执行机制（重要变化）

早期 ACT 草案用"写指令到内存 + fence.i + 跳转执行"的自修改代码方式。当前 `rvtest_trap_handler.h` 已改为**静态指令表**：

- 预生成指令表 `tsbi_instr_table`：每个允许的 CSR 用宏 `TSBI_CSR_INSTR_TABLE(csr)` 生成 4 条指令（csrr a0 / csrw a1 / csrs a1 / csrc a1），每条后跟 `ret`；表尾以 `.word 0` 哨兵结束；
- handler 线性查表匹配 a0 的编码，命中则 `jalr` 进表执行（ra 指向表后统一出口），经 `ret` 返回后统一推进 xEPC 并 xret；
- 未命中 → 打印编码并 HALT_FAIL。
- load/store 条目同样是表内静态指令（`lw a0,0(a1)` / `sw a2,0(a1)` 等，RV64 另加 ld/sd）。

**本方案采用静态指令表方案**（与 ACT 当前实现对齐，避免自修改代码在各平台的 i-cache/PMP-X 问题）。

### 2.5 RVA23 CRD 对执行环境的额外要求

**Boot 与委托：**

- 执行环境 boot 到确定性状态并让测试运行在 S-mode；
- `medeleg = 0xFCB5FF`：委托所有异常，**除** ecall-from-S（9）、double-trap（16）、ecall-from-M（11）；
- `mideleg = 0x366`：委托 SSI/VSSI/STI/VSTI/SEI/VSEI/SGEI/LCOFI；
- ecall-from-S **不得委托**（T-SBI 必须 trap 到 M-mode 处理）；
- 硬件不支持委托的 trap，由 M-mode handler **软件 relay**：用 mstatus/sstatus、sepc、scause、stval 重建 S 端现场后跳入 S-mode handler；
- 预留 **invisible trap** 钩子（固件透明修复硬件缺陷的机制，如 emulate 某条坏指令）。

**T-SBI 必须可控制（emulate）的 M-mode CSR：**

| CSR | 字段 |
|-----|------|
| mstatus | TSR、TW、TVM |
| mcounteren | 所有非硬连线为 0 的字段 |
| mcountinhibit | 所有非硬连线为 0 的字段 |
| mip | SEIP、STIP、SSIP（经 T-SBI 访问控制） |
| menvcfg | STCE、PBMTE、ADUE、PMM、CBZE、CBCFE、CBIE、SSE、LPE |
| mseccfg | SSEED、USEED（Zkr 时） |
| mstateen0 | SE0、ENVCFG、CONTEXT |
| medeleg | 除 ecall-from-S 外尽量委托，不可写 1 的位走软件 relay |
| mideleg | SEI/STI/SSI/LCOFI 尽量委托；VSEI/VSTI/VSSI/SGEI 按规范只读 1 |

**仅读：** mhartid、mtime。
**OUT-OF-SCOPE：** mstatus 其余字段（可经 sstatus/hstatus 观察者不测）、mie（经 sie/hie 控制）、misa、mvendorid/marchid/mimpid/mconfigptr、mtvec、mepc、mcause、mscratch、mtval、mtinst、mtval2、mtimecmp（MTI 不测）、msip（MSI 不测）、PMP CSR（PMA 等价）。

**中断范围：** SEI/STI/SSI/LCOFI + VSEI/VSTI/VSSI/SGEI IN-SCOPE；MEI/MTI/MSI OUT-OF-SCOPE。

### 2.6 Boot 序列 CSR 初始化（abstraction.adoc）

M-mode 阶段：mie/mip=0、medeleg/mideleg=0（S-boot 时再配）、mstatus（SXLEN/UXLEN=MXLEN 等）、menvcfg、mstateen0、mcountinhibit、mseccfg、mnstatus（Smrnmi）、pmpaddr0/pmpcfg0、关虚拟内存 + sfence.vma。
S-mode 阶段：medeleg=0xFCB5FF、mideleg=0x366、sstateen0、scounteren=全 1、senvcfg。
Hypervisor 测试定义 BOOT_TO_SMODE 进入 HS-mode，VS/VU 由测试自行建立。

### 2.7 约定内容与未约定内容清单

**已明确约定（规范性内容）：**

| 类别 | 内容 | 出处 |
|------|------|------|
| 调用约定 | `ecall` 发起；操作码放 a0；参数放 a1（必要时 a2）；结果经 a0 返回；其它 a0 值保留 | abstraction.adoc §T-SBI |
| 基础操作码表 | ECALL_TEST=0x73；CSR_SET/CLEAR/WRITE/READ（= 完整 CSR 指令编码）；LW/LW+4/LD/SW/SW+4/SD（= load/store 指令编码） | abstraction.adoc t-tsbi 表 |
| ACT 附加调用 | GOTO_MMODE/SMODE/UMODE/VSMODE/VUMODE = 1~5 | abstraction.adoc t-syscalls 表 |
| 分层 dispatch 语义 | S/HS 和 VS handler 实现同样的调用：本地能做的就地做，做不了的（GOTO_MMODE、M-CSR 访问）以不变的 a0/a1 再 ecall 上传 | abstraction.adoc NOTE |
| 寄存器契约 | handler 不得把 a0/a1 用作内部临时寄存器；唯一允许留给调用者可见修改的是 a0（返回值） | rvtest_trap_handler.h 文件头注释 |
| 执行环境可观察契约（认证侧） | 必须处理 ecall-from-S 并实现 T-SBI；必须能 emulate 影响低特权级行为的 M-CSR 字段；委托策略；invisible-trap relay；中断 scope（M 中断 OUT-OF-SCOPE） | rva23_crd.adoc |

**未约定 / 明确留白（文档中可查证的 TODO 与开放点）：**

1. **服务端内部实现完全自由**——CSR 指令如何执行（自修改代码 vs 静态指令表）文档只给 "a possible implementation" 示例，ACT 自己的参考实现前后两版就换了方案；
2. **VS-mode 是否需要 T-SBI**：rvtest_trap_handler.h 明确 TODO（"TODO DH 7/7/26: is another T-SBI required for VS-mode trap handler?"），VS 段当前为空；
3. **S→M 转发 ecall 的 mepc 修正**：header 标注 TODO，目前仅在 medeleg[8]=0 时正确工作；
4. **medeleg/mideleg 只读 0 位的软件委托记录**：指令表中这两个 CSR 被注释掉，标注 TODO；
5. **trap handler 本身算不算 T-SBI 的一部分**：abstraction.adoc 末尾原文 TODO——"Is trap handler considered part of T-SBI?"，ACT 自身未定稿边界；
6. 请求不可满足时的行为（如无 H 扩展时 GOTO_VSMODE）、错误码细节（CTP 层面只说 "other values of a0 are reserved"，"返回 -1" 是实现层约定）；
7. **GOTO_xMODE 不属于核心契约**——文档明说是 "ACT trap handlers implement an additional set of convenience calls"，且 "Another suite with different trap handlers might not support these calls"。核心强制的只有 ECALL_TEST + CSR + 内存访问这一组。

### 2.8 T-SBI 的性质：接口约定 vs M 态执行代码

两层含义并存，取决于出处：

- **CTP（abstraction.adoc）角度**：T-SBI 是一个**接口约定**——调用约定 + 操作码表 + dispatch 语义。文档说 "The test must **implement** a custom T-SBI"，即接口必须被实现，但规范本身只到接口层；
- **CRD（rva23_crd.adoc）角度**：T-SBI 的交付物被**扩展为整个执行环境**，原文："The T-SBI also encompasses boot code and an M-mode trap handler. Both might need to be modified by the user to accommodate custom M-mode behavior." 即认证语境下 T-SBI = boot 代码 + M-mode trap handler + 服务 dispatch 这一整套 SEE（Supervisor Execution Environment）。

**关键设计意图**：M 态代码本身**故意不被标准化**——RVA23 允许 Custom M-mode，标准只约束"从 S-mode 看进去的可观察契约"（ecall 能被处理、M-CSR 字段行为可 emulate、trap 能 relay 到 S）。ACT 提供的那套 M 态实现只是**跑在标准 M-mode 上的参考实现**，CRD 原文："The certification suite provides a reference T-SBI implementation that runs on standard M-mode, but custom M-mode ... might have to provide its own T-SBI implementation."

准确表述：**T-SBI = 接口约定（规范核心）+ 参考实现的 M 态执行环境（ACT 提供、允许替换）**。

### 2.9 实现责任：客户端与服务端均须自行实现

**结论：两边都在本框架上自己实现。"服务端直接用 ACT 的"在技术上等于整体迁移到 ACT 框架，不是组件级复用。** 理由：

1. **ACT 的服务端不是独立组件**：它是 rvtest_trap_handler.h 中的 `RVTEST_TRAP_HANDLER` 汇编宏，强耦合 ACT4 一整套基础设施（per-mode savearea/trampoline 布局、xSCRATCH 切换入口、link.ld 符号 `rvtest_sig_begin`/`rvtest_code_begin`、`RVTEST_TRAP_PROLOG/EPILOG/SAVEAREA`、UDB 参数、RVMODEL 宏），不能单独摘出来链进 C 工程；
2. **测试模型根本不同**：ACT 服务端的核心动作是把 trap 信息写成 **signature word** 到签名区，供仿真结束后与 golden signature 比对；本框架是**运行时 trap_record + arm/expect + TEST_ASSERT + printf**。ACT handler 不做运行时断言，本框架用例全部依赖运行时断言——直接搬入 ACT 服务端会使所有用例失去验证能力；
3. **服务端本来就是"本项目要提供的执行环境"**：被测对象包括自研硬件（xiaohui、com260_k3 等 custom M 态平台），按 CRD 设计这类平台本就应该由用户提供自己的 T-SBI 服务端（invisible trap、软件 relay、CSR emulate 都要贴合硬件）。`common/trap.c` 就是那个服务端，改造它实现 T-SBI 语义即本方案 Phase 1；
4. **从 ACT 复用的是"约定"而非"代码"**：操作码表、dispatch 顺序、寄存器契约、转发链语义、静态指令表设计（已固化于 §4.2/§4.4），保证未来与 ACT 生态的语义互操作。

> 战略选项备注：若未来决定**直接跑 ACT 官方用例**，那是另一条路线——整体采用 ACT 框架（本项目 `config/<plat>/rvmodel_macros.h` 目录结构已与其对齐，届时只需补 ACT 的 config 文件），与本方案"自有框架 T-SBI 化"并行不悖，但不在本方案范围内。

### 2.10 rvtest_trap_handler.h 的定位：语义基准，不整体引入

该文件共 2857 行，内容构成及对本项目的可复用性：

| 节 | 内容 | 性质 |
|----|------|------|
| 1 | 寄存器别名（T1~T6=x6~x9/x14/x15；a0/a1 刻意不用作 handler 临时寄存器） | ACT 专用 |
| 2 | 架构常量（cause 数量、掩码） | ACT 专用 |
| 3 | T-SBI 操作码常量（GOTO 1-5、ECALL_TEST 0x73、CSR_ACCESS 编码规则） | ★ 约定，需对齐 |
| 4 | XCSR_RENAME 宏（按 M/H/S/V 参数化 CSR 名） | ACT 专用（本项目有自己的 CSR 层） |
| 5 | legacy GOTO 宏（a0=0 旧约定 + ALT_GOTO_M 非法指令备用路径） | ACT 专用 |
| 6 | T-SBI 便捷宏 RVTEST_TSBI_*（客户端） | ★ 语义需对齐 |
| 7 | RVTEST_GOTO_LOWER_MODE（boot 期经 mret 降权，含 PA↔VA 重定位） | ACT 专用 |
| 8 | RVMODEL 中断宏缺省 stub | 本项目已有对应机制 |
| 9 | RVTEST_TRAP_PROLOG（per-mode 初始化） | ACT 专用 |
| 10 | RVTEST_TRAP_HANDLER 主体：trampoline → ecall 检测 → T-SBI dispatch → signature 记录 → 异常/中断处理 → 恢复返回 | 核心参考对象 |
| 11 | RVTEST_TRAP_EPILOG | ACT 专用 |
| 12 | RVTEST_TRAP_SAVEAREA（.data per-mode 保存区布局） | ACT 专用 |
| 15B | RVTEST_FAST_TRAP_HANDLER（大量非法指令 trap 的简化 handler） | ACT 专用 |
| 附 | tsbi_instr_table 静态指令表 + 查表 dispatch（M-mode 服务端核心） | ★ 设计直接借鉴 |

**不整体引入的理由：**

1. **形态不兼容**：gas 宏文件，必须在 ACT 的 .S 测试结构里按 mode 实例化（`RVTEST_TRAP_HANDLER M/S/V`），依赖同框架 prolog/epilog/savearea/link.ld 符号，不是可独立编译的单元；
2. **模型冲突**：其"记录 signature word"与本框架"运行时 arm/expect 断言"互斥（见 §2.9-2）；
3. **本项目已有的 C handler（`trap_asm.S` 全量保存 + `trap.c`）完成同样的上下文保存职责**，且 C 实现 dispatch 比 2857 行汇编宏更可维护（帧读写机制见 §6.1）；
4. **需借鉴并逐条对齐的部分**：dispatch 顺序（`(a0-1)<5` → `0x73` → 指令编码查表 → reserved）、静态指令表布局（`TSBI_CSR_INSTR_TABLE` 宏 + ret + 0 哨兵）、S→M 转发链、"handler 不动 a0/a1、结果只经 a0 返回"的寄存器契约——这些是未来与 ACT 行为对拍的基准。

---

## 3. 现状与差距分析

### 3.1 现有框架的 ecall/trap 模型

| 组件 | 现状 |
|------|------|
| ecall 约定 | `ecall_args[2]` **内存全局变量**传参：a0=ECALL_GOTO_PRIV(1)，a1=目标特权级（`common/encoding.h`、`common/privilege.c`） |
| 特权切换 | `goto_priv()`：上行经 ecall（M/S handler 读内存 ecall_args），下行直接 mret/sret（`lower_priv()`） |
| trap 模型 | M-mode 为中心：默认不委托，所有 trap 进 `m_trap_handler`，`trap_record` 记录 + arm/expect 机制 |
| trap 汇编层 | `common/trap_asm.S` 全量保存 x1-x31 到栈，C handler 无参数，返回码决定 mret/sret |
| S-mode handler | `s_trap_handler` 仅在个别套件（VM/HYP）按需启用，也读内存 ecall_args |
| boot | `common/entry.S`：hart0、栈、BSS、RVMODEL_BOOT、_platform_init、main；**无** medeleg/mideleg/menvcfg 等确定性初始化 |
| RVMODEL 宏 | `config/<plat>/rvmodel_macros.h` 已实现 HALT_PASS/FAIL、IO_WRITE_STR、DATA_SECTION，部分平台有 SET/CLR 中断宏 |
| 平台地址 | `config/<plat>/platform_config.h` 提供 PLATFORM_CLINT_BASE/MSIP/MTIMECMP/MTIME 等（对应 ACT 的 RVMODEL_MTIME_ADDRESS 等） |

### 3.2 差距清单

| # | 差距 | 影响 | 处置 |
|---|------|------|------|
| G1 | ecall 参数走内存而非 a0/a1 寄存器 | 与 T-SBI 约定根本不兼容；且 VM 开启时 U/S 端写全局变量需额外映射 | 迁移为寄存器约定（P0） |
| G2 | 操作码空间冲突：ECALL_GOTO_PRIV=1 与 TSBI_GOTO_MMODE=1 同值不同义 | 无法共存 | 全面采用 ACT 操作码，废弃 ECALL_GOTO_PRIV（P0） |
| G3 | 无 CSR/MMIO 服务层：S/U 测试访问 M-CSR 必须先切回 M-mode | 不符合"测试不假设标准 M-mode 可运行"的认证模型；无法支持 custom M-mode | 实现 TSBI_CSR_* / TSBI_LW/SW/LD/SD 服务（P1） |
| G4 | trap 记录全部在 M-mode；medeleg/mideleg 无确定性初始化 | 无法支持"S-mode 为中心"的 profile 模式 | 新增 RVA23 boot 策略 + S 端 trap 记录（P2） |
| G5 | C handler 无法接触调用者 a0/a1（trap_asm.S 全量保存但 handler 签名无参） | 无法实现 T-SBI dispatch 的"读 a0 派发、写 a0 返回" | 改造 handler 签名，传入 trap 栈帧指针（P0） |
| G6 | 无 invisible trap / 软件委托 relay 钩子 | custom M-mode（medeleg 位只读 0）场景无法运行 | 预留 weak hook（P3，按需） |
| G7 | RVMODEL 缺 ACCESS_FAULT_ADDRESS、TIMER_INT_SOON_DELAY、INTERRUPT_LATENCY 等 ACT 宏；部分平台缺 SET/CLR 中断宏 | 与 ACT 抽象层宏清单不齐 | 盘点补齐，映射到 platform_config.h（P3） |
| G8 | VS/VU 的 ecall 上行路径（hyp_priv.c）同样用内存约定 | 同 G1 | 随 G1 一并迁移（P0） |

### 3.3 有利条件

- `config/<plat>/rvmodel_macros.h` 目录结构与 ACT 的 `config/<core>/<version>/rvmodel_macros.h` **完全一致**，宏名一致，无需重组；
- trap 汇编层已经全量保存寄存器（含 a0/a1），改造成本低于 ACT 的 scratch/savearea 体系；
- 已有成熟的 trap_record/arm/expect 断言体系，只需复制到 S 端 handler；
- 平台地址已参数化（platform_config.h），与 RVMODEL_*_ADDRESS 一一对应。

---

## 4. 总体设计

### 4.1 分层架构

```
测试用例（C）
   │  tsbi_csr_read()/tsbi_goto()/goto_priv() ...        ← 客户端 API（P1/P2）
   ▼
common/tsbi.h + common/tsbi_client.c                       ← T-SBI 客户端（寄存器约定 ecall）
   │  ecall（a0=操作码, a1=参数）
   ▼
m_trap_handler / s_trap_handler（T-SBI dispatch 前置）      ← 服务端（P1）
   │  GOTO_* / ECALL_TEST / CSR表 / MEM表 / 上传转发
   ▼
common/tsbi_instr_table.S（静态指令表）+ tsbi hooks         ← 执行层（P1/P3）
```

### 4.2 关键设计决策

| 决策点 | 决策 | 理由 |
|--------|------|------|
| D1 传参方式 | 寄存器 a0/a1（废除 ecall_args 内存传参） | 与 ACT 对齐；VM 下无需映射共享变量；custom M-mode 下内存可能不可信 |
| D2 操作码 | 完全采用 ACT 数值（1-5、0x73、指令编码） | 未来与 ACT 测试套件/工具互通；避免自造方言 |
| D3 CSR 执行 | 静态指令表（不用自修改代码） | 与 ACT 当前实现对齐；规避 fence.i/i-cache/PMP-X 平台差异 |
| D4 指令表范围 | 覆盖 CRD 要求的全部 CSR（mstatus/mcounteren/mcountinhibit/mip/menvcfg/mseccfg/mstateen0/medeleg/mideleg + sstatus/sie/scounteren/senvcfg/sip/stimecmp/satp + mcycle/minstret），可按 `#ifdef` 扩展 h-CSR | 与 CRD"T-SBI 可控 CSR 集合"一致 |
| D5 handler 签名 | `m_trap_handler(struct trap_frame *f)`，a0/a1 经栈帧读写 | trap_asm.S 已保存全部寄存器，仅改签名与调用约定 |
| D6 真 ecall vs 服务 ecall | a0 命中 T-SBI 编码 → 服务；否则走原 trap_record 流程 | 与 ACT 一致；代价：测试不得用保留编码伪造"真 ecall"（见 §7.2） |
| D7 下行切换 | M-mode 内 `goto_priv` 下行仍可直接 mret/sret（参考实现路径）；S/U/VS/VU 中一律走 TSBI_GOTO_* | M-mode 代码本就不受认证可移植性约束；低特权级代码必须走 ecall 才能兼容 custom M-mode |
| D8 兼容策略 | 一次性迁移 do_ecall 到寄存器约定（同一 ecall 路径不保留双约定）；旧 ecall_args 仅作"真 ecall"触发器保留 | 双约定并存会产生 a0 取值歧义（如旧调用时 a0 恰好=2 被误判为 GOTO_SMODE） |
| D9 VS-mode | 第一阶段 VS/VU ecall 一律上传 HS/M 处理；HS handler 实现本地子集 + 转发 | 与 ACT 现状对齐（ACT 的 VS dispatch 亦是 TODO）；后续再本地化 |
| D10 profile 模式开关 | 构建期宏 `T_SBI_PROFILE_S`（Makefile CONFIG 注入），缺省关闭=现行为 | 迁移期间两套模型并存，套件逐个迁移 |

### 4.3 服务端 dispatch 流程（m_trap_handler 前置逻辑）

```
ecall(cause 8/9/10/11)?
 ├─ 否 → 原流程（arm/expect/interrupt/unexpected）
 └─ 是 → 读 f->a0
      ├─ (a0-1) < 5            → tsbi_goto_mode(f)      // 设置 MPP/MPV，mepc+=4，mret
      ├─ a0 == 0x73            → f->a0 = mepc; mepc+=4  // ECALL_TEST
      ├─ a0[6:0]==0x73,f3!=0   → 查 CSR 表 → 执行 → mepc+=4
      ├─ a0 匹配 mem-op 编码    → 查 MEM 表 → 执行 → mepc+=4
      ├─ a0 == LEGACY_GOTO_PRIV(兼容窗口期) → 旧路径
      └─ 其它 → 记录为真 ecall（trap_record），走 expect 断言流程
```

S-mode handler（profile 模式下）同理：S-CSR 本地执行（查表，表项可在 S 权限执行），M-CSR/GOTO_M/GOTO_VS/VU **转发**：恢复自身临时状态后直接 `ecall`（a0/a1 原样在栈帧/寄存器中），M 处理完 mret 回 S handler，S handler 再 sret 回原调用者。注意 ACT 参考实现中被标记 TODO 的 **转发 ecall 的 mepc 修正**（mepc 指向 S handler 内的转发 ecall 而非原调用者）：M 端对 cause=9 的转发请求需以 `sepc` 为返回点（S handler 已自增 sepc），本方案直接实现，不等 ACT。

### 4.4 静态指令表设计（common/tsbi_instr_table.S）

```asm
.macro TSBI_CSR_INSTR_TABLE csr_addr
    .word (\csr_addr << 20) | 0x02573   /* csrr a0, csr  */
    ret
    .word (\csr_addr << 20) | 0x59073   /* csrw csr, a1  */
    ret
    .word (\csr_addr << 20) | 0x5a073   /* csrs csr, a1  */
    ret
    .word (\csr_addr << 20) | 0x5b073   /* csrc csr, a1  */
    ret
.endm

tsbi_instr_table:
    TSBI_CSR_INSTR_TABLE 0x300   /* mstatus */
    TSBI_CSR_INSTR_TABLE 0x302   /* medeleg */
    ... (按 D4 清单)
    lw a0, 0(a1); ret
    lw a0, 4(a1); ret
    sw a2, 0(a1); ret
    ... (RV64: ld/sd)
    .word 0                      /* 哨兵 */
```

- 表放 `.text`（保证 X 权限；PMP 配置时不得剥离执行权限）；
- 查表在 C 中实现（比 ACT 的汇编循环更清晰）：遍历 `.word` 与 a0 比较，命中则 `jalr` 进表；
- custom M-mode emulate 钩子：`__attribute__((weak)) int tsbi_csr_emulate(uint32_t csr, int op, uintptr_t *val)`，查表前调用，返回已处理则跳过真实指令（为 §2.5 的"字段 emulate"要求预留）。

### 4.5 客户端 API（common/tsbi.h）

```c
/* privilege switch via T-SBI (works from any mode) */
void tsbi_goto_mmode(void);              /* a0=1 */
void tsbi_goto_smode(void);              /* a0=2 */
void tsbi_goto_umode(void);              /* a0=3 */
void tsbi_goto_vsmode(void);             /* a0=4, H ext */
void tsbi_goto_vumode(void);             /* a0=5, H ext */

uintptr_t tsbi_ecall_test(void);         /* a0=0x73, returns xEPC */

/* CSR services (encoding built from 12-bit CSR number) */
uintptr_t tsbi_csr_read(unsigned csr);
void      tsbi_csr_write(unsigned csr, uintptr_t val);
void      tsbi_csr_set(unsigned csr, uintptr_t mask);
void      tsbi_csr_clear(unsigned csr, uintptr_t mask);

/* M-privileged memory access (mtime / MMIO) */
uint32_t  tsbi_lw(uintptr_t addr);
void      tsbi_sw(uintptr_t addr, uint32_t val);
#if __riscv_xlen == 64
uint64_t  tsbi_ld(uintptr_t addr);
void      tsbi_sd(uintptr_t addr, uint64_t val);
#endif
```

实现为小型汇编 stub（`.option norvc`，`li a0, op; mv a1, arg; ecall`），与 ACT 的 `RVTEST_TSBI_*` 宏语义一致。`goto_priv()` 内部改经 tsbi_goto_*，对外签名不变（现有用例无感）。

### 4.6 RVA23 profile 模式（T_SBI_PROFILE_S）

构建宏开启后，boot/初始化按 CRD 执行：

1. `main()` 入口仍在 M-mode 完成一次性环境搭建：mtvec/stvec、PMP、medeleg=0xFCB5FF、mideleg=0x366、menvcfg/mcountinhibit/mcounteren 确定性初值；
2. 经 TSBI_GOTO_SMODE 进入 S-mode 后运行测试编排（相当于 ACT 的 BOOT_TO_SMODE）；Hypervisor 套件进 HS-mode 后自建 VS/VU；
3. **trap 记录迁移到 S 端**：`trap_record`/arm/expect 逻辑抽成与特权级无关的公共层（`common/trap_common.c`），M/S handler 共用；未委托的 trap（ecall-from-S、double-trap 等）仍由 M 端记录并通过既有 `_trace` 机制与 S 端合并；
4. M-mode 中断测试在 profile 模式下自动跳过（MEI/MTI/MSI OUT-OF-SCOPE），通过套件级 guard 实现；
5. 预留 invisible-trap 钩子 `__attribute__((weak)) int tsbi_invisible_trap(struct trap_frame *f)`（返回 0=非 invisible，继续 relay/记录）。

### 4.7 trap handler 职责切分模型与依赖关系

#### 4.7.1 结论：不是"搬迁"而是"职责切分"

适配 T-SBI 不是把 trap handler 从 M-mode 搬到 S-mode，而是按 CRD 模型切分职责：**测试断言中心移到 S-mode，M-mode handler 瘦身为 T-SBI 服务端 + relay；两个 handler 必须同时存在**。CRD 明确 M-mode trap handler 只剩三项职责：

1. **检查是否为 T-SBI 调用**，是则执行服务并从 ecall 返回；
2. 调用 **invisible trap** 定制固件（如有）；
3. 对硬件无法委托但应交给 S-mode 的 trap，用 mstatus/sstatus、sepc、scause、stval **重建 S 端现场后软件 relay** 到 S-mode handler。

同时 CRD 规定 "Testing M-mode trap behavior is OUT-OF-SCOPE"，几乎所有异常（medeleg=0xFCB5FF）和全部 supervisor 中断（mideleg=0x366）都委托给 S-mode——**测试真正"看到"的 trap 行为、做断言记录的主战场是 S-mode handler**。但 M-mode handler 不能消失：ecall-from-S **不得委托**（否则 T-SBI 无处落地），它就是 T-SBI server 的宿主。

#### 4.7.2 双模式下的 handler 职责矩阵

| | M-only 模式（现状，M 系套件） | profile 模式（T_SBI_PROFILE_S，S 系套件） |
|---|---|---|
| M handler | 全功能：trap_record + arm/expect + 中断清除 | **瘦身**：T-SBI dispatch + invisible-trap hook + 软件 relay；仅对少数留在 M 的 trap（ecall-from-S 作为被测对象、double-trap 等）做记录 |
| S handler | 按需启用（VM/HYP 场景） | **主力**：完整复制 trap_record/arm/expect、sepc 推进、委托中断清除（SSIP/STIP via stimecmp/LCOFI 等） |
| HS handler（ENABLE_HYP） | 现状 | VS/VU 委托 trap（hideleg）的记录主体；VS/VU ecall 按 D9 上传 |
| 委托配置 | 默认不委托（测试自设） | medeleg=0xFCB5FF、mideleg=0x366（boot 期确定性设置并强制校验） |
| M 中断（MEI/MTI/MSI） | 正常测试 | OUT-OF-SCOPE，套件级 guard 跳过 |

套件归属推论：**专门测 M-mode trap 行为的套件（Sm_Exceptions、Sm_Interrupts、Ssdbltrp 升级路径等）本质上不属于 profile 模型**，永远留在 M-only 模式——它们测的是 CRD 认证不覆盖的 OUT-OF-SCOPE 部分，但作为特权扩展测试集仍需要保留。

#### 4.7.3 四条典型路径

```
路径 A：S-mode 测试请求 M 服务（T-SBI 主路径）
  S 测试代码 ecall(a0=CSR编码)
    → M handler（ecall-from-S 未委托）→ T-SBI server 查表执行
    → 结果写回帧 a0，mepc+=4，mret → 回到 S 测试代码

路径 B：普通异常/中断（profile 模式测试断言主路径）
  S/U 测试代码触发 trap（medeleg/hideleg 已委托）
    → S handler：记录 trap_record → 断言消费 → sepc 推进 → sret
    （M handler 完全不参与）

路径 C：硬件不可委托的 trap（软件 relay）
  trap 无法委托 → M handler
    → invisible-trap hook（修复并返回，测试不可见）
    → 或：填 sstatus/sepc/scause/stval，跳转到 stvec 入口
    → S handler 如同正常收到该 trap 一样记录/断言

路径 D：S handler 需要 M 权限（转发链，见 §6.3）
  U/VS 代码 ecall(a0=M-CSR编码) → S handler 判定无法本地执行
    → S handler 再 ecall（a0/a1 不变）→ M handler 执行并返回
    → S handler 搬运结果 → sret 回原调用者
```

#### 4.7.4 trap handler 与 T-SBI server 的依赖关系

结构关系澄清：**T-SBI server 不是独立于 trap handler 的组件，而是内嵌在 M-mode handler 中的功能层**（即 m_trap_handler 的 ecall 前置 dispatch，§4.3）。依赖分三个方向：

**（1）M-mode handler → server（宿主依赖）**

| 依赖项 | 内容 |
|--------|------|
| dispatch 逻辑 | ecall 判定后必须先走 T-SBI 判定，**顺序先于 trap_record**——否则框架自身的服务 ecall 会被当成"被测 trap"记录，污染断言（D6） |
| 操作码匹配 | `(a0-1)<5` / `0x73` / 指令编码 / mem-op 编码，未命中回落为"真 ecall"记录 |
| 指令表执行 | `tsbi_instr_table` 查表 + `jalr` 执行；表必须在 M 可达的**可执行区域**（PMP/PMA X 权限） |
| GOTO 实现 | MPP/MPV/MPRV 设置逻辑（复用现有特权切换代码） |
| emulate 钩子 | `tsbi_csr_emulate()` weak hook，custom M 态平台在查表前拦截 |

**（2）S-mode handler → server（客户端依赖）**

1. **转发**：收到 U/VS 的 ecall 且请求超出自身权限（M-CSR、GOTO_M/VS/VU）时，依赖 server 能接收参数不变的二次 ecall 并正确返回（路径 D）；
2. **测试辅助**：S handler 自身或 S 态测试代码需要读 mtime、配 mtimecmp、读写 M-CSR 做测试前置时，经 `tsbi_*` 客户端 API 依赖 server——这正是"T-SBI 使 S 态测试不必假设能跑回 M-mode"的价值所在。

**（3）server → trap handler 基础设施（反向依赖）**

| 依赖项 | 内容 |
|--------|------|
| 寄存器帧 | server 读写调用者 a0/a1 完全依赖 `trap_asm.S` 的全量保存帧（`struct trap_frame`，§6.1）；帧布局变更即 server 失效 |
| xEPC 管理 | 所有服务完成后 `xEPC += 4`（ecall 恒为 4 字节非压缩，现有 `next_instruction()` 约定） |
| 返回路径 | mret + MPP/MPV；转发场景下 M 端须以 `sepc` 为返回点修正 mepc（ACT 的 TODO，本方案 §6.3 自行实现） |
| 中断屏蔽 | 服务执行期间依赖 trap 入口硬件关中断，避免服务中途被打断导致 a0/a1 语义破坏 |

#### 4.7.5 关键前置契约（违约即系统性失效）

1. **medeleg[9]=0（ecall-from-S 不委托）**——整个 T-SBI 链的地基。`common/vm/satp.c` 已有同款约束和历史教训注释（委托后 s_trap_handler → goto_priv(M) → ecall → 又回 S handler 的死循环），profile 模式必须在 boot 期强制并加运行时校验；
2. **服务 ecall 与真 ecall 的区分只靠 a0 编码**——测试用例不得用保留编码伪造真 ecall（§6.4 硬约束）；
3. **指令表 X 权限**：VM 开启时 M handler 以 satp=off 执行天然保证；S handler 若本地执行 S-CSR 表项，需保证表在 S 态页表中可执行（R4）；
4. **a0/a1 透传**：转发链上任一层不得破坏调用者 a0/a1（本框架全量保存帧天然满足，ACT 是靠刻意不用这两个寄存器满足）。

#### 4.7.6 S handler 成为主力后的二次 trap 防护约束

S handler 承担主力记录职责后，必须沿用既有工程教训（项目历史缺陷记录）：

1. **禁止在 S/HS handler 内无条件读 M-mode-only CSR**（如 mtval2）：在不向 S 暴露该 CSR 的实现（Spike）上会触发 illegal-instruction 二次 trap，覆盖 armed 记录导致断言拿到 cause=2、epc 落在 handler 内部；mtval2 等字段一律在 M 端入口捕获，S 端传 0（现有 `trap.c` 已如此实现，迁移时保持）；
2. **double trap 平台（无 SMRNMI）：M handler 入口必须显式清 MDT**，避免 M 态异常升级为不可恢复 critical error；profile 模式下 relay 路径进入 S 前同样需确认 SDT 状态不会级联（现有 Ssdbltrp 恢复逻辑可复用）；
3. 向任一 handler 新增 CSR 读取前，必须确认该 CSR 在 handler 所在特权级可访问且跨平台一致（QEMU 通常宽松、Spike 严格）。

---

## 5. 分阶段实施计划

### Phase 0：接口与基础约定（1-2 天）
- [ ] 新增 `common/tsbi.h`：TSBI_* 操作码、CSR 编码构造宏、客户端 API 原型、`struct trap_frame` 定义（与 trap_asm.S 栈布局严格对应）；
- [ ] `trap_asm.S`：SAVE/RESTORE 布局冻结并文档化；`m_trap_entry`/`s_trap_entry` 将栈帧指针作为第一参数传入 C handler；
- [ ] `encoding.h`：定义 TSBI 操作码常量，标记 ECALL_GOTO_PRIV 为 deprecated。

### Phase 1：服务端（3-5 天）
- [ ] `common/tsbi_server.c`：dispatch 逻辑（§4.3），GOTO_xMODE（含 MPV/MPR 处理）、ECALL_TEST；
- [ ] `common/tsbi_instr_table.S`：静态指令表（D4 清单）+ C 查表执行器；
- [ ] m_trap_handler 前置 T-SBI dispatch，未命中回落原 trap_record 流程；
- [ ] `framework_test` 增加 T-SBI 服务端用例（见 §8）。

### Phase 2：客户端迁移（3-5 天）
- [ ] `common/tsbi_client.c`/`.S`：客户端 stub；
- [ ] `privilege.c` 的 `do_ecall()` 改为寄存器传参；`goto_priv()`/`run_in_priv()` 改经 tsbi_goto_*；
- [ ] `hyp_priv.c` 两处直写 ecall_args 的裸 ecall 改为 tsbi 调用；
- [ ] 删除 handler 中对 ecall_args 的读取（hyp_test_helpers 的"真 ecall"场景改为显式 a0=非法编码）；
- [ ] 全量回归：framework_test + 抽样 3-5 个套件（Sm_CSR、Ss_Interrupts、Sv39、Hypervisor_CSR、cfi.Zicfilp），qemu/spike/sail 三平台。

### Phase 3：S-mode 转发与 profile 引导（5-8 天）
- [ ] s_trap_handler T-SBI 本地子集 + 向 M 转发（含转发返回链 sret）；
- [ ] M 端转发 ecall 的 mepc 修正（cause=9 且来自 handler 转发：以 sepc 为返回点）；
- [ ] `T_SBI_PROFILE_S` boot 路径：委托配置、S 端 trap_record、M 中断 guard；
- [ ] trap 公共层抽取（trap_record 与 handler 特权级解耦）。

### Phase 4：套件适配审计（持续）
- [ ] 按套件分类：**M-only**（Sm_*、pmp、spmp、Smepmp、Ssdbltrp 等——profile 模式不适用，保持 M 模式运行）；**profile-portable**（S_*、U_*、Sv*、Hypervisor_*、Sstc、Zic*、zpm.Ssnpm 等——逐步切到 profile 模式验证）；
- [ ] 每个迁移套件更新其 testplan 文档的"运行模式"说明。

### Phase 5：RVMODEL 完备性（按需）
- [ ] 盘点 12 个平台 config 的 rvmodel_macros.h：补齐 RVMODEL_SET/CLR_{MEXT,MSW,SEXT,SSW}_INT；
- [ ] platform_config.h 增加 ACT 对齐宏：RVMODEL_ACCESS_FAULT_ADDRESS（映射到现有 fault 区域定义）、RVMODEL_MTIME_ADDRESS 等（直接 alias 现有 PLATFORM_* 定义）；
- [ ] 文档：`DOCS/framework/build_framework.md` 增补 T-SBI 相关 CONFIG 开关说明。

---

## 6. 详细设计要点

### 6.1 trap 栈帧与 a0/a1 读写

`trap_asm.S` 现布局：`sp` 指向 31×REGSIZE 帧，xN 位于 `(N-1)*REGSIZE`（x2/sp 不存）。a0=x10 → 偏移 `9*REGSIZE`，a1=x11 → `10*REGSIZE`，a2=x12 → `11*REGSIZE`。

```c
struct trap_frame {
    uintptr_t x[31];   /* x1..x31, index = regno-1, sp(x2) unused */
};
#define F_A0(f) ((f)->x[9])
#define F_A1(f) ((f)->x[10])
#define F_A2(f) ((f)->x[11])
```

handler 返回前改写 `F_A0(f)` 即完成"结果经 a0 返回"，RESTORE_CONTEXT 自然带出。无需像 ACT 那样小心避免触碰 a0/a1（C 编译器在 handler 内用的寄存器本来就在帧中保存）。

### 6.2 GOTO_xMODE 的 MPP/MPV 处理

复用现有 `m_trap_handler` 特权切换代码（清 MPP/MPRV、按 target 置 MPP、ENABLE_HYP 下置 MPV），但目标码来自 a0（1=M 需特判：GOTO_MMODE 置 MPP=11 并清 MPV）而非内存。mepc 统一 `next_instruction(epc)`（框架 ecall 均为 4 字节非压缩，已有约定）。

### 6.3 S→M 转发链

```
U/S/VS 测试代码 ecall(a0=M-CSR编码)
   → S handler: 判定 CSR[11:10]==11 → 直接 ecall（帧中 a0/a1 未动）
      → M handler: 查表执行 CSR 指令，结果写入其帧 a0，mepc+=4，mret
   → 回到 S handler 的转发 ecall 之后：把 M 帧结果搬回本帧 a0，sepc 已 +=4，sret
   → 回到测试代码，a0 = CSR 值
```
实现注意：S handler 转发前应已完成自身临时寄存器恢复（本项目帧全保存，天然满足）；M 端识别"转发请求"依据是 cause=9 且 mepc 落在 S handler 转发点（用符号区间判断），此时返回点取 `sepc`。

### 6.4 与真 ecall 测试的冲突处理（D6 细则）

现有用例存在"构造 ecall 并断言 trap 记录"的场景（如 Exceptions 套件、hyp_test_helpers 的 `ecall_args[0]=0` 真 ecall）。迁移规则：

1. 需要"真 ecall trap"的用例：ecall 前置 `li a0, 0xFFFFFFFF`（保留非法值），handler 判为非 T-SBI → 记录 trap；
2. 需要测试 ecall 路径本身的：使用 `tsbi_ecall_test()`（返回 xEPC 自证），与 ACT 的 `RVTEST_TSBI_ECALL_TEST` 语义一致；
3. 任何用例不得在 a0 放置 `[1..5]`、`0x73`、`[6:0]==0x73 && funct3!=0`、mem-op 编码后声称预期普通 ecall trap——在 testplan 中列为硬约束。

### 6.5 VM/HS 场景

- satp 开启时寄存器约定零依赖内存，天然工作（这正是放弃 ecall_args 的核心动机之一）；
- HS 模式下 VS/VU 的 ecall 默认经 hideleg 未委托路径进 HS handler（cause 10/11 在 hedeleg/hideleg 语义下），第一阶段 HS handler 一律转发 M（D9）；
- `vm_run_in_smode` 中"不得委托 cause 9"的既有约束（common/vm/satp.c）与 CRD 一致，保留并引用 CRD 依据。

---

## 7. 迁移影响面

### 7.1 需要修改的框架文件

| 文件 | 修改 |
|------|------|
| common/trap_asm.S | handler 传帧指针；布局文档化 |
| common/trap.c | 前置 T-SBI dispatch；删 ecall_args 读取；转发链 |
| common/privilege.c | do_ecall/goto_priv 切到 tsbi 客户端 |
| common/hyp/hyp_priv.c、hyp_test_helpers.c | 裸 ecall 改 tsbi |
| common/encoding.h | TSBI 常量 |
| common/entry.S（profile 模式） | 委托/环境配置、boot-to-S |
| 新增 common/tsbi.h、tsbi_client.c/.S、tsbi_server.c、tsbi_instr_table.S | — |

### 7.2 测试用例面

- **无感迁移**（占绝大多数）：仅用 `goto_priv/run_in_priv/PRIV_DO/CHECK_TRAP` 的用例，接口不变；
- **需改写**：直接裸写 `asm("ecall")`、读写 `ecall_args` 的用例（grep 显示集中于 common/hyp 与少量 Exceptions 套件，迁移时逐一修正）；
- **需标注**：以特定 a0 值构造 ecall 验证 trap 行为的用例（§6.4）。

---

## 8. 验证计划

1. **T-SBI 单元测试**（framework_test 新增 test_tsbi.c）：
   - ECALL_TEST：返回值 == ecall 指令地址；
   - CSR 四操作：mstatus.TVM 置/清后 S 端访问 satp 的行为变化（行为级验证，不只读回）；
   - LW/SW/LD/SD：读写 PLATFORM_MTIME_ADDR/MTIMECMP 并与 M-mode 直读对拍；
   - GOTO 全链：U→S→M→VS→VU→M 逐跳断言 `get_current_priv()`；
   - 转发链：VM 开启的 U-mode 中发起 M-CSR 读；
   - 保留值：a0=0xDEAD 等非法值 → 记录真 ecall trap。
2. **回归**：Phase 2 后跑 framework_test 全量 + 抽样套件 × qemu/spike/sail；Phase 3 后 profile 模式下跑 S 系套件抽样。
3. **失败判定**：平台行为与 SPEC/CRD 不符时保持失败并报告（项目准则），不做 workaround。

---

## 9. 风险与开放问题

| # | 问题 | 处置 |
|---|------|------|
| R1 | ACT 侧 VS-mode T-SBI dispatch 与转发 mepc 修正仍是 TODO，后续可能调整 | 本方案按 §6.3 先落地；跟踪 ACT 仓库，差异处以 ACT 正式 release 为准回改 |
| R2 | ACT 文档头部注释显示 abstraction.adoc 仍含 `***`/TODO 草稿标记，数值（如 0xFCB5FF、操作码）可能微调 | Phase 0 冻结前与 ACT 最新 commit 核对一次；tsbi.h 中集中定义便于跟改 |
| R3 | medeleg 部分位只读 0 的 custom M-mode（如某些 HW）需软件 relay | Phase 3 预留 relay 框架（§2.5 第 3 步），按平台需要填充 |
| R4 | 静态指令表在开启 PMP/Sv 时的 X 权限 | 表置于 .text 并纳入现有 PMP/identity-map 的 X 区域；用例中加显式检查 |
| R5 | profile 模式与 M-only 套件并存带来的双路径维护成本 | 以 `T_SBI_PROFILE_S` 编译期隔离，运行时不混用；文档明确套件适用范围 |
| R6 | RV32（qemu-rv32-max）下 TSBI_LD/SD 不存在 | tsbi.h 以 `__riscv_xlen` 门控，与 ACT 一致 |

---

## 10. 附：ACT 概念与本项目映射表

| ACT 概念 | 本项目对应 | 动作 |
|----------|-----------|------|
| tests/env/rvtest_trap_handler.h | common/trap.c + trap_asm.S + 新 tsbi_server | 改造+新增 |
| RVTEST_TSBI_* 宏 | common/tsbi.h 客户端 API | 新增 |
| tsbi_instr_table | common/tsbi_instr_table.S | 新增 |
| RVMODEL_BOOT/HALT/IO_* | config/*/rvmodel_macros.h | 已有，复用 |
| RVMODEL_*_ADDRESS / ACCESS_FAULT_ADDRESS | config/*/platform_config.h | 补齐别名 |
| BOOT_TO_SMODE / 委托配置 | entry.S + profile boot 路径 | 新增 |
| invisible trap hook | tsbi_invisible_trap weak hook | 预留 |
| signature 记录 | trap_record/arm/expect 体系 | 扩展到 S 端 |
