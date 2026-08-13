### CFI测试计划 ###
基于 SPEC/machine.adoc 中 CFI (Control-flow Integrity) 相关规范，编写 Zicfilp（前向控制流/Landing Pad）和 Zicfiss（后向控制流/Shadow Stack）的测试计划，并记录框架层面需要补充的内容。

# CFI Control-flow Integrity 扩展测试计划

## 背景

CFI 扩展旨在保护软件控制流完整性，防御 ROP/JOP 攻击。包含两个子扩展：
- **Zicfilp**：前向控制流保护（Landing Pad），确保间接跳转目标合法
- **Zicfiss**：后向控制流保护（Shadow Stack），确保函数返回地址未被篡改

违规时触发 **software-check exception**（mcause=18），通过 `mtval`/`stval` 区分具体类型：
- `mtval=2`：Landing Pad Fault（Zicfilp）
- `mtval=3`：Shadow Stack Fault（Zicfiss）

---

## 框架层面需要修改的内容

> [!IMPORTANT]
> 以下修改是 CFI 测试的前置依赖，必须先完成。

### [MODIFY] [encoding.h](file://../..//common/encoding.h)

需要新增以下宏定义：

```diff
+ /* Software-check exception (CFI violations) */
+ #define CAUSE_SOFTWARE_CHECK        18
+ #define CAUSE_HARDWARE_ERROR        19
+
+ /* mtval encodings for software-check exception */
+ #define SWCHECK_NONE                0
+ #define SWCHECK_LANDING_PAD_FAULT   2
+ #define SWCHECK_SHADOW_STACK_FAULT  3
+
+ /* menvcfg CFI fields */
+ #define MENVCFG_LPE     (1ULL << 2)    /* Landing Pad Enable (Zicfilp) for S-mode */
+ #define MENVCFG_SSE     (1ULL << 3)    /* Shadow Stack Enable (Zicfiss) for S-mode */
+
+ /* senvcfg CFI fields */
+ #define SENVCFG_LPE     (1ULL << 2)    /* Landing Pad Enable (Zicfilp) for U-mode */
+ #define SENVCFG_SSE     (1ULL << 3)    /* Shadow Stack Enable (Zicfiss) for U-mode */
+
+ /* mstatus ELP fields */
+ #define MSTATUS_MPELP_BIT   BIT(41)   /* M-mode Previous ELP */
+ #define MSTATUS_SPELP_BIT   BIT(33)   /* S-mode Previous ELP (in sstatus too) */
+
+ /* CSR_SSP - Shadow Stack Pointer (unprivileged CSR) */
+ #define CSR_SSP         0x011
```

### [MODIFY] [trap.c](file://../..//common/trap.c)

trap handler 需要识别 software-check exception (cause=18)，在 armed trap 场景下正确捕获并记录 `mtval` 值（2=Landing Pad Fault，3=Shadow Stack Fault），然后跳过触发指令继续执行。

---

## Proposed Changes

### 测试文件结构

```
cfi/
├── Makefile
├── kernel.ld
├── main.c
└── tests/
    ├── test_helpers.h
    ├── test_register.c
    ├── test_zicfilp_csr.c         # Group 1: Zicfilp CSR 控制测试
    ├── test_zicfilp_lpad.c        # Group 2: Landing Pad 功能测试
    ├── test_zicfilp_trap.c        # Group 3: Zicfilp 异常行为测试
    ├── test_zicfiss_csr.c         # Group 4: Zicfiss CSR 控制测试
    ├── test_zicfiss_shadow.c      # Group 5: Shadow Stack 功能测试
    └── test_zicfiss_trap.c        # Group 6: Zicfiss 异常行为测试
```

---

### Group 1：Zicfilp CSR 控制测试

**规范依据**：
- `norm:menvcfg_lpe_op_lead-in`（`SPEC/machine.adoc:2325`）
- `norm:mseccfg_mlpe_presence`（`SPEC/machine.adoc:2530`）
- `norm:mseccfg_mlpe_rst`（`SPEC/machine.adoc:2825`）
- `norm:senvcfg_lpe_Zicfilp`（`SPEC/supervisor.adoc:946`）
- `norm:henvcfg_lpe_op`（`SPEC/hypervisor.adoc:814`）

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| CFI-LP-CSR-01 | mseccfg.MLPE 可写性 | 写 `mseccfg.MLPE=1` 后回读 | MLPE=1（若 Zicfilp 实现）或保持 0（未实现） |
| CFI-LP-CSR-02 | mseccfg.MLPE 复位值 | 复位后读取 `mseccfg.MLPE` | MLPE=0（`norm:mseccfg_mlpe_rst`） |
| CFI-LP-CSR-03 | menvcfg.LPE 可写性 | 写 `menvcfg.LPE=1` 后回读 | LPE=1（若 Zicfilp 实现）或保持 0 |
| CFI-LP-CSR-04 | senvcfg.LPE 可写性 | 在 S-mode 写 `senvcfg.LPE=1` 后回读 | LPE=1 |
| CFI-LP-CSR-05 | mstatus.MPELP/SPELP 存在性 | 读取 mstatus 验证 MPELP/SPELP 字段可读 | 字段值为 0 或 1 |

---

### Group 2：Zicfilp Landing Pad 功能测试

**规范依据**：
- `norm:mstatus_spelp_mpelp_op`（`SPEC/machine.adoc:1200`）
- `norm:menvcfg_lpe_op_list`（`SPEC/machine.adoc:2332–2334`）
- `norm:mseccfg_mlpe_clr_op_list`（`SPEC/machine.adoc:2535–2537`）

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| CFI-LP-FN-01 | LPAD 作为 no-op（LPE=0） | LPE=0 时执行 LPAD 指令 | LPAD 执行为 no-op，不触发异常 |
| CFI-LP-FN-02 | ELP 不更新（LPE=0） | LPE=0 时执行间接跳转 | ELP 保持 `NO_LP_EXPECTED`，无异常 |
| CFI-LP-FN-03 | 合法间接跳转（LPE=1） | LPE=1，间接跳转目标以 LPAD 开头 | 正常执行，无异常 |
| CFI-LP-FN-04 | MPELP/SPELP trap 保存恢复 | LPE=1，ELP=LP_EXPECTED 时触发 trap | xPELP 保存当前 ELP；xRET 恢复 |

---

### Group 3：Zicfilp 异常行为测试

**规范依据**：
- `norm:mtval_swchk_lead-in`（`SPEC/machine.adoc:2118`）：mtval=2 表示 Landing Pad Fault

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| CFI-LP-EXC-01 | 非法间接跳转触发 LP Fault（S-mode） | menvcfg.LPE=1，S-mode 间接跳转到非 LPAD 目标 | software-check exception, mcause=18, mtval=2 |
| CFI-LP-EXC-02 | 非法间接跳转触发 LP Fault（M-mode） | mseccfg.MLPE=1，M-mode 间接跳转到非 LPAD 目标 | software-check exception, mcause=18, mtval=2 |
| CFI-LP-EXC-03 | 非法间接跳转触发 LP Fault（U-mode） | senvcfg.LPE=1，U-mode 间接跳转到非 LPAD 目标 | software-check exception, scause=18, stval=2 |
| CFI-LP-EXC-04 | LP Fault 异常委托 | medeleg[18]=1，S-mode 触发 LP Fault | 异常委托到 S-mode 处理 |

---

### Group 4：Zicfiss CSR 控制测试

**规范依据**：
- `norm:menvcfg_sse_op_lead-in`（`SPEC/machine.adoc:2336`）
- `norm:menvcfg_sse_rdonly0`（`SPEC/machine.adoc:2346`）
- `norm:senvcfg_sse_Zicfilp`（`SPEC/supervisor.adoc:955`）
- `norm:henvcfg_sse_op`（`SPEC/hypervisor.adoc:823`）

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| CFI-SS-CSR-01 | menvcfg.SSE 可写性 | 写 `menvcfg.SSE=1` 后回读 | SSE=1（若 Zicfiss 实现）或保持 0 |
| CFI-SS-CSR-02 | menvcfg.SSE=0 时 senvcfg.SSE 只读零 | menvcfg.SSE=0，读 senvcfg.SSE | senvcfg.SSE=0 且不可写 |
| CFI-SS-CSR-03 | CSR_SSP 可访问性 | SSE=1 时读写 CSR_SSP | 正常读写无异常 |
| CFI-SS-CSR-04 | SSE=0 时 SSAMOSWAP 触发异常 | menvcfg.SSE=0，执行 SSAMOSWAP | illegal-instruction exception |

---

### Group 5：Zicfiss Shadow Stack 功能测试

**规范依据**：
- `norm:menvcfg_sse_op_list`（`SPEC/machine.adoc:2340–2344`）
- `SPEC/zpm.adoc:137`：Zicfiss 指令列表

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| CFI-SS-FN-01 | SSPUSH/SSPOPCHK 基本流程 | SSE=1，SSPUSH 压栈后 SSPOPCHK 弹栈检查 | 匹配时无异常 |
| CFI-SS-FN-02 | Shadow Stack 写保护 | 尝试用普通 store 写入 shadow stack 页 | store page fault（pte.xwr=010 为 shadow stack 页） |
| CFI-SS-FN-03 | SSE=0 时 Zicfiss 指令回退 | SSE=0，执行 32-bit Zicfiss 指令 | 退化为 Zimop 行为（no-op） |
| CFI-SS-FN-04 | SSE=0 时 16-bit Zicfiss 指令回退 | SSE=0，执行 C.SSPUSH/C.SSPOPCHK | 退化为 Zcmop 行为（no-op） |

---

### Group 6：Zicfiss 异常行为测试

**规范依据**：
- `norm:mtval_swchk_lead-in`（`SPEC/machine.adoc:2118`）：mtval=3 表示 Shadow Stack Fault

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| CFI-SS-EXC-01 | SSPOPCHK 不匹配触发 SS Fault | SSPUSH(addr_A) 后篡改 shadow stack，再 SSPOPCHK | software-check exception, mcause=18, mtval=3 |
| CFI-SS-EXC-02 | SS Fault 异常委托 | medeleg[18]=1，S-mode 触发 SS Fault | 异常委托到 S-mode 处理 |
| CFI-SS-EXC-03 | pte.xwr=010 含义（SSE=1） | SSE=1，VS/S-stage 页表中 pte.xwr=010 | 页表有效，标记为 shadow stack 页 |
| CFI-SS-EXC-04 | pte.xwr=010 含义（SSE=0） | SSE=0，VS/S-stage 页表中 pte.xwr=010 | 保留编码，触发 page fault |

---

## Verification Plan

### Automated Tests
- `cd cfi && make -j && make spike` — Spike 模拟器（需 `--isa=rv64imac_zicsr_zifencei_zicfilp_zicfiss`）
- `cd cfi && make qemu` — QEMU（需确认 CFI 扩展支持状态）

### Manual Verification
- 在硬件平台上通过 `make PLATFORM=haps_xiaohui` 编译后使用 `remote_debug.py` 部署测试


updateAtTime: 2026/5/22 10:48:35

planId: 74a53614-0566-4a5a-8333-4af8bd7377b9
