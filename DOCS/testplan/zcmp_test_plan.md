# Zcmp 扩展测试计划（Compressed Prologues and Epilogues v1.0）

## 概述

本测试计划覆盖 RISC-V Zcmp（Compressed Prologues and Epilogues）扩展，验证 6 条 16 位压缩指令的指令编码、功能语义与 trap 重执行行为：

- `cm.push`：创建栈帧，将 ra 与 0~12 个 s 寄存器存储到栈中，sp 减去 stack_adj
- `cm.pop`：从栈帧恢复寄存器，sp 加上 stack_adj
- `cm.popret`：等价 cm.pop 后返回 ra
- `cm.popretz`：等价 cm.pop 并将 a0 清零后返回 ra
- `cm.mvsa01`：将 a0/a1 原子地移动到两个不同的 s0-s7 寄存器
- `cm.mva01s`：将两个 s0-s7 寄存器原子地移动到 a0/a1

本测试计划依据 `SPEC/riscv-isa-manual/src/unpriv/zcmp.adoc` 中的规范点（norm 标记）编写。

### 本文档覆盖的 SPEC 章节
- Zcmp Extension for Compressed Prologues and Epilogues（push/pop 功能概述、寄存器列表处理、栈指针调整处理）
- Fault handling（norm:Zcmp_reexecute / norm:Zcmp_trap / norm:Zcmp_push_sp_commit / norm:Zcmp_pop_sp_commit / norm:Zcmp_bus_fault）
- Software view of execution（push/pop/popret 序列的软件可见行为）
- cm.push / cm.pop / cm.popretz / cm.popret / cm.mvsa01 / cm.mva01s 指令定义（编码、汇编语法、rlist 映射、stack_adj_base 表、Operation 伪代码）

### 范围说明
- Zcmp 属于 Zc 系列压缩扩展，6 条指令均为 16 位压缩编码，SPEC 上依赖 Zca。平台是否支持 Zcmp 以 `config/<platform>/rvtest_config.h` 中是否定义 `ZCMP_SUPPORTED` 为准（声明 Zcmp 即隐含 Zca 支持，不再单独检查 Zca）：未定义则判定为不支持，每个用例通过 `CHECK_PLATFORM_SUPPORT_ZCMP()` 自行 TEST_SKIP。
- rlist 字段取值 0~3 保留（future EABI variant cm.push.e/cm.pop.e 等），执行应触发 illegal-instruction exception（保留编码规则）。
- 寄存器列表不支持 `{ra, s0-s10}` 组合（无对应 rlist 编码），需要包含 s10 时必须使用 rlist=15 的 `{ra, s0-s11}`，此为编码空间约束，通过 rlist 遍历用例间接验证。
- RV32E 变体（rlist 仅 4~6）不适用于本框架测试平台（RV64/RV32I），不设用例；RV32I 与 RV64 的 stack_adj_base 表差异由用例按 XLEN 参数化覆盖。
- `norm:interrupts_allowed_in_pushpop`（序列执行期间能否接收中断）与 `norm:Zcmp_bus_fault`（总线非精确 fault 处理时机）分别为 implementation-defined 与 platform-defined，不设可判定用例。

### 由其他测试计划覆盖
- illegal-instruction / load-store access fault 的 trap 递送、委托与 CSR 写入行为 → `Sm_Exceptions_test_plan.md` / `Ss_Exceptions_test_plan.md` / `Hypervisor_Exceptions_test_plan.md`
- PMP 访问权限配置（本计划 trap 用例的 fault 注入手段） → `pmp_test_plan.md`
- Zca 基础压缩指令编码规则与保留编码行为 → 基础压缩指令相关测试
- Zcmop / Zimop may-be-operations 扩展 → `zcmop_test_plan.md` / `zimop_test_plan.md`

---

## 覆盖的规范点

本章节列出本文档所有测试组中引用的规范点（norm ID），已去重并按字母顺序排列。

| Norm ID | 原文 | 中文说明 |
|---------|------|----------|
| `norm:Zcmp_reexecute` | Correct execution requires that sp refers to idempotent memory, because the core must be able to handle traps detected during the sequence. The entire push/pop sequence is re-executed after returning from the trap handler, and multiple traps are possible during the sequence. | 正确执行要求 sp 指向幂等内存，因为核必须能处理序列执行期间检测到的 trap。从 trap handler 返回后整个 push/pop 序列被重新执行，序列期间可能发生多次 trap。 |
| `norm:Zcmp_trap` | If a trap occurs during the sequence then xEPC is updated with the PC of the instruction, xTVAL (if not read-only-zero) updated with the bad address if it was an access fault and xCAUSE updated with the type of trap. | 序列执行期间发生 trap 时，xEPC 更新为该指令的 PC，xTVAL（若非只读零）在访问 fault 时更新为故障地址，xCAUSE 更新为 trap 类型。 |
| `norm:Zcmp_push_sp_commit` | The stack pointer adjustment must only be committed only when it is certain that the entire cm.push instruction will commit. | cm.push 的栈指针调整只有在能确定整条指令将提交时才提交。 |
| `norm:Zcmp_pop_sp_commit` | The optional li[a0,0], stack pointer adjustment and optional ret must only be committed only when it is certain that the entire cm.pop/cm.popret instruction will commit. | cm.pop/cm.popret 序列中可选的 li[a0,0]、栈指针调整与可选的 ret 只有在能确定整条指令将提交时才提交。 |
| `norm:Zcmp_bus_fault` | Stores may also return imprecise faults from the bus. It is platform defined whether the core implementation waits for the bus responses before continuing to the final stage of the sequence, or handles errors responses after completing the cm.push instruction. | store 可能从总线返回非精确 fault。核实现等待总线响应后再进入序列末段、还是在 cm.push 完成后处理错误响应，是平台定义的。（不设可判定用例） |
| `norm:interrupts_allowed_in_pushpop` | It is implementation defined whether interrupts can also be taken during the sequence execution. | 序列执行期间能否接收中断是实现定义的。（不设可判定用例） |
| `norm:cm-push_op` | This instruction pushes (stores) the registers in reg_list to the memory below the stack pointer, and then creates the stack frame by decrementing the stack pointer by stack_adj, including any additional stack space requested by the value of spimm. | cm.push 将 reg_list 中的寄存器存储到栈指针下方的内存，然后以 stack_adj（含 spimm 请求的额外空间）递减栈指针创建栈帧。 |
| `norm:cm-pop_op` | This instruction pops (loads) the registers in reg_list from stack memory, and then adjusts the stack pointer by stack_adj. | cm.pop 从栈内存加载 reg_list 中的寄存器，然后将栈指针增加 stack_adj。 |
| `norm:cm-popret_op` | This instruction pops (loads) the registers in reg_list from stack memory, adjusts the stack pointer by stack_adj and then returns to ra. | cm.popret 从栈内存加载 reg_list 中的寄存器，将栈指针增加 stack_adj，然后返回 ra。 |
| `norm:cm-popretz_op` | This instruction pops (loads) the registers in reg_list from stack memory, adjusts the stack pointer by stack_adj, moves zero into a0 and then returns to ra. | cm.popretz 从栈内存加载 reg_list 中的寄存器，将栈指针增加 stack_adj，将 a0 清零，然后返回 ra。 |
| `norm:cm-mvsa01_op` | This instruction moves a0 into r1s' and a1 into r2s'. r1s' and r2s' must be different. The execution is atomic, so it is not possible to observe state where only one of r1s' or r2s' has been updated. | cm.mvsa01 将 a0 移入 r1s'、a1 移入 r2s'，两者必须不同。执行是原子的，不可观察到仅一个目的寄存器被更新的中间状态。 |
| `norm:cm-mvsa01_res` | For the encoding to be legal r1s' != r2s'. | r1s' != r2s' 是合法编码的必要条件。 |
| `norm:cm-mvsa01_sreg` | The encoding uses sreg number specifiers instead of xreg number specifiers to save encoding space. The mapping between them is specified in the pseudocode below. | 编码使用 sreg 编号而非 xreg 编号以节省编码空间，映射关系由伪代码给出（sreg 0..7 → x8,x9,x18..x23）。 |
| `norm:cm-mva01s_op` | This instruction moves r1s' into a0 and r2s' into a1. The execution is atomic, so it is not possible to observe state where only one of a0 or a1 have been updated. | cm.mva01s 将 r1s' 移入 a0、r2s' 移入 a1。执行是原子的，不可观察到仅 a0 或 a1 之一被更新的中间状态。 |
| `norm:cm-mva01s_sreg` | The encoding uses sreg number specifiers instead of xreg number specifiers to save encoding space. The mapping between them is specified in the pseudocode below. | 编码使用 sreg 编号而非 xreg 编号，映射关系由伪代码给出。 |

**非 norm 标记的规范性描述与 SPEC NOTE 中的可测属性**：
- 编码结构：6 条指令均位于 quadrant 2（bits[1:0]=0b10），funct3=0b101（bits[15:13]）；bits[12:8] 分别为 0x18（cm.push）/0x1a（cm.pop）/0x1c（cm.popretz）/0x1e（cm.popret）；cm.mvsa01/cm.mva01s 的 bits[12:10]=0b011、bits[6:5]=0b01/0b11。
- rlist 取值 0~3 保留（SPEC NOTE），对应编码执行应触发 illegal-instruction exception（保留编码基础规则）。
- 寄存器列表映射（RV32I/RV64）：rlist=4..14 对应 `{ra}`、`{ra,s0}`、…、`{ra,s0-s9}`；rlist=15 对应 `{ra,s0-s11}`（不存在 `{ra,s0-s10}` 编码）。
- stack_adj = stack_adj_base + spimm*16；RV64 stack_adj_base 表：rlist 4-5→16，6-7→32，8-9→48，10-11→64，12-13→80，14→96，15→112；RV32I 表：rlist 4-7→16，8-11→32，12-14→48，15→64。
- push 存储顺序：从编号最高的寄存器开始，自 sp-bytes 向下逐个存储（RV64 bytes=8，RV32 bytes=4）；pop 加载地址自 sp+stack_adj-bytes 向下，寄存器顺序相同。
- pop 序列 trap 时已执行的部分 load 允许更新架构状态（重执行会覆盖），最终结果必须正确。
- popret 序列中栈指针调整一旦提交，ret 必须执行（software view 规范性描述）。
- cm.popretz 编码图中 bits[3:2] 字段标注为 spimm[5:4]，但文档规范性汇编语法与 Valid values 表均定义为 stack_adj = stack_adj_base + spimm*16，用例以后者为准（见实现说明 6）。

---

## Group 1. 指令编码验证

**规范依据**：
- 各指令 Encoding 章节：cm.push（bits[12:8]=0x18）、cm.pop（0x1a）、cm.popretz（0x1c）、cm.popret（0x1e）、cm.mvsa01/cm.mva01s（bits[12:10]=0b011，bits[6:5]=0b01/0b11）
- SPEC NOTE：rlist 取值 0~3 保留（cm.push.e/cm.pop.e/cm.popretz.e/cm.popret.e）
- `norm:cm-mvsa01_res`：r1s' != r2s' 为合法编码的必要条件
- 基础 ISA 规则：压缩指令保留编码执行触发 illegal-instruction exception

**测试职责**：验证 6 条指令的机器码字段与 SPEC 编码图完全一致，汇编助记符与手工构造编码一致，rlist 保留取值与 mvsa01 非法组合不被接受。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZCMPENC-01 | cm.push 指令字编码验证 | 取 cm.push 的 16 位机器码（汇编器生成或 `.half` 构造），逐字段比对 SPEC 编码图，rlist/spimm 抽样遍历 | bits[1:0]=0b10，bits[3:2]=spimm，bits[7:4]=rlist，bits[12:8]=0b11000，bits[15:13]=0b101；基础编码 0xB802 \| (rlist<<4) \| (spimm<<2) |
| ZCMPENC-02 | cm.pop 指令字编码验证 | 同上，比对 cm.pop 编码 | bits[12:8]=0b11010，基础编码 0xBA02 |
| ZCMPENC-03 | cm.popretz 指令字编码验证 | 同上，比对 cm.popretz 编码 | bits[12:8]=0b11100，基础编码 0xBC02 |
| ZCMPENC-04 | cm.popret 指令字编码验证 | 同上，比对 cm.popret 编码 | bits[12:8]=0b11110，基础编码 0xBE02 |
| ZCMPENC-05 | cm.mvsa01/cm.mva01s 指令字编码验证 | 比对两条 move 指令的 r1s'/r2s' 字段与固定位域 | mvsa01：bits[12:10]=0b011，bits[6:5]=0b01，bits[9:7]=r1s'，bits[4:2]=r2s'，基础编码 0xAC22；mva01s：bits[6:5]=0b11，基础编码 0xAC62 |
| ZCMPENC-06 | 助记符编码与手工构造一致 | 比对汇编器对 6 条指令助记符生成的编码与手工 `.half` 构造值 | 两种形式编码完全一致 |
| ZCMPENC-07 | rlist=0..3 保留编码触发异常 | trap-armed 执行 cm.push/cm.pop/cm.popretz/cm.popret 的 rlist=0..3 编码（抽样） | illegal-instruction exception (cause=2) |
| ZCMPENC-08 | mvsa01/mva01s r1s'=r2s' 非法编码 | trap-armed 执行 r1s'=r2s' 的 cm.mvsa01 与 cm.mva01s 编码（8 组抽样） | illegal-instruction exception (cause=2)（norm:cm-mvsa01_res） |

---

## Group 2. cm.push 功能语义

**规范依据**：
- `norm:cm-push_op`：将 reg_list 寄存器存储到 sp 下方内存，sp -= stack_adj（含 spimm 额外空间）
- Operation 伪代码：存储顺序 i in 27..18,9,8,1，addr 自 sp-bytes 递减；RV64 用 sd（8 字节），RV32 用 sw（4 字节）
- stack_adj = stack_adj_base + spimm*16，RV64/RV32I stack_adj_base 表

**测试职责**：验证 cm.push 的寄存器存储内容、内存布局（地址与顺序）、sp 调整量、rlist/spimm 全遍历，以及不影响列表外寄存器。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZCMPPUSH-01 | 最小列表 {ra} 基本行为 | rlist=4，stack_adj=16（spimm=0），ra 预置模式值，执行 cm.push | ra 存储于原 sp-8（RV64）/sp-4（RV32），sp 减少 16 |
| ZCMPPUSH-02 | spimm 额外栈空间遍历 | rlist=4，spimm=0..3（stack_adj=16/32/48/64）逐一执行 | 存储位置不变，sp 分别减少 16/32/48/64 |
| ZCMPPUSH-03 | 全列表 {ra,s0-s11} 内存布局 | rlist=15，13 个寄存器预置互异模式值，执行 cm.push（RV64 stack_adj_base=112） | s11@sp-8、s10@sp-16、…、s0@sp-96、ra@sp-104，sp 减少 112+spimm*16 |
| ZCMPPUSH-04 | rlist=14 不含 s10/s11 | rlist=14（{ra,s0-s9}），s10/s11 预置模式值，执行后检查 | s10/s11 保持原值，存储元素仅 11 个（RV64），sp 减少 96+spimm*16 |
| ZCMPPUSH-05 | rlist=4..15 全遍历 | 每个 rlist 预置列表内寄存器模式值，验证存储数量、地址与 sp 调整 | 各 rlist 的寄存器集合、内存布局与 stack_adj_base 均符合 SPEC 表 |
| ZCMPPUSH-06 | RV64 stack_adj_base 表验证 | rlist 分组边界值（4/5、6/7、8/9、10/11、12/13、14、15）spimm=0 执行 | stack_adj_base 依次为 16/32/48/64/80/96/112（RV32I 平台按对应表判定） |
| ZCMPPUSH-07 | 列表外寄存器不受影响 | 执行前后比对所有不在 reg_list 中的通用寄存器（含 a0-a7、t0-t6） | 全部保持原值 |
| ZCMPPUSH-08 | 存储值与预置模式一致 | 各寄存器预置 0xA5A5.../递增序列等模式值，执行后逐元素回读内存 | 内存内容与预置值逐一匹配，元素宽度 = XLEN/8 |

---

## Group 3. cm.pop 功能语义

**规范依据**：
- `norm:cm-pop_op`：从栈内存加载 reg_list 寄存器，sp += stack_adj
- Operation 伪代码：addr = sp+stack_adj-bytes，加载顺序与 push 存储顺序相同
- stack_adj 表同 cm.push

**测试职责**：验证 cm.pop 的寄存器加载来源地址、sp 调整量、与 cm.push 的往返对称性，以及不影响列表外寄存器。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZCMPPOP-01 | 最小列表 {ra} 基本行为 | 手工构造栈帧，rlist=4，stack_adj=16，执行 cm.pop | ra 自 sp+stack_adj-8（RV64）加载，sp 增加 16 |
| ZCMPPOP-02 | push/pop 往返对称 | cm.push {ra,s0-sN},-stack_adj 后执行同参数 cm.pop | 所有寄存器与 sp 完全恢复至 push 前状态 |
| ZCMPPOP-03 | 全列表 {ra,s0-s11} 加载地址 | rlist=15，手工按 SPEC 布局填充栈内存，执行 cm.pop | s11 自 sp+stack_adj-8 加载、依次向下，ra 自 sp+stack_adj-104 加载，全部值正确 |
| ZCMPPOP-04 | spimm 遍历 | rlist 固定，spimm=0..3 逐一执行 | sp 分别增加 base/base+16/base+32/base+48 |
| ZCMPPOP-05 | 列表外寄存器不受影响 | pop 前后比对不在 reg_list 中的通用寄存器 | 全部保持原值 |
| ZCMPPOP-06 | rlist=4..15 全遍历 | 每个 rlist 手工构造栈帧并 pop，验证加载数量、地址与 sp 调整 | 各 rlist 行为符合 SPEC 表 |

---

## Group 4. cm.popret / cm.popretz 功能语义

**规范依据**：
- `norm:cm-popret_op`：加载 reg_list、sp += stack_adj、返回 ra
- `norm:cm-popretz_op`：同上，并将 a0 清零
- software view：`li a0,0`、sp 调整与 ret 属于原子末段；popret 栈指针调整提交后 ret 必须执行

**测试职责**：验证 popret/popretz 的返回行为、a0 处理差异、寄存器恢复与完整函数调用流程。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZCMPRET-01 | cm.popret 返回 ra 并恢复 sp | ra 指向返回标记点，构造栈帧后执行 cm.popret | 控制流到达 ra 指向位置，sp 增加 stack_adj，列表寄存器恢复 |
| ZCMPRET-02 | cm.popretz 清零 a0 | a0 预置非零值，执行 cm.popretz | 返回后 a0=0，sp 与列表寄存器正确恢复 |
| ZCMPRET-03 | cm.popret 不修改 a0 | a0 预置非零模式值，执行 cm.popret | 返回后 a0 保持预置值 |
| ZCMPRET-04 | popret/popretz rlist 与 spimm 遍历 | rlist=4..15、spimm=0..3 抽样组合执行 | 寄存器恢复、sp 调整、返回行为均正确 |
| ZCMPRET-05 | 完整函数调用流程 | 被测函数以 cm.push 开场、cm.popretz 收尾，caller 验证返回值与栈平衡 | 返回值为 0，caller 的 sp 与 callee-saved 寄存器全部恢复 |
| ZCMPRET-06 | 原子末段不可分割 | cm.popretz 后在返回点检查 a0、sp、寄存器三者同时生效（与 Group 6 trap 用例互补） | 返回点观察到完整末段状态，无部分提交状态 |

---

## Group 5. cm.mvsa01 / cm.mva01s 功能语义

**规范依据**：
- `norm:cm-mvsa01_op`：a0→r1s'、a1→r2s'，原子执行
- `norm:cm-mva01s_op`：r1s'→a0、r2s'→a1，原子执行
- `norm:cm-mvsa01_sreg` / `norm:cm-mva01s_sreg`：sreg→xreg 映射（xreg = {rsc[2:1]>0, rsc[2:1]==0, rsc[2:0]}，即 sreg 0..7 → x8,x9,x18..x23）

**测试职责**：验证两条 move 指令的数据搬移方向、sreg 映射全遍历、原子性（双目的同时更新）、源寄存器不受影响。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZCMPMV-01 | cm.mvsa01 基本搬移 | a0/a1 预置互异模式值，执行 cm.mvsa01 s0,s1 | x8=a0 原值，x9=a1 原值 |
| ZCMPMV-02 | cm.mvsa01 sreg 映射全遍历 | sreg=0..7 作为目的逐一测试 | sreg 0→x8、1→x9、2→x18、3→x19、4→x20、5→x21、6→x22、7→x23，搬移值正确 |
| ZCMPMV-03 | cm.mva01s 基本搬移与映射遍历 | s 寄存器预置模式值，执行 cm.mva01s，r1s'/r2s' 全映射遍历 | a0=r1s' 原值，a1=r2s' 原值，映射同上 |
| ZCMPMV-04 | 双目的寄存器同时更新 | a0/a1（或 s 寄存器）预置互异值，执行后检查两个目的寄存器 | 两个目的寄存器均被更新为正确值（原子性：不存在仅一个更新的合法终态） |
| ZCMPMV-05 | 源寄存器不受影响（mvsa01） | 执行 cm.mvsa01 后回读 a0/a1 | a0/a1 保持原值 |
| ZCMPMV-06 | 源寄存器不受影响（mva01s） | 执行 cm.mva01s 后回读源 s 寄存器 | 源 s 寄存器保持原值 |
| ZCMPMV-07 | r1s'≠r2s' 合法组合遍历 | 56 组合法组合抽样执行（含 sreg 0/1 与 2..7 交叉） | 全部正常执行，结果正确 |

---

## Group 6. Trap 处理与指令重执行

**规范依据**：
- `norm:Zcmp_reexecute`：sp 必须指向幂等内存；trap 返回后整个序列重新执行；序列期间可能发生多次 trap
- `norm:Zcmp_trap`：trap 时 xEPC=指令 PC，access fault 时 xTVAL=故障地址，xCAUSE=trap 类型
- `norm:Zcmp_push_sp_commit`：push 的 sp 调整仅在整条指令确定提交时提交
- `norm:Zcmp_pop_sp_commit`：pop/popret 的 li[a0,0]/sp 调整/ret 仅在整条指令确定提交时提交
- software view：pop 序列 trap 时已执行的部分 load 允许更新架构状态，重执行后最终值正确

**测试职责**：通过 PMP/页表 fault 注入验证序列中途 trap 的 CSR 记录、sp/a0 提交时序、xRET 后整序列重执行完成，以及多次 trap 场景。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZCMPTRAP-01 | cm.push 中途 store fault 的 trap 记录 | PMP 将栈区设为不可写，执行 cm.push（多寄存器列表） | store access fault (cause=7)，xEPC=cm.push 的 PC，xTVAL=故障存储地址 |
| ZCMPTRAP-02 | push trap 时 sp 未提交 | 在 ZCMPTRAP-01 的 handler 中读取 sp | sp 保持执行前的原值（norm:Zcmp_push_sp_commit） |
| ZCMPTRAP-03 | push trap 后重执行完成 | handler 解除 PMP 限制后 xRET | 整个 cm.push 序列重新执行并完成：所有寄存器正确存储，sp 正确减少 |
| ZCMPTRAP-04 | cm.pop 中途 load fault 的 trap 记录 | PMP 将栈区设为不可读，执行 cm.pop | load access fault (cause=5)，xEPC=cm.pop 的 PC，xTVAL=故障加载地址 |
| ZCMPTRAP-05 | pop trap 时 sp/a0/ret 未提交 | 在 ZCMPTRAP-04 的 handler 中检查 sp；改用 cm.popretz 且 a0 预置非零，检查 a0 | sp 保持原值，a0 保持非零（未执行 li a0,0），未发生 ret（norm:Zcmp_pop_sp_commit） |
| ZCMPTRAP-06 | pop trap 后重执行完成 | handler 解除限制后 xRET | 整个 cm.pop 序列重新执行并完成：寄存器全部正确恢复，sp 正确增加 |
| ZCMPTRAP-07 | 序列期间多次 trap | push 首个存储地址与第二个存储地址依次设为故障（handler 第一次仅放行第一个），触发两次 trap 后放行 | 两次 trap 的 xEPC 均为 cm.push 的 PC，最终序列完整完成，结果正确 |
| ZCMPTRAP-08 | pop 部分 load 后 trap 的最终正确性 | 使第二个加载地址故障（首个 load 已执行），handler 放行后 xRET，检查全部目的寄存器 | 允许 trap 时部分寄存器已更新；重执行后所有目的寄存器最终值正确 |
| ZCMPTRAP-09 | 故障地址定位到序列中部 | 使列表中第二个寄存器对应的存储地址故障（首个放行） | xTVAL 为第二个存储地址（非首个），验证 trap 报告精确故障访问 |
| ZCMPTRAP-10 | U-mode 执行时 fault 委托到 S-mode | 委托配置后在 U-mode 执行 cm.push 触发栈区故障 | sepc=cm.push PC，scause=7，stval=故障地址，S-mode handler 放行后指令完成 |

---

## Group 7. 各特权级可执行性

**规范依据**：
- Zcmp 指令为普通非特权指令，SPEC 未定义任何特权级限制或 CSR 门控，应在所有特权级正常执行
- smstateen.adoc：stateen CSR 仅对访问受保护状态的指令生效；Zcmp 指令不访问任何扩展状态，不受 stateen 门控

**测试职责**：验证 Zcmp 指令在 M/S/U 及（若实现 H 扩展）HS/VS/VU 各特权级下均可正常执行，不受状态使能门控。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZCMPPR-01 | M-mode 执行 | M-mode 执行 cm.push/cm.pop/cm.mvsa01 组合并验证结果 | 正常执行，无 trap |
| ZCMPPR-02 | S-mode 执行 | 切换至 S-mode 执行同样序列 | 正常执行，结果正确 |
| ZCMPPR-03 | U-mode 执行 | 切换至 U-mode 执行同样序列 | 正常执行，结果正确 |
| ZCMPPR-04 | HS-mode 执行（H 扩展） | 实现 H 扩展时，HS-mode 执行 | 正常执行，无异常 |
| ZCMPPR-05 | VS-mode 执行（H 扩展） | 实现 H 扩展时，切入 VS-mode 执行 | 正常执行，无 virtual-instruction exception |
| ZCMPPR-06 | VU-mode 执行（H 扩展） | 实现 H 扩展时，切入 VU-mode 执行 | 正常执行，无异常 |
| ZCMPPR-07 | Smstateen 不门控 Zcmp 指令 | 若实现 Smstateen，清零 mstateen0/hstateen0/sstateen0 后在 U/VU 模式执行 | 正常执行（Zcmp 指令不访问任何受保护状态） |
| ZCMPPR-08 | sstatus.FS/VS=0 不影响 Zcmp 指令 | 设 sstatus.FS=0 且 sstatus.VS=0（若实现），U-mode 执行 | 正常执行（Zcmp 不属于浮点/向量指令，不受 FS/VS 门控） |

---

## 测试实现说明

1. **指令构造**：若工具链（`riscv64-unknown-linux-elf-gcc` 的 `-march` 含 `zcmp`）支持 `cm.push` 等助记符则直接使用；否则按 SPEC 编码图用 `.half` 构造 16 位指令字，两种形式的一致性由 ZCMPENC-06 验证。基础编码：cm.push=0xB802、cm.pop=0xBA02、cm.popretz=0xBC02、cm.popret=0xBE02（均 `| (rlist<<4) | (spimm<<2)`）；cm.mvsa01=0xAC22、cm.mva01s=0xAC62（均 `| (r1s'<<7) | (r2s'<<2)`）。用例中统一通过宏生成指令字。
2. **扩展探测**：用例以 `config/*/rvtest_config.h` 中的 `ZCMP_SUPPORTED` 宏为执行前提，且要求 `ZCA_SUPPORTED` 同时成立（Zcmp 属 Zc 系列压缩扩展）；当前各平台 config 尚未定义 `ZCMP_SUPPORTED`，实现时需按各平台实际 ISA 能力补充声明，未声明的平台整体跳过本测试集（非用例失败）。
3. **汇编实现**：cm.push/pop/popret 操作 ra 与 s0-s11，与编译器寄存器分配强冲突，功能用例必须以独立汇编片段或显式约束的内联汇编实现，手工控制寄存器预置、栈缓冲区与回读比对；栈缓冲区使用 16 字节对齐的静态数组并预留边界保护字。
4. **trap handler 适配**：Zcmp 指令均为 16 位压缩指令，trap-armed 用例的 xEPC 推进按 2 字节处理（框架 handler 已支持按指令低 2 位判定长度）；Group 6 的重执行用例要求 handler 在 xRET 前解除故障条件（调整 PMP/页表），且不得推进 xEPC。
5. **fault 注入手段**：优先使用 PMP 将专用栈缓冲区设为不可写（push）/不可读（pop）以触发精确的 access fault；多次 trap 用例通过 handler 内分阶段调整 PMP 权限实现。
6. **cm.popretz spimm 字段处置**：SPEC 编码图中该 2 位字段标注为 spimm[5:4]，但规范性汇编语法与 Valid values 表均定义 stack_adj = stack_adj_base + spimm*16（四条指令一致），用例以规范性文本为准；若被测实现按 spimm*64 等不同语义实现，属与本文档 SPEC 不符，用例保持 FAIL 并与 SPEC 比对后记录。
7. **XLEN 参数化**：存储/加载元素宽度、stack_adj_base 表、内存布局偏移按 XLEN（RV64=8 字节 / RV32=4 字节）参数化，qemu-rv32-max 平台按 RV32I 表判定。
8. **平台差异处置**：若任一平台（QEMU/Spike/Sail/whisper/硬件）出现编码与 SPEC 不符、存储/加载布局错误、sp 调整错误、trap 时 xEPC/xTVAL/xCAUSE 记录错误、sp/a0/ret 提前提交或保留编码不触发异常，均属违反 SPEC 的实现缺陷，用例保持 FAIL 并记录至 `bugs/` 目录，不做跳过或 workaround。

---

## 附录 A：规范点覆盖矩阵

| Norm ID | 覆盖用例 | 备注 |
|---------|----------|------|
| `norm:cm-push_op` | ZCMPENC-01, ZCMPPUSH-01 ~ ZCMPPUSH-08, ZCMPTRAP-01 ~ ZCMPTRAP-03, ZCMPPR-01 ~ ZCMPPR-08 | |
| `norm:cm-pop_op` | ZCMPENC-02, ZCMPPOP-01 ~ ZCMPPOP-06, ZCMPTRAP-04 ~ ZCMPTRAP-06, ZCMPPR-01 ~ ZCMPPR-08 | |
| `norm:cm-popret_op` | ZCMPENC-04, ZCMPRET-01, ZCMPRET-03, ZCMPRET-04, ZCMPPR-01 ~ ZCMPPR-03 | |
| `norm:cm-popretz_op` | ZCMPENC-03, ZCMPRET-02, ZCMPRET-04 ~ ZCMPRET-06, ZCMPTRAP-05 | |
| `norm:cm-mvsa01_op` | ZCMPENC-05, ZCMPENC-08, ZCMPMV-01, ZCMPMV-02, ZCMPMV-04, ZCMPMV-05, ZCMPMV-07, ZCMPPR-01 ~ ZCMPPR-03 | |
| `norm:cm-mvsa01_res` | ZCMPENC-08 | |
| `norm:cm-mvsa01_sreg` | ZCMPMV-02, ZCMPMV-07 | |
| `norm:cm-mva01s_op` | ZCMPENC-05, ZCMPMV-03, ZCMPMV-04, ZCMPMV-06, ZCMPMV-07 | |
| `norm:cm-mva01s_sreg` | ZCMPMV-03, ZCMPMV-07 | |
| `norm:Zcmp_reexecute` | ZCMPTRAP-03, ZCMPTRAP-06 ~ ZCMPTRAP-10 | |
| `norm:Zcmp_trap` | ZCMPTRAP-01, ZCMPTRAP-04, ZCMPTRAP-07, ZCMPTRAP-09, ZCMPTRAP-10 | |
| `norm:Zcmp_push_sp_commit` | ZCMPTRAP-02 | |
| `norm:Zcmp_pop_sp_commit` | ZCMPTRAP-05, ZCMPRET-06 | |
| `norm:Zcmp_bus_fault` | — | platform-defined，不设可判定用例 |
| `norm:interrupts_allowed_in_pushpop` | — | implementation-defined，不设可判定用例 |
| rlist 0~3 保留（SPEC NOTE） | ZCMPENC-07 | 保留编码基础规则 |
| 寄存器列表映射与 stack_adj 表 | ZCMPPUSH-03 ~ ZCMPPUSH-06, ZCMPPOP-03, ZCMPPOP-06, ZCMPRET-04 | 规范性汇编语法章节 |
| pop 序列部分 load 允许更新架构状态 | ZCMPTRAP-08 | software view 规范性描述 |
| popret sp 提交后 ret 必须执行 | ZCMPRET-01, ZCMPRET-05 | software view 规范性描述 |
| RV32E 变体（rlist 4~6） | — | 测试平台均为 RV32I/RV64，不适用 |
