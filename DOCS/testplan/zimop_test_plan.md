# Zimop 扩展测试计划（May-Be-Operations v1.0）

## 概述

本测试计划覆盖 RISC-V Zimop（May-Be-Operations，v1.0）扩展，验证 40 条 MOP 指令（32 条 MOP.R.n + 8 条 MOP.RR.n）的指令编码、"写零到 x[rd]"的核心操作语义、寄存器操作数边界行为、架构状态影响、各特权级可执行性，以及与 Zicfiss 扩展的编码重定义交互。MOP 被设计为可被后续扩展重定义的"可能是操作"的指令，本计划仅验证未被重定义时的基线 Zimop 行为。

本测试计划依据 `SPEC/riscv-isa-manual/src/unpriv/zimop.adoc` 中的规范点（norm 标记）编写，norm 定义参照 `SPEC/riscv-isa-manual/normative_rule_defs/zimop.yaml`，指令编码以 `SPEC/riscv-isa-manual/src/unpriv/images/wavedrom/mop-r.edn` 与 `mop-rr.edn` 为准。

### 本文档覆盖的 SPEC 章节
- Zimop Extension for May-Be-Operations（MOP.R.n / MOP.RR.n 指令定义、编码与语义）
- MOP.R 编码表（norm:Zimop_mop-r_enc，SYSTEM 主操作码内 funct3=100 编码空间）
- MOP.RR 编码表（norm:Zimop_mop-rr_enc，SYSTEM 主操作码内 funct3=101 编码空间）

### 范围说明
- Zimop 扩展仅定义 40 条 32 位指令，不涉及任何 CSR；指令在 RV32 与 RV64 下均存在，本框架主目标为 RV64（XLEN=64），RV32 平台（如 qemu-rv32-max）可复用同一语义用例。
- 各平台配置（`config/*/rvtest_config.h`）已定义 `ZIMOP_SUPPORTED` / `ZIMOP1P0P0_SUPPORTED` 门控宏，用例以宏探测为前提条件。
- SPEC NOTE 中"MOP 行为预期受特权 CSR 状态调制"（modulated by privileged CSR state）是对**未来重定义扩展**的预期描述，基线 Zimop 未定义任何 CSR 门控，本计划以"无 CSR 门控、各特权级均可执行"作为断言方向（MOPPR 组）。
- "MOP 不携带从 rs1/rs2 到 rd 的语法依赖"（syntactic dependency）是面向乱序实现的微架构设计约束，软件层面不可观测，不设用例。
- MOP.R.n 的 n 取值范围为 0..31（32 条），MOP.RR.n 的 n 取值范围为 0..7（8 条），超出该范围不存在对应指令。

### 由其他测试计划覆盖
- Zcmop 压缩 MOP 扩展（c.mop.n，16 位，不写任何寄存器） → `zcmop_test_plan.md`
- Zicfiss 影子栈指令对 MOP.R.28 / MOP.RR.7 编码的重定义、SSE=0 时的指令回退细节 → `cfi_test_plan.md` / `Hypervisor_CFI_test_plan.md`（本计划仅验证回退后符合 Zimop 写零基线）
- illegal-instruction exception 的 trap 递送、委托与 CSR 写入行为 → `Sm_Exceptions_test_plan.md` / `Ss_Exceptions_test_plan.md` / `Hypervisor_Exceptions_test_plan.md`
- Smstateen 对各扩展状态的门控机制本身 → `smstateen_test_plan.md`

---

## 覆盖的规范点

本章节列出本文档所有测试组中引用的规范点（norm ID），已去重并按字母顺序排列。

| Norm ID | 原文 | 中文说明 |
|---------|------|----------|
| `norm:Zimop_mop-r_enc` | Encoding for MOP.R.n（见 mop-r.edn 编码图） | MOP.R.n 的编码：opcode(bits[6:0])=0x73(SYSTEM)、funct3(bits[14:12])=0b100、bits[19:15]=rs1（无约束）、bits[21:20]=n[1:0]（占用 rs2 字段位置低 2 位）、bits[24:22]=0b111（rs2 字段位置高 3 位固定）、bit25=0（与 MOP.RR 区分）、bits[27:26]=n[3:2]、bits[29:28]=0、bit30=n[4]、bit31=1；等价于 funct7=0b1_n[4]_00_n[3]_n[2]_0。 |
| `norm:Zimop_mop-r_op` | The Zimop extension defines 32 MOP instructions named MOP.R.n, where n is an integer between 0 and 31, inclusive. Unless redefined by another extension, these instructions simply write 0 to x[rd]. | Zimop 扩展定义 32 条 MOP.R.n 指令（n=0..31）。除非被其他扩展重定义，这些指令仅向 x[rd] 写入 0。 |
| `norm:Zimop_mop-rr_enc` | Encoding for MOP.RR.n（见 mop-rr.edn 编码图） | MOP.RR.n 的编码：opcode(bits[6:0])=0x73(SYSTEM)、funct3(bits[14:12])=0b100（与 MOP.R 相同）、标准 R 型 rs2 字段(bits[24:20])、bit25=1、bits[27:26]=n[1:0]、bits[29:28]=0、bit30=n[2]、bit31=1；等价于 funct7=0b1_n[2]_00_n[1]_n[0]_1。 |
| `norm:Zimop_mop-rr_op` | The Zimop extension additionally defines 8 MOP instructions named MOP.RR.n, where n is an integer between 0 and 7, inclusive. Unless redefined by another extension, these instructions simply write 0 to x[rd]. | Zimop 扩展另定义 8 条 MOP.RR.n 指令（n=0..7）。除非被其他扩展重定义，这些指令仅向 x[rd] 写入 0。 |

**非 norm 标记的规范性描述与 SPEC NOTE 中的可测属性**：
- MOP 编码位于 SYSTEM 主操作码（0x73）；MOP.R 与 MOP.RR 的 funct3 均为 0b100，以 bit25 区分（MOP.R=0，MOP.RR=1）。funct3=0b100 空间内不满足 MOP.R/MOP.RR 位模式的编码，以及 funct3=0b101/0b110/0b111 等未被任何指令定义的 SYSTEM 编码，执行应触发 illegal-instruction exception（基础 ISA 未定义指令规则）。
- MOP 与 HINT 的区别：MOP 允许修改架构状态（写 x[rd]），因此不能编码为 HINT；"rd 被写零"是可观测的架构状态修改（SPEC NOTE，可测）。
- MOP 写零而非 no-op 的设计意图：允许软件通过对结果判零来探测特性存在（SPEC NOTE，可用分支序列功能验证）。
- 推荐汇编语法 MOP.R.n rd, rs1 / MOP.RR.n rd, rs1, rs2，任意 x 寄存器说明符均合法（SPEC NOTE，通过编码一致性用例间接覆盖）。
- MOP.R.28 与 MOP.RR.7 的**部分**编码被 Zicfiss 重定义（SSPOPCHK/SSRDP 与 SSPUSH）；未被 Zicfiss 使用的位组合仍保持 Zimop 行为（`SPEC/riscv-isa-manual/src/unpriv/cfi.adoc` norm:zicfiss_sspush_enc / norm:zicfiss_sspopchk_enc / norm:zicfiss_ssrdp_enc）。

---

## Group 1. 指令编码验证

**规范依据**：
- `norm:Zimop_mop-r_enc`：MOP.R.n 编码（opcode=0x73，funct3=0b100，bit25=0，funct7=0b1_n[4]_00_n[3]_n[2]_0，bits[21:20]=n[1:0]，bits[24:22]=0b111，rs1 无约束）
- `norm:Zimop_mop-rr_enc`：MOP.RR.n 编码（opcode=0x73，funct3=0b100，bit25=1，funct7=0b1_n[2]_00_n[1]_n[0]_1，标准 R 型 rs2 字段）
- `norm:Zimop_mop-r_op` / `norm:Zimop_mop-rr_op`：按正确编码执行的指令应实现写零语义
- 基础 ISA：SYSTEM 主操作码内未定义编码执行触发 illegal-instruction exception

**测试职责**：验证 MOP.R/MOP.RR 指令的机器码字段与 SPEC 编码图完全一致，汇编助记符与手工构造编码一致，n 的全部取值产生互不重复且符合位域规律的编码，编码空间外的保留编码触发非法指令异常。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| MOPENC-01 | MOP.R.n 指令字编码验证 | 取 MOP.R.0 与 MOP.R.1 的 32 位机器码（汇编器生成或 `.insn` 构造），逐字段比对 mop-r.edn | opcode=0x73，funct3(bits[14:12])=0b100，bits[21:20]=n[1:0]，bits[24:22]=0b111，bit25=0，bits[27:26]=n[3:2]，bits[29:28]=0，bit30=n[4]，bit31=1，rd/rs1 位于标准字段位置 |
| MOPENC-02 | MOP.RR.n 指令字编码验证 | 取 MOP.RR.0 与 MOP.RR.1 的 32 位机器码，逐字段比对 mop-rr.edn | opcode=0x73，funct3=0b100，bit25=1，bits[27:26]=n[1:0]，bits[29:28]=0，bit30=n[2]，bit31=1，rd/rs1/rs2 位于标准字段位置 |
| MOPENC-03 | 助记符编码与手工构造一致 | 比对汇编器对 `mop.r.n`/`mop.rr.n` 助记符生成的编码与手工 `.insn` 构造值（各取 2 个 n 值） | 两种形式编码完全一致 |
| MOPENC-04 | MOP.R.n n 遍历编码规律性 | 构造 n=0..31 全部 32 条 MOP.R.n 指令字，逐条检查 n[4:0] 位域映射（bits[21:20]=n[1:0]、bits[27:26]=n[3:2]、bit30=n[4]）并比对互不重复 | 32 个编码唯一，n[4:0] 位域映射与编码图一致 |
| MOPENC-05 | MOP.RR.n n 遍历编码规律性 | 构造 n=0..7 全部 8 条 MOP.RR.n 指令字，逐条检查 n[2:0] 位域映射（bits[27:26]=n[1:0]、bit30=n[2]）并比对互不重复 | 8 个编码唯一，n[2:0] 位域映射与编码图一致 |
| MOPENC-06 | SYSTEM 空间保留编码触发非法指令异常 | 构造 SYSTEM 空间内未定义编码并执行（trap-armed）：① funct3=0b100 且 funct7=0x00（bit31=0，不满足 MOP.R/MOP.RR 的 bit31=1 模式）；② funct3=0b101 且 funct7=0x41（SYSTEM 空间无任何已定义指令使用 funct3=0b101）；③ funct3=0b110 与 0b111 且 funct7=0x00 各一条 | 均触发 illegal-instruction exception (cause=2) |

---

## Group 2. MOP.R.n 操作语义

**规范依据**：
- `norm:Zimop_mop-r_op`：MOP.R.n 未被重定义时仅写 0 到 x[rd]，结果与 rs1 无关；编码允许未来扩展读取 rs1，但基线行为不得依赖 rs1 的值

**测试职责**：验证 MOP.R.n 的写零语义对任意 rs1 值成立，并遍历全部 32 条指令。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| MOPR-01 | rd 写零（rs1 为模式值） | rs1=0xA5A5A5A5A5A5A5A5，rd 预置非零，执行 MOP.R.0 | rd=0，无 trap |
| MOPR-02 | rd 写零（rs1=全 1） | rs1=0xFFFFFFFFFFFFFFFF，执行 MOP.R.1 | rd=0（结果不受 rs1 值影响） |
| MOPR-03 | rd 写零（rs1=x0） | rs1=x0，执行 MOP.R.2 | rd=0 |
| MOPR-04 | rd 写零（rs1=仅最高位非零） | rs1=0x8000000000000000，执行 MOP.R.3 | rd=0 |
| MOPR-05 | MOP.R.n 全遍历写零 | n=0..31 逐条执行，rd 每次预置不同非零模式值，rs1 取非零值 | 32 条指令全部 rd=0，无一触发异常 |
| MOPR-06 | rd=x0 执行无效果 | rd=x0，rs1 非零，执行 MOP.R.n | 无 trap，x0 读回恒为 0，程序继续执行 |
| MOPR-07 | rd=rs1 同寄存器 | rd 与 rs1 同为 xN，xN 预置非零，执行 MOP.R.n | xN=0（写零语义对自身操作数同样成立） |

---

## Group 3. MOP.RR.n 操作语义

**规范依据**：
- `norm:Zimop_mop-rr_op`：MOP.RR.n 未被重定义时仅写 0 到 x[rd]，结果与 rs1、rs2 均无关
- `SPEC/riscv-isa-manual/src/unpriv/cfi.adoc`：MOP.RR.7 的部分编码被 Zicfiss 重定义，未使用位组合保持 Zimop 行为

**测试职责**：验证 MOP.RR.n 的写零语义对任意 rs1/rs2 值组合成立，并遍历全部 8 条指令，妥善处理与 Zicfiss 的编码重叠。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| MOPRR-01 | rd 写零（rs1/rs2 均为模式值） | rs1/rs2 预置不同非零模式值，rd 预置非零，执行 MOP.RR.0 | rd=0，无 trap |
| MOPRR-02 | rd 写零（rs1/rs2 全边界组合） | rs1/rs2 取 {0, 全 1, 仅最高位非零} 组合，执行 MOP.RR.1 | rd=0（结果不受任何操作数值影响） |
| MOPRR-03 | MOP.RR.n 全遍历写零 | n=0..7 逐条执行，rd 每次预置非零模式值 | 8 条指令全部 rd=0，无一触发异常；若实现 Zicfiss 且 SSE=1，MOP.RR.7 中 Zicfiss 占用的位组合按 cfi_test_plan 职责处置，其余位组合仍验证写零 |
| MOPRR-04 | rd=x0 执行无效果 | rd=x0，rs1/rs2 非零，执行 MOP.RR.n | 无 trap，x0 读回恒为 0 |
| MOPRR-05 | rd=rs1=rs2 同寄存器 | 三操作数同为 xN，xN 预置非零，执行 MOP.RR.n | xN=0 |

---

## Group 4. 架构状态影响与边界

**规范依据**：
- `norm:Zimop_mop-r_op` / `norm:Zimop_mop-rr_op`：MOP 仅写 x[rd]，不产生其他架构可见效果
- SPEC NOTE：MOP 允许修改架构状态（区别于 HINT），写零结果可用于特性探测分支

**测试职责**：验证 MOP 除写 rd 外不修改任何架构状态、PC 正常前进、写零结果对后续指令可观测，并验证 SPEC NOTE 描述的"判零探测"用法功能正确。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| MOPSB-01 | 不修改其他架构状态 | 执行前将可保存通用寄存器置为已知模式值，执行 MOP.R/MOP.RR 后逐一比对，并读取 mstatus/mcause/mtval 等 CSR | 除 rd 外所有寄存器与 CSR 值不变 |
| MOPSB-02 | PC 正确前进 | 单条 MOP 指令后紧跟可观测指令（写内存/寄存器标记） | 指令正常退休，PC 前进 4 字节，标记正确写入 |
| MOPSB-03 | 写零结果立即可观测 | MOP 写 rd 后紧跟依赖 rd 的指令（如 add/st） | 后续指令观察到 rd=0 |
| MOPSB-04 | 判零特性探测序列 | 按 SPEC NOTE 设计意图执行 `MOP.R.n rd, rs1` + `beqz rd` 分支序列 | MOP 存在时 rd=0，分支被采取，进入"特性存在"路径 |
| MOPSB-05 | MOP 修改架构状态（非 HINT） | 验证 rd 预置非零值在 MOP 执行后确实被改写为 0（而非保持原值） | rd 从非零变为 0，证明 MOP 产生架构可见修改 |

---

## Group 5. 各特权级可执行性

**规范依据**：
- `norm:Zimop_mop-r_op` / `norm:Zimop_mop-rr_op`：MOP 为普通非特权指令，基线 Zimop 未定义任何特权级限制或 CSR 门控，应在所有特权级正常执行
- SPEC NOTE："行为受特权 CSR 状态调制"仅适用于未来重定义 MOP 的扩展，基线行为不受任何 CSR 门控
- smstateen.adoc：stateen CSR 仅对访问受保护**状态**的指令生效；MOP 不访问任何扩展状态，不受 stateen 门控

**测试职责**：验证 MOP 在 M/S/U 及（若实现 H 扩展）HS/VS/VU 各特权级下均可正常执行，不受状态使能门控，并验证 Zicfiss 未激活时指令回退符合 Zimop 写零基线。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| MOPPR-01 | M-mode 执行 MOP | M-mode 执行 MOP.R 与 MOP.RR 各一条并验证 rd=0 | 正常执行，无 trap |
| MOPPR-02 | S-mode 执行 MOP | 切换至 S-mode 执行两类指令 | 正常执行，无异常，rd=0 |
| MOPPR-03 | U-mode 执行 MOP | 切换至 U-mode 执行两类指令 | 正常执行，无异常，rd=0 |
| MOPPR-04 | HS-mode 执行 MOP（H 扩展） | 实现 H 扩展时，HS-mode 执行两类指令 | 正常执行，无异常 |
| MOPPR-05 | VS-mode 执行 MOP（H 扩展） | 实现 H 扩展时，切入 VS-mode 执行两类指令 | 正常执行，无 virtual-instruction exception |
| MOPPR-06 | VU-mode 执行 MOP（H 扩展） | 实现 H 扩展时，切入 VU-mode 执行两类指令 | 正常执行，无异常 |
| MOPPR-07 | Smstateen 不门控 MOP | 若实现 Smstateen，清零 mstateen0/hstateen0/sstateen0 后在 U/VU 模式执行两类指令 | 正常执行（MOP 不访问任何受保护状态） |
| MOPPR-08 | sstatus.FS/VS=0 不影响 MOP | 设 sstatus.FS=0 且 sstatus.VS=0（若实现），U-mode 执行两类指令 | 正常执行（MOP 位于 SYSTEM 编码空间但不属于浮点/向量指令，不受 FS/VS 门控） |
| MOPPR-09 | Zicfiss SSE=0 时回退符合写零基线 | 若实现 Zicfiss，设 xSSE=0（senvcfg.SSE=0），U-mode 执行 32-bit SSPUSH/SSPOPCHK 编码 | 指令按 Zimop 基线执行，rd=0，无异常（回退细节的完整验证归 cfi_test_plan） |

---

## 测试实现说明

1. **指令构造**：框架构建 march 不含 zimop，助记符不可用，一律按 mop-r.edn / mop-rr.edn 编码图用 `.insn r` / `.4byte` 构造。参考编码（rd/rs1/rs2 均为 x0 时）：
   - MOP.R.n：`.insn r 0x73, 0x4, funct7, rd, rs1, rs2`，其中 funct7=0x40 \| ((n>>4)<<5) \| (((n>>3)&1)<<2) \| (((n>>2)&1)<<1)（funct7 只编码 n[4:2]：MOP.R.0=0x40，MOP.R.16=0x60，MOP.R.28 与 MOP.R.31 均为 0x66，n[1:0] 区分于 bits[21:20]）；n[1:0] 编码在 bits[21:20]（rs2 字段位置低 2 位，高 3 位固定 0b111），即 rs2 位置的寄存器号必须为 0b111_n[1:0]（x28..x31），rs1 无约束；
   - MOP.RR.n：`.insn r 0x73, 0x4, funct7, rd, rs1, rs2`，其中 funct7=0x41 \| ((n>>2)<<5) \| (((n>>1)&1)<<2) \| ((n&1)<<1)（MOP.RR.0=0x41，MOP.RR.1=0x43，MOP.RR.7=0x67）；
   交叉验证：Zicfiss 的 SSPOPCHK x1=0xCDC0C073 即 mop.r.28（rd=x0, rs1=x1），SSPUSH x1=0xCE104073 即 mop.rr.7（rd=x0, rs1=x0, rs2=x1）。用例中统一通过宏生成全部 40 条指令字。
2. **扩展探测**：用例以 `config/*/rvtest_config.h` 中的 `ZIMOP_SUPPORTED` 宏为执行前提；未声明该宏的平台整体跳过本测试集（非用例失败）。
3. **语法依赖声明不设用例**：SPEC 中"do not carry a syntactic dependency from rs1/rs2 to rd"是对乱序实现的依赖追踪约束，软件不可观测；其可观测推论（结果与 rs1/rs2 的值无关）已由 MOPR-01..04 / MOPRR-01..02 覆盖。
4. **保留编码预期**：MOPENC-06 的非法指令预期基于 SYSTEM 编码空间中仅 CSR/特权指令与 MOP 编码已定义及基础 ISA 未定义指令规则；选取的保留位组合已避开 CSR 指令与 MOP 编码域，执行前须先与 SPEC 比对确认无其他扩展占用。
5. **Zicfiss 编码重叠处置**：MOP.R.28 与 MOP.RR.7 被 Zicfiss 部分重定义。全遍历用例在 Zicfiss 已实现且 SSE=1 的上下文中执行时，对被 Zicfiss 占用的位组合不要求写零结果（该行为归 `cfi_test_plan.md`）；其余位组合与未实现 Zicfiss 的平台一律要求写零。
6. **平台差异处置**：若任一平台（QEMU/Spike/Sail/whisper/硬件）出现写零语义错误、编码与 SPEC 不符、保留编码不触发异常或在合法特权级下拒绝执行，属违反 SPEC 的实现缺陷，用例保持 FAIL 并记录至 `bugs/` 目录，不做跳过或 workaround。

---

## 附录 A：规范点覆盖矩阵

| Norm ID | 覆盖用例 | 备注 |
|---------|----------|------|
| `norm:Zimop_mop-r_op` | MOPR-01 ~ MOPR-07, MOPSB-01 ~ MOPSB-05, MOPPR-01 ~ MOPPR-09 | |
| `norm:Zimop_mop-r_enc` | MOPENC-01, MOPENC-03, MOPENC-04 | |
| `norm:Zimop_mop-rr_op` | MOPRR-01 ~ MOPRR-05, MOPSB-01 ~ MOPSB-05, MOPPR-01 ~ MOPPR-09 | MOP.RR.7 与 Zicfiss 重叠位组合见实现说明 5 |
| `norm:Zimop_mop-rr_enc` | MOPENC-02, MOPENC-03, MOPENC-05 | |
| SYSTEM 空间未定义编码规则（基础 ISA） | MOPENC-06 | 非 Zimop norm 锚点，依据基础 ISA 未定义指令规则 |
| MOP 允许修改架构状态（SPEC NOTE，区别于 HINT） | MOPSB-05 | informational NOTE 的可观测推论 |
| 写零结果可用于特性探测分支（SPEC NOTE） | MOPSB-04 | informational NOTE 的功能验证 |
| 语法依赖声明（无 rs1/rs2→rd 依赖） | — | 微架构依赖追踪约束，软件不可观测；其可观测推论由 MOPR/MOPRR 语义用例覆盖 |
| 行为受特权 CSR 状态调制（SPEC NOTE） | — | 面向未来重定义扩展的预期，基线 Zimop 无 CSR 门控，反向断言由 MOPPR 组承担 |
| `norm:Zcmop_op` / `norm:Zcmop_enc` / `norm:Zcmop_instr_write` | — | 属 Zcmop 扩展，归 `zcmop_test_plan.md` |
