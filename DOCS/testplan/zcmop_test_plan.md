# Zcmop 扩展测试计划（Compressed May-Be-Operations v1.0）

## 概述

本测试计划覆盖 RISC-V Zcmop（Compressed May-Be-Operations，v1.0）扩展，验证 8 条 16 位压缩 MOP 指令（C.MOP.1 / C.MOP.3 / ... / C.MOP.15）的指令编码、"不写任何寄存器"的核心操作语义、架构状态影响与各特权级可执行性，以及与 Zicfiss 扩展对 C.MOP.1 / C.MOP.5 编码重定义的交互边界。C.MOP.N 被设计为可被后续扩展重定义的"可能是操作"的压缩指令，本计划仅验证未被重定义时的基线 Zcmop 行为。

本测试计划依据 `SPEC/riscv-isa-manual/src/unpriv/zcmop.adoc` 中的规范点（norm 标记）编写，指令编码以 `SPEC/riscv-isa-manual/src/unpriv/images/wavedrom/c-mop.edn` 与 SPEC 编码表为准。

### 本文档覆盖的 SPEC 章节
- Zcmop Extension for Compressed May-Be-Operations（C.MOP.N 指令定义与语义）
- C.MOP.N 编码表（norm:Zcmop_enc，C.LUI 保留编码空间，quadrant 1 / funct3=011）

### 范围说明
- Zcmop 扩展仅定义 8 条 16 位指令，N 取 1..15 之间的**奇数**；不涉及任何 CSR。
- Zcmop 依赖 Zca 扩展（SPEC 原文："The zcmop extension depends upon the zca extension"），用例以 `ZCMOP_SUPPORTED` 与 `ZCA_SUPPORTED` 宏同时成立为执行前提。
- 与 Zimop 的 32 位 MOP 不同，C.MOP.N 被明确定义为**不写任何寄存器**（norm:Zcmop_instr_write），这是本计划语义验证的核心断言方向：执行后对应隐式寄存器 xN 及所有架构状态保持原值。
- "编码允许未来扩展定义其读取寄存器 xN"是对未来重定义扩展的许可声明，基线行为不可观测，不设用例。
- SPEC NOTE "每条 Zcmop 指令等价于某条 Zimop 指令，但展开方式留给重定义扩展决定"面向未来扩展，基线不可测，不设用例。
- 推荐汇编语法为零操作数 `c.mop.n`（隐式访问 xN），通过编码一致性用例间接覆盖。

### 由其他测试计划覆盖
- Zimop 32 位 MOP 扩展（MOP.R.n / MOP.RR.n，写零到 x[rd]） → `zimop_test_plan.md`
- Zicfiss 影子栈对 C.MOP.1（C.SSPUSH x1）与 C.MOP.5（C.SSPOPCHK x5）编码的重定义、SSE=1 时的完整语义 → `cfi_test_plan.md` / `Hypervisor_CFI_test_plan.md`（本计划仅验证 Zicfiss 未激活时回退符合 Zcmop 基线）
- illegal-instruction exception 的 trap 递送、委托与 CSR 写入行为 → `Sm_Exceptions_test_plan.md` / `Ss_Exceptions_test_plan.md` / `Hypervisor_Exceptions_test_plan.md`
- Smstateen 对各扩展状态的门控机制本身 → `smstateen_test_plan.md`

---

## 覆盖的规范点

本章节列出本文档所有测试组中引用的规范点（norm ID），已去重并按字母顺序排列。

| Norm ID | 原文 | 中文说明 |
|---------|------|----------|
| `norm:Zcmop_enc` | c.mop.n is encoded in the reserved encoding space corresponding to c.lui[xN,0]. | C.MOP.N 编码在 C.LUI[xN,0] 对应的保留编码空间中（quadrant 1，funct3=011，bits[10:8]=N[3:1]，bit7=1，其余特征位固定）。 |
| `norm:Zcmop_instr_write` | Unlike the MOPs defined in the Zimop extension, the c.mop.n instructions are defined to _not_ write any register. | 与 Zimop 扩展定义的 MOP 不同，C.MOP.N 指令被定义为不写任何寄存器。 |
| `norm:Zcmop_op` | This section defines the Zcmop extension, which defines eight 16-bit MOP instructions named c.mop.n, where N is an odd integer between 1 and 15, inclusive. | Zcmop 扩展定义 8 条 16 位 MOP 指令 C.MOP.N，N 为 1 到 15 之间的奇数。 |

**非 norm 标记的规范性描述与 SPEC NOTE 中的可测属性**：
- Zcmop 依赖 Zca 扩展（规范性依赖声明，以 `ZCMOP_SUPPORTED` && `ZCA_SUPPORTED` 双宏门控落实）。
- C.MOP.N 编码与 `c.lui xN, 0` 的编码完全相同；该空间中 N=2 的编码为 C.ADDI16SP（基础 C/Zca 规范），偶数 N（N≥4）且 nzimm=0 的编码在基础压缩指令规范中为保留编码，执行应触发 illegal-instruction exception（基础 ISA 规则，非 Zcmop norm）。
- 推荐汇编语法为零操作数 `c.mop.n`，隐式访问的寄存器为 xN（SPEC NOTE，通过编码一致性用例间接覆盖）。
- C.MOP.N 为 16 位压缩指令，正常退休时 PC 前进 2 字节，可位于 2 字节对齐（非 4 字节对齐）地址。

---

## Group 1. 指令编码验证

**规范依据**：
- `norm:Zcmop_enc`：C.MOP.N 编码位于 C.LUI[xN,0] 保留编码空间
- `norm:Zcmop_op`：8 条指令，N=1,3,5,7,9,11,13,15
- 基础 ISA：压缩指令保留编码执行触发 illegal-instruction exception

**测试职责**：验证 C.MOP.N 指令的机器码字段与 SPEC 编码表及 c-mop.edn 完全一致，汇编助记符与手工构造编码一致，N 的全部奇数取值产生互不重复且符合位域规律的编码，偶数 N 保留编码不被 Zcmop 占用。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| CMOPENC-01 | C.MOP.N 指令字编码验证 | 取 8 条 C.MOP.N 的 16 位机器码（汇编器生成或 `.half` 构造），逐字段比对 SPEC 编码表与 c-mop.edn | bits[1:0]=0b01（quadrant 1），bits[6:2]=0，bit7=1，bits[10:8]=N[3:1]，bits[12:11]=0b00，bits[15:13]=0b011（funct3）；十六进制值依次为 0x6081/0x6181/0x6281/0x6381/0x6481/0x6581/0x6681/0x6781 |
| CMOPENC-02 | 助记符编码与手工构造一致 | 比对汇编器对 `c.mop.n` 助记符生成的编码与手工 `.half` 构造值（全部 8 条） | 两种形式编码完全一致 |
| CMOPENC-03 | N 遍历编码规律性 | 构造 N=1..15（奇数）全部 8 条指令字，逐条检查 N[3:1] 位域映射（bits[10:8]）并比对互不重复 | 8 个编码唯一，N[3:1] 位域映射与编码图一致 |
| CMOPENC-04 | 编码与 c.lui xN,0 空间一致 | 将 C.MOP.N 编码按基础压缩指令格式解码为 `c.lui xN, 0` 字段布局，验证 rd'/imm 字段值 | 解码字段为 rd=xN、nzimm=0，确认占用的是 C.LUI[xN,0] 保留编码空间 |
| CMOPENC-05 | 偶数 N 编码不被 Zcmop 占用 | trap-armed 执行偶数编码：① bits[10:8]=0b000、bit7=0 其余同 C.MOP 模式（对应 x2 编码，C.ADDI16SP nzimm=0 保留）；② bits[10:8]=0b010（对应 x4，c.lui nzimm=0 保留） | 均触发 illegal-instruction exception (cause=2)（基础压缩规范保留编码规则，非 Zcmop 行为） |

---

## Group 2. C.MOP.N 操作语义（不写任何寄存器）

**规范依据**：
- `norm:Zcmop_instr_write`：C.MOP.N 被定义为不写任何寄存器
- `norm:Zcmop_op`：8 条指令均适用该语义；编码允许未来扩展读取 xN，但基线行为不得依赖或修改 xN 的值

**测试职责**：验证 C.MOP.N 执行后隐式关联的寄存器 xN 及所有通用寄存器保持原值，并遍历全部 8 条指令。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| CMOPSEM-01 | 隐式 xN 不被写入 | xN 预置非零模式值（如 0xA5A5A5A5A5A5A5A5），执行 C.MOP.N | xN 保持原值，无 trap |
| CMOPSEM-02 | 全遍历不写寄存器 | N=1,3,...,15 逐条执行，每次将 xN 预置不同非零模式值 | 8 条指令执行后对应 xN 全部保持原值，无一触发异常 |
| CMOPSEM-03 | 结果与 xN 预置值无关 | xN 分别预置 {全 1, 仅最高位非零, 0x0123456789ABCDEF}，执行同一条 C.MOP.N | xN 均保持各自预置原值 |
| CMOPSEM-04 | 区别于 Zimop 写零语义 | xN 预置非零值，执行 C.MOP.N 后回读 | xN ≠ 0（保持非零原值，证明不是写零行为） |
| CMOPSEM-05 | 连续重复执行状态稳定 | 同一条 C.MOP.N 连续执行 3 次，每次前后比对 xN | xN 始终不变，无累积副作用 |

---

## Group 3. 架构状态影响与边界

**规范依据**：
- `norm:Zcmop_instr_write`：C.MOP.N 不写任何寄存器，除 PC 前进外不产生架构可见效果
- 压缩指令基础规则：16 位指令退休时 PC 前进 2 字节，允许 2 字节对齐

**测试职责**：验证 C.MOP.N 除 PC 前进外不修改任何架构状态，压缩指令的 2 字节对齐与 PC 步进行为正确。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| CMOPSB-01 | 不修改其他架构状态 | 执行前将可保存通用寄存器置为已知模式值，执行 C.MOP.N 后逐一比对，并读取 mstatus/mcause/mtval 等 CSR | 所有寄存器与 CSR 值不变 |
| CMOPSB-02 | PC 正确前进 2 字节 | 单条 C.MOP.N 后紧跟可观测指令（写内存/寄存器标记），用 trap 或读回 PC 验证步长 | 指令正常退休，PC 前进 2 字节，标记正确写入 |
| CMOPSB-03 | 2 字节对齐地址执行 | 将 C.MOP.N 放置于 2 字节对齐但非 4 字节对齐的地址执行 | 正常执行，无指令地址对齐异常 |
| CMOPSB-04 | 与其他压缩指令混排 | C.MOP.N 与 c.addi/c.mv 等压缩指令交替排列成序列执行 | 全部正常退休，各指令结果正确 |

---

## Group 4. 各特权级可执行性

**规范依据**：
- `norm:Zcmop_op` / `norm:Zcmop_instr_write`：C.MOP.N 为普通非特权指令，基线 Zcmop 未定义任何特权级限制或 CSR 门控，应在所有特权级正常执行
- smstateen.adoc：stateen CSR 仅对访问受保护**状态**的指令生效；C.MOP.N 不访问任何扩展状态，不受 stateen 门控
- `SPEC/riscv-isa-manual/src/unpriv/cfi.adoc` norm:zicfiss_c-sspush_enc / norm:zicfiss_c-sspopchk_enc：C.MOP.1 / C.MOP.5 被 Zicfiss 重定义，Zicfiss 未实现或未激活时回退 Zcmop 定义行为

**测试职责**：验证 C.MOP.N 在 M/S/U 及（若实现 H 扩展）HS/VS/VU 各特权级下均可正常执行，不受状态使能门控，并验证 Zicfiss 未激活时 C.MOP.1/C.MOP.5 回退符合 Zcmop 基线。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| CMOPPR-01 | M-mode 执行 C.MOP.N | M-mode 执行 C.MOP.N 并验证 xN 不变 | 正常执行，无 trap |
| CMOPPR-02 | S-mode 执行 C.MOP.N | 切换至 S-mode 执行 | 正常执行，无异常，xN 不变 |
| CMOPPR-03 | U-mode 执行 C.MOP.N | 切换至 U-mode 执行 | 正常执行，无异常，xN 不变 |
| CMOPPR-04 | HS-mode 执行 C.MOP.N（H 扩展） | 实现 H 扩展时，HS-mode 执行 | 正常执行，无异常 |
| CMOPPR-05 | VS-mode 执行 C.MOP.N（H 扩展） | 实现 H 扩展时，切入 VS-mode 执行 | 正常执行，无 virtual-instruction exception |
| CMOPPR-06 | VU-mode 执行 C.MOP.N（H 扩展） | 实现 H 扩展时，切入 VU-mode 执行 | 正常执行，无异常 |
| CMOPPR-07 | Smstateen 不门控 C.MOP.N | 若实现 Smstateen，清零 mstateen0/hstateen0/sstateen0 后在 U/VU 模式执行 | 正常执行（C.MOP.N 不访问任何受保护状态） |
| CMOPPR-08 | sstatus.FS/VS=0 不影响 C.MOP.N | 设 sstatus.FS=0 且 sstatus.VS=0（若实现），U-mode 执行 | 正常执行（C.MOP.N 不属于浮点/向量指令，不受 FS/VS 门控） |
| CMOPPR-09 | Zicfiss SSE=0 时回退符合基线 | 若实现 Zicfiss，设 xSSE=0（senvcfg.SSE=0），U-mode 执行 C.MOP.1 与 C.MOP.5 编码 | 指令按 Zcmop 基线执行，x1/x5 保持不变，无异常（回退细节的完整验证归 cfi_test_plan） |

---

## 测试实现说明

1. **指令构造**：若工具链（`riscv64-unknown-linux-elf-gcc` 的 `-march` 含 `zcmop`）支持 `c.mop.n` 助记符则直接使用；否则按 SPEC 编码表用 `.half` 构造 16 位指令字，两种形式的一致性由 CMOPENC-02 验证。参考编码：C.MOP.1=0x6081，C.MOP.3=0x6181，C.MOP.5=0x6281，C.MOP.7=0x6381，C.MOP.9=0x6481，C.MOP.11=0x6581，C.MOP.13=0x6681，C.MOP.15=0x6781；用例中统一通过宏生成全部 8 条指令字。
2. **扩展探测**：用例以 `config/*/rvtest_config.h` 中的 `ZCMOP_SUPPORTED` 宏为执行前提，且要求 `ZCA_SUPPORTED` 同时成立（Zcmop 依赖 Zca）；未声明宏的平台整体跳过本测试集（非用例失败）。
3. **trap handler 适配**：C.MOP.N 为 16 位指令，trap-armed 用例的 sepc 推进须按压缩指令规则前进 2 字节（框架 handler 已支持按指令低 2 位判定长度）。
4. **隐式寄存器选择**：语义用例优先选择调用约定中易失/可保存寄存器以外的寄存器做预置比对时，需注意编译器寄存器分配；建议以汇编内联或独立汇编片段实现，显式控制 xN 的预置与回读。
5. **Zicfiss 编码重叠处置**：C.MOP.1 与 C.MOP.5 被 Zicfiss 重定义为 C.SSPUSH x1 / C.SSPOPCHK x5。全遍历用例在 Zicfiss 已实现且 SSE=1 的上下文中执行时，这两个编码不要求 Zcmop 基线结果（该行为归 `cfi_test_plan.md`）；其余编码与未实现 Zicfiss（或 SSE=0）的平台一律要求"不写任何寄存器"。
6. **平台差异处置**：若任一平台（QEMU/Spike/Sail/whisper/硬件）出现 C.MOP.N 修改寄存器、编码与 SPEC 不符、保留编码不触发异常或在合法特权级下拒绝执行，属违反 SPEC 的实现缺陷，用例保持 FAIL 并记录至 `bugs/` 目录，不做跳过或 workaround。

---

## 附录 A：规范点覆盖矩阵

| Norm ID | 覆盖用例 | 备注 |
|---------|----------|------|
| `norm:Zcmop_op` | CMOPENC-01 ~ CMOPENC-03, CMOPSEM-02, CMOPPR-01 ~ CMOPPR-09 | |
| `norm:Zcmop_enc` | CMOPENC-01 ~ CMOPENC-05 | CMOPENC-05 的保留编码预期基于基础压缩指令规范 |
| `norm:Zcmop_instr_write` | CMOPSEM-01 ~ CMOPSEM-05, CMOPSB-01, CMOPPR-01 ~ CMOPPR-09 | 核心语义：不写任何寄存器 |
| Zcmop 依赖 Zca（规范性依赖声明） | 实现说明 2（双宏门控） | 以配置前提落实，不设独立用例 |
| 编码允许未来扩展读取 xN | — | 面向未来重定义扩展的许可声明，基线不可观测 |
| 每条 C.MOP.N 等价于某条 Zimop 指令（SPEC NOTE） | — | 展开方式留给重定义扩展决定，基线不可测 |
| 推荐汇编语法为零操作数 c.mop.n（SPEC NOTE） | CMOPENC-02 | informational NOTE 的间接覆盖 |
| `norm:zicfiss_c-sspush_enc` / `norm:zicfiss_c-sspopchk_enc` | CMOPPR-09（仅回退基线） | 完整语义归 `cfi_test_plan.md` |
