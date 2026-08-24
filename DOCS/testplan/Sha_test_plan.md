**中文 | [English](../testplan_en/Sha_test_plan_en.md)**

# Sha 扩展测试计划

本文档描述 Sha（Augmented Hypervisor Extension）扩展的测试计划。Sha 是一个 profile 级别的组合扩展，将 H 扩展所要求的全部子特性聚合为一个整体，包含以下 8 个子扩展：

1. **H** — Hypervisor 扩展
2. **Ssstateen** — Supervisor-mode 视图的状态使能扩展（sstateen0-3 与 hstateen0-3 必须提供）
3. **Shcounterenw** — 对于非只读零的 hpmcounter，hcounteren 对应位必须可写
4. **Shvstvala** — vstval 必须在所有 stval 定义的场景下被写入
5. **Shtvala** — htval 必须在 ISA 允许的所有情况下写入错误 guest 物理地址
6. **Shvstvecd** — vstvec.MODE 必须能持有 Direct(0)；MODE=Direct 时 BASE 能持有任意 4 字节对齐地址
7. **Shvsatpa** — satp 支持的所有翻译模式在 vsatp 中也必须被支持
8. **Shgatpa** — satp 中支持的 SvNN 对应的 hgatp SvNNx4 必须被支持；hgatp Bare 也必须被支持

---

## 测试范围

### 本文档覆盖的 SPEC 章节

本方案依据 RISC-V Privileged Architecture 规范（Hypervisor 扩展及其 Sh* 子扩展章节、Smstateen 扩展章节）编写：

- 本地 SPEC 路径：
  - `SPEC/riscv-isa-manual/src/priv/sha.adoc` — Sha Augmented Hypervisor Extension 定义
  - `SPEC/riscv-isa-manual/src/priv/hypervisor.adoc` — H 扩展规范（vsatp/vstvec/vstval/hgatp/hcounteren 等寄存器定义）
  - `SPEC/riscv-isa-manual/src/priv/smstateen.adoc` — Smstateen/Ssstateen 扩展规范（sstateen/hstateen CSR 定义与行为）
  - `SPEC/riscv-isa-manual/src/priv/shcounterenw.adoc` — Shcounterenw 规范
  - `SPEC/riscv-isa-manual/src/priv/shvstvala.adoc` — Shvstvala 规范
  - `SPEC/riscv-isa-manual/src/priv/shtvala.adoc` — Shtvala 规范
  - `SPEC/riscv-isa-manual/src/priv/shvstvecd.adoc` — Shvstvecd 规范
  - `SPEC/riscv-isa-manual/src/priv/shvsatpa.adoc` — Shvsatpa 规范
  - `SPEC/riscv-isa-manual/src/priv/shgatpa.adoc` — Shgatpa 规范
  - `SPEC/riscv-isa-manual/src/priv/supervisor.adoc` — satp/stvec/stval 等基线寄存器定义
- 官方 GitHub 仓库：https://github.com/riscv/riscv-isa-manual （按 `.gitmodules` 中 `SPEC/riscv-isa-manual` 映射）

### 关键参考文件

| 路径 | 说明 |
|------|------|
| `sha.adoc` | Sha 组合扩展定义（列出 8 个子扩展依赖） |
| `smstateen.adoc:46-183` | Ssstateen 规范：sstateen0-3/hstateen0-3 CSR 定义与层次控制 |
| `hypervisor.adoc:986-1089` | hgatp 寄存器规范 |
| `hypervisor.adoc:1382-1425` | vsatp 寄存器规范 |
| `hypervisor.adoc:1300-1312` | vstvec 寄存器规范 |
| `hypervisor.adoc:1364-1380` | vstval 寄存器规范 |
| `hypervisor.adoc:846-877` | hcounteren 寄存器规范 |
| `common/encoding.h` | CSR 地址定义 |
| `common/hyp/hyp_defs.h` | MAKE_HGATP 等宏定义 |
| `common/hyp/hyp_priv.h` | run_in_vs_mode / run_in_vu_mode |
| `common/hyp/hyp_csr.h` | hcounteren_read/write、hyp_delegate_to_vs 等 |
| `common/hyp/hyp_test.h` | HYP_TEST_END 等测试宏 |
| `DOCS/testplan/shstvecd_test_plan.md` | Shvstvecd 子扩展详细测试方案 |
| `DOCS/testplan/shcounterenw_test_plan.md` | Shcounterenw 子扩展详细测试方案 |
| `DOCS/testplan/shvstvala_test_plan.md` | Shvstvala 子扩展详细测试方案 |
| `DOCS/testplan/shtvala_test_plan.md` | Shtvala 子扩展详细测试方案 |
| `DOCS/testplan/shvsatpa_test_plan.md` | Shvsatpa 子扩展详细测试方案 |
| `DOCS/testplan/shgatpa_test_plan.md` | Shgatpa 子扩展详细测试方案 |

### 覆盖的规范点

| Norm ID | 原文 | 中文说明 |
|---------|------|----------|
| `norm:shvstvecd_vstvec_mode_direct` | If the Shvstvecd extension is implemented, then `vstvec.MODE` must be capable of holding the value 0 (Direct). | 若实现了 Shvstvecd 扩展，`vstvec.MODE` 必须能够保持值 0（Direct）。 |
| `norm:shvstvecd_vstvec_base_aligned_address` | Furthermore, when `vstvec.MODE`=Direct, `vstvec.BASE` must be capable of holding any valid four-byte-aligned address. | 此外，当 `vstvec.MODE`=Direct 时，`vstvec.BASE` 必须能够保持任何有效的四字节对齐地址。 |
| `norm:shcounterenw_hpmcounter_hcounteren` | If the Shcounterenw extension is implemented, then for any `hpmcounter` that is not read-only zero, the corresponding bit in `hcounteren` must be writable. | 若实现了 Shcounterenw 扩展，则对于任何非只读零的 `hpmcounter`，`hcounteren` 中的对应位必须可写。 |
| `norm:shvstvala_vstval_written` | If the Shvstvala extension is implemented, `vstval` must be written in all cases described for `stval`. | 若实现了 Shvstvala 扩展，`vstval` 必须在所有为 `stval` 描述的情况下被写入。 |
| `norm:shtvala_htval_faulting_gpa` | If the Shtvala extension is implemented, `htval` must be written with the faulting guest physical address in all circumstances permitted by the ISA. | 若实现了 Shtvala 扩展，在 ISA 允许的所有情况下，`htval` 必须写入故障的客户物理地址。 |
| `norm:shvsatpa_satp_vsatp_modes` | If the Shvsatpa extension is implemented, all translation modes supported in `satp` must be supported in `vsatp`. | 若实现了 Shvsatpa 扩展，`satp` 中支持的所有翻译模式在 `vsatp` 中也必须支持。 |
| `norm:shgatpa_satp_hgatp_mode_support` | If the Shgatpa extension is implemented, then for each supported virtual memory scheme Sv_NN_ supported in `satp`, the corresponding hgatp Sv_NN_x4 mode must be supported. | 若实现了 Shgatpa 扩展，则 `satp` 中支持的每种虚拟内存方案 Sv_NN_，`hgatp` 中对应的 Sv_NN_x4 模式也必须支持。 |
| `norm:shgatpa_hgatp_bare_mode` | Furthermore, the `hgatp` mode Bare must also be supported. | 此外，`hgatp` 的 Bare 模式也必须支持。 |
| `norm:sstateen_user_access_control` | Each bit of a supervisor-level `sstateen` CSR controls user-level access (from U-mode or VU-mode) to an extension's state. | 监管级 `sstateen` CSR 的每一位控制用户级（来自 U 模式或 VU 模式）对扩展状态的访问。 |
| `norm:hstateen_encoding` | With the hypervisor extension, the `hstateen` CSRs have identical encodings to the `mstateen` CSRs, except controlling accesses for a virtual machine (from VS and VU modes). | 使用 hypervisor 扩展时，`hstateen` CSR 具有与 `mstateen` CSR 相同的编码，只是控制虚拟机（来自 VS 和 VU 模式）的访问。 |
| `norm:stateen_op` | The `stateen` registers at each level control access to state at all less-privileged levels, but not at its own level. | 各级 `stateen` 寄存器控制对所有较低特权级别的状态访问，但不控制其自身级别。 |
| `norm:stateen_illegal_state_access` | Just as with the `counteren` CSRs, when a `stateen` CSR prevents access to state by less-privileged levels, an attempt in one of those privilege modes to execute an instruction that would read or write the protected state raises an illegal-instruction exception, or, if executing in VS or VU mode and the circumstances for a virtual-instruction exception apply, raises a virtual-instruction exception instead of an illegal-instruction exception. | 与 `counteren` CSR 类似，当 `stateen` CSR 阻止较低特权级别访问状态时，在这些特权模式下尝试执行读取或写入受保护状态的指令会触发非法指令异常，或者如果在 VS 或 VU 模式下执行且满足虚拟指令异常的条件，则触发虚拟指令异常而非非法指令异常。 |
| `norm:hstateen_bit_63_writable` | Bit 63 of each `hstateen` CSR is always writable (not read-only). | 每个 `hstateen` CSR 的位 63 始终可写（非只读）。 |
| `norm:mstateen_zero_initialization` | On reset, all writable `mstateen` bits are initialized by the hardware to zeros. | 复位时，所有可写的 `mstateen` 位由硬件初始化为零。 |
| `norm:hstateen_sstateen_zero_initialization` | If machine-level software changes these values, it is responsible for initializing the corresponding writable bits of the `hstateen` and `sstateen` CSRs to zeros too. | 如果机器级软件更改这些值，它负责将 `hstateen` 和 `sstateen` CSR 的相应可写位也初始化为零。 |
| `norm:sstateen_rv64_csrs` | If supervisor mode is implemented, another four CSRs are defined at supervisor level: `sstateen0`, `sstateen1`, `sstateen2`, and `sstateen3`. | 若实现了 supervisor 模式，在 supervisor 级定义 sstateen0-3 四个 CSR。 |
| `norm:hstateen_rv64_csrs` | And if the hypervisor extension is implemented, another set of CSRs is added: `hstateen0`, `hstateen1`, `hstateen2`, and `hstateen3`. | 若实现了 hypervisor 扩展，额外增加 hstateen0-3 一组 CSR。 |
| `norm:stateen_warl_access` | Each standard-defined bit of a `stateen` CSR is WARL and may be read-only zero or one, subject to the following conditions. | `stateen` CSR 的每个标准定义位为 WARL，可能是只读零或只读一，受后续条件约束。 |
| `norm:stateen_reserved_roz` | Likewise, all reserved bits not yet given a defined meaning are also read-only zeros. | 所有尚未赋予定义含义的保留位同样为只读零。 |
| `norm:mstateen_lower_priv_roz` | For every bit in an `mstateen` CSR that is zero (whether read-only zero or set to zero), the same bit appears as read-only zero in the matching `hstateen` and `sstateen` CSRs. | `mstateen` 中为零的每一位（无论只读零还是被置零），在匹配的 `hstateen` 和 `sstateen` 中表现为只读零。 |
| `norm:sstateen_vsmode_access_roz` | For every bit in an `hstateen` CSR that is zero (whether read-only zero or set to zero), the same bit appears as read-only zero in `sstateen` when accessed in VS-mode. | `hstateen` 中为零的每一位，在 VS-mode 访问 `sstateen` 时对应位表现为只读零。 |
| `norm:mstateen_bit_63_op` | For each `mstateen` CSR, bit 63 is defined to control access to the matching `sstateen` and `hstateen` CSRs. | 每个 `mstateen` CSR 的 bit 63 控制对匹配的 `sstateen` 和 `hstateen` CSR 的访问。 |
| `norm:hstateen_bit_63_op` | Likewise, bit 63 of each `hstateen` correspondingly controls access to the matching `sstateen` CSR. | 同样，每个 `hstateen` 的 bit 63 相应控制对匹配的 `sstateen` CSR 的访问。 |
| `HSyncExcPrio` | Synchronous exception priority table: when multiple synchronous exceptions could be taken, the one listed earlier (e.g. instruction guest-page fault over VS-stage page fault) takes priority. | hypervisor.adoc 中 `[[HSyncExcPrio]]` 同步异常优先级表：G-stage fault 优先于 VS-stage fault。 |

> [!IMPORTANT]
> Sha 作为组合扩展，其各子扩展均有独立的详细测试方案（见上述参考文件）。本测试计划的定位是：(1) 验证所有子扩展**同时存在**且功能正常（集成级存在性测试）；(2) 验证 Ssstateen 扩展在 H 扩展上下文中的功能（此扩展暂无独立测试方案）；(3) 验证子扩展间的**交互行为**（如 hstateen 禁止时对 vstvec/vstval/vsatp 访问的影响）。

### 不在测试范围内

- **各子扩展的完整功能测试**：由各自独立测试方案覆盖（shstvecd_test_plan.md 等）
- **H 扩展的基础功能**：如 hedeleg/hideleg 委托、V 模式切换等，由 hyp 基础测试覆盖
- **M-mode stateen（mstateen）的完整测试**：Sha 仅要求 Ssstateen（sstateen + hstateen）
- **多 hart 场景**：项目为单核测试环境
- **HSXLEN=32 / SXLEN=32 场景**：仅覆盖 RV64 环境
- **Smstateen 相对于 Ssstateen 的额外特性**：mstateen 的特定行为不在 Sha 范围内

---

## 设计要点

### 1. Sha 测试的分层策略

Sha 作为组合扩展，测试分为两层：

- **第一层：存在性验证** — 确认每个子扩展的核心特征在平台上**同时可用**（不重复完整测试，只做"烟雾测试"级别的快速确认）
- **第二层：Ssstateen 功能验证** — Ssstateen 是 Sha 中唯一没有独立 `Sh*` 测试方案的子扩展，需在此计划中详细验证
- **第三层：跨扩展交互** — 验证 hstateen 对其他 Sh* 子扩展 CSR 的访问控制效果

### 2. Ssstateen 在 H 扩展上下文中的行为

Ssstateen 要求 sstateen0-3 和 hstateen0-3 均存在。关键行为：

- `hstateen` 控制 VS/VU 模式对状态的访问（`norm:hstateen_encoding`）
- 当 `hstateen` 某位为 0 时，VS-mode 中对应的 `sstateen` 位强制为只读零（`norm:sstateen_vsmode_access_roz`）
- bit 63 特殊：`hstateen0` bit 63 控制 VS/VU 对 `sstateen0` 的访问（`norm:hstateen_bit_63_op`）
- `hstateen` bit 63 始终可写（`norm:hstateen_bit_63_writable`）

### 3. CSR 地址

| CSR | 地址 | 说明 |
|-----|------|------|
| sstateen0 | 0x10C | Supervisor state enable 0 |
| sstateen1 | 0x10D | Supervisor state enable 1 |
| sstateen2 | 0x10E | Supervisor state enable 2 |
| sstateen3 | 0x10F | Supervisor state enable 3 |
| hstateen0 | 0x60C | Hypervisor state enable 0 |
| hstateen1 | 0x60D | Hypervisor state enable 1 |
| hstateen2 | 0x60E | Hypervisor state enable 2 |
| hstateen3 | 0x60F | Hypervisor state enable 3 |
| vstvec | 0x205 | Virtual supervisor trap vector |
| vstval | 0x243 | Virtual supervisor trap value |
| vsatp | 0x280 | Virtual supervisor address translation |
| hgatp | 0x680 | Hypervisor guest address translation |
| hcounteren | 0x606 | Hypervisor counter enable |

### 4. 子扩展存在性快速验证方法

| 子扩展 | 快速验证方法 |
|--------|-------------|
| H | 读写 hstatus（CSR 0x600）不触发异常 |
| Ssstateen | 读写 hstateen0（CSR 0x60C）和 sstateen0（CSR 0x10C）不触发异常 |
| Shcounterenw | hcounteren 的 CY 位（bit 0）可写（cycle 计数器通常非只读零） |
| Shvstvala | 在 VS-mode 触发 page fault 后 vstval 非零 |
| Shtvala | 在 G-stage 触发 guest page fault 后 htval 非零 |
| Shvstvecd | vstvec.MODE 写 0 后回读为 0 |
| Shvsatpa | satp 支持的 MODE 写入 vsatp 后回读一致 |
| Shgatpa | hgatp Bare 写入后回读 MODE=0；satp 支持的 SvNN 对应 hgatp SvNNx4 可持有 |

---

## 测试分组

> [!IMPORTANT]
> 共 6 个测试组、28 个测试用例。Group 1 验证所有子扩展的存在性（烟雾测试）；Group 2 验证 Ssstateen CSR 的基本可访问性与 WARL 行为；Group 3 验证 hstateen 对 VS/VU 模式的访问控制；Group 4 验证 hstateen bit 63 对 sstateen 的门控行为；Group 5 验证 hstateen 与其他 Sh* 子扩展 CSR 的交互；Group 6 验证两阶段翻译中的异常优先级。每组提供：规范依据、测试职责、测试用例表（ID/名称/描述/预期结果）。

---

### Group 1：子扩展存在性验证（集成烟雾测试）

**规范依据**：
- `sha.adoc:4-14`：Sha 依赖 H + Ssstateen + Shcounterenw + Shvstvala + Shtvala + Shvstvecd + Shvsatpa + Shgatpa

**测试职责**：快速验证每个子扩展的核心特征在平台上可用，确认 Sha 组合扩展的完整性。每个子扩展做一个最小化的"能力存在"断言。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| SHA-EXIST-01 | H 扩展存在 | M-mode 读 hstatus（CSR 0x600），不触发异常 | 读取成功，hstatus 可访问 |
| SHA-EXIST-02 | Ssstateen 存在（hstateen0） | M-mode 读写 hstateen0（CSR 0x60C），不触发异常 | 读写成功 |
| SHA-EXIST-03 | Ssstateen 存在（sstateen0） | HS-mode 读写 sstateen0（CSR 0x10C），不触发异常 | 读写成功 |
| SHA-EXIST-04 | Shcounterenw 存在 | M-mode 写 hcounteren bit 0 (CY)=1 后回读=1 | hcounteren.CY 可写 |
| SHA-EXIST-05 | Shvstvecd 存在 | M-mode 写 vstvec=0（MODE=Direct），回读 MODE=0 | vstvec.MODE 持有 Direct |
| SHA-EXIST-06 | Shvsatpa 存在 | 探测 satp 支持的一个 MODE，写入 vsatp 同 MODE 后回读一致 | vsatp.MODE == 写入值 |
| SHA-EXIST-07 | Shgatpa 存在（Bare） | M-mode 写 hgatp=0（Bare），回读 MODE=0 | hgatp Bare 可持有 |
| SHA-EXIST-08 | Shgatpa 存在（SvNNx4） | 对 satp 支持的 SvNN，写 hgatp 对应 SvNNx4，回读 MODE 一致 | hgatp 支持对应 SvNNx4 |

> [!NOTE]
> Shvstvala 和 Shtvala 的存在性验证需要触发 trap，放在 Group 5 的交互测试中覆盖（涉及 hstateen 交互），此处不重复设置 trap 环境。

---

### Group 2：Ssstateen CSR 基本可访问性与 WARL 行为

**规范依据**：
- `norm:sstateen_rv64_csrs`（`smstateen.adoc:56-59`）：sstateen0-3 存在
- `norm:hstateen_rv64_csrs`（`smstateen.adoc:61-63`）：hstateen0-3 存在
- `norm:stateen_warl_access`（`smstateen.adoc:134-136`）：stateen 各位为 WARL
- `norm:stateen_reserved_roz`（`smstateen.adoc:139-140`）：保留位为只读零
- `norm:mstateen_zero_initialization`（`smstateen.adoc:153`）：复位时 mstateen 可写位为零
- `norm:hstateen_bit_63_writable`（`smstateen.adoc:182-183`）：hstateen bit 63 始终可写

**测试职责**：验证 sstateen0-3 和 hstateen0-3 共 8 个 CSR 全部可访问；验证 hstateen bit 63 可写；验证保留位为只读零行为。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| SHA-STATEEN-01 | hstateen0-3 可读 | 依次读 CSR 0x60C-0x60F，均不触发异常 | 4 个 CSR 全部可访问 |
| SHA-STATEEN-02 | sstateen0-3 可读 | 依次读 CSR 0x10C-0x10F，均不触发异常 | 4 个 CSR 全部可访问 |
| SHA-STATEEN-03 | hstateen0 bit 63 可写 | 写 hstateen0 bit 63=1 后回读；写 0 后回读 | bit 63 跟随写入值翻转 |
| SHA-STATEEN-04 | hstateen1/2/3 bit 63 可写 | 对 hstateen1-3 分别写 bit 63=1 再写 0，验证翻转 | 每个 CSR 的 bit 63 均可写 |
| SHA-STATEEN-05 | hstateen0 保留位只读零 | 写 hstateen0 全 1（0xFFFFFFFFFFFFFFFF），回读检查保留位 | 已定义位可能为 1，保留位（未定义用途的位）回读为 0 |

> [!NOTE]
> `norm:stateen_warl_access` 规定 stateen 各位为 WARL。已定义位（如 bit 63、bit 0 C 位等）按功能可写或只读零；未定义/保留位为只读零。SHA-STATEEN-05 通过全 1 写入来探测哪些位被实现。

---

### Group 3：hstateen 对 VS/VU 模式的访问控制

**规范依据**：
- `norm:hstateen_encoding`（`smstateen.adoc:129-132`）：hstateen 控制 VS/VU 模式对状态的访问
- `norm:stateen_illegal_state_access`（`smstateen.adoc:89-95`）：stateen 禁止时访问触发 illegal-instruction 或 virtual-instruction 异常
- `norm:sstateen_vsmode_access_roz`（`smstateen.adoc:144-146`）：hstateen 位为 0 时，VS-mode 访问对应 sstateen 位表现为只读零

**测试职责**：验证 hstateen0 bit 63=0 时，VS-mode 访问 sstateen0 触发 virtual-instruction exception；bit 63=1 时 VS-mode 可正常访问 sstateen0。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| SHA-HCTL-01 | hstateen0 bit63=0 → VS 访问 sstateen0 触发 virtual-inst | 设 hstateen0 bit63=0；VS-mode `csrr sstateen0` | 触发 virtual-instruction exception (cause=22) |
| SHA-HCTL-02 | hstateen0 bit63=1 → VS 可访问 sstateen0 | 设 hstateen0 bit63=1；VS-mode `csrr sstateen0` | 正常读取，无异常 |
| SHA-HCTL-03 | hstateen0 bit63=0 → VS 写 sstateen0 触发 virtual-inst | 设 hstateen0 bit63=0；VS-mode `csrw sstateen0, val` | 触发 virtual-instruction exception (cause=22) |
| SHA-HCTL-04 | hstateen0 bit63=0 对 sstateen0 读表现为只读零 | 设 hstateen0 bit63=0；HS-mode 检查 VS-mode 视图的 sstateen0 | 从 VS-mode 角度 sstateen0 表现为只读零 |
| SHA-HCTL-05 | hstateen0 bit59 门控 AIA CSR 访问 | 若 AIA（Ssaia）已实现：设 hstateen0 bit59=0；VS-mode 访问 siselect/sireg | 触发 virtual-instruction exception (cause=22)；若 AIA 未实现则 `TEST_SKIP` |

> [!NOTE]
> `norm:stateen_illegal_state_access` 指出：在 VS/VU 模式下，如果 hstateen 禁止了对某状态的访问，且满足 virtual-instruction exception 的条件（V=1 且 hstateen 位为 0 而 mstateen 位为 1），则触发 virtual-instruction exception（cause=22）而非 illegal-instruction。

---

### Group 4：hstateen bit 63 对 sstateen 的层次门控

**规范依据**：
- `norm:mstateen_bit_63_op`（`smstateen.adoc:162-164`）：mstateen0 bit 63 控制对 sstateen0 和 hstateen0 的访问
- `norm:hstateen_bit_63_op`（`smstateen.adoc:165-166`）：hstateen0 bit 63 控制对 VS-mode 看到的 sstateen0 的访问
- `norm:mstateen_lower_priv_roz`（`smstateen.adoc:141-143`）：mstateen 位为 0 时，对应 hstateen 和 sstateen 位表现为只读零

**测试职责**：验证 mstateen0 → hstateen0 → sstateen0(VS) 的层次门控链路。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| SHA-HIER-01 | mstateen0 bit63=1 → hstateen0 bit63 可写 | 设 mstateen0 bit63=1；写 hstateen0 bit63=1 后回读 | hstateen0 bit63 == 1 |
| SHA-HIER-02 | mstateen0 bit63=0 → hstateen0 bit63 只读零 | 设 mstateen0 bit63=0；读 hstateen0 bit63 | hstateen0 bit63 == 0（只读零） |
| SHA-HIER-03 | mstateen0 bit63=0 → HS 访问 hstateen0 异常 | 设 mstateen0 bit63=0；HS-mode 访问 hstateen0 | 触发 illegal-instruction (cause=2) |

> [!NOTE]
> SHA-HIER-03 验证的是 mstateen 对 HS-mode 的门控。当 mstateen0 bit63=0 时，HS-mode 尝试访问 hstateen0 将触发 illegal-instruction exception。这是 Smstateen/Ssstateen 的层次安全特性。

---

### Group 5：Ssstateen 与其他 Sh* 子扩展 CSR 的交互

**规范依据**：
- `norm:stateen_op`（`smstateen.adoc:86-88`）：stateen 控制低特权级对状态的访问
- `norm:stateen_illegal_state_access`（`smstateen.adoc:89-95`）：违反时触发异常
- 各子扩展 norm（参见各子扩展规范）

**测试职责**：验证当 hstateen 相关位禁止特定状态访问时，VS-mode 对相关 CSR（如果受 stateen 控制）的访问行为正确；同时验证 Shvstvala 和 Shtvala 的存在性（通过触发 trap 验证 vstval/htval 被正确写入）。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| SHA-CROSS-01 | Shvstvala 存在性：VS page fault 写入 vstval | 建立 G-stage 恒等映射；在 VS-mode 触发 load page fault（委托到 VS）；检查 vstval | vstval 包含错误虚拟地址（非零） |
| SHA-CROSS-02 | Shtvala 存在性：G-stage fault 写入 htval | 在 VS-mode 访问未映射的 GPA 触发 guest page fault | htval 包含错误 GPA >> 2（非零） |
| SHA-CROSS-03 | Shvsatpa + Shgatpa 协同：vsatp 与 hgatp 同时支持对应模式 | 探测 satp 支持 Sv39；验证 vsatp 支持 Sv39 且 hgatp 支持 Sv39x4 | 两者同时持有对应 MODE |
| SHA-CROSS-04 | Shvstvecd + Shvstvala 协同：Direct trap 写入 vstval | VS-mode 设 vstvec=Direct 自定义 entry；触发 page fault 委托到 VS；验证 trap 跳到 BASE 且 vstval 被写入 | trap PC == BASE 且 vstval 包含错误地址 |
| SHA-CROSS-05 | Shtvala + Shvstvala 不干扰性 | 连续触发 G-stage fault（验证 htval）和 VS page fault（验证 vstval），确认两者互不污染 | 第一次 trap：htval==GPA>>2 且 vstval 未被修改；第二次 trap：vstval==fault addr 且 htval 未被修改 |

> [!NOTE]
> SHA-CROSS-01/02 同时验证了 Shvstvala/Shtvala 的存在性（Group 1 中跳过了这两个需要 trap 环境的验证）。SHA-CROSS-03 验证 Shvsatpa 和 Shgatpa 的模式映射一致性在同一平台上同时成立。SHA-CROSS-04 验证 Shvstvecd（Direct trap 跳转）与 Shvstvala（vstval 写入）的端到端协同。

---

### Group 6：异常优先级验证

**规范依据**：
- `HSyncExcPrio`：G-stage fault 优先于 VS-stage fault
- `hypervisor.adoc:2043-2051`：两阶段翻译中 G-stage 检查先于 VS-stage 完成

**测试职责**：验证 Hypervisor 扩展规范中定义的异常优先级关系在两阶段翻译场景下被正确遵守。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| SHA-PRIO-01 | G-stage fault 优先于 VS-stage fault | 构造场景：VS-stage PTE walk 路径上，VS 根表 PTE 本身 VS-invalid（会导致 VS page fault）但该 PTE 所在 GPA 在 G-stage 也未映射（会导致 G-stage fault）；VS-mode 访问目标地址 | 实际触发 G-stage guest-page-fault（cause=20/21/23）而非 VS page fault（cause=12/13/15），htval==PTE GPA>>2 |
| SHA-PRIO-02 | 指令 fetch fault 优先于 illegal-instruction | 构造场景：某 GPA 在 G-stage 缺 X 权限；该地址放置了一条非法指令编码；VS-mode 跳转执行 | 实际触发 inst guest-page-fault（cause=20）而非 illegal-instruction（cause=2），htval==GPA>>2 |

---

## 附录：相关常量与 API 参考

### CSR 地址

| 名称 | 值 | 说明 |
|------|-----|------|
| `CSR_HSTATUS` | `0x600` | Hypervisor status |
| `CSR_HSTATEEN0` | `0x60C` | Hypervisor state enable 0 |
| `CSR_HSTATEEN1` | `0x60D` | Hypervisor state enable 1 |
| `CSR_HSTATEEN2` | `0x60E` | Hypervisor state enable 2 |
| `CSR_HSTATEEN3` | `0x60F` | Hypervisor state enable 3 |
| `CSR_SSTATEEN0` | `0x10C` | Supervisor state enable 0 |
| `CSR_SSTATEEN1` | `0x10D` | Supervisor state enable 1 |
| `CSR_SSTATEEN2` | `0x10E` | Supervisor state enable 2 |
| `CSR_SSTATEEN3` | `0x10F` | Supervisor state enable 3 |
| `CSR_MSTATEEN0` | `0x30C` | Machine state enable 0 |
| `CSR_VSTVEC` | `0x205` | Virtual supervisor trap vector |
| `CSR_VSTVAL` | `0x243` | Virtual supervisor trap value |
| `CSR_VSATP` | `0x280` | Virtual supervisor address translation |
| `CSR_HGATP` | `0x680` | Hypervisor guest address translation |
| `CSR_HCOUNTEREN` | `0x606` | Hypervisor counter enable |
| `CSR_SATP` | `0x180` | Supervisor address translation |

### 异常代码

| 常量 | 值 | 说明 |
|------|-----|------|
| `CAUSE_ILLEGAL_INSTRUCTION` | 2 | Illegal instruction |
| `CAUSE_VIRTUAL_INSTRUCTION` | 22 | Virtual instruction（VS/VU 下的特权违规） |
| `CAUSE_LOAD_PAGE_FAULT` | 13 | Load page fault |
| `CAUSE_GUEST_LOAD_PAGE_FAULT` | 21 | Guest load page fault（G-stage 故障） |

### Sha 子扩展测试方案索引

| 子扩展 | 独立测试方案 | 用例数 |
|--------|-------------|--------|
| H | `hyp_2_stage_translation_test_plan.md` 等 | — |
| Ssstateen | **本文档 Group 2-4**（无独立方案） | 12 |
| Shcounterenw | `shcounterenw_test_plan.md` | 21 |
| Shvstvala | `shvstvala_test_plan.md` | 20 |
| Shtvala | `shtvala_test_plan.md` | — |
| Shvstvecd | `shstvecd_test_plan.md` | 14 |
| Shvsatpa | `shvsatpa_test_plan.md` | 18 |
| Shgatpa | `shgatpa_test_plan.md` | 19 |

### 测试框架 API

- `run_in_vs_mode(fn, arg)`：在 VS-mode (V=1) 执行 fn(arg)，通过 ecall 返回 M-mode
- `run_in_vu_mode(fn, arg)`：在 VU-mode (V=1, U) 执行 fn(arg)
- `hyp_delegate_to_vs(hedeleg_mask, hideleg_mask)`：配置异常/中断委托到 VS-mode
- `hyp_reset_state()`：重置所有 hypervisor CSR
- `HYP_TEST_END()`：测试结束宏，含 hyp_reset_state + 结果记录
- `MAKE_HGATP(mode, vmid, ppn)`：构造 hgatp 值
- `HGATP_GET_MODE(v)` / `HGATP_GET_PPN(v)`：提取 hgatp 字段
- `TEST_REGISTER` / `TEST_BEGIN` / `TEST_ASSERT` / `TEST_SKIP` / `TEST_END`：测试用例注册与断言宏

---

## 测试执行说明

### 运行环境

- Group 1：M-mode 直接操作各 CSR，快速存在性验证
- Group 2：M-mode 直接操作 hstateen/sstateen CSR
- Group 3：M-mode 配置 hstateen 后通过 `run_in_vs_mode` 进入 VS-mode 验证门控行为
- Group 4：M-mode 验证 mstateen → hstateen 层次关系
- Group 5：综合环境（M-mode + VS-mode + G-stage 映射），验证跨子扩展交互
- 需在构建配置中启用所有子扩展（如 `_sha` 或等效 ISA 配置）
- 单核环境，无需 IPI
- Group 5 的 trap 相关测试需要 G-stage 恒等映射

### 与独立子扩展测试的关系

> [!IMPORTANT]
> 本测试计划**不替代**各子扩展的独立测试方案。Sha 测试的目标是：
> 1. **集成验证**：确认所有 8 个子扩展在同一平台上同时可用
> 2. **Ssstateen 功能覆盖**：作为 Ssstateen 在 H 扩展上下文中的唯一详细测试
> 3. **交互验证**：覆盖子扩展间协同工作的场景
>
> 完整的子扩展功能测试应运行各自独立的测试方案。

### 失败定位指引

| 现象 | 可能原因 |
|------|----------|
| SHA-EXIST-01 失败 | H 扩展未实现或未启用 |
| SHA-EXIST-02/03 失败 | Ssstateen 未实现（sstateen/hstateen CSR 不存在） |
| SHA-EXIST-04 失败 | Shcounterenw 未满足：hcounteren.CY 不可写 |
| SHA-EXIST-05 失败 | Shvstvecd 未满足：vstvec.MODE 不能持有 Direct |
| SHA-EXIST-06 失败 | Shvsatpa 未满足：vsatp 不支持 satp 支持的模式 |
| SHA-EXIST-07/08 失败 | Shgatpa 未满足：hgatp 不支持 Bare 或对应 SvNNx4 |
| SHA-STATEEN-03/04 失败 | hstateen bit 63 不可写，违反 `norm:hstateen_bit_63_writable` |
| SHA-HCTL-01 失败（未触发 trap） | hstateen0 bit63=0 未正确门控 VS-mode 对 sstateen0 的访问 |
| SHA-HCTL-01 cause != 22 | 触发了 illegal-instruction 而非 virtual-instruction（mstateen 配置问题） |
| SHA-HCTL-02 失败（触发了 trap） | hstateen0 bit63=1 时 VS-mode 仍无法访问 sstateen0 |
| SHA-HIER-02/03 失败 | mstateen → hstateen 层次门控链路异常 |
| SHA-CROSS-01 失败（vstval=0） | Shvstvala 未满足，vstval 在 page fault 时未被写入 |
| SHA-CROSS-02 失败（htval=0） | Shtvala 未满足，htval 在 guest page fault 时未被写入 |
| SHA-CROSS-03 失败 | Shvsatpa 或 Shgatpa 模式映射在同一平台不一致 |
| SHA-CROSS-04 失败 | Shvstvecd 的 Direct 跳转或 Shvstvala 的 vstval 写入协同异常 |

---

## 附录：规范点覆盖矩阵

| Norm ID | 覆盖用例 | 备注 |
|---------|----------|------|
| `norm:shvstvecd_vstvec_mode_direct` | SHA-EXIST-05, SHA-CROSS-04 | 完整 BASE/MODE 能力归 Shvstvecd 独立方案 |
| `norm:shvstvecd_vstvec_base_aligned_address` | SHA-CROSS-04 | 集成级验证：Direct trap 跳转到 BASE |
| `norm:shcounterenw_hpmcounter_hcounteren` | SHA-EXIST-04 | 完整可写性验证归 Shcounterenw 独立方案 |
| `norm:shvstvala_vstval_written` | SHA-CROSS-01, SHA-CROSS-04, SHA-CROSS-05 | 完整场景覆盖归 Shvstvala 独立方案 |
| `norm:shtvala_htval_faulting_gpa` | SHA-CROSS-02, SHA-CROSS-05, SHA-PRIO-01 | 完整场景覆盖归 Shtvala 独立方案 |
| `norm:shvsatpa_satp_vsatp_modes` | SHA-EXIST-06, SHA-CROSS-03 | 完整模式一致性归 Shvsatpa 独立方案 |
| `norm:shgatpa_satp_hgatp_mode_support` | SHA-EXIST-08, SHA-CROSS-03 | 完整模式映射归 Shgatpa 独立方案 |
| `norm:shgatpa_hgatp_bare_mode` | SHA-EXIST-07 | |
| `norm:sstateen_user_access_control` | SHA-HCTL-01 ~ SHA-HCTL-05 | 间接覆盖：经 hstateen 门控 VS 访问验证；U/VU 级 sstateen 门控未设独立用例 |
| `norm:hstateen_encoding` | SHA-EXIST-02, SHA-HCTL-01 ~ SHA-HCTL-05 | |
| `norm:stateen_op` | SHA-HCTL-01 ~ SHA-HCTL-05, SHA-HIER-01 ~ SHA-HIER-03 | |
| `norm:stateen_illegal_state_access` | SHA-HCTL-01, SHA-HCTL-03, SHA-HCTL-05, SHA-HIER-03 | |
| `norm:hstateen_bit_63_writable` | SHA-EXIST-02, SHA-STATEEN-03, SHA-STATEEN-04 | |
| `norm:mstateen_zero_initialization` | — | 不覆盖：复位初始状态在测试运行期不可重现，属平台复位行为 |
| `norm:hstateen_sstateen_zero_initialization` | — | 不覆盖：同上，属机器级软件职责声明，非硬件可观测行为 |
| `norm:sstateen_rv64_csrs` | SHA-EXIST-03, SHA-STATEEN-02 | |
| `norm:hstateen_rv64_csrs` | SHA-EXIST-02, SHA-STATEEN-01 | |
| `norm:stateen_warl_access` | SHA-STATEEN-03 ~ SHA-STATEEN-05 | |
| `norm:stateen_reserved_roz` | SHA-STATEEN-05 | |
| `norm:sstateen_vsmode_access_roz` | SHA-HCTL-04 | |
| `norm:mstateen_bit_63_op` | SHA-HIER-01 ~ SHA-HIER-03 | |
| `norm:hstateen_bit_63_op` | SHA-HCTL-01 ~ SHA-HCTL-04, SHA-HIER-01 ~ SHA-HIER-03 | |
| `norm:mstateen_lower_priv_roz` | SHA-HIER-02, SHA-HIER-03 | |
| `HSyncExcPrio` | SHA-PRIO-01, SHA-PRIO-02 | 源自 hypervisor.adoc `[[HSyncExcPrio]]` 优先级表 |
