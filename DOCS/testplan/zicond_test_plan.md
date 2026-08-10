# Zicond 整数条件操作测试计划（Zicond v1.0）

## 概述

本测试计划覆盖 RISC-V Zicond（Integer Conditional Operations，v1.0）扩展，验证两条 R 型分支无关条件操作指令 `czero.eqz` 与 `czero.nez` 的指令编码、操作语义、寄存器操作数边界行为、各特权级可执行性，以及 SPEC 给出的条件运算指令序列的功能正确性。

本测试计划依据 `SPEC/riscv-isa-manual/src/unpriv/zicond.adoc` 中的规范点（norm 标记）编写，norm 定义与官方可测性标注参照 `SPEC/riscv-isa-manual/normative_rule_defs/zicond.yaml`。

### 本文档覆盖的 SPEC 章节
- Zicond Extension for Integer Conditional Operations（czero.eqz / czero.nez 指令定义、编码与语义）
- Usage examples 中的条件运算指令序列（informational，功能验证仅依赖 czero 指令的规范性语义）

### 范围说明
- Zicond 扩展仅定义两条指令，不涉及任何 CSR；指令在 RV32 与 RV64 下均存在，本框架主目标为 RV64（XLEN=64），RV32 平台（如 qemu-rv32-max）可复用同一语义用例。
- 各平台配置（`config/*/rvtest_config.h`）已定义 `ZICOND_SUPPORTED` / `ZICOND1P0_SUPPORTED`（或 `ZICOND1P0P0_SUPPORTED`）门控宏，用例以宏探测为前提条件。
- `norm:czero-eqz_inst_ctime` 与 `norm:czero-nez_zkt_timing`（Zkt 恒定时间要求）在官方 `zicond.yaml` 中标注为 "Untestable since timing behavior is a microarchitectural property"，本计划不设定量用例，仅在规范点表中记录。
- "指令对 rd 携带来自 rs1 与 rs2 的语法依赖"（syntactic dependency）是面向乱序实现的微架构设计约束，软件层面不可观测，不设用例。

### 由其他测试计划覆盖
- illegal-instruction exception 的 trap 递送、委托与 CSR 写入行为 → `Sm_Exceptions_test_plan.md` / `Ss_Exceptions_test_plan.md` / `Hypervisor_Exceptions_test_plan.md`
- Zkt/Zkr 恒定时间策略的整体指令集合约束 → `Zkr_test_plan.md`（恒定时间时序本身不设用例）
- 基础整数指令（add/sub/or/xor/and）自身的语义 → 基础 ISA 测试范围，本计划仅将其用于组合序列验证

---

## 覆盖的规范点

本章节列出本文档所有测试组中引用的规范点（norm ID），已去重并按字母顺序排列。

| Norm ID | 原文 | 中文说明 |
|---------|------|----------|
| `norm:czero-eqz_inst_ctime` | Furthermore, if the Zkt extension is implemented, this instruction's timing is independent of the data values in _rs1_ and _rs2_. | 若实现了 Zkt 扩展，czero.eqz 的执行时间与 rs1/rs2 的数据值无关。官方 yaml 标注为微架构属性，不可测试。 |
| `norm:czero-eqz_op` | If _rs2_ contains the value zero, this instruction writes the value zero to _rd_. Otherwise, this instruction copies the contents of _rs1_ to _rd_. | 若 rs2 的值为零，则向 rd 写入零；否则将 rs1 的内容拷贝到 rd。 |
| `norm:czero-nez_zkt_timing` | Furthermore, if the Zkt extension is implemented, this instruction's timing is independent of the data values in _rs1_ and _rs2_. | 若实现了 Zkt 扩展，czero.nez 的执行时间与 rs1/rs2 的数据值无关。官方 yaml 标注为微架构属性，不可测试。 |
| `norm:czero-nez_op` | If _rs2_ contains a nonzero value, this instruction writes the value zero to _rd_. Otherwise, this instruction copies the contents of _rs1_ to _rd_. | 若 rs2 的值非零，则向 rd 写入零；否则将 rs1 的内容拷贝到 rd。 |

**非 norm 标记的规范性描述**（编码表中的可测要求）：
- czero.eqz 编码：opcode=`0110011`(OP, 0x33)、funct3=`101`(0x5)、funct7=`0000111`(0x7)。
- czero.nez 编码：opcode=`0110011`(OP, 0x33)、funct3=`111`(0x7)、funct7=`0000111`(0x7)。
- 该编码空间（opcode=0x33 且 funct7=0x7）中仅定义了上述两条指令，其余 funct3 编码未定义，执行应触发 illegal-instruction exception（基础 ISA 未定义指令规则）。

---

## Group 1. 指令编码验证

**规范依据**：
- czero.eqz 编码表：opcode=0x33(OP)、funct3=0x5、funct7=0x7(CZERO)
- czero.nez 编码表：opcode=0x33(OP)、funct3=0x7、funct7=0x7(CZERO)
- `norm:czero-eqz_op` / `norm:czero-nez_op`：按正确编码执行的指令应实现对应语义

**测试职责**：验证两条指令的机器码字段与 SPEC 编码表完全一致，汇编助记符与手工构造编码一致，保留编码触发非法指令异常。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZCOND-01 | czero.eqz 指令字编码验证 | 取 czero.eqz 指令的 32 位机器码（汇编器生成或 `.insn r` 构造），逐字段比对 | opcode=0110011(0x33)，funct3=101(0x5)，funct7=0000111(0x7)，rd/rs1/rs2 位于标准 R 型字段位置 |
| ZCOND-02 | czero.nez 指令字编码验证 | 取 czero.nez 指令的 32 位机器码，逐字段比对 | opcode=0110011(0x33)，funct3=111(0x7)，funct7=0000111(0x7) |
| ZCOND-03 | 助记符编码与手工构造一致 | 比对汇编器对 `czero.eqz`/`czero.nez` 助记符生成的编码与 `.insn r 0x33, f3, 0x7, rd, rs1, rs2` 构造值 | 两种形式编码完全一致 |
| ZCOND-04 | 保留编码触发非法指令异常 | 构造 opcode=0x33、funct7=0x7、funct3 分别为 0x0/0x1/0x2/0x3/0x4/0x6 的指令字并执行（trap-armed） | 均触发 illegal-instruction exception (cause=2) |

---

## Group 2. czero.eqz 操作语义

**规范依据**：
- `norm:czero-eqz_op`：rs2=0 时 rd 写零，否则 rd 拷贝 rs1

**测试职责**：验证 czero.eqz 的条件判断与数据搬运语义，覆盖零/非零条件的全部判定路径与典型边界数据。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZCOND-05 | rs2=0 时 rd 写零 | rs1=非零模式值（如 0xA5A5A5A5A5A5A5A5），rs2=0，执行 czero.eqz | rd=0 |
| ZCOND-06 | rs2=1 时 rd 拷贝 rs1 | rs1=模式值，rs2=1，执行 czero.eqz | rd=rs1 原值 |
| ZCOND-07 | rs2=-1 时 rd 拷贝 rs1 | rs1=模式值，rs2=0xFFFFFFFFFFFFFFFF，执行 czero.eqz | rd=rs1 原值（非零判定成立） |
| ZCOND-08 | rs2 仅最高位非零 | rs2=0x8000000000000000，rs1=模式值，执行 czero.eqz | rd=rs1（条件按完整 XLEN 值判断，非符号判断） |
| ZCOND-09 | rs2 仅最低位非零 | rs2=0x1，rs1=模式值（与 ZCOND-06 使用不同模式数据） | rd=rs1 |
| ZCOND-10 | rs1=0 且 rs2≠0 时拷贝零 | rs1=0，rs2=非零，执行 czero.eqz | rd=0（拷贝 rs1 的零值，与条件清零结果一致） |
| ZCOND-11 | rs1=全 1 数据完整搬运 | rs1=0xFFFFFFFFFFFFFFFF，rs2≠0，执行 czero.eqz | rd=0xFFFFFFFFFFFFFFFF（无截断、无符号扩展行为） |
| ZCOND-12 | rs1=rs2=0 | rs1=0，rs2=0，执行 czero.eqz | rd=0 |

---

## Group 3. czero.nez 操作语义

**规范依据**：
- `norm:czero-nez_op`：rs2≠0 时 rd 写零，否则 rd 拷贝 rs1

**测试职责**：验证 czero.nez 的条件判断与数据搬运语义，与 czero.eqz 语义互补，覆盖全部判定路径与边界数据。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZCOND-13 | rs2≠0 时 rd 写零 | rs1=非零模式值，rs2=1，执行 czero.nez | rd=0 |
| ZCOND-14 | rs2=0 时 rd 拷贝 rs1 | rs1=模式值，rs2=0，执行 czero.nez | rd=rs1 原值 |
| ZCOND-15 | rs2=-1 时 rd 写零 | rs1=模式值，rs2=0xFFFFFFFFFFFFFFFF，执行 czero.nez | rd=0 |
| ZCOND-16 | rs2 仅最高位非零 | rs2=0x8000000000000000，rs1=模式值，执行 czero.nez | rd=0（非零判定成立） |
| ZCOND-17 | rs1=0 且 rs2=0 时拷贝零 | rs1=0，rs2=0，执行 czero.nez | rd=0（拷贝 rs1 的零值） |
| ZCOND-18 | rs1=全 1 数据完整搬运 | rs1=0xFFFFFFFFFFFFFFFF，rs2=0，执行 czero.nez | rd=0xFFFFFFFFFFFFFFFF（无截断、无符号扩展行为） |

---

## Group 4. 寄存器操作数边界与架构状态影响

**规范依据**：
- `norm:czero-eqz_op` / `norm:czero-nez_op`：指令仅写入 rd，语义对任意寄存器编号成立
- 基础 ISA：x0 恒为零，写入 x0 无效果

**测试职责**：验证特殊寄存器编号组合（rd=x0、操作数重叠）下的行为正确，且指令不产生其他架构可见副作用。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZCOND-19 | rd=x0 写入无效果 | rd=x0，rs1=非零，rs2≠0（应拷贝 rs1），执行 czero.eqz | 无 trap，x0 读回恒为 0，程序继续执行 |
| ZCOND-20 | rd=rs1=rs2 同寄存器（eqz） | 三操作数同为 xN：xN=非零时执行 czero.eqz xN,xN,xN | rd=rs1（非零条件成立，拷贝自身），xN 保持原值；xN=0 时结果为 0 |
| ZCOND-21 | rd=rs1=rs2 同寄存器（nez） | 三操作数同为 xN：xN=非零时执行 czero.nez xN,xN,xN | rd=0（非零条件成立，清零自身）；xN=0 时结果为 0（拷贝零值） |
| ZCOND-22 | rd=rs1≠rs2 | rd 与 rs1 相同，rs2 独立，eqz/nez 两种条件各执行一次 | 结果符合语义，覆盖写 rd 不影响条件判断读取的 rs2 |
| ZCOND-23 | rd=rs2≠rs1 | rd 与 rs2 相同，rs1 独立，eqz/nez 两种条件各执行一次 | 结果按执行前 rs2 的原始值判断（先读条件后写 rd） |
| ZCOND-24 | 不修改其他架构状态 | 执行前将可保存通用寄存器置为已知模式值，执行 czero.eqz/czero.nez 后逐一比对，并读取 mstatus/mcause/mtval 等 CSR | 除 rd 外所有寄存器与 CSR 值不变 |
| ZCOND-25 | PC 正确前进 | 单条 czero 指令后紧跟可观测指令（写内存/寄存器标记） | 指令正常退休，PC 前进 4 字节，标记正确写入 |

---

## Group 5. 各特权级可执行性

**规范依据**：
- `norm:czero-eqz_op` / `norm:czero-nez_op`：czero 为普通整数指令，无任何特权级限制或 CSR 门控，应在所有特权级正常执行

**测试职责**：验证两条指令在 M/S/U 及（若实现 H 扩展）HS/VS/VU 各特权级下均可正常执行，不受状态使能门控。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZCOND-26 | M-mode 执行 czero | M-mode 执行 czero.eqz 与 czero.nez 各一次并验证结果 | 正常执行，无 trap，结果正确 |
| ZCOND-27 | S-mode 执行 czero | 切换至 S-mode 执行两条指令 | 正常执行，无异常，结果正确 |
| ZCOND-28 | U-mode 执行 czero | 切换至 U-mode 执行两条指令 | 正常执行，无异常，结果正确 |
| ZCOND-29 | HS-mode 执行 czero（H 扩展） | 实现 H 扩展时，HS-mode 执行两条指令 | 正常执行，无异常 |
| ZCOND-30 | VS-mode 执行 czero（H 扩展） | 实现 H 扩展时，切入 VS-mode 执行两条指令 | 正常执行，无 virtual-instruction exception |
| ZCOND-31 | VU-mode 执行 czero（H 扩展） | 实现 H 扩展时，切入 VU-mode 执行两条指令 | 正常执行，无异常 |
| ZCOND-32 | Smstateen 不门控 czero | 若实现 Smstateen，清零 mstateen0/hstateen0/sstateen0 后在各特权级执行两条指令 | 正常执行（基础整数指令不受 stateen 门控） |

---

## Group 6. SPEC 条件运算指令序列功能验证

**规范依据**：
- `norm:czero-eqz_op` / `norm:czero-nez_op`：序列功能正确性的基础（序列本身为 SPEC informational 示例）

**测试职责**：按 SPEC Usage examples 给出的指令序列构造条件运算，验证序列在条件成立与不成立两种情形下均产生预期结果。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZCOND-33 | 条件加法序列 | 按 `czero.nez/eqz + add` 序列实现 `rd = (rc==0/rc!=0) ? (rs1+rs2) : rs1`，rc=0 与 rc≠0 各测一次 | 条件成立时 rd=rs1+rs2，不成立时 rd=rs1 |
| ZCOND-34 | 条件减法序列 | 按 `czero.nez/eqz + sub` 序列实现条件减法，rc=0 与 rc≠0 各测一次 | 条件成立时 rd=rs1-rs2，不成立时 rd=rs1 |
| ZCOND-35 | 条件按位或序列 | 按 `czero.nez/eqz + or` 序列实现条件 OR，rc=0 与 rc≠0 各测一次 | 条件成立时 rd=rs1\|rs2，不成立时 rd=rs1 |
| ZCOND-36 | 条件按位异或序列 | 按 `czero.nez/eqz + xor` 序列实现条件 XOR，rc=0 与 rc≠0 各测一次 | 条件成立时 rd=rs1^rs2，不成立时 rd=rs1 |
| ZCOND-37 | 条件按位与序列（需临时寄存器） | 按 `and + czero.eqz/nez + or` 三指令序列实现条件 AND，rc=0 与 rc≠0 各测一次 | 条件成立时 rd=rs1&rs2，不成立时 rd=rs1 |
| ZCOND-38 | 条件选择序列（if zero） | 按 `czero.nez + czero.eqz + add` 序列实现 `rd = (rc==0) ? rs1 : rs2` | rc=0 时 rd=rs1，rc≠0 时 rd=rs2 |
| ZCOND-39 | 条件选择序列（if non-zero） | 按 `czero.eqz + czero.nez + add` 序列实现 `rd = (rc!=0) ? rs1 : rs2` | rc≠0 时 rd=rs1，rc=0 时 rd=rs2 |

---

## 测试实现说明

1. **指令构造**：若工具链（`riscv64-unknown-linux-elf-gcc` 的 `-march` 含 `zicond1p0`）支持助记符则直接使用；否则用 `.insn r 0x33, 0x5, 0x7, rd, rs1, rs2`（eqz）与 `.insn r 0x33, 0x7, 0x7, rd, rs1, rs2`（nez）构造，两种形式的一致性由 ZCOND-03 验证。
2. **扩展探测**：用例以 `config/*/rvtest_config.h` 中的 `ZICOND_SUPPORTED` 宏为执行前提；未声明该宏的平台整体跳过本测试集（非用例失败）。
3. **时序规范点不设用例**：`norm:czero-eqz_inst_ctime` 与 `norm:czero-nez_zkt_timing` 的恒定时间要求属微架构属性（官方 `zicond.yaml` 明确标注 Untestable），不做定量断言。
4. **语法依赖声明不设用例**：SPEC 中 "syntactic dependency from both rs1 and rs2 to rd" 是对乱序实现的依赖追踪约束，软件不可观测。
5. **保留编码预期**：ZCOND-04 的非法指令预期基于编码表仅定义两条指令及基础 ISA 未定义指令规则；若某平台有扩展占用同一编码空间（当前无已知冲突），需先与 SPEC 比对确认。
6. **平台差异处置**：若任一平台（QEMU/Spike/Sail/whisper/硬件）出现语义错误、保留编码不触发异常或在合法特权级下拒绝执行，属违反 SPEC 的实现缺陷，用例保持 FAIL 并记录至 `bugs/` 目录，不做跳过或 workaround。
