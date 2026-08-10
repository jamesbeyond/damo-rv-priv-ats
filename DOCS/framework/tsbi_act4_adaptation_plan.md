# T-SBI ACT4 适配方案（rvtest_trap_handler.h 入库 + S 态常态执行模型）

> **规范来源**：
> - 协议规范：[ACT4 abstraction.adoc § T-SBI](https://github.com/riscv/riscv-arch-test/blob/act4/docs/ctp/src/abstraction.adoc)（已镜像至 `SPEC/abstraction.adoc`）
> - 参考实现：[ACT4 rvtest_trap_handler.h](https://github.com/riscv/riscv-arch-test/blob/act4/tests/env/rvtest_trap_handler.h)（本方案将其**原样入库**）
> - 架构图：[T-SBI 适配（语雀画板）](https://aliyuque.antfin.com/bsum0k/sng3io/drterrh9vr6ft63i)——trap 路由联动图 + handler 判定链 + 路由速查表，与本文 §5 互为引用
>
> **与旧方案的关系**：[tsbi_adaptation_plan.md](./tsbi_adaptation_plan.md)（下称"旧方案"）确立了"纯自研、不拉取 ACT4 源文件"路线，
> 其 Phase 1\~4 的协议实现成果（寄存器约定、GOTO/ECALL_TEST/CSR_ACCESS dispatch、策略 A、raw-ecall 逃逸、软件委托）已验证可用，
> 现存于 `git stash`（`tsbi-local-dev-backup-20260806`，基于旧 master 72a8eaa），在 P2 rebase 恢复复用（见 §1.1、§7）。
> 本方案是其演进版，两点升级：
> 1. **协议权威源升级**：`rvtest_trap_handler.h` 原样入库，成为 T-SBI 协议常量、便利宏与参考实现的单一权威源；
> 2. **执行模型升级**：测试常态从 M 态改为 **S 态**，一切 M 态操作必须经 T-SBI（GOTO excursion 或 CSR_ACCESS 代理）完成。

---

## 0. T-SBI 简介

> 本节内容**仅依据上游两份官方资料**：[abstraction.adoc § T-SBI](https://github.com/riscv/riscv-arch-test/blob/act4/docs/ctp/src/abstraction.adoc)（规范）
> 与 [rvtest_trap_handler.h](https://github.com/riscv/riscv-arch-test/blob/act4/tests/env/rvtest_trap_handler.h)（参考实现），不涉及本仓库现状。

### 0.1 是什么

**T-SBI（Test Supervisor Binary Interface）** 是 ACT4（Architecture Compliance Test v4）
框架要求测试环境实现的一套**定制监督者二进制接口**：为 S 态与 U 态测试程序提供抽象层，
使其可以向一个**可能是定制实现（custom M-mode）的 M 态执行环境**请求服务——包括测试
ecall 到执行环境本身，以及代理访问 CSR。

背景（abstraction.adoc 基本设计考量）：
- 测试不能假设 DUT 实现了 *Standard Machine Mode*——DUT 可能只有最小化/带安全加固的
  定制 M 态；认证重点关注的 RVA23S64 profile 甚至不要求标准 M-mode，只要求执行环境能
  响应 `ecall` 等行为；
- 因此低特权级测试触碰 M 态资源的通道必须协议化：这就是 T-SBI。

### 0.2 为什么不用标准 SBI（OpenSBI）

abstraction.adoc 给出两条理由：

| 问题 | 说明 |
|------|------|
| 体积 | ratified SBI 编译出 \~1MB 的 OpenSBI 固件，随每个测试加载过于庞大 |
| 调用约定 | 标准 SBI 用 `a7`/`a6` 传 EID/FID，RV32E 上不存在这两个寄存器 |

因此 T-SBI 是一个**更轻量、采用自定义调用约定**的实现，与 ratified SBI **不兼容**。

### 0.3 调用约定

| 项目 | 规范定义 |
|------|---------|
| 触发方式 | `ecall` 指令 |
| 操作码 | `a0` |
| 参数 | `a1`（必要时可扩展 `a2`） |
| 返回值 | 需要时经 `a0` 返回 |
| 保留值 | 其余 `a0` 取值 reserved（参考实现返回 `TSBI_RESERVED_RET = -1`） |

参考实现的寄存器纪律：handler 的内部临时寄存器为 T1\~T6（x6\~x9, x14, x15），
**a0/a1 被刻意排除在 handler 临时寄存器之外**——a0/a1 是 T-SBI 的参数/返回寄存器，
handler 全程不将其用作暂存，保证操作码/参数穿透各级 handler、返回值可靠送达调用方。

### 0.4 服务集

**核心 T-SBI 调用**（abstraction.adoc 表 T-SBI Calls，测试套件必须实现）：

| 调用 | a0 编码 | 语义 |
|------|---------|------|
| TSBI_ECALL_TEST | `0x00000073`（ecall 指令机器码本身） | 测试 ecall 到执行环境的路径，返回 xEPC 到 a0，证明 trap handler 被进入 |
| TSBI_CSR_WRITE | `0x<CSR>59073`（= `csrrw x0, csr, a1`） | 代执行 CSRW，a1 为写入值 |
| TSBI_CSR_SET | `0x<CSR>5a073`（= `csrrs x0, csr, a1`） | 代执行 CSRS，a1 为置位掩码 |
| TSBI_CSR_CLEAR | `0x<CSR>5b073`（= `csrrc x0, csr, a1`） | 代执行 CSRC，a1 为清位掩码 |
| TSBI_CSR_READ | `0x<CSR>02573`（= `csrrs a0, csr, x0`） | 代执行 CSRR，结果返回 a0 |
| TSBI_LW / LW+4 / TSBI_LD | `0x0005a503` / `0x0045a503` / `0x0005b503` | 以 **M 态权限**从 a1 指向的物理地址装载 word/doubleword 返回 a0（读 `mtime` 等 MMIO；LD 仅 RV64） |
| TSBI_SW / SW+4 / TSBI_SD | `0x00a5a023` / `0x00a5a223` / `0x00a5b023` | 以 M 态权限把 a0 存入 a1 指向的物理地址（写 MMIO；SD 仅 RV64） |

**关键设计：a0 既是操作码，又是待执行指令本身**。CSR/访存类调用的 a0 **就是一条完整的
RISC-V 指令 32 位编码**（`csr[31:20] | rs1[19:15] | funct3[14:12] | rd[11:7] | 0x73[6:0]`，
rd/rs1/funct3 已由调用方正确编入），handler 不需要维护"操作码→行为"的翻译表，拿到编码
直接执行（或仿真）即可。

**语义契约**：CSR_ACCESS 对调用方承诺的是**达成这条 CSR 指令应有的架构效果**，而非
"代为执行一条指令"——怎么达成是执行环境的事：
- **标准 M-mode** 的参考做法（adoc 原文）：handler 将 a0 值写入内存 → `fence.i` 同步
  指令流 → 执行该指令 → 返回测试。指令真实执行，行为自然正确；
- **定制 M-mode** 若未实现对应 CSR（如 `mstatus.TVM` 位不存在），上述做法走不通（执行
  无效果或直接非法指令），但它**必须以其他私有机制仿真出等价行为**（例如用平台自定义的
  陷入控制手段实现"S 态访问 satp/执行 sfence.vma 时陷入"的同等效果）。这正是 T-SBI 作为
  抽象层的意义：同一份 S/U 态测试代码，在标准与定制 M-mode 上都能得到一致的架构语义。

**编码示例 ①**：S 态置 `mstatus.TVM`（TSBI_CSR_SET，等价 `csrrs x0, mstatus, a1`）。
S 态直接执行这条指令会触发非法指令异常（mstatus 是 M CSR），故把指令**编码**放进 a0、
掩码放进 a1，ecall 请求代执行：

```
a0 = 0x3005a073
     ┌─ 0x300 = mstatus CSR 地址        (bits 31:20)
     │    ┌─ 0x0B = rs1 = x11 = a1      (bits 19:15)
     │    │  ┌─ 2 = funct3 = CSRRS 置位 (bits 14:12)
     │    │  │ ┌─ 0x00 = rd = x0 弃旧值 (bits 11:7)
     │    │  │ │  ┌─ 0x73 = SYSTEM      (bits 6:0)
   0x300__5___a__0___73        ← 模板 0x<CSR>5a073, CSR=0x300
a1 = (1 << 20)                 ← TVM 位掩码（csrrs 的 rs1 值）
ecall                          ← handler 代为完成 mstatus |= a1
```

**编码示例 ②**：S 态读 `mip`（TSBI_CSR_READ，等价 `csrrs a0, mip, x0`）：

```
a0 = 0x34402573
     ┌─ 0x344 = mip CSR 地址
     │      ┌─ rs1 = x0（纯读不写）
     │      │    ┌─ funct3 = 2 (CSRRS)
     │      │    │  ┌─ rd = x10 = a0 ← 读结果直接落进 a0
   0x344__0___2__5(a0)__73     ← 模板 0x<CSR>02573, CSR=0x344
ecall                          ← 返回后 a0 即 mip 的值
```

注意读操作把 **rd 编码为 a0**：handler 代执行该指令时结果直接写入 a0 寄存器，配合
"handler 从不把 a0/a1 用作临时寄存器"的纪律（§0.3），返回值原路带回调用方，无需任何
额外搬运。

**ACT 扩展系统调用**（abstraction.adoc 表 Additional ACT System Calls——由 ACT trap
handler 额外提供的特权切换便利调用，**不属于核心 T-SBI**；换一套 trap handler 可以不支持）：

| 调用 | a0 | 语义 |
|------|-----|------|
| TSBI_GOTO_MMODE | `0x00000001` | 从任意特权级切换到 M 态；仅当 `STANDARD_SM_SUPPORTED` 为真时行为可预期 |
| TSBI_GOTO_SMODE | `0x00000002` | 切换到 S/HS 态 |
| TSBI_GOTO_UMODE | `0x00000003` | 切换到 U 态 |
| TSBI_GOTO_VSMODE | `0x00000004` | 切换到 VS 态 |
| TSBI_GOTO_VUMODE | `0x00000005` | 切换到 VU 态 |

**参考实现的 dispatch 判定顺序**（rvtest_trap_handler.h §3）：
`(a0-1) < 5` → GOTO_xMODE；`a0 == 0x73` → ECALL_TEST；
`a0[6:0]==0x73 且 a0[14:12]≠0` → CSR_ACCESS；其余 → 查指令表，未命中返回 -1 / 终止。

### 0.5 分层处理与链式转发

abstraction.adoc 明确：**ACT 的 S/HS 与 VS 态 trap handler 实现同一套调用集**。每级
handler 在本特权级能办则办，办不了就以**参数原封不动的再一次 ecall** 把请求沿特权链上传：

| Handler 层 | 本层办结 | 向上转发 |
|-----------|---------|---------|
| M | 全部请求 | — |
| S/HS | ECALL_TEST、GOTO_S/U、S/U CSR 访问 | GOTO_MMODE（恒上传）、M CSR 访问（CSR addr[9:8]==11）、GOTO_VS/VU |
| VS | （参考实现尚为 TODO） | 全部 |

参考实现的转发细节：S 态 handler 先恢复全部临时寄存器与 sp，再执行 ecall——因 handler
从不触碰 a0/a1，调用方的操作码与参数无需重装即穿透到 M 态。

另注意 adoc 的启动模型本身就是**委托优先**：S-mode boot 将 `medeleg` 配为 `0xFCB5FF`
（除 ecall-from-M/S 与 double-trap 外全量委托）、`mideleg=0x366`（委托全部 S/U 中断）。
因此 S/U 态测试激励 trap 的常态归宿是 **S handler 水平委托就地消化**（全程不经 M）；
T-SBI 服务请求（ecall-from-S）是唯一恒到 M 的通道。

### 0.6 测试侧便利宏与配套设施（参考实现提供）

- **便利宏**（非必需，开发者便利）：`RVTEST_TSBI_ECALL_TEST` / `RVTEST_TSBI_CSR_ACCESS
  encoding, arg` / `RVTEST_TSBI_GOTO_{M,S,U,VS,VU}MODE`——置 a0（及 a1）后 ecall 的封装，
  CLOBBERS 仅 a0/a1；
- **legacy 模式切换宏**：`RVTEST_GOTO_MMODE`（a0=0 信号约定）、`RVTEST_GOTO_DELEGATED_MMODE`
  （用非法指令绕过被委托的 ecall）——旧约定，与 T-SBI 并存于参考实现；
- **boot 降权宏**：`RVTEST_GOTO_LOWER_MODE`（M 态 boot 期降入低特权级，含 PA↔VA 重定位），
  配合 adoc 的启动模型——boot 序列降到"足以运行测试的最低特权级"，测试用
  `BOOT_TO_MMODE`/`BOOT_TO_SMODE` 符号声明目标态（hypervisor 测试应定义 `BOOT_TO_SMODE`
  进入 HS，再自行切 VS/VU）；
- **M 态实现方式**：参考实现当前用**静态指令表** `tsbi_instr_table`（预置常用 CSR ×
  读/写/置/清 4 变体 + LW/LD/SW/SD 项）查表执行，避免自修改代码；S 态则用 scratch 区
  写指令 + `fence.i` 动态执行。

---

## 1. 现状盘点（基线：master@da4b2ca）

### 1.1 现有代码的 ecall / 特权切换机制（无 T-SBI 实现）

最新 master 的代码侧**尚无任何 T-SBI 协议实现**，ecall 走仓库自有的旧协议：

| 组件 | 文件 | 现状 |
|------|------|------|
| ecall 发起 | `common/privilege.c` `do_ecall()` | ❌ 旧协议：参数写**全局变量** `ecall_args[2]`（非寄存器约定），`ecall_args[0]=ECALL_GOTO_PRIV(1)`、`ecall_args[1]=目标特权级` |
| M 态 handler | `common/trap.c` `m_trap_handler` | 🔶 仅有 GOTO 分支（读 `ecall_args[]` 判 `ECALL_GOTO_PRIV`）；无 ECALL_TEST、无 CSR_ACCESS、无链式转发 |
| S 态 handler | `common/trap.c` `s_trap_handler` | 🔶 同上，仅旧协议 GOTO 分支 |
| 汇编入口 | `common/trap_asm.S` | ❌ SAVE_CONTEXT 不含 a0/a1 的 T-SBI 语义使用（handler 不从 frame 读操作码） |
| Hypervisor 编排 | `common/hyp/hyp_priv.c` | ❌ `_v_trampoline`/`return_to_hs_mode` 直接写 `ecall_args[]` |
| 协议头 / CSR 代理 | `common/tsbi.h`、`common/csr_access_proxy.c` | ❌ 不存在 |

> **已验证参考实现存于 stash**：一份完整的 T-SBI 协议实现（a0/a1 寄存器约定、M/S 双层
> dispatch、CSR_ACCESS 动态注入 + 策略 A、raw-ecall 逃逸、软件委托，含 Sm/Ss raw-ecall
> 用例适配，QEMU 四模块回归通过）保存在 `git stash`（`tsbi-local-dev-backup-20260806`）。
> 该实现基于旧 master（72a8eaa），恢复时需随最新 master 的 trap.c/privilege.c 变更（+369/+30 行）做 rebase 适配，可作为 P2 协议基座的起点或参考。

### 1.2 已就绪的 T-SBI 配套设施（文档与平台配置）

| 设施 | 位置 | 说明 |
|------|------|------|
| 旧适配方案（纯自研路线） | `DOCS/framework/tsbi_adaptation_plan.md` | 协议解构与 Phase 划分仍具参考价值（其 Phase 状态描述对应 stash 中的实现，非 master 代码） |
| 接口契约参考 | `DOCS/framework/tsbi_interface_reference.md` | 协议接口/实现侧接口/项目封装 API 三分类 |
| T-SBI 详解 | `DOCS/developer_guide/T-SBI_explained.md` | T-SBI 定义、与官方 SBI 的对比（EID/FID vs a0 一级寻址等） |
| 官方规范镜像 | `SPEC/abstraction.adoc` | abstraction.adoc § T-SBI |
| 平台配置 ACT 化 | `common/config/*/rvtest_config.h`（UDB 生成）、`rvmodel_macros.h`、`link.ld` | `*_SUPPORTED`/`UDB_*`/`RVMODEL_*` 宏经 Makefile `-include` 注入 C 与汇编——正是 `rvtest_trap_handler.h` 消费的配置约定 |
| CI | `.github/workflows/ci.yml` + spike/sail/toolchain 安装脚本 | 框架改动的回归执行底座 |

> **注意区分**：master 新增的 `sbi/` 套件是**标准 SBI（OpenSBI 接口）的测试套件**
> （Base/HSM/IPI/PMU/DBCN…），被测对象是 ratified SBI 实现，与 T-SBI（测试基础设施）
> 完全是两回事，互不影响。

### 1.3 现有执行模型（本次要改的核心）

```
M 态 boot(entry.S) → main() @ M 态 → 测试默认 M 态直接 CSRR/CSRW
    → run_in_priv(S/U/VS/VU) 临时降权 → TEST_END: goto_priv(PRIV_M) 回 M 态收尾
```

与目标模型（S 态常态、M 操作必经 T-SBI）方向相反，涉及 `entry.S`、`test_framework.{c,h}`、
`reset_state()`、各套件 M CSR 直访点的系统性调整。

### 1.4 ACT4 rvtest_trap_handler.h 解构（2857 行，act4 分支现行版）

**性质**：纯 GNU 汇编宏文件（含裸 `.macro`/`.set` 指令），只能被 **.S 翻译单元** `#include`，C 文件无法直接包含。

**提供的能力**（按 SECTION）：

| 段 | 内容 | 对本仓库的价值 |
|----|------|---------------|
| §1\~§5 | 寄存器别名（T1\~T6=x6\~x9,x14,x15；a0/a1 保留为 T-SBI 参数寄存器）、`TSBI_*` 操作码、模式编码常量 | **协议权威源**：常量必须与 `common/tsbi.h` 锁定一致 |
| §9 XCSR_RENAME | 按模式参数化 CSR 名（CSR_XEPC→mepc/sepc/vsepc） | 参考；本框架 C handler 已按模式分函数 |
| §10 | `RVTEST_TSBI_GOTO_xMODE` / `RVTEST_TSBI_ECALL_TEST` / `RVTEST_TSBI_CSR_ACCESS` 便利宏 | 汇编用例可直接使用 |
| §11\~§12 | legacy a0==0 GOTO、`RVTEST_GOTO_LOWER_MODE`（boot 降权，含 PA↔VA 重定位） | boot 降 S 的参考实现 |
| §14\~§16 | `RVTEST_TRAP_PROLOG/HANDLER/EPILOG/SAVEAREA` 每模式实例化：trampoline、T-SBI dispatch、trap signature 记录、中断清除表 | ACT 认证形态的完整 handler（见 §3.3 路线 2） |

**M/S 两级 T-SBI dispatch 语义**（与目标实现——stash 中已验证的 C 协议基座——逐条对照，
master 代码尚无 T-SBI，见 §1.1）：

| 行为 | ACT4 参考实现 | 目标 C 实现（stash 已验证） | 差异结论 |
|------|--------------|------------|---------|
| GOTO 1\~5 | `(a0-1)<5` 范围判定 → 配 MPP/MPV → xret | 相同（`TSBI_IS_GOTO_MODE`） | ✅ 一致 |
| ECALL_TEST 0x73 | 返回 xEPC 到 a0，epc+=4 | 相同 | ✅ 一致 |
| legacy a0==0 | 支持（rtn2mmode 快速路径） | **不支持**（a0=0 走 armed-trap） | 有意差异，旧方案 §7.2 已决策，维持 |
| **M 态 CSR_ACCESS** | **静态指令白名单表** `tsbi_instr_table`（mstatus/mie/mip/... 若干 CSR × 4 变体），查表执行；**未匹配 → 打印编码 + halt fail** | **动态指令注入** + fence.i + **策略 A 错误恢复**（注入指令 fault → 返回 -1，测试可继续） | ⚠️ 实现策略不同，见 §4.4 决策 |
| S 态 CSR_ACCESS | scratch 区写指令 + fence.i + jalr（动态注入） | 相同思路（csr_access_proxy） | ✅ 同源 |
| 内存代理 TSBI_LW/LD/SW/SD | 核心调用（以 M 态权限读写 a1 指向的物理地址，读 mtime/MMIO 用），在指令表中预置 | **未实现**（stash 中亦无） | ⚠️ 能力缺口，需在 P2 补齐（csr_access_proxy 同机制扩展支持 load/store 编码） |
| S→M 转发 | 恢复 T1\~T6/sp 后再 ecall，a0/a1 原样穿透 | `s_trap_handler` 内 `tsbi_ecall(a0,a1)` 嵌套 ecall | ✅ 语义等价 |
| 普通 trap | 记录 **trap signature**（vect+mode/cause/epc/tval 打包写签名区） | 记录 **trap_record**（C 结构体 + trap_expect 断言） | 两套断言体系，见 §3.3 |

**入库前必须认清的外部依赖清单**（辅助头要补齐的全部内容）：

| 依赖类别 | 符号举例 | 本仓库现状 |
|---------|---------|-----------|
| 汇编工具宏 | `LI()/LA()/SREG/LREG/REGWIDTH/WDBYTSZ/WDBYTMSK` | ❌ 无，辅助头提供 |
| 架构常量 | `CAUSE_USER_ECALL`、`MSTATUS_MPP`、`SSTATUS_SPP`、`CSR_MSCRATCH`… | 🔶 `common/encoding.h` 大部分有，需核对补齐 |
| 配置宏 | `UDB_MXLEN`、`S_SUPPORTED/H_SUPPORTED`、`SM1P11P0_SUPPORTED`、`UDB_MTVEC_MODES_*` | ✅ 远端 `rvtest_config.h`（UDB 生成）已提供 |
| 平台宏 | `RVMODEL_MTIMECMP_ADDRESS`、`RVMODEL_SET/CLR_*_INT`、`RVMODEL_HALT_*` | ✅ 远端 `rvmodel_macros.h` 已提供（缺项有 §13 默认桩） |
| 签名机制 | `TRAP_SIGUPD` 宏、`rvtest_sig_begin`、`sig_end_canary`、`saved_xepc` | ❌ 仅 ACT 认证形态需要，见 §3.3 |
| 运行时符号 | `rvmodel_io_write_str`、`rvmodel_halt_fail`、`cleanup_epilogs`、`rvtest_Xroot_pg_tbl`（S PROLOG 恒等页表） | ❌ 仅 ACT 认证形态需要 |

---

## 2. 目标与原则

1. `rvtest_trap_handler.h` **原样入库**（一字不改），作为 T-SBI 协议的单一权威源；所有适配放进辅助头，升级该文件 = 直接替换；
2. 测试程序**常态运行在 S 态**；凡涉及 M 态的操作（读写 M CSR、进入 M 态执行激励、操纵 M 中断），**必须通过 T-SBI**（CSR_ACCESS 代理或 GOTO excursion），测试代码不得假设自己拥有 M 特权；
3. 用例作者只面对高层 API（`run_in_priv`/`goto_priv`/`tsbi_csr_*`/`trap_expect`），不直接手写 `ecall a0=...`（既有设计规范，维持）；
4. 现有 trap_record 断言体系、四模块回归金标准、20 个虚拟化目录基线**不回退**。

---

## 3. 框架适配方案

### 3.1 文件落位与入库纪律

```
common/
├── act4/                            # 新增：ACT4 上游文件专区
│   ├── rvtest_trap_handler.h        # 原样 vendor（BSD-3-Clause，记录来源 commit）
│   ├── VERSION                      # 记录上游 branch/commit/日期，升级时更新
│   └── rvtest_env.h                 # 辅助头（本方案新增，见 §3.2）
├── tsbi.h                           # C 侧 wrapper（保留，常量与 act4 文件锁定一致）
├── csr_access_proxy.c               # CSR_ACCESS 注入引擎（保留）
├── trap.c / trap_asm.S              # C handler 运行时（保留，P2 在其上落地 T-SBI dispatch）
└── ...
```

入库纪律：
- `common/act4/rvtest_trap_handler.h` 提交后**禁止本地修改**（diff 上游必须为零），
  一切"特殊需要适配的地方"只进 `rvtest_env.h`；
- `sync_opensource.sh` 白名单已含 `common`，`common/act4/` 自动随同步走，无需额外配置；
- 上游更新时：替换文件 → 更新 `VERSION` → 跑 §6 回归。

### 3.2 辅助头 rvtest_env.h（"特殊需要适配的地方"集中营）

单一头文件，用 `#ifdef __ASSEMBLER__` 分汇编/C 两半，经 `Makefile.common` 与
`rvmodel_macros.h` 同样方式 `-include` 注入（或由需要方显式包含）：

**汇编半区**（让 vendored 文件在本仓库可装配）：
- 工具宏：`LI()/LA()`（li/la 的常量安全封装）、`SREG/LREG`（sd/ld 按 XLEN）、
  `REGWIDTH/WDBYTSZ/WDBYTMSK`（8/4 按 XLEN）；
- `encoding.h` 缺失常量补齐：核对 `CAUSE_*`、`MSTATUS_MPP`、`SSTATUS_SPP`、
  `SATP64_MODE/SATP_MODE_SV39/48/57`、`CSR_MSTATUSH` 等，缺则补（补进 encoding.h 或本头）；
- `RVTEST_FENCEI`（fence.i 封装）、`RVTEST_CLR_STIMER_INT` 等文件引用但非 RVMODEL 命名空间的宏；
- ACT 认证形态（§3.3 路线 2）才需要的桩：`TRAP_SIGUPD`、签名区符号、`rvmodel_io_write_str` →
  绑定到本仓库 uart 例程 —— **一期不做，只留 `#ifdef TSBI_ACT4_HANDLER` 空壳**。

**C 半区**（让 C 测试与 vendored 文件协议同源）：
- `#include "tsbi.h"` 透传（tsbi.h 保持 C 侧唯一入口）；
- **一致性锁**：新增编译期校验单元（`common/act4/protocol_check.S` 或宏比对），
  以 `.if TSBI_GOTO_MMODE != 0x1 ; .err ; .endif` 方式将 vendored 文件与 `tsbi.h`
  的全部协议常量互锁，两边任何一侧漂移即编译失败；
- S 态自适应 CSR 访问器（给存量用例的迁移杠杆，见 §4.3）：
  ```c
  /* 按当前特权级自动选择直访或 T-SBI 代理 */
  static inline uintptr_t xcsr_read(uint16_t csr) {
      if (get_current_priv() == PRIV_M || !is_m_mode_csr(csr))
          return csr_read(csr);          /* 有权限：直访 */
      return tsbi_csr_read(csr);         /* S 态访 M CSR：T-SBI 代理 */
  }
  /* xcsr_write / xcsr_set / xcsr_clear 同构 */
  ```

### 3.3 T-SBI 运行时：双形态并存，一期以 C handler 为准

| | **形态 A（一期，默认）** | **形态 B（二期，ACT 认证跑法，可选）** |
|---|---|---|
| handler | 现有 `trap_asm.S + trap.c` 之上落地 T-SBI dispatch（P2 协议基座；master 尚为 ecall_args 旧协议，stash 有已验证实现可 rebase 复用） | 实例化 vendored 文件的 `RVTEST_TRAP_PROLOG/HANDLER/SAVEAREA M/S` |
| 协议源 | `rvtest_trap_handler.h` 常量（经一致性锁保证 tsbi.h 同源） | 同左（直接用其实现） |
| 断言 | trap_record + trap_expect（保留全部 C 断言能力） | trap signature 区 + 签名比对 |
| 构建 | 默认 | `make TSBI_ACT4_HANDLER=1`（独立开关，替换 trap 对象与入口） |

**决策依据**：
- 形态 A 保住既有的 trap_expect/CHECK_TRAP/策略 A/软件委托全部能力和 4 模块回归基线，
  改动集中在执行模型（§3.4），风险可控；
- 形态 B 依赖清单长（TRAP_SIGUPD、签名区、恒等页表 `rvtest_Sroot_pg_tbl`、io/halt 例程），
  且其 CSR_ACCESS 白名单表"未知编码即 halt fail"的语义会砍掉我们的负面测试能力（§4.4），
  只作为将来跑官方 ACT 套件/认证时的兼容形态，本方案先立框架（目录、开关、桩），不落实现；
- 两形态对**测试代码完全透明**：用例只见高层 API 和 tsbi_* wrapper，底下 handler 可切换。

### 3.4 执行模型改造：S 态常态（本方案的主工程量）

#### 3.4.1 启动流程（entry.S + main）

```
M 态：rvtest_entry_point
  ├─ hart0 筛选 / 栈 / BSS / RVMODEL_BOOT / _platform_init     （不变）
  ├─ mtvec = m_trap_entry, stvec = s_trap_entry                （提前到 boot，原在 reset_state）
  ├─ PMP 全开放（S/U 可 RWX 全地址段）                          （从各 main.c 上收到 boot，常态化）
  ├─ 配置委托基线（委托优先，见 §3.4.3）：medeleg≈0xFCB5FF 语义、
  │    mideleg≈0x366 语义；ecall-from-S 恒不委托——框架不变量
  ├─ M 态基线快照（framework 内部保存 mstatus/medeleg 等初值，供 reset 用）
  └─ 降权进 S：goto_priv(PRIV_S) 语义（等价 RVTEST_GOTO_LOWER_MODE Smode）
S 态：main()  ← 测试常态从这里开始
  ├─ uart_init/printf（S 态直访外设，PMP 已放行）
  ├─ reset_state()（S 态版本，见 3.4.2）
  └─ 逐个跑 _test_table[]，测试函数入口即 S 态
```

- 降权实现优先复用现有 `goto_priv(PRIV_S)`（M→S 走 mret，天然合规：降权不经 T-SBI）；
  `RVTEST_GOTO_LOWER_MODE` 宏保留为形态 B 的 boot 路径；
- 例外套件（见 §4.2 D 类）经 per-suite 开关 `TEST_BOOT_MODE=M` 维持 M 态入口。

#### 3.4.2 test_framework 适配

| 改造点 | 现状 | 目标 |
|--------|------|------|
| `TEST_END/TEST_SKIP/TEST_FATAL` | `goto_priv(PRIV_M)` + reset | `goto_priv(PRIV_S)` + reset（S 态收尾） |
| `reset_state()` | M 态直接 CSRW(mtvec/medeleg/...) | S 态运行：S CSR 直访复位；M CSR 复位（含**恢复委托基线值**，非清零）走**单次 M excursion**（`run_in_priv(PRIV_M, m_reset_worker)`，内部批量恢复基线后回 S）——避免逐 CSR 一次 trap 往返的性能开销 |
| `get_current_priv()` 常态断言 | 无 | 框架在每个测试入口检查 `== PRIV_S`，违约打印告警（迁移期定位利器） |
| `run_in_priv(PRIV_M, fn)` | 已走 T-SBI GOTO | 不变——这正是"M 态操作必须通过 T-SBI"的合规形态 |
| 中断编排 | M 态直写 mie/mip | S 态经 `tsbi_csr_set/clear(CSR_MIE/CSR_MIP, ...)` |
| `trap_expect` 系列 | trap_record 由 M handler 记录，`CHECK_TRAP` 在 M 态断言 | 记录点随主路径移到 **s_trap_handler**（委托直投），断言在 S 态执行（S 态常态下 printf 可用）；`trap_get_priv()` 继续区分 M/S 实际投递 |

#### 3.4.3 委托基线：委托优先（S handler 为测试 trap 第一现场）

> **决策更新（2026-08）**：放弃早期"medeleg/mideleg=0、一切 trap 先到 M"的 M 态优先基线，
> 改为对齐 ACT4 原生启动模型的**委托优先**基线。旧基线是 M 态常态时代的惯性，与 S 态常态
> 执行模型方向相反（每个激励 trap 平白多一次 M 往返）。

**分流原理（哪些请求归谁，是怎么"设定"的）**：

没有任何软件在逐个请求做路由决策——分流由一组硬件开关（medeleg/mideleg，boot 写一次）
与请求天生的 cause 值共同决定，trap 发生瞬间硬件查表定落点。对 S 态测试而言规则极简：
**服务找 M，事故归 S**——服务请求（一切 tsbi_* 调用与升权）都是 ecall，cause 恒为 9，
而 medeleg[9]=0，故全部直达 M handler；激励 trap（非法指令/缺页/断点…）各有自己的
cause，基线全委托，故全部直投 S handler。

> **易混点澄清**：S handler 的 P2 服务分支**不服务于 S 测试**——它只接被委托下来的
> ecall-from-U（cause=8，U 测试的请求）。S 测试的 ecall 是 cause=9，永远看不到
> S handler。也不应委托给它：① 权限不够（升 M/读 M CSR/访 MMIO 都需 M 权限，
> 委托了也得再转发，白费一跳）；② 死循环（`run_in_priv` 返回 ecall 落 S handler 的
> GOTO 分支会无限套娃）。

"设定"发生在三个层次：

```
层次 1：架构硬规则（不可配置）：trap 永不下投，M 源恒到 M；委托位只影响 S/U 源
层次 2：boot 基线（写一次全局生效——"设定"的主体，见下表）
层次 3：per-test 临时改：trap_undelegate_exc(cause) 把某激励 cause 切回"落 M"，
         测完 reset_state() 恢复基线
```

**基线配置**（boot 期写入，`reset_state()` 恢复的目标值）。medeleg 逐位设定
（对齐 adoc S-mode boot 值 `0xFCB5FF`）：

| bit | cause | 基线 | 理由 |
|-----|-------|:---:|------|
| 0\~7 | 指令/访存 misaligned、access fault、非法指令(2)、断点(3) | **1** | 激励类，S handler 就地消化 |
| 8 | ecall-from-U | **1** | U 测试的 T-SBI 请求先到 S handler（小单本层办、大单链式转发 M） |
| **9** | **ecall-from-S** | **0** | **T-SBI 服务通道，恒不委托**（框架不变量，`trap_delegate_exc()` 护栏拒绝此位） |
| 10 | ecall-from-VS（H） | 1 | VS 请求先到 HS handler（链式模型） |
| **11** | **ecall-from-M** | **0** | 架构规定 read-only-zero（M 源不可委托；QEMU WARL 行为见 EDELG-10/11 已知项） |
| 12/13/15 | 指令/load/store 缺页 | **1** | 激励类，VM 套件主力 cause |
| 14/17 | reserved | 0 | 保留位 |
| **16** | **double trap**（Ssdbltrp） | **0** | 恒不委托（双 trap 必须 M 处理） |
| 18/19 | software check（CFI 等）/ hardware error | 1\* | 激励类；\*仅当平台支持对应扩展 |
| 20\~23 | guest 缺页 ×3、virtual instruction（H） | 1\* | 落 HS handler（虚拟化套件主力 cause） |

mideleg（对齐 adoc `0x366` 语义）：SSI(1)/STI(5)/SEI(9) 及 H 扩展下 VSSI(2)/VSTI(6)
委托（H 存在时 VS 中断位本就 read-only-one）；LCOFI(13) 视 Sscofpmf/Shlcofideleg
能力并入；M 中断（MSI/MTI/MEI）架构上不可委托，恒落 M handler。

**生成方式**：基线值不硬编码常数，由框架按平台 `rvtest_config.h` 能力宏（H_SUPPORTED、
SSDBLTRP_SUPPORTED、SSCOFPMF_SUPPORTED 等）拼装；平台不支持的 cause 对应位写 0（WARL
自然忽略）。boot 写入后回读实际生效值存入基线快照，`reset_state()` 以快照为准恢复，
并据此识别 read-only-zero 位（需要 Delegate relay fallback 的依据）。

**路由结果**（与 §5.3 联动图对应）：

- S/U 态激励 trap → **硬件水平委托直投 S handler**（主路径，全程不经 M）；
- ecall-from-S → **恒到 M handler**（T-SBI 服务通道；委托它会使 `run_in_priv` 返回 ecall
  在 S handler 的 GOTO 分支上死循环——框架不变量，`trap_delegate_exc()` 护栏拒绝该位）；
- 未委托腿（`trap_dual_run` 显式切换 / 架构不可委托的 cause）→ M handler；
- M 源 trap → 恒 M 内消化（trap 不可下投；sret 也回不到 M 现场）；
- **Delegate relay（软件委托）= SW fallback**：仅当平台委托位 read-only-zero 等硬件委托
  不可用时，由 M 合成 sepc/scause/stval/sstatus 后 mret 进 stvec 补投——不是常态路径。

**联动语义变化**：

- `trap_dual_run()` 默认腿翻转：**委托腿免费**（基线即委托），不委托腿需显式
  `trap_undelegate_exc()` 切出；
- B 类（Sm_*）用例凡验证"trap 落 M"行为的，必须**显式切不委托腿**后再断言——这是委托
  基线翻转带来的主要迁移工作量（见 §8 风险表）；
- `tsbi_arm_soft_delegate()` 语义不变，但使用场景收窄为 fallback 验证与认证形态。

---

## 4. 测试代码适配方案

### 4.1 用例编写规范（新版契约）

```c
/* 常态：函数体默认运行在 S 态 */
static bool test_xxx(void) {
    TEST_BEGIN("XXX-01");

    /* ① 读写 S CSR：直访（本特权级权限内） */
    uintptr_t sst = CSRR(sstatus);

    /* ② 读写 M CSR：必须走 T-SBI CSR_ACCESS 代理（或 xcsr_* 自适应访问器） */
    uintptr_t mst = tsbi_csr_read(CSR_MSTATUS);
    tsbi_csr_set(CSR_MSTATUS, MSTATUS_TVM);

    /* ③ 必须在 M 态执行的激励序列：T-SBI GOTO excursion（经高层 API） */
    run_in_priv(PRIV_M, m_mode_stimulus, arg);   /* 底层 = TSBI_GOTO_MMODE */

    /* ④ ecall 自身作为激励：raw-ecall 逃逸（consume-once） */
    tsbi_arm_raw_ecall(true);
    run_in_priv(PRIV_U, raw_ecall_stimulus, 0);
    CHECK_TRAP(CAUSE_USER_ECALL, ...);

    TEST_END();   /* 回 S 态收尾 */
}
```

禁止项：
- 直接 `asm("ecall")` 编排特权级（走高层 API，T-SBI 是底层机制不外露——既有规范）；
- S 态直接 `CSRR(mstatus)` 一类 M CSR 直访（触发 illegal instruction，迁移期由框架入口
  特权级断言 + UNEXPECTED TRAP 双重暴露）。

### 4.2 存量用例迁移策略（按套件画像分四类）

| 类别 | 画像 | 迁移动作 | 代表套件 |
|------|------|---------|---------|
| A：已高层 API 化 | 特权编排全走 `run_in_priv`/`goto_priv`，M CSR 触碰少 | 仅将 M CSR 直访点换成 `tsbi_csr_*`/`xcsr_*`；逻辑零改动 | Ss_*、Hypervisor 系（HS 即 S 态，天然满足）、虚拟化 20 目录 |
| B：M 态重度套件 | 测试对象本身是 M 态行为（M CSR、PMP、M 中断） | 用例主体改为「S 态编排 + `run_in_priv(PRIV_M, 激励)` + `tsbi_csr_*` 断言取值」；批量 M 操作合并进单次 excursion 控制开销；**验证 trap-落-M 行为的用例需先 `trap_undelegate_exc()` 切不委托腿（§3.4.3）** | Sm_CSR、Sm_Exceptions、Sm_Interrupts、pmp/smepmp、Smstateen… |
| C：raw-ecall 用例 | ecall 自身是被测激励 | 随 P2 提供 `tsbi_arm_raw_ecall` 后适配（stash 中已有 Sm/Ss 套件的完整适配可复用），并随所在套件切 S 态常态 | Sm/Ss_Exceptions 的 ECALL-*、EXCC-* |
| D：例外套件 | 依赖 M 态早期环境或特殊 trap 入口，暂不宜 S 常态 | 保持 `TEST_BOOT_MODE=M`，列入例外清单排期单独评估 | Smdbltrp/smrnmi（RNMI/双 trap 需 M 态原生入口）、ntrace、iopmp、clic 等 |

迁移顺序（每步过 §6 验收再进下一步）：

```
① framework_test（S 态常态 smoke，新增用例见 §6.1）
② Ss_* 金标准两模块（Ss_Interrupts / Ss_Exceptions——本就以 S 视角设计，改动最小）
③ Sm_* 金标准两模块（Sm_Interrupts / Sm_Exceptions——B 类改造范式在此定型）
④ 虚拟化 20 目录（A 类，预期近零改动）
⑤ 其余套件按 A→B 批量推进；D 类单独立项
```

### 4.3 迁移杠杆：xcsr_* 自适应访问器

B 类套件中 M CSR 直访点数量大（`grep -c CSRR.*m[a-z]` 粗估数千处）。逐点改 `tsbi_csr_*`
工程量大且在 M excursion 内反而多余（M 态内直访即可）。`xcsr_*`（§3.2）按运行时特权级
自动选路，使同一段代码在「M excursion 内」与「S 态直调」都正确，把 B 类迁移简化为
**机械替换 CSRR/CSRW → xcsr_read/xcsr_write（仅 M CSR 触点）**，可脚本化 + 人工复核。

### 4.4 CSR_ACCESS 实现策略决策：维持动态注入 + 策略 A

ACT4 参考实现的 M 态 CSR_ACCESS 用静态白名单指令表，未知编码 → 打印 + halt fail。
本仓库**不跟随**，维持动态注入 + 策略 A，理由：

1. 我们大量负面用例需要"代理访问不存在/无权限 CSR → 返回 -1 且不崩溃"（策略 A 语义），
   白名单表直接终止测试，能力是倒退；
2. 协议对外语义不变（a0=指令编码、a1=rs1、返回 a0），调用方无感知；
3. 白名单表的动机是"避免自修改代码"（某些 DUT 指令区不可写）——本仓库镜像整体 RWX，
   无此约束；若将来某平台指令区只读，再在 csr_access_proxy 内加表模式作为 fallback；
4. 形态 B（ACT 认证跑法）实例化参考实现时自然拿到表方案，两形态各取所需。

---

## 5. 系统框图

### 5.1 分层架构

```
┌─────────────────────────────────────────────────────────────────────┐
│ 测试用例层（C，常态 S 态）  Sm_*/Ss_*/Hypervisor_*/pmp/...          │
│   TEST_BEGIN/END · TEST_ASSERT · CHECK_TRAP                          │
└───────────────┬─────────────────────────────────────────────────────┘
                │ 只调用高层 API，不直接 ecall
┌───────────────▼─────────────────────────────────────────────────────┐
│ 框架 API 层（C）                                                     │
│  特权编排: run_in_priv / goto_priv / run_in_vs_mode / run_in_vu_mode │
│  M 资源代理: tsbi_csr_read/write/set/clear · xcsr_*(自适应)          │
│  trap 断言: trap_expect_* / trap_record / trap_get_priv              │
│  逃逸控制: tsbi_arm_raw_ecall · tsbi_arm_soft_delegate               │
└───────────────┬─────────────────────────────────────────────────────┘
                │ tsbi_ecall(a0=op, a1=arg) —— 寄存器约定
┌───────────────▼─────────────────────────────────────────────────────┐
│ T-SBI 协议层（单一权威源）                                           │
│  common/act4/rvtest_trap_handler.h  ←原样 vendor（协议常量+参考实现）│
│  common/act4/rvtest_env.h           ←辅助头（环境宏+一致性锁+桥接）  │
│  common/tsbi.h                      ←C 侧镜像（编译期与上游互锁）    │
│  操作码: GOTO 0x1~0x5 │ ECALL_TEST 0x73 │ CSR_ACCESS(指令编码)      │
└───────────────┬─────────────────────────────────────────────────────┘
                │ ecall trap
┌───────────────▼─────────────────────────────────────────────────────┐
│ Trap/Handler 运行时（二选一，对上透明）                              │
│ ┌─ 形态 A（默认）────────────────┐ ┌─ 形态 B（ACT 认证，可选）─────┐│
│ │ trap_asm.S: SAVE_CONTEXT       │ │ RVTEST_TRAP_PROLOG/HANDLER/    ││
│ │ trap.c: m/s_trap_handler       │ │ SAVEAREA M/S 实例化            ││
│ │  · T-SBI dispatch M+S 两级     │ │  · trampoline + spreader       ││
│ │  · csr_access_proxy(策略 A)    │ │  · tsbi_instr_table(白名单)    ││
│ │  · trap_record 断言            │ │  · trap signature 签名         ││
│ └────────────────────────────────┘ └────────────────────────────────┘│
└───────────────┬─────────────────────────────────────────────────────┘
┌───────────────▼─────────────────────────────────────────────────────┐
│ 平台配置层  rvtest_config.h(UDB 生成) · rvmodel_macros.h · link.ld   │
│ 硬件/模拟器  QEMU max · xiaohui_c9082/c9204/e908a · Spike            │
└─────────────────────────────────────────────────────────────────────┘
```

### 5.2 启动与常态执行时序

```
 M 态                                    │ S 态（测试常态）
─────────────────────────────────────────┼──────────────────────────────
 rvtest_entry_point                      │
   ├ 栈/BSS/RVMODEL_BOOT/_platform_init  │
   ├ mtvec/stvec 装载 (m/s_trap_entry)   │
   ├ PMP 全开放 (S/U RWX)                │
   ├ 委托基线（委托优先，§3.4.3）       │
   ├ M 态基线快照                        │
   └ goto_priv(PRIV_S) ──── mret ──────► │ main()
                                         │   ├ uart_init / reset_state(S 版)
                                         │   └ for tc in _test_table:
                                         │        tc()  ← 入口即 S 态
          ┌──────────────────────────────┤        │
          │  M excursion（T-SBI GOTO）    ◄────────┤ run_in_priv(PRIV_M, fn)
 m_trap_handler: GOTO_MMODE → mret       │        │   = ecall a0=0x1
   fn() 在 M 态执行激励                  │        │
   goto_priv(PRIV_S) ── ecall+mret ────► │ ◄──────┘ 返回 S 态继续断言
          │                              │
          │  CSR 代理（T-SBI CSR_ACCESS） ◄────────┐ tsbi_csr_read(mstatus)
 m_trap_handler: 注入执行 csrrs a0,...   │         │   = ecall a0=0x30002573
   mepc+=4 → mret，a0 带回 CSR 值 ─────► │ ────────┘ 断言取值，不离开 S 态
```

### 5.3 trap 路由联动图（委托优先模型，六条路径）

> 可视化版见文档头语雀画板；线型约定：紫=T-SBI 通路，灰=trap 投递/返回。

```
① 激励 trap（委托命中——常态主路径，流量最大）
   Test Code (S/U) ─硬件水平委托直投─► s_trap_handler ─记录+修 epc+sret─► 回测试断言

② T-SBI 服务（ecall-from-S，恒不委托）
   Test Code (S) ─ecall(a0=op)─► m_trap_handler 办结 ─mret─► ecall+4 继续
                                  （GOTO 时特权级已变；M excursion 由此进入）

③ 链式转发（被委托的 ecall-from-U，S 层办不了的大单）
   Test Code (U) ─ecall─► s_trap_handler ─小单本层办/大单 re-ecall(a0/a1 穿透)─►
   m_trap_handler 办结 ─mret 回 s_trap_handler─► sret 回 U 调用点

④ 未委托腿（trap_dual_run 显式切出 / 不可委托 cause）
   Test Code (S/U) ──► m_trap_handler ── (a) M 侧记录，mret 回测试；或
                                          (b) Delegate relay（SW fallback，
                                              仅 S/U 源）─► s_trap_handler ①路径

⑤ M 源激励（GOTO excursion 内；架构上不可下投）
   Test Code (M) ──► m_trap_handler ─记录+mret─► 回 M excursion 继续

⑥ 嵌套 fault（CSR 代理注入指令自身出错）
   handler ↺ 自环（P0 策略 A 认领，代理调用返回 -1）
```

总路由规则：落点由"来源特权级 + medeleg/mideleg"硬件决定；ecall-from-S 是唯一恒到 M
的通道；委托命中的 trap 不碰 M；M 源 trap 不出 M。升权必经 T-SBI GOTO（陷入 handler
代办），降权直接 mret/sret——不存在"上行直达通道"。

### 5.4 handler 内部判定链（P0~P5，规范性顺序）

M/S 两个 handler 共用同一判定模板，**从上到下即优先级，命中即出口（xret），换序即 bug**：

```
入口：SAVE_CONTEXT，读 xcause/xepc
 P0 ◇ csr_access_record.armed？      ─是► 嵌套 fault（策略 A）：记 cause、xepc=recover，
 │                                        代理最终返回 -1
 P1 ◇ is_ecall 且 _skip_tsbi_dispatch？─是► 清标志（consume-once），跳过 P2/P3 直插 P4
 │                                        （raw-ecall 激励改道）
 P2 ◇ is_ecall 且 a0 为合法操作码？   ─是► T-SBI dispatch：GOTO / ECALL_TEST /
 │                                        CSR_ACCESS / LW-SW；S 实例只办子集+上传大单
 P3 ◇ is_interrupt？                   ─是► 清中断源、按需记录
 P4 ◇ trap_record.armed？             ─是► 预期激励：记 cause/epc/tval、修 epc 放行
 │                                        （M 实例另有 (b) Delegate relay 选项）
 P5 ■ 无人认领                        ──► UNEXPECTED TRAP：打印现场，FAIL 终止
```

三条顺序硬约束（违反即真实 bug）：

| 约束 | 违反后果 |
|------|---------|
| P0 绝对第一 | handler 自己注入的指令炸了会被误记进 trap_record（污染测试断言）或落 UNEXPECTED |
| P1 必须在 P2 之前 | raw-ecall 激励时 a0 不受控，若恰为 1~5/0x73 会被当服务执行，激励凭空消失 |
| P2 必须在 P4 之前 | `trap_expect` 布防窗口内 `run_in_priv` 自身要发 GOTO ecall，若先查 armed 会把框架 ecall 当成预期激励吞掉 |

S/M 实例的参数化差异（同一模板、两处不同，根源都是特权不对称）：

| 判定级 | S 实例 | M 实例 |
|--------|--------|--------|
| P2 服务能力 | 子集（ECALL_TEST、GOTO_S/U、S/U 级 CSR）+ 转发出口 | 全集（GOTO 1~5、全部 CSR、LW-SW），链路终点 |
| P4 记录后 | 仅 (a) 记录放行 | (a) 记录 或 (b) Delegate relay（SW fallback） |
| 出口指令 | sret | mret |

### 5.5 路由速查表（哪类请求落哪个 handler）

**T-SBI 服务类**（图中紫线）：

| 请求 | T-SBI 功能 (a0) | 发起方 | 落点 | 判定级 |
|------|----------------|--------|------|--------|
| 升权（S→M 等） | GOTO_xMODE (0x1~0x5) | S 测试 | M handler | P2 |
| 升权（U 发起） | GOTO_S/U 本层；GOTO_M/VS/VU 转发 | U 测试 | S handler* → M | P2/转发 |
| 访 M 级 CSR | CSR_ACCESS（CSR 指令编码） | S 测试 | M handler | P2 |
| 访 M 级 CSR（U 发起） | CSR_ACCESS，转发 | U 测试 | S handler* → M | P2 转发 |
| ecall 通路探针 | ECALL_TEST (0x73) | S/U 测试 | M / S* handler | P2 |
| 读写 mtime/MMIO | TSBI_LW/LD/SW/SD | S 测试 | M handler | P2 |
| 代理指令自身出错 | 策略 A（嵌套捕获） | handler 自身 | 同一 handler | P0 |

**测试框架 trap 类**（非 T-SBI，图中灰线）：

| 请求 | T-SBI 功能 | 发起方 | 落点 | 判定级 |
|------|-----------|--------|------|--------|
| 设计的激励 trap（委托腿，常态） | 无 | S/U 测试 | S handler | P4 |
| 设计的激励 trap（不委托腿） | 无 | S/U 测试 | M handler | P4 |
| M excursion 内的激励 | 无 | M 测试代码 | 仅 M handler | P4 |
| raw-ecall 激励 | 仅逃逸标志（tsbi_arm_raw_ecall） | S/U 测试 | M / S* handler | P1→P4 |
| 中断（S 中断，已委托） | 无 | 任意 | S handler | P3 |
| 中断（M 中断） | 无 | 任意 | M handler | P3 |

\* 假定 ecall-from-U 已委托（medeleg[8]=1，委托优先基线下成立）；否则直落 M handler。

---

## 6. 验证策略

### 6.1 framework_test 新增 S 态常态 smoke（第一道门）

在既有 T-SBI smoke 六类路径（GOTO 切换 / ECALL_TEST / CSR 读写 / CSR 位操作 /
策略 A 错误恢复 / raw-ecall 逃逸 / HS 转发）基础上新增：

1. `main()` 入口 `get_current_priv() == PRIV_S`；
2. S 态 `tsbi_csr_read(CSR_MSTATUS)` == M excursion 内直读值（代理正确性交叉验证）；
3. `run_in_priv(PRIV_M, fn)` 往返后仍回 S 态、trap_record 干净；
4. TEST_END 后特权级 == S；
5. 协议一致性锁编译单元（tsbi.h vs vendored 文件常量互斥校验）随构建强制生效；
6. 委托基线验证：S 态非法指令激励 `trap_get_priv()==PRIV_S`（委托直投）；
   `trap_undelegate_exc()` 切不委托腿后同激励 `trap_get_priv()==PRIV_M`；
   ecall-from-S 始终落 M（服务通道不受委托基线影响）。

### 6.2 回归金标准（沿用 + 升级验收线）

QEMU `-cpu max,zicfilp=false,zicfiss=false`：

| 模块 | 基线 | S 态常态化后验收 |
|------|------|-----------------|
| Sm_Interrupts | Total=91 Pass=76 Fail=0 Skip=15 | Pass/Skip/Fail 不回退 |
| Sm_Exceptions | Total=100 Pass=86 Fail=2(EDELG-10/11 已知) Skip=12 | 除已知 2 fail 外 0 fail |
| Ss_Interrupts | Total=38 Pass=36 Fail=0 Skip=2 | 不回退 |
| Ss_Exceptions | Total=52 Pass=37 Fail=0 Skip=15 | 不回退 |

统一硬指标：**0 UNEXPECTED TRAP**；每用例入口特权级断言 0 告警。

### 6.3 全量回归

虚拟化 20 目录（旧方案 §6 清单）+ 已迁移套件逐批跑；硬件平台（xiaohui_c9082/c9204）
在 QEMU 全绿后抽测 B 类代表套件。

---

## 7. 实施阶段划分

| 阶段 | 内容 | 交付判据 |
|------|------|---------|
| P1 文件入库 | vendor `rvtest_trap_handler.h` + `VERSION`；`rvtest_env.h` 汇编工具宏 + 协议一致性锁 | 一致性锁编译过；全套件构建不破 |
| P2 协议基座 | 在最新 master 上落地 T-SBI 协议（可从 stash `tsbi-local-dev-backup-20260806` rebase 恢复）：`tsbi.h` + `trap_asm.S` a0/a1 frame 语义 + M/S 双层 dispatch + `csr_access_proxy`（策略 A）+ raw-ecall 逃逸；`do_ecall/ecall_args` 迁移至 `tsbi_ecall` 寄存器约定（含 `hyp_priv.c` 调用点）；补齐 TSBI_LW/LD/SW/SD 内存代理 | T-SBI smoke 六类路径全过；§6.2 四模块基线不回退 |
| P3 S 态常态框架 | entry.S 降权、**委托优先基线落地（§3.4.3，含 ecall-from-S 护栏不变量）**、test_framework S 版 reset/TEST_END（reset 恢复委托基线）、trap_record 记录点移 S、`xcsr_*` 访问器、入口特权级断言、`TEST_BOOT_MODE` 开关 | framework_test smoke（§6.1，含委托基线验证项）全绿 |
| P4 金标准迁移 | Ss_* → Sm_*（B 类范式定型；**Sm_* 验证 trap-落-M 的用例显式切不委托腿**） | §6.2 四模块达验收线 |
| P5 面上推广 | 虚拟化 20 目录 → 其余 A/B 类套件；D 类例外清单评审 | §6.3 全量不回退 |
| P6（可选）形态 B | ACT 认证 handler 实例化：TRAP_SIGUPD/签名区/恒等页表/io 例程补齐，`TSBI_ACT4_HANDLER=1` 独立构建 | 官方 ACT 用例可在本环境装配运行 |

---

## 8. 风险与对策

| 风险 | 影响 | 对策 |
|------|------|------|
| M CSR 代理的 trap 往返开销（每次 \~百条指令） | B 类套件用例内高频 M CSR 访问变慢 | 批量 M 操作合并进单次 `run_in_priv(PRIV_M, ...)` excursion；`xcsr_*` 在 M 态内自动直访 |
| S 态访问 uart/printf 被 PMP/PMA 拦截 | 常态输出不可用 | boot 期 PMP 全开放常态化；硬件平台核对外设区 PMA 属性 |
| 存量 M CSR 直访点漏改 | S 态 illegal instruction | UNEXPECTED TRAP 即暴露 + 入口特权级断言 + 静态 grep 清单核销 |
| 委托基线翻转冲击存量 M 侧断言用例 | Sm_* 等假定"trap 落 M"的用例在委托基线下断言失败 | 框架提供 per-test `trap_undelegate_exc()` 显式切不委托腿；P4 阶段在 Sm_* 金标准上定型该范式；`trap_get_priv()` 断言兼容两腿 |
| ecall-from-S 被误委托 | GOTO 死循环挂死 | `trap_delegate_exc()` 护栏（随 P2 落地）升级为框架不变量并在 smoke 中验证 |
| vendored 文件被顺手改动 | 失去上游可替换性 | CI/review 规则：`common/act4/rvtest_trap_handler.h` 任何 diff 必须伴随 `VERSION` 更新且 diff 与上游一致 |
| D 类套件（Smdbltrp/smrnmi 等）强行 S 常态化 | 功能性破坏 | `TEST_BOOT_MODE=M` 例外机制，白名单管理，单独排期评估 |
| 上游 act4 分支协议再演进（如 GOTO 编码变更） | 协议漂移 | 一致性锁使漂移在替换文件当天编译期暴露 |

---

## 9. 决策记录

1. ✅ `rvtest_trap_handler.h` **原样入库**至 `common/act4/`，为协议单一权威源；适配一律进辅助头 `rvtest_env.h`；
2. ✅ 执行模型改为 **S 态常态**：boot M 态初始化后降 S，M 态操作必经 T-SBI（GOTO excursion / CSR_ACCESS 代理）；
3. ✅ 一期运行时维持**形态 A**（C handler 上落地 T-SBI 协议，stash 中已验证实现可 rebase 复用）；ACT4 汇编 handler 作为**形态 B** 二期可选构建；
4. ✅ CSR_ACCESS 维持**动态注入 + 策略 A**，不跟随上游白名单表（保负面测试能力）；表模式留作只读指令区平台的 fallback；
5. ✅ 沿用既有决策：GOTO 0x1\~0x5、不引入 a0==0 legacy 路径、ecall-from-S 恒不委托、raw-ecall consume-once 逃逸、用例不直接触碰 T-SBI 原语；
6. ✅ 存量迁移用 `xcsr_*` 自适应访问器收敛改动，B 类套件范式在 Sm_* 金标准上定型后推广；
7. ✅ 例外套件经 `TEST_BOOT_MODE=M` 白名单保留 M 态入口，单独排期；
8. ✅ **委托基线采用委托优先**（对齐 ACT4 原生 medeleg≈0xFCB5FF/mideleg≈0x366 语义）：
   S handler 是测试 trap 的第一现场（水平委托主路径），ecall-from-S 为唯一恒到 M 的服务
   通道，Delegate relay 仅作硬件委托不可用时的 SW fallback；早期"medeleg=0 一切先到 M"
   基线废弃（与 S 态常态模型方向相反）。

---

## 附录 A：测试程序接口清单与符合性对照

### A.1 接口四层清单

**第 1 层：高层封装 API（用例首选，方案规定的正门）**

| 接口 | 用途 | 底层 T-SBI 操作 | 状态 |
|------|------|----------------|------|
| `run_in_priv(priv, fn, arg)` | 去目标特权级执行函数并返回 | GOTO_xMODE（升权段） | master 已有，P2 底层切 T-SBI 协议 |
| `goto_priv(target)` | 单程特权切换 | GOTO_xMODE（升权时） | 同上 |
| `run_in_vs_mode` / `run_in_vu_mode` | H 扩展世界编排 | GOTO_VSMODE/VUMODE | 同上 |
| `get_current_priv()` | 查询当前特权级 | —（纯查询） | master 已有 |
| `xcsr_read/write/set/clear(csr)` | 自适应 CSR 访问（够权限直访，不够走代理） | CSR_ACCESS（按需） | P3 新增 |

**第 2 层：T-SBI C wrapper（`tsbi.h`，单点服务调用）**

| 接口 | 语义 | 对应协议调用 | 状态 |
|------|------|-------------|------|
| `tsbi_csr_read(csr)` | 代理读 CSR，返回值 | `0x<CSR>02573` | stash 已验证，P2 恢复 |
| `tsbi_csr_write/set/clear(csr, val)` | 代理写/置位/清位 | `0x<CSR>59073/5a073/5b073` | 同上 |
| `tsbi_ecall_test()` | trap 通路探针，返回 xEPC | `a0=0x73` | 同上 |
| `tsbi_lw/ld/sw/sd(...)` | M 权限读写物理地址（MMIO） | `0x0005a503` 等 | **P2 待补**（能力缺口） |

**第 3 层：控制面 API（改变下一次 trap 的处理方式）**

| 接口 | 用途 | 状态 |
|------|------|------|
| `tsbi_arm_raw_ecall(true)` | 下一个 ecall 绕过 dispatch、当被测激励记录（consume-once） | stash 已验证，P2 恢复 |
| `tsbi_arm_soft_delegate(cause)` | 下一个到 M 的 armed 异常按硬件委托语义 relay 给 S（SW fallback） | 同上 |
| `trap_delegate_exc/trap_undelegate_exc(cause)` | 带护栏配置 medeleg（恒拒绝 ecall-from-S） | 同上；委托优先基线下 `undelegate` 成为 B 类用例常用项 |
| `trap_expect_begin/end()` + `CHECK_TRAP()` + `trap_get_priv()` | 被动 trap 的声明与断言（**非 T-SBI**，与之共存） | master 已有 |

**第 4 层：汇编便利宏（vendored 文件直接提供，供纯汇编用例/boot）**

| 宏 | 来源 | 状态 |
|----|------|------|
| `RVTEST_TSBI_GOTO_{M,S,U,VS,VU}MODE` | rvtest_trap_handler.h §10 | P1 入库后即可用 |
| `RVTEST_TSBI_ECALL_TEST` / `RVTEST_TSBI_CSR_ACCESS encoding, arg` | 同上 | 同上 |
| `RVTEST_GOTO_LOWER_MODE`（仅 boot 期降权） | 同上 §12 | 同上，形态 B/boot 参考 |

### A.2 与 rvtest_trap_handler.h / abstraction.adoc 的符合性核验

| 上游要求 | 我方接口 | 符合？ |
|---------|-----------|:---:|
| 调用约定：ecall，a0=操作码，a1=参数，a0=返回值 | `tsbi_ecall()` 寄存器约定内联汇编 | ✅ |
| GOTO 编码 0x1\~0x5 | `TSBI_GOTO_*` 常量 + 编译期一致性锁 | ✅ |
| ECALL_TEST=0x73 返回 xEPC | `tsbi_ecall_test()` | ✅ |
| CSR_ACCESS 编码模板（rd=a0 读、rs1=a1 写） | `TSBI_CSR_*_ENCODE()` 宏逐位一致 | ✅ |
| 未知 a0 返回 -1（RESERVED） | handler 返回 -1 / armed-trap | ✅ |
| handler 不占用 a0/a1 作临时寄存器 | C handler 从 frame 读、写回 frame[a0]，语义等价 | ✅ |
| S/HS handler 同调用集 + 链式转发 | s_trap_handler 对称 dispatch + 嵌套 ecall 转发 | ✅ |
| 委托优先启动模型（medeleg=0xFCB5FF/mideleg=0x366） | §3.4.3 委托优先基线 | ✅（本次对齐） |
| 便利宏 RVTEST_TSBI_*（adoc 注明 not required） | 汇编版原样入库；C 侧功能等价 wrapper | ✅ |
| TSBI_LW/LW+4/LD/SW/SW+4/SD 内存代理 | 暂缺 | ⚠️ P2 补齐项 |
| legacy a0==0 GOTO（RVTEST_GOTO_MMODE） | 有意不支持（a0=0 走 armed-trap） | ⚠️ 有意差异，§9 决策 5 |

### A.3 协议外的项目扩展（协议透明，不占用 a0 编码空间）

1. `tsbi_arm_raw_ecall()`——上游无"ecall 当激励"的逃逸机制（其参考实现把一切 ecall 当
   SBI 调用，未匹配即 fail）；我们需真实执行 ecall 异常用例，故加此调用方侧控制面；
2. `tsbi_arm_soft_delegate()` / `trap_dual_run()`——服务于委托/不委托双跑验证（RVA23 CRD
   要求的测试场景，上游参考实现未提供现成接口）；
3. `xcsr_*` 自适应访问器——纯迁移工程杠杆，上游无 C API 概念（其为纯汇编框架）。

结论：凡走到 a0/a1/ecall 协议线上的部分与上游完全一致且有编译期一致性锁兜底；差异仅
两处且均为显式决策（legacy a0==0 不支持、LW/SW 待补）；其余是上游未定义、我方补充的
调用方侧便利层，不影响协议互操作。
