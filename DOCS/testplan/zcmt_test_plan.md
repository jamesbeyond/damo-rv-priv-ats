# Zcmt 压缩表跳转扩展测试计划（Zcmt v1.0）

## 概述

本测试计划覆盖 RISC-V Zcmt（Extension for Compressed Table Jumps，v1.0）扩展，验证表跳转指令 `cm.jt` / `cm.jalt` 的指令编码、操作语义、跳转向量表（JVT）寻址与异常处理行为，以及 `jvt` CSR（地址 0x017，URW）的 WARL 字段属性、mode 发现机制、虚拟内存语义、各特权级可执行性与表更新可见性（fence.i）要求。

本测试计划依据 `SPEC/riscv-isa-manual/src/unpriv/zcmt.adoc` 中的规范点（norm 标记）编写（`normative_rule_defs/` 目录下无 zcmt.yaml，规范点以 adoc 原文锚点为准）。

### 本文档覆盖的 SPEC 章节
- ext:zcmt Extension for Compressed Table Jumps（扩展依赖/冲突关系、指令表）
- Table Jump Overview（256 项 XLEN 宽跳转表、64 字节对齐、表项字节序）
- jvt CSR（地址 0x017、URW、RV32/RV64 格式、BASE/MODE 字段 WARL 约束、虚拟地址语义）
- Table Jump Fault handling（两次隐式取指、执行权限要求、xEPC/xCAUSE/xTVAL 规则、fence.i 可见性）
- cm.jt / cm.jalt 指令定义（编码、index<32 与 index>=32 解码划分、伪代码语义）

### 范围说明
- Zcmt 依赖 Zca 与 Zicsr 扩展，且与 Zcd 扩展冲突（SPEC 明确声明）；本计划将依赖/冲突关系作为环境前提（配置声明层面）处理，不设用例。
- 本框架主目标为 RV64（XLEN=64）：表项宽度 8 字节，表地址偏移为 index<<3；RV32 平台（如 qemu-rv32-max）复用同一语义用例（表项 4 字节，偏移 index<<2）。
- `jvt` 按 SPEC 允许为只读实现（`norm:jvt_op`）：用例以 ZJVT-05 的写能力探测结果分支，只读实现下无法由测试构造跳转表的功能用例标记为不适用（非 FAIL）。
- `jvt`.mode 仅 mode 0（Jump table mode）有定义行为；写保留值后读回必须是某个已实现模式（WARL），SPEC 未定义 mode 非 0 时 cm.jt/cm.jalt 的行为（"also reserved"），本计划不对该场景做行为断言。
- 平台是否支持 Zcmt 以 `config/<platform>/rvtest_config.h` 中是否定义 `ZCMT_SUPPORTED` 为准：未定义则判定为不支持，每个用例通过 `CHECK_PLATFORM_SUPPORT_ZCMT()` 自行 TEST_SKIP。当前 qemu-rv64-max、spike-rv64-max 已声明。
- SPEC 中"建议第二次取指忽略硬件触发器与断点"为推荐性描述（recommend），且依赖 Sdtrig 调试设施，不设用例。
- "cm.jalt 等价于 jal ra，若实现返回地址栈则应 push"属微架构实现提示，软件不可观测，不设用例。
- "jvt 增加架构状态，上下文切换时必须保存/恢复"是对系统软件的要求而非硬件行为，不设用例。

### 由其他测试计划覆盖
- `jvt` CSR 的 state enable 门控详细行为（mstateen0/hstateen0/sstateen0 的 JVT 位，`norm:stateen0_jvt_op`） → `Smstateen_test_plan.md` / `Ssstateen_test_plan.md`（本计划 Group 7 仅做轻量交叉验证）
- fence.i 指令本身的内存序与同步语义 → `zifencei_test_plan.md`
- illegal-instruction / access-fault / page-fault 的 trap 递送、委托与 CSR 写入通用行为 → `Sm_Exceptions_test_plan.md` / `Ss_Exceptions_test_plan.md` / `Hypervisor_Exceptions_test_plan.md`
- PMP 配置项自身行为 → pmp 相关测试计划；页表翻译机制 → Sv39/Sv48/Sv57 相关测试计划
- Zihpm 硬件性能计数器 → `zihpm_test_plan.md`

---

## 覆盖的规范点

本章节列出本文档所有测试组中引用的规范点（norm ID），已去重并按字母顺序排列。

| Norm ID | 原文 | 中文说明 |
|---------|------|----------|
| `norm:cm-jalt_op` | cm.jalt reads an entry from the jump vector table in memory and jumps to the address that was read, linking to _ra_. | cm.jalt 从内存中的跳转向量表读取一个表项并跳转到读到的地址，同时将返回地址链接到 ra。 |
| `norm:cm-jt_op` | cm.jt reads an entry from the jump vector table in memory and jumps to the address that was read. | cm.jt 从内存中的跳转向量表读取一个表项并跳转到读到的地址。 |
| `norm:jvt_base_vm` | jvt[base] is a virtual address, whenever virtual memory is enabled. | 虚拟内存启用时，jvt.base 是虚拟地址。 |
| `norm:jvt_mode_acc` | jvt[mode] is a WARL field, so can only be programmed to modes which are implemented. ... Jump table mode _must_ be implemented. | jvt.mode 是 WARL 字段，仅可编程到已实现的模式；mode 0（Jump table mode）必须实现；发现机制为写不同值后读回。 |
| `norm:jvt_op` | If Zcmt is implemented then jvt must also be implemented, but can contain a read-only value. ... The value in the BASE field must always be aligned on a 64-byte boundary. ... the lower six bits of _base_ are filled with zeroes to obtain an XLEN-bit jump-table base address that is always aligned on a 64-byte boundary. | 实现 Zcmt 必须实现 jvt（允许只读值）；BASE 字段恒按 64 字节对齐，计算表访问时低 6 位以零填充。 |
| `norm:jvt_reg` | The jvt register is an XLEN-bit WARL read/write register that holds the jump table configuration, consisting of the jump table base address (BASE) and the jump table mode (MODE). | jvt 是 XLEN 位 WARL 读写寄存器，保存跳转表基址（BASE）与模式（MODE）。 |
| `norm:Zcmt_endian` | Table entries follow the current data endianness. This is different from normal instruction fetch which is always little-endian. | 表项遵循当前数据字节序，与始终小端的普通取指不同。 |
| `norm:Zcmt_entry_sz` | The base of the table is in the jvt CSR, each table entry is XLEN bits. | 表基址在 jvt CSR 中，每个表项为 XLEN 位。 |
| `norm:Zcmt_fetch` | ... the execution of a table jump instruction involves two instruction fetches, the first to read the instruction (cm.jt/cm.jalt) and the second to read from the jump vector table (JVT). Both instruction fetches are _implicit_ reads, and both require execute permission; read permission is irrelevant. | 表跳转涉及两次指令取指：第一次取指令本身，第二次取 JVT 表项；两次均为隐式读且都要求执行权限，读权限无关。 |
| `norm:Zcmt_table` | Table jump uses a 256-entry XLEN wide table in instruction memory to contain function addresses. The table must be a minimum of 64-byte aligned. | 表跳转使用位于指令内存中的 256 项 XLEN 宽跳转表，表至少 64 字节对齐。 |
| `norm:Zcmt_trap` | If an exception occurs on either instruction fetch, xEPC is set to the PC of the table jump instruction, xCAUSE is set as expected for the type of fault and xTVAL (if not set to zero) contains the fetch address which caused the fault. | 任一次取指发生异常时，xEPC 设为表跳转指令的 PC，xCAUSE 按故障类型设置，xTVAL（若实现写非零）为引发故障的取指地址。 |

**非 norm 标记的规范性描述**（编码表与正文中的可测要求）：
- `jvt` CSR 地址 0x017，权限 URW；RV64 格式为 mode[5:0] + base[XLEN-1:6]（RV32 为 mode[5:0] + base[31:6]）。
- cm.jt 与 cm.jalt 共享编码空间：quadrant=C2（bits[1:0]=10）、bits[12:10]=000、FUNCT3=101（bits[15:13]）、bits[9:2]=index；index<32 解码为 cm.jt，index>=32 解码为 cm.jalt。
- 伪代码语义：RV64 下 table_address = jvt.base + (index<<3)（RV32 为 index<<2）；跳转目标 pc = entry & ~0x1（清 bit 0）；cm.jalt 额外执行 ra = pc+2（pc 为 cm.jalt 指令地址）。
- 对跳转表的内存写入须执行 fence.i 后才能保证对取指可见；若表内容自上次 fence.i 后未被更新，切换 jvt 指向不同表无需指令屏障。
- 若实现 Smstateen，jvt CSR 的访问需要 state enable（对应 `norm:stateen0_jvt_op`，属 smstateen 章节）。
- mode 为保留值时 cm.jt/cm.jalt 行为亦为保留（NOTE 描述），不做行为断言。

---

## Group 1. jvt CSR 基本属性与 WARL 行为

**规范依据**：
- `norm:jvt_reg`：jvt 为 XLEN 位 WARL 读写寄存器，由 BASE 与 MODE 组成
- `norm:jvt_op`：实现 Zcmt 必须实现 jvt（可只读）；BASE 恒 64 字节对齐，低 6 位以零填充
- CSR 定义：地址 0x017、权限 URW、RV64 字段布局 mode[5:0] + base[XLEN-1:6]

**测试职责**：验证 jvt 的存在性、读写能力、字段布局与 WARL 约束（64 字节对齐、低 6 位零填充、只读实现分支）。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZJVT-01 | jvt 存在性与可读性探测 | trap-armed 读取 CSR 0x017（平台声明 ZCMT_SUPPORTED 前提下） | 访问不触发异常；实现 Zcmt 而 jvt 访问触发异常属实现缺陷（`norm:jvt_op`） |
| ZJVT-02 | jvt 基本读写 | 写 mode=0、base=64 字节对齐地址（测试表），读回验证 | 读回值与写入一致（可写实现） |
| ZJVT-03 | base 低 6 位零填充 | 写 base 低 6 位非零的地址（如 table_base\|0x3F），读回 | 读回值低 6 位为 0（CSR 仅保存 bits XLEN-1:6） |
| ZJVT-04 | base 恒 64 字节对齐 | 多组写入后读回，检查 (读回值 & 0x3F)==0 | 任意读回值均满足 64 字节对齐（`norm:jvt_op`） |
| ZJVT-05 | jvt 可写性发现 | 写特定值并读回，区分可写实现与只读实现 | 探测结果记录为后续用例分支依据；只读实现合法（`norm:jvt_op`），功能用例按不适用处理 |
| ZJVT-06 | WARL 值域探测 | 依次写多个候选 base（不同对齐、不同区域），读回 | 每次读回为写入值或某个确定合法值，写入过程不触发异常 |
| ZJVT-07 | base 高位边界 WARL | 写 base 高位模式值（如高位全 1），读回 | 读回符合 WARL 约束（实现可截断高位），不触发异常 |
| ZJVT-08 | csrs/csrc 原子位操作 | 用 csrs/csrc 对 jvt 的 base 域置位/清位，读回验证 | 置位/清位按原子读-改-写生效，mode/base 字段结果符合写入掩码 |

---

## Group 2. jvt.mode 字段 WARL 与发现机制

**规范依据**：
- `norm:jvt_mode_acc`：mode 为 WARL 字段，仅可编程到已实现模式；mode 0 必须实现；发现机制为写后读回
- mode 定义表：000000 = Jump table mode，其余保留

**测试职责**：验证 mode 字段的 WARL 约束与 SPEC 规定的发现机制。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZJVT-09 | mode=0 必须保持 | 写 jvt.mode=0（Jump table mode），读回 | 读回 mode=0（mode 0 必须实现，`norm:jvt_mode_acc`） |
| ZJVT-10 | mode WARL 发现扫描 | 依次写 mode=1..0x3F（保留值），逐一读回 | 每次读回为某个已实现模式（当前 SPEC 下预期回落到 0），不保留保留编码，写入不触发异常 |
| ZJVT-11 | mode 与 base 字段写独立性 | 固定 base 仅改 mode、固定 mode 仅改 base，分别读回 | 字段间写入互不污染（受 WARL 约束的最终值除外） |

---

## Group 3. cm.jt 指令编码与操作语义

**规范依据**：
- `norm:cm-jt_op`：cm.jt 读取 JVT 表项并跳转到读到的地址
- `norm:Zcmt_table` / `norm:Zcmt_entry_sz`：256 项 XLEN 宽表，表项 XLEN 位
- 编码规范：quadrant=C2、bits[12:10]=000、FUNCT3=101、bits[9:2]=index，index<32 解码为 cm.jt
- 伪代码：RV64 table_address = jvt.base + (index<<3)；pc = entry & ~0x1

**测试职责**：验证 cm.jt 的机器码编码、表地址计算、跳转目标取值（bit0 清除）与边界 index 行为。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZCMT-01 | cm.jt 指令字编码验证 | 取 cm.jt 的 16 位机器码（汇编器生成或手工构造），逐字段比对 | bits[1:0]=10(C2)，bits[12:10]=000，bits[15:13]=101，bits[9:2]=index |
| ZCMT-02 | 助记符编码与手工构造一致 | 比对汇编器对 `cm.jt N` 生成的编码与按编码表手工构造的 .half 值 | 两种形式编码完全一致 |
| ZCMT-03 | 基本跳转功能 | 表项 index=k 写入已知目标地址（目标处写标记后返回），jvt 指向该表，执行 cm.jt k | 控制流转移到目标地址，标记被写入 |
| ZCMT-04 | 表地址计算 offset=index*8（RV64） | index 0/1/7/31 的表项各写不同目标，依次执行 cm.jt | 每次跳转到 base+index*8 处表项所含地址（验证偏移计算） |
| ZCMT-05 | 表项 bit0 清除 | 表项写 target\|0x1，执行 cm.jt | 跳转到 target & ~0x1（pc = entry & ~0x1） |
| ZCMT-06 | cm.jt 边界 index | cm.jt 0 与 cm.jt 31（index 上界） | 均按各自表项正确跳转 |
| ZCMT-07 | index>=32 解码为 cm.jalt | 手工构造 index=32 的共享编码并执行 | 按 cm.jalt 语义执行（ra 被链接），确认编码空间按 index 划分 |
| ZCMT-08 | 全 XLEN 宽度目标地址 | 表项写含高地址位的目标（验证 64 位地址不被截断），执行 cm.jt | 跳转到完整 64 位目标地址 |
| ZCMT-09 | cm.jt 不写 ra | 执行前记录 ra，执行 cm.jt 到达目标后检查 | ra 保持不变（cm.jt 无链接语义） |
| ZCMT-10 | 不修改其他架构状态 | 跳转前将通用寄存器置已知模式值，到达目标后逐一比对，并抽查 mstatus/mcause 等 CSR | 除 pc 外所有寄存器与 CSR 不变 |

---

## Group 4. cm.jalt 操作语义

**规范依据**：
- `norm:cm-jalt_op`：cm.jalt 读取 JVT 表项并跳转，同时链接 ra
- 编码规范：与 cm.jt 共享编码空间，index>=32 解码为 cm.jalt
- 伪代码：ra = pc+2（pc 为 cm.jalt 指令地址）；RV64 table_address = jvt.base + (index<<3)；pc = entry & ~0x1

**测试职责**：验证 cm.jalt 的链接值精确性、返回路径与边界 index 行为。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZCMT-11 | cm.jalt 编码验证与助记符一致 | 对 index=32/100/255 比对汇编器编码与手工构造值，逐字段核对 | bits[1:0]=10，bits[12:10]=000，bits[15:13]=101，bits[9:2]=index |
| ZCMT-12 | 链接跳转基本功能 | 表项 index=32 写目标地址，执行 cm.jalt 32 | 跳转到表项目标，且 ra = cm.jalt 指令地址 + 2 |
| ZCMT-13 | 返回路径验证 | 目标函数以 `ret`（jalr x0, ra）返回 | 控制流回到 cm.jalt 的下一条指令并继续执行（写标记验证） |
| ZCMT-14 | cm.jalt 边界 index | cm.jalt 32（index 下界）与 cm.jalt 255（表项上界），表地址分别为 base+32*8 与 base+255*8 | 均按各自表项正确跳转并链接 |
| ZCMT-15 | 表项 bit0 清除且 ra 正确 | 表项写 target\|0x1，执行 cm.jalt | 跳转到 target & ~0x1，ra = cm.jalt 地址 + 2 |
| ZCMT-16 | 连续 cm.jalt 链接值更新 | 在不同位置连续执行两次 cm.jalt（不同表项） | 每次 ra 均为当条 cm.jalt 地址 + 2，后一次覆盖前一次 |

---

## Group 5. 表跳转异常处理

**规范依据**：
- `norm:Zcmt_fetch`：两次取指均为隐式读，均要求执行权限；读权限无关
- `norm:Zcmt_trap`：任一次取指异常时 xEPC=表跳转指令 PC、xCAUSE 按故障类型、xTVAL=故障取指地址

**测试职责**：验证第一次/第二次取指故障下的 cause、epc、tval 规则，执行权限要求与恢复重试语义（本组以 M/S-mode + PMP 物理路径为主，虚拟内存路径见 Group 6）。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZCMT-17 | 第二次取指无执行权限（PMP） | jvt.base 指向 PMP 无 X 权限区域，执行 cm.jt | instruction access fault (cause=1)，mepc=cm.jt 指令 PC（非表地址） |
| ZCMT-18 | 读权限无关（X-only 区域） | jvt.base 指向 PMP 仅 X 无 R 权限区域，执行 cm.jt | 跳转正常完成，不触发异常（`norm:Zcmt_fetch`：read permission is irrelevant） |
| ZCMT-19 | 第二次取指故障 xEPC 规则 | S-mode（satp=0）下 jvt 指向无权限区域，执行 cm.jt/cm.jalt | sepc=表跳转指令 PC（第二次取指故障不回表地址），spc/mepc 规则与 M-mode 一致 |
| ZCMT-20 | 第二次取指故障 xTVAL 规则 | 同 ZCMT-17/19 场景，检查 mtval/stval | xTVAL = 故障表项取指地址 jvt.base + index*8（实现写非零时），或实现规定的零值 |
| ZCMT-21 | 第一次取指故障 | 将 cm.jt 指令本身置于无执行权限区域（PMP），执行 | cause 按故障类型（access fault=1），xEPC=cm.jt PC，xTVAL=cm.jt PC（与普通取指故障一致） |
| ZCMT-22 | 故障恢复与重试 | 第二次取指故障后，handler 中修复权限/映射，xret 返回 | cm.jt 重新执行并成功跳转（xEPC 指向表跳转指令保证可恢复） |
| ZCMT-23 | 有效/无效表项混合 | 同一表中一个表项有效、另一个表项位于无权限区域，分别执行对应 cm.jt | 有效表项跳转成功；无效表项触发故障，故障仅发生在对应指令执行时 |

---

## Group 6. 虚拟内存下的表跳转

**规范依据**：
- `norm:jvt_base_vm`：虚拟内存启用时 jvt.base 为虚拟地址
- `norm:Zcmt_fetch` / `norm:Zcmt_trap`：第二次取指同样经过翻译并遵循取指异常规则

**测试职责**：验证 satp 启用时表项取指经页表翻译，X 位/映射缺失触发正确的 page fault，以及 H 扩展下 VS-mode 的两阶段翻译行为。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZCMT-24 | S-mode 翻译路径正常跳转 | S-mode 启用 satp，表所在页映射为 X=1，执行 cm.jt | 跳转成功（jvt.base 作为虚拟地址经页表翻译） |
| ZCMT-25 | 表页 X=0 触发指令页故障 | 表所在页映射但 X=0，执行 cm.jt | instruction page fault (cause=12)，sepc=cm.jt PC，stval=表项虚拟地址 |
| ZCMT-26 | 表页未映射触发指令页故障 | jvt.base 指向未映射虚拟地址，执行 cm.jt | instruction page fault (cause=12)，stval=表项取指虚拟地址 |
| ZCMT-27 | VS-mode 翻译路径正常跳转（H 扩展） | VS-mode 启用 vsatp，VS-stage 映射表页 X=1，执行 cm.jt | 跳转成功（VS-stage 翻译生效） |
| ZCMT-28 | VS-mode 第二次取指 G-stage 故障（H 扩展） | G-stage 表页映射无效，VS-mode 执行 cm.jt | guest instruction page fault (cause=20) 递送至 HS-mode，hstatus.GVA=1，htval=故障表项 GPA>>2（norm:htval_trapval 允许为零） |

---

## Group 7. 各特权级执行与 jvt 访问控制

**规范依据**：
- jvt 权限 URW：所有特权级（含 U/VU）可直接访问 jvt CSR
- `norm:cm-jt_op` / `norm:cm-jalt_op`：表跳转为普通指令，无特权级限制
- SPEC：若实现 Smstateen，jvt CSR 需要 state enable（详细门控行为归 Smstateen/Ssstateen 测试计划）

**测试职责**：验证表跳转指令与 jvt CSR 在各特权级的可用性，以及 stateen 门控的边界（门控 CSR 访问、不门控指令执行）。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZACC-01 | M/S/U-mode 访问 jvt | 三种特权级分别 csrr/csrw jvt（stateen 未实现或已使能） | 访问正常，无异常（URW 权限） |
| ZACC-02 | M/S/U-mode 执行表跳转 | 各特权级在各自翻译上下文执行 cm.jt 与 cm.jalt | 均正常跳转与链接，无异常 |
| ZACC-03 | HS/VS/VU-mode 执行表跳转（H 扩展） | 实现 H 扩展时，HS/VS/VU 三种模式各执行 cm.jt 与 cm.jalt | 均正常执行，无 virtual-instruction exception |
| ZACC-04 | VS/VU-mode 访问 jvt（H 扩展） | stateen 使能状态下，VS/VU-mode csrr/csrw jvt | 访问正常（URW 权限 + stateen 使能） |
| ZACC-05 | stateen 门控 jvt 访问（轻量交叉验证） | 实现 Smstateen 时清零 mstateen0.JVT，S-mode 访问 jvt；再按层级清零 hstateen0/sstateen0 JVT 位，VS/VU 访问 jvt | S-mode 触发 illegal-instruction；VS/VU 触发 virtual-instruction/illegal-instruction（详细用例见 `Smstateen_test_plan.md` / `Ssstateen_test_plan.md`） |
| ZACC-06 | stateen 不门控表跳转指令执行 | 实现 Smstateen 时清零各级 stateen 的 JVT 位，执行 cm.jt/cm.jalt | 指令正常执行（state enable 仅门控 jvt CSR 访问，不门控指令本身） |

---

## Group 8. 表更新可见性与字节序

**规范依据**：
- SPEC：对 JVT 的内存写入须 fence.i 后对取指可见；表未更新时切换 jvt 无需指令屏障
- `norm:Zcmt_endian`：表项遵循当前数据字节序

**测试职责**：验证表项更新后的可见性规则、jvt 切换语义与表项字节序。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZVIS-01 | 表项更新经 fence.i 可见 | 表项先指向目标 A，cm.jt 验证跳到 A；改写表项为目标 B，执行 fence.i 后再执行 cm.jt | 跳转到新目标 B（fence.i 保证写入对取指可见） |
| ZVIS-02 | jvt 切换无需指令屏障 | 准备内容不同的两张表 A/B（写完后已 fence.i），cm.jt 在 jvt=A 时跳到 A 表目标；切换 jvt=B（不执行 fence.i）再执行同一 index 的 cm.jt | 跳转到 B 表目标（表内容未更新，切换 jvt 无需屏障） |
| ZVIS-03 | 表项字节序与数据字节序一致（小端默认） | 以字节写（sb）方式按小端序构造表项，比较 ld 读回值与 cm.jt 实际跳转目标 | 跳转目标与按当前数据字节序（小端）解释的值一致 |
| ZVIS-04 | 大端模式表项字节序（探测性） | 仅当平台支持可切换数据字节序（如 mstatus.UBE 可写）时，在大端模式构造表项并执行 cm.jt | 表项按大端字节序解释（`norm:Zcmt_endian`）；不支持字节序切换的平台不适用（非 FAIL） |

---

## 测试实现说明

1. **指令构造**：若工具链（`riscv64-unknown-linux-elf-gcc` 的 `-march` 含 `zcmt`）支持 `cm.jt`/`cm.jalt` 助记符则直接使用；否则按编码表以 `.half` 手工构造 16 位指令字（quadrant=C2、FUNCT3=101、bits[12:10]=000、bits[9:2]=index），两种形式的一致性由 ZCMT-02/ZCMT-11 验证。
2. **跳转表布置**：JVT 置于测试数据段 64 字节对齐区域（256 项 × 8 字节 = 2KB，RV64），以普通 store 写入表项后执行 fence.i；异常场景用 PMP（M/S-mode）或页表 X 位/映射（S/VS-mode）构造。
3. **扩展探测**：用例以 `config/*/rvtest_config.h` 中的 `ZCMT_SUPPORTED` 宏为执行前提（当前各平台均未声明，启用 Zcmt 的平台按惯例补充）；jvt CSR 存在性探测（ZJVT-01）必须 trap-armed。
4. **只读 jvt 分支**：`norm:jvt_op` 允许 jvt 为只读值；ZJVT-05 探测写能力，只读实现下无法构造跳转表的功能用例（Group 3/4/6/8）标记为不适用，不判 FAIL。
5. **保留 mode 不设行为用例**：mode 非 0 时 cm.jt/cm.jalt 行为 SPEC 标注为 reserved，仅做 WARL 写回探测（ZJVT-10），不执行表跳转行为断言。
6. **平台差异处置**：若任一平台（QEMU/Spike/Sail/whisper/硬件）出现跳转目标错误、链接值错误、第二次取指权限检查缺失、xEPC/xTVAL 不符合 `norm:Zcmt_trap`、或 X-only 区域误报读故障等，均属违反 SPEC 的实现缺陷，用例保持 FAIL 并记录至 `bugs/` 目录，不做跳过或 workaround。

---

## 附录 A：规范点覆盖矩阵

| Norm ID | 覆盖用例 | 备注 |
|---------|----------|------|
| `norm:Zcmt_table` | ZCMT-04, ZCMT-06, ZCMT-14 | 256 项表与 index 边界由 0/31/32/255 用例覆盖；64 字节对齐由 ZJVT-04 保证 |
| `norm:Zcmt_entry_sz` | ZCMT-04, ZCMT-08 | RV64 表项 XLEN=64 位；RV32 平台复用时表项 32 位 |
| `norm:Zcmt_endian` | ZVIS-03, ZVIS-04 | 大端用例为探测性，依赖平台字节序切换能力 |
| `norm:Zcmt_fetch` | ZCMT-17, ZCMT-18, ZCMT-19, ZCMT-24 ~ ZCMT-28 | 硬件触发器忽略建议（recommend）依赖 Sdtrig，不设用例 |
| `norm:Zcmt_trap` | ZCMT-17, ZCMT-19 ~ ZCMT-23, ZCMT-25, ZCMT-26, ZCMT-28 | 两次取指的 xEPC/xCAUSE/xTVAL 规则 |
| `norm:jvt_reg` | ZJVT-01 ~ ZJVT-08 | |
| `norm:jvt_op` | ZJVT-01, ZJVT-03 ~ ZJVT-07 | 只读 jvt 实现由 ZJVT-05 分支处理 |
| `norm:jvt_base_vm` | ZCMT-24 ~ ZCMT-28 | |
| `norm:jvt_mode_acc` | ZJVT-09 ~ ZJVT-11 | 保留 mode 下的指令行为 SPEC 标注 reserved，不设行为用例 |
| `norm:cm-jt_op` | ZCMT-03 ~ ZCMT-10, ZCMT-17 ~ ZCMT-27 | |
| `norm:cm-jalt_op` | ZCMT-07, ZCMT-11 ~ ZCMT-16 | 返回地址栈 push 为微架构提示，不设用例 |
| 编码空间划分（index<32 → cm.jt，index>=32 → cm.jalt） | ZCMT-01, ZCMT-02, ZCMT-07, ZCMT-11 | 非 norm 锚点规范 |
| 伪代码语义（offset=index<<3、pc=entry&~1、ra=pc+2） | ZCMT-04, ZCMT-05, ZCMT-12, ZCMT-15 | 非 norm 锚点规范 |
| fence.i 表更新可见性 | ZVIS-01 | 非 norm 锚点规范；fence.i 指令自身语义归 `zifencei_test_plan.md` |
| jvt 切换无需指令屏障（表未更新时） | ZVIS-02 | 非 norm 锚点规范 |
| jvt state enable（`norm:stateen0_jvt_op`） | ZACC-05, ZACC-06（轻量） | 完整门控用例归 `Smstateen_test_plan.md` / `Ssstateen_test_plan.md` |
| 依赖/冲突（依赖 Zca+Zicsr、与 Zcd 冲突） | — | 配置声明层面的环境前提，不设用例 |
| jvt 上下文保存/恢复（架构状态说明） | — | 系统软件职责，非硬件可测行为 |

---

## 平台验证记录

| 平台 | 结果 | 说明 |
|------|------|------|
| Spike 1.1.1-dev（--isa=rv64imach_zicsr_zifencei_zcmt） | 45 PASS / 1 FAIL / 3 SKIP | FAIL：ZJVT-10（Spike 保留 jvt.mode 保留值，违反 norm:jvt_mode_acc，见 `bugs/spike_zcmt_jvt_mode_warl_bug.md`）；SKIP：ZACC-05/06（未实现 Smstateen）、ZVIS-04（UBE 不可写，仅小端） |
| QEMU 8.2.94 cskysim（-cpu max） | 2 FAIL / 47 SKIP | `-cpu max` 声明 Zcmt，但套件内合法 jvt 访问被误判非法指令（fork 缺陷），ZJVT-01/05 如实 FAIL，见 `bugs/qemu_zcmt_jvt_access_bug.md` |
| Sail（sail_riscv_sim） | 整体 SKIP | 未实现 Zcmt（配置未声明 ZCMT_SUPPORTED）；且 ENABLE_HYP 套件在 Sail 上的已知问题见 `bugs/sail_hyp_trap_loop_bug.md` |

实现备注：

- 套件目录 `Zcmt/`，平台支持判定采用用例级门控：`rvtest_config.h` 未定义 `ZCMT_SUPPORTED` 时逐用例 SKIP（当前 qemu-rv64-max、spike-rv64-max 声明）。
- jvt 访问统一收敛为单一出口函数（`zcmt_jvt_read/write`），以降低 QEMU fork 缺陷的触发面；jvt 只读实现（norm:jvt_op 允许）由 ZJVT-05 探测分支，功能用例标记 SKIP。
- Group 5 的故障注入经 PMP（entry1-3 布局，与 Zihintntl 同款）在 S-mode 执行；首次取指故障用例经 `exec_at()` 恢复路径处理。
- Group 6 的 S/VS 阶段用例依赖 ENABLE_VM/ENABLE_HYP 框架对象，故 test_vm.c 整体以 ENABLE_HYP 编译门控。
