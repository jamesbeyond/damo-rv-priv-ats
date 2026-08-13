## RISC-V Pointer Masking (ZPM) 合规性测试方案

**规范依据**：RISC-V Pointer Masking Extension, Version 1.0.0 (`SPEC/zpm.adoc`)

---

### 测试目标

验证 RISC-V 处理器的 Pointer Masking 实现是否符合规范，统一覆盖三个有硬件行为的扩展：

1. **Ssnpm**（U-mode PM）：`senvcfg.PMM` 控制 U-mode 地址变换
2. **Smnpm**（S-mode PM）：`menvcfg.PMM` 控制 S-mode 地址变换
3. **Smmpm**（M-mode PM）：`mseccfg.PMM` 控制 M-mode 地址变换

重点覆盖：

- PMM CSR 字段的读写行为（WARL 约束）
- Ignore 变换的正确性（VA sign-extend / PA zero-extend）
- 各类显式内存访问指令的 PM 适用性（load/store/AMO）
- PM 不作用的场景（fetch、implicit access、CSR 读写）
- PM 与 MPRV、MXR 的交互
- 异常时 stval/mtval 的硬件变换写入

> [!NOTE]
> **Sspm** 和 **Supm** 是纯 profile 描述扩展，没有硬件行为，不编写测试用例。

---

### 测试范围

#### 规范来源

| 文件 | 行/章节 | 内容 |
|------|---------|------|
| `SPEC/zpm.adoc` | 全文 | PM 扩展定义：ignore 变换、PMLEN、扩展分类、指令覆盖、MPRV/MXR 交互、stval 写入规则 |
| `SPEC/machine.adoc` | 2307-2322 | `menvcfg.PMM` 字段编码（bits [33:32]）、Smnpm 控制 S-mode PM |
| `SPEC/machine.adoc` | 2506-2520 | `mseccfg.PMM` 字段编码（bits [33:32]）、Smmpm 控制 M-mode PM |
| `SPEC/supervisor.adoc` | 927-943 | `senvcfg.PMM` 字段编码（bits [33:32]）、Ssnpm 控制 U-mode PM |

#### 覆盖的规范点

| 规范标签 | 描述 |
|----------|------|
| `norm:pm_ignore_va` | VA ignore 变换：upper PMLEN bits 替换为 bit(XLEN-PMLEN-1) 的 sign-extend |
| `norm:pm_ignore_pa` | PA ignore 变换：upper PMLEN bits 替换为 0（zero-extend） |
| `norm:pm_apply_explicit` | PM 应用于所有显式内存访问（load/store/AMO/FP load-store） |
| `norm:pm_not_apply_implicit` | PM 不应用于隐式访问（page-table walks、instruction fetch） |
| `norm:pm_deterministic_effect` | 相同 PMLEN 下 PM 变换结果确定性一致 |
| `norm:pm_per_mode_control` | PM 按特权模式独立控制 |
| `norm:pm_config_next_higher` | PM 由上一级特权模式的 CSR 配置 |
| `norm:pm_mprv_spvp` | MPRV/SPVP 影响 PM，使用有效特权模式的 PM 设置 |
| `norm:pm_mxr_exception` | MXR 生效时 PM 不应用 |
| `norm:pm_no_csr_sw` | CSR 软件读写不受 PM 影响 |
| `norm:pm_csr_hw_apply` | 硬件写 CSR（如 stval）时应用 PM 变换 |
| `norm:pm_no_trap_vector_mask` | trap 投递时 trap handler 地址不受 PM 影响 |
| `norm:pm_warl_unaffected` | PM 不影响 CSR 的 WARL 宽度 |
| `norm:pm_rv64_only` | PM 仅适用于 RV64 |
| `norm:pmlen_supported_values` | 仅支持 PMLEN=7（XLEN-57）和 PMLEN=16（XLEN-48） |
| `norm:pmlen_illegal_warl` | 不支持的 PMM 值写入遵循 WARL 语义 |
| `norm:ssnpm_definition` | Ssnpm 提供 U-mode PM |
| `norm:smnpm_definition` | Smnpm 提供 S-mode PM |
| `norm:smmpm_definition` | Smmpm 提供 M-mode PM |
| `norm:senvcfg_pmm_Ssnpm` | senvcfg.PMM 字段编码和语义 |
| `norm:menvcfg_pmm_op` | menvcfg.PMM 字段编码和语义 |
| `norm:menvcfg_pmm_rdonly0` | Smnpm 未实现时 menvcfg.PMM 为 read-only zero |
| `norm:mseccfg_pmm_presence_op` | mseccfg.PMM 字段编码和语义 |
| `norm:mseccfg_pmm_rdonly0` | Smmpm 未实现时 mseccfg.PMM 为 read-only zero |
| `norm:pm_misaligned_equivalence` | 非对齐访问等价于对每个字节分别应用 PM |
| `norm:pm_cpu_only` | PM 仅应用于 CPU 指令生成的访问 |

#### 不在测试范围内

- **Hypervisor 场景**：henvcfg.PMM、hstatus.HUPMM、VS/VU-mode（涉及 H 扩展）
- **Sspm / Supm 扩展**：纯 profile 描述，无硬件行为
- **RV32 场景**：PM 仅适用于 RV64
- **Vector / FP / CMO 指令**：当前框架不包含 V/F/Zicbo 扩展支持
- **多 hart 一致性**：当前框架为单核环境
- **Debug trigger 地址匹配**：涉及 Debug 扩展

---

### Ignore 变换定义（共享参考）

**虚拟地址变换**（Sv39/Sv48/Sv57 模式下）：

```
transformed_ea = {{PMLEN{ea[XLEN-PMLEN-1]}}, ea[XLEN-PMLEN-1:0]}
```

即：将上 PMLEN 位替换为 bit(XLEN-PMLEN-1) 的 sign-extend。

**物理地址变换**（Bare/M-mode 模式下）：

```
transformed_ea = {{PMLEN{0}}, ea[XLEN-PMLEN-1:0]}
```

即：将上 PMLEN 位清零。

---

### 测试分组

> [!IMPORTANT]
> 共 10 个测试组。Ssnpm 测试使用 `vm_run_in_umode()`（Sv39），Smnpm 测试使用 `vm_run_in_smode()`（Sv39），Smmpm 测试在 M-mode Bare 模式下直接执行。

---

### Group 1：硬件能力探测

**规范依据**：
- `norm:ssnpm_definition`：Ssnpm 由 senvcfg.PMM 控制
- `norm:smnpm_definition`：Smnpm 由 menvcfg.PMM 控制
- `norm:smmpm_definition`：Smmpm 由 mseccfg.PMM 控制
- `norm:pmlen_supported_values`：仅支持 PMLEN=7 和 PMLEN=16
- `norm:menvcfg_pmm_rdonly0`：Smnpm 未实现时 menvcfg.PMM 为 read-only zero
- `norm:mseccfg_pmm_rdonly0`：Smmpm 未实现时 mseccfg.PMM 为 read-only zero

**测试职责**：在 M-mode 下检测各 PM 扩展是否实现，以及支持哪些 PMLEN 值。后续 Group 依赖此探测结果进行 SKIP 判定。

#### 1.a：Ssnpm 能力探测

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZPM-CAP-01 | 探测 Ssnpm 实现 | 写 senvcfg.PMM=0b11，读回检查非零 | PMM 非零则 Ssnpm 已实现 |
| ZPM-CAP-04 | Ssnpm 支持的 PMLEN | 分别写 PMM=0b10/0b11 到 senvcfg，读回 | 记录 PMLEN=7/16 支持情况 |

#### 1.b：Smnpm 能力探测

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZPM-CAP-02 | 探测 Smnpm 实现 | 写 menvcfg.PMM=0b11，读回检查非零 | PMM 非零则 Smnpm 已实现 |
| ZPM-CAP-05 | Smnpm 支持的 PMLEN | 分别写 PMM=0b10/0b11 到 menvcfg，读回 | 记录 PMLEN=7/16 支持情况 |

#### 1.c：Smmpm 能力探测

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZPM-CAP-03 | 探测 Smmpm 实现 | 写 mseccfg.PMM=0b11，读回检查非零 | PMM 非零则 Smmpm 已实现 |
| ZPM-CAP-06 | Smmpm 支持的 PMLEN | 分别写 PMM=0b10/0b11 到 mseccfg，读回 | 记录 PMLEN=7/16 支持情况 |

```c
/* ZPM-CAP-01/02/03 示例 */
#include "pm_cfg.h"

TEST_REGISTER(test_zpm_cap_detect);
bool test_zpm_cap_detect(void) {
    TEST_BEGIN("ZPM-CAP: Detect PM extensions");

    bool has_ssnpm = detect_ssnpm();
    bool has_smnpm = detect_smnpm();
    bool has_smmpm = detect_smmpm();

    printf("  Ssnpm: %s\n", has_ssnpm ? "implemented" : "not implemented");
    printf("  Smnpm: %s\n", has_smnpm ? "implemented" : "not implemented");
    printf("  Smmpm: %s\n", has_smmpm ? "implemented" : "not implemented");

    TEST_ASSERT("at least one PM extension implemented",
                has_ssnpm || has_smnpm || has_smmpm);
    TEST_END();
}

/* ZPM-CAP-04 示例：探测 PMLEN */
TEST_REGISTER(test_zpm_cap_ssnpm_pmlen);
bool test_zpm_cap_ssnpm_pmlen(void) {
    TEST_BEGIN("ZPM-CAP-04: Ssnpm supported PMLEN");
    if (!detect_ssnpm()) TEST_SKIP("Ssnpm not implemented");

    unsigned saved = pm_get_umode();
    pm_set_umode(PMM_PMLEN7);
    bool pmlen7 = (pm_get_umode() == PMM_PMLEN7);
    pm_set_umode(PMM_PMLEN16);
    bool pmlen16 = (pm_get_umode() == PMM_PMLEN16);
    pm_set_umode(saved);

    TEST_ASSERT("at least one PMLEN supported", pmlen7 || pmlen16);
    TEST_END();
}
```

> [!NOTE]
> 探测逻辑使用 `common/pm/pm_cfg.h` 中的 `detect_ssnpm()` 等 API，内部通过 `trap_expect_begin/end` 保护，即使 CSR 不存在也不会崩溃。

---

### Group 2：CSR PMM 字段控制

**规范依据**：
- `norm:senvcfg_pmm_Ssnpm`（supervisor.adoc:927）：senvcfg.PMM 字段编码
- `norm:menvcfg_pmm_op`（machine.adoc:2307）：menvcfg.PMM 字段编码
- `norm:mseccfg_pmm_presence_op`（machine.adoc:2506）：mseccfg.PMM 字段编码
- `norm:pmlen_illegal_warl`：不支持的 PMM 值写入遵循 WARL 语义
- `norm:pm_warl_unaffected`：PM 不影响 CSR 的 WARL 宽度

**测试职责**：验证三个 CSR 的 PMM 字段读写一致性、WARL 约束（reserved 编码 01 被忽略）、PMM 操作不影响同 CSR 中其他字段、以及扩展未实现时 PMM 为 read-only zero。

#### 2a：senvcfg.PMM（Ssnpm）

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZPM-CSR-01 | senvcfg.PMM 可写 0->PMLEN7 | 写 PMM=0b10，读回 | PMM=0b10（若 PMLEN=7 支持） |
| ZPM-CSR-02 | senvcfg.PMM 可写 0->PMLEN16 | 写 PMM=0b11，读回 | PMM=0b11（若 PMLEN=16 支持） |
| ZPM-CSR-03 | senvcfg.PMM 可清零 | 先写非零 PMM，再清零，读回 | PMM=0b00 |
| ZPM-CSR-04 | senvcfg.PMM reserved 值 | 写 PMM=0b01（reserved），读回 | PMM 不变（WARL 忽略无效写） |
| ZPM-CSR-05 | senvcfg.PMM 不影响其他字段 | 切换 PMM 前后读取 FIOM/CBIE 等字段 | 其他字段值不变 |

#### 2b：menvcfg.PMM（Smnpm）

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZPM-CSR-06 | menvcfg.PMM 可写 0->PMLEN7 | 写 PMM=0b10，读回 | PMM=0b10 |
| ZPM-CSR-07 | menvcfg.PMM 可写 0->PMLEN16 | 写 PMM=0b11，读回 | PMM=0b11 |
| ZPM-CSR-08 | menvcfg.PMM 可清零 | 先写非零再清零，读回 | PMM=0b00 |
| ZPM-CSR-09 | menvcfg.PMM reserved 值 | 写 PMM=0b01，读回 | PMM 不变 |
| ZPM-CSR-10 | menvcfg.PMM 不影响 PBMTE/ADUE | 切换 PMM 前后读取其他字段 | 其他字段值不变 |

#### 2c：mseccfg.PMM（Smmpm）

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZPM-CSR-11 | mseccfg.PMM 可写 0->PMLEN7 | 写 PMM=0b10，读回 | PMM=0b10 |
| ZPM-CSR-12 | mseccfg.PMM 可写 0->PMLEN16 | 写 PMM=0b11，读回 | PMM=0b11 |
| ZPM-CSR-13 | mseccfg.PMM 可清零 | 先写非零再清零，读回 | PMM=0b00 |
| ZPM-CSR-14 | mseccfg.PMM reserved 值 | 写 PMM=0b01，读回 | PMM 不变 |
| ZPM-CSR-15 | mseccfg.PMM 不影响 MML/MMWP/RLB | 切换 PMM 前后读取其他字段 | 其他字段值不变 |

```c
/* ZPM-CSR-01 示例 */
TEST_REGISTER(test_zpm_csr_senvcfg_pmm_write_pmlen7);
bool test_zpm_csr_senvcfg_pmm_write_pmlen7(void) {
    TEST_BEGIN("ZPM-CSR-01: senvcfg.PMM writable 0->PMLEN7");
    if (!detect_ssnpm()) TEST_SKIP("Ssnpm not implemented");

    pm_set_umode(PMM_DISABLED);
    TEST_ASSERT("PMM starts at 0", pm_get_umode() == PMM_DISABLED);
    pm_set_umode(PMM_PMLEN7);
    unsigned readback = pm_get_umode();
    if (readback == PMM_PMLEN7)
        TEST_ASSERT("PMM=PMLEN7 readback", readback == PMM_PMLEN7);
    else
        printf("  PMLEN=7 not supported (WARL)\n");
    pm_set_umode(PMM_DISABLED);
    TEST_END();
}

/* ZPM-CSR-04 示例：reserved 编码 */
TEST_REGISTER(test_zpm_csr_senvcfg_pmm_reserved);
bool test_zpm_csr_senvcfg_pmm_reserved(void) {
    TEST_BEGIN("ZPM-CSR-04: reserved value rejected");
    if (!detect_ssnpm()) TEST_SKIP("Ssnpm not implemented");
    pm_set_umode(PMM_DISABLED);
    unsigned before = pm_get_umode();
    pm_set_umode(PMM_RESERVED);  /* 0b01 */
    TEST_ASSERT("reserved write ignored", pm_get_umode() == before);
    pm_set_umode(PMM_DISABLED);
    TEST_END();
}
```

---

### Group 3：Ssnpm — U-mode VA Ignore 变换（load/store）

**规范依据**：
- `norm:pm_ignore_va`：虚拟地址 ignore 变换，sign-extend from bit(XLEN-PMLEN-1)
- `norm:pm_apply_explicit`：PM 应用于所有显式内存访问
- `norm:pm_deterministic_effect`：相同 PMLEN 下变换结果确定性一致
- `norm:ssnpm_definition`：Ssnpm 控制 U-mode PM

**测试职责**：在 VM（Sv39）+ U-mode 下，验证 Ssnpm 使能后 tagged load/store 的 ignore 变换正确性。使用 `vm_run_in_umode()` 执行 U-mode 代码，页表需设置 `PTE_U` 标志。覆盖 PMLEN=7 和 PMLEN=16。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZPM-UVA-01 | PMLEN7 tagged load | PMM=PMLEN7，U-mode load tagged addr（tag=全1） | load 成功，值与 untagged 地址一致 |
| ZPM-UVA-02 | PMLEN16 tagged load | PMM=PMLEN16，U-mode load tagged addr | load 成功，值正确 |
| ZPM-UVA-03 | PMLEN7 tagged store | PMM=PMLEN7，U-mode store via tagged addr | store 成功，untagged 读回值正确 |
| ZPM-UVA-04 | PMLEN16 tagged store | PMM=PMLEN16，U-mode store via tagged addr | store 成功，值正确 |
| ZPM-UVA-05 | 不同 tag 访问同一位置 | 同一 base 嵌入 tag=0x55 和 tag=0x7F，分别 load | 两次 load 读到相同值 |
| ZPM-UVA-06 | tag=0 等价于无 tag | PMM=PMLEN7，tag=0 的地址 load | load 成功，与 PM 禁用时一致 |
| ZPM-UVA-07 | PM 禁用时 tagged addr | PMM=DISABLED，load tagged addr | page-fault（tagged VA 无有效映射） |
| ZPM-UVA-08 | sign-extend 正确性 | PMLEN7，bit 56=1 的 base 嵌入 tag 后 load | bits [63:57] 全为 1，访问正确地址 |

```c
/* ZPM-UVA-01 示例：PMLEN7 tagged load in U-mode */
#include "pm_cfg.h"
#include "pm_addr.h"
#include "vm.h"

static volatile uint64_t test_data_uva = 0xDEADBEEFCAFE1234ULL;

static uintptr_t umode_tagged_load(uintptr_t arg) {
    return *(volatile uint64_t *)arg;
}

TEST_REGISTER(test_zpm_uva_pmlen7_load);
bool test_zpm_uva_pmlen7_load(void) {
    TEST_BEGIN("ZPM-UVA-01: PMLEN7 tagged load in U-mode");
    if (!detect_ssnpm()) TEST_SKIP("Ssnpm not implemented");
    pm_set_umode(PMM_PMLEN7);
    if (pm_get_umode() != PMM_PMLEN7) TEST_SKIP("PMLEN=7 not supported");

    pt_context_t ctx;
    pt_pool_reset();
    pt_init(&ctx, SATP_MODE_SV39);
    pt_setup_identity_mapping(&ctx, PLATFORM_MEM_BASE, 0x4000000,
        PTE_V|PTE_R|PTE_W|PTE_X|PTE_U|PTE_A|PTE_D, PT_LEVEL_2M);

    uintptr_t base_addr = (uintptr_t)&test_data_uva;
    uintptr_t tagged = pm_tag_address(base_addr, pm_max_tag(7), 7);
    TEST_ASSERT("tagged differs from base", tagged != base_addr);

    uintptr_t result = vm_run_in_umode(&ctx, umode_tagged_load, tagged);
    TEST_ASSERT("correct value", result == 0xDEADBEEFCAFE1234ULL);

    pm_set_umode(PMM_DISABLED);
    pt_pool_reset();
    TEST_END();
}
```

> [!NOTE]
> ZPM-UVA-08 验证 sign-extend 正确性：当 bit(XLEN-PMLEN-1)=1 时，变换后上位全为 1（负地址空间），确保 kernel 地址空间的 tagged pointer 也能正确变换。

---

### Group 4：Ssnpm — U-mode VA Ignore 变换（AMO）

**规范依据**：
- `norm:pm_apply_explicit`：PM 应用于所有显式内存访问，包括 atomics（RV64A）
- `norm:pm_ignore_va`：虚拟地址 sign-extend 变换

**测试职责**：验证 U-mode 下 AMO 指令使用 tagged 地址时 PM 变换正确应用。AMO 同时涉及 load 和 store，是验证 PM 对原子操作覆盖的关键场景。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZPM-UAMO-01 | PMLEN7 amoadd.d | PMM=PMLEN7，U-mode amoadd.d via tagged addr | 旧值正确，内存更新正确 |
| ZPM-UAMO-02 | PMLEN16 amoadd.d | PMM=PMLEN16，U-mode amoadd.d via tagged addr | 旧值正确，内存更新正确 |
| ZPM-UAMO-03 | PMLEN7 amoswap.d | PMM=PMLEN7，U-mode amoswap.d via tagged addr | 旧值返回正确，新值写入正确 |
| ZPM-UAMO-04 | 不同 tag AMO 同一位置 | 两个不同 tag 对同一位置做 amoadd | 两次累加到同一位置 |

```c
/* ZPM-UAMO-01 示例 */
static volatile uint64_t amo_test_var = 100;

static uintptr_t umode_tagged_amoadd(uintptr_t arg) {
    uint64_t old_val, addend = 42;
    asm volatile("amoadd.d %0, %1, (%2)"
                 : "=r"(old_val) : "r"(addend), "r"(arg) : "memory");
    return old_val;
}

TEST_REGISTER(test_zpm_uamo_pmlen7_amoadd);
bool test_zpm_uamo_pmlen7_amoadd(void) {
    TEST_BEGIN("ZPM-UAMO-01: PMLEN7 amoadd.d via tagged addr");
    if (!detect_ssnpm()) TEST_SKIP("Ssnpm not implemented");
    pm_set_umode(PMM_PMLEN7);
    if (pm_get_umode() != PMM_PMLEN7) TEST_SKIP("PMLEN=7 not supported");

    /* setup VM + identity mapping with PTE_U ... */
    amo_test_var = 100;
    uintptr_t tagged = pm_tag_address((uintptr_t)&amo_test_var, 0x55, 7);
    uintptr_t old_val = vm_run_in_umode(&ctx, umode_tagged_amoadd, tagged);

    TEST_ASSERT("old value 100", old_val == 100);
    TEST_ASSERT("updated to 142", amo_test_var == 142);
    pm_set_umode(PMM_DISABLED);
    TEST_END();
}
```

---

### Group 5：Smnpm — S-mode VA Ignore 变换（load/store/AMO）

**规范依据**：
- `norm:pm_ignore_va`：虚拟地址 ignore 变换，sign-extend from bit(XLEN-PMLEN-1)
- `norm:pm_apply_explicit`：PM 应用于所有显式内存访问
- `norm:smnpm_definition`：Smnpm 提供 S-mode PM，由 menvcfg.PMM 控制
- `norm:pm_config_next_higher`：S-mode PM 由 M-mode 的 menvcfg.PMM 配置

**测试职责**：在 VM（Sv39）+ S-mode 下，验证 Smnpm 使能后 tagged load/store/AMO 的 ignore 变换正确性。使用 `vm_run_in_smode()` 执行 S-mode 代码。与 Group 3/4 的关键区别：PM 配置通过 `menvcfg.PMM`（而非 `senvcfg.PMM`），作用于 S-mode（而非 U-mode）。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZPM-SVA-01 | PMLEN7 S-mode tagged load | PMM=PMLEN7，S-mode load tagged addr（tag=全1） | load 成功，值与 untagged 地址一致 |
| ZPM-SVA-02 | PMLEN16 S-mode tagged load | PMM=PMLEN16，S-mode load tagged addr | load 成功，值正确 |
| ZPM-SVA-03 | PMLEN7 S-mode tagged store | PMM=PMLEN7，S-mode store via tagged addr | store 成功，untagged 读回值正确 |
| ZPM-SVA-04 | PMLEN16 S-mode tagged store | PMM=PMLEN16，S-mode store via tagged addr | store 成功，值正确 |
| ZPM-SVA-05 | PMLEN7 S-mode amoadd.d | PMM=PMLEN7，S-mode amoadd.d via tagged addr | 旧值正确，内存更新正确 |
| ZPM-SVA-06 | 不同 tag 访问同一位置 | 同一 base 嵌入不同 tag，S-mode 分别 load | 两次 load 读到相同值 |
| ZPM-SVA-07 | PM 禁用时 tagged addr | PMM=DISABLED，S-mode load tagged addr | page-fault |
| ZPM-SVA-08 | S-mode PM 独立于 U-mode | menvcfg.PMM=PMLEN7（S-mode PM开），senvcfg.PMM=DISABLED（U-mode PM关） | S-mode tagged load 成功，U-mode tagged load fault |
| ZPM-SVA-09 | sign-extend 正确性 | PMLEN7，bit 56=1 的 base 嵌入 tag 后 S-mode load | bits [63:57] 全为 1，访问正确地址 |

```c
/* ZPM-SVA-01 示例：PMLEN7 tagged load in S-mode */
static volatile uint64_t test_data_sva = 0xCAFEBABE12345678ULL;

static uintptr_t smode_tagged_load(uintptr_t arg) {
    return *(volatile uint64_t *)arg;
}

TEST_REGISTER(test_zpm_sva_pmlen7_load);
bool test_zpm_sva_pmlen7_load(void) {
    TEST_BEGIN("ZPM-SVA-01: PMLEN7 tagged load in S-mode");
    if (!detect_smnpm()) TEST_SKIP("Smnpm not implemented");
    pm_set_smode(PMM_PMLEN7);
    if (pm_get_smode() != PMM_PMLEN7) TEST_SKIP("PMLEN=7 not supported");

    pt_context_t ctx;
    pt_pool_reset();
    pt_init(&ctx, SATP_MODE_SV39);
    pt_setup_identity_mapping(&ctx, PLATFORM_MEM_BASE, 0x4000000,
        PTE_V|PTE_R|PTE_W|PTE_X|PTE_A|PTE_D, PT_LEVEL_2M);

    uintptr_t base_addr = (uintptr_t)&test_data_sva;
    uintptr_t tagged = pm_tag_address(base_addr, pm_max_tag(7), 7);

    uintptr_t result = vm_run_in_smode(&ctx, smode_tagged_load, tagged);
    TEST_ASSERT("correct value", result == 0xCAFEBABE12345678ULL);

    pm_set_smode(PMM_DISABLED);
    pt_pool_reset();
    TEST_END();
}

/* ZPM-SVA-08 示例：S-mode PM 独立于 U-mode PM */
TEST_REGISTER(test_zpm_sva_independent_of_umode);
bool test_zpm_sva_independent_of_umode(void) {
    TEST_BEGIN("ZPM-SVA-08: S-mode PM independent of U-mode PM");
    if (!detect_smnpm()) TEST_SKIP("Smnpm not implemented");
    if (!detect_ssnpm()) TEST_SKIP("Ssnpm not implemented");

    /* S-mode PM on, U-mode PM off */
    pm_set_smode(PMM_PMLEN7);
    pm_set_umode(PMM_DISABLED);

    TEST_ASSERT("S-mode PMM=PMLEN7", pm_get_smode() == PMM_PMLEN7);
    TEST_ASSERT("U-mode PMM=DISABLED", pm_get_umode() == PMM_DISABLED);

    /* ... S-mode tagged load succeeds, U-mode tagged load faults ... */

    pm_set_smode(PMM_DISABLED);
    TEST_END();
}
```

> [!NOTE]
> S-mode 页表映射不需要 `PTE_U` 标志（与 Group 3 的 U-mode 测试不同）。`vm_run_in_smode()` 使用 `run_in_priv(PRIV_S)` 执行，ecall 返回走 M-mode handler。

---

### Group 6：Smmpm — M-mode PA Ignore 变换（load/store/AMO）

**规范依据**：
- `norm:pm_ignore_pa`：物理地址 ignore 变换，zero-extend（清除上 PMLEN 位）
- `norm:pm_apply_explicit`：PM 应用于所有显式内存访问
- `norm:smmpm_definition`：Smmpm 提供 M-mode PM，由 mseccfg.PMM 控制
- `norm:pm_deterministic_effect`：相同 PMLEN 下变换结果确定性一致

**测试职责**：在 M-mode Bare 翻译模式下，验证 Smmpm 使能后 tagged load/store/AMO 的 ignore 变换正确性。M-mode 使用物理地址，ignore 变换为 zero-extend（而非 VA 的 sign-extend）。这是与 Group 3/5 的核心区别。

> [!IMPORTANT]
> M-mode PM 测试直接在 M-mode 执行，不需要 VM。tagged 物理地址的上 PMLEN 位被清零后应指向正确的物理内存位置。注意：tag 值不能使清零后的地址超出物理内存范围。

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZPM-MPA-01 | PMLEN7 M-mode tagged load | PMM=PMLEN7，M-mode load tagged PA（tag=全1） | load 成功，值与 untagged 地址一致 |
| ZPM-MPA-02 | PMLEN16 M-mode tagged load | PMM=PMLEN16，M-mode load tagged PA | load 成功，值正确 |
| ZPM-MPA-03 | PMLEN7 M-mode tagged store | PMM=PMLEN7，M-mode store via tagged PA | store 成功，untagged 读回正确 |
| ZPM-MPA-04 | PMLEN16 M-mode tagged store | PMM=PMLEN16，M-mode store via tagged PA | store 成功，值正确 |
| ZPM-MPA-05 | PMLEN7 M-mode amoadd.d | PMM=PMLEN7，M-mode amoadd.d via tagged PA | 旧值正确，内存更新正确 |
| ZPM-MPA-06 | zero-extend 正确性 | PMLEN7，在 PA 上嵌入 tag=0x7F | 变换后 bits [63:57] 全为 0，访问正确地址 |
| ZPM-MPA-07 | 不同 tag 访问同一位置 | 同一 base 嵌入 tag=0x55 和 tag=0x7F，M-mode 分别 load | 两次 load 读到相同值 |
| ZPM-MPA-08 | PM 禁用时 tagged PA | PMM=DISABLED，M-mode load tagged PA | 访问错误地址（可能 access fault 或读到错误值） |
| ZPM-MPA-09 | PA vs VA 变换差异验证 | 同一 tagged addr，分别用 pm_transform_pa 和 pm_transform_va | 结果不同（PA zero-ext vs VA sign-ext） |

```c
/* ZPM-MPA-01 示例：PMLEN7 tagged load in M-mode */
#include "pm_cfg.h"
#include "pm_addr.h"

static volatile uint64_t test_data_mpa = 0x1234567890ABCDEFULL;

TEST_REGISTER(test_zpm_mpa_pmlen7_load);
bool test_zpm_mpa_pmlen7_load(void) {
    TEST_BEGIN("ZPM-MPA-01: PMLEN7 tagged load in M-mode");
    if (!detect_smmpm()) TEST_SKIP("Smmpm not implemented");
    pm_set_mmode(PMM_PMLEN7);
    if (pm_get_mmode() != PMM_PMLEN7) TEST_SKIP("PMLEN=7 not supported");

    uintptr_t base_addr = (uintptr_t)&test_data_mpa;
    uintptr_t tagged = pm_tag_address(base_addr, pm_max_tag(7), 7);
    TEST_ASSERT("tagged differs from base", tagged != base_addr);

    /* M-mode direct load with tagged physical address */
    uint64_t val = *(volatile uint64_t *)tagged;
    TEST_ASSERT("correct value via tagged PA",
                val == 0x1234567890ABCDEFULL);

    pm_set_mmode(PMM_DISABLED);
    TEST_END();
}

/* ZPM-MPA-06 示例：zero-extend 正确性验证 */
TEST_REGISTER(test_zpm_mpa_zero_extend);
bool test_zpm_mpa_zero_extend(void) {
    TEST_BEGIN("ZPM-MPA-06: zero-extend correctness");
    if (!detect_smmpm()) TEST_SKIP("Smmpm not implemented");

    unsigned pmlen = 7;
    uintptr_t base = (uintptr_t)&test_data_mpa;
    uintptr_t tagged = pm_tag_address(base, 0x7F, pmlen);

    /* PA 变换应为 zero-extend */
    uintptr_t transformed = pm_transform_pa(tagged, pmlen);
    TEST_ASSERT("upper bits cleared", transformed == base);

    /* 对比 VA 变换（sign-extend）结果不同 */
    uintptr_t va_transformed = pm_transform_va(tagged, pmlen);
    TEST_ASSERT("PA != VA transform", transformed != va_transformed);

    TEST_END();
}
```

> [!NOTE]
> M-mode PM 测试不涉及页表，但需注意 PMP 配置。如果 PMP 基于清零后的物理地址做访问控制，tagged PA 经过 PM 变换后的地址必须在 PMP 允许的范围内。当前测试框架在 M-mode 下默认无 PMP 限制，因此不需要额外配置。

---

### Group 7：PM 不作用的场景

**规范依据**：
- `norm:pm_not_apply_implicit`：PM 不应用于隐式访问（page-table walks、instruction fetch）
- `norm:pm_no_csr_sw`：CSR 软件读写不受 PM 影响
- `norm:pm_cpu_only`：PM 仅应用于 CPU 指令生成的访问
- `norm:pm_warl_unaffected`：PM 不影响 CSR 的 WARL 宽度

**测试职责**：验证 PM 使能后，以下场景的地址不受 PM 变换影响：instruction fetch、page-table walk、CSR 软件读写、SFENCE.VMA 地址参数。这些"负面测试"确保 PM 不会过度应用。

#### 7.a：PM 不作用场景 - U-mode（Ssnpm）

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZPM-NEG-01 | instruction fetch 不受 PM | U-mode PM=PMLEN7，跳转到 tagged PC 地址 | 取指不应用 PM，tagged PC 指向无效地址则 fault |
| ZPM-NEG-02 | page-table walk 不受 PM | 设置 PTE 中 next-level PT addr 为 tagged，PM 开 | page-table walk 使用原始地址，tagged PT addr 导致 fault |

#### 7.b：PM 不作用场景 - S-mode（Smnpm）

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZPM-NEG-03 | CSR 写入 tagged addr | PM=PMLEN7，将 tagged addr 写入 sepc CSR | 读回 sepc 得到原始 tagged 值（PM 不变换） |
| ZPM-NEG-04 | CSR WARL 宽度不受 PM | PM=PMLEN16，写 tagged addr 到 pmpaddr CSR | WARL 截断行为与 PM 禁用时一致 |
| ZPM-NEG-05 | SFENCE.VMA addr 不受 PM | PM=PMLEN7，执行 sfence.vma(tagged_addr, 0) | sfence 使用原始 tagged 地址（不变换） |

```c
/* ZPM-NEG-03 示例：CSR 写入不受 PM 影响 */
TEST_REGISTER(test_zpm_neg_csr_write_no_pm);
bool test_zpm_neg_csr_write_no_pm(void) {
    TEST_BEGIN("ZPM-NEG-03: CSR write not affected by PM");
    if (!detect_smnpm()) TEST_SKIP("Smnpm not implemented");
    pm_set_smode(PMM_PMLEN7);

    /* 构造 tagged address */
    uintptr_t base = 0x0000008012345678ULL;
    uintptr_t tagged = pm_tag_address(base, 0x7F, 7);

    /* 写入 sepc（S-mode exception PC） */
    CSRW(0x141, tagged);  /* sepc */
    uintptr_t readback = CSRR(0x141);

    /* CSR 软件读写不受 PM 影响，应保留 tagged 值 */
    /* 注意：sepc 是 WARL，低位可能被截断，但上位 tag 应保留 */
    uintptr_t tag_readback = pm_extract_tag(readback, 7);
    uintptr_t tag_written = pm_extract_tag(tagged, 7);
    TEST_ASSERT("tag preserved in CSR", tag_readback == tag_written);

    pm_set_smode(PMM_DISABLED);
    TEST_END();
}

/* ZPM-NEG-01 示例概述：instruction fetch 不受 PM
 *
 * 测试思路：将函数地址嵌入 tag，直接跳转。
 * 由于 fetch 不受 PM 变换，tagged PC 不会被变换回正确地址，
 * 应该触发 instruction page-fault 或 access fault。
 *
 * 这是一个"负面"测试：验证 PM 不会错误地应用于 fetch。
 */
```

> [!NOTE]
> ZPM-NEG-01 和 ZPM-NEG-02 是关键的安全测试：如果 PM 错误地应用于 instruction fetch 或 page-table walk，将导致严重的安全漏洞。这些测试确保 PM 仅限于显式数据访问。

---

### Group 8：PM 与 MPRV 交互

**规范依据**：
- `norm:pm_mprv_spvp`：MPRV 和 SPVP 影响 PM，使用有效特权模式的 PM 设置
- `norm:pm_auto_apply_active_mode`：硬件自动应用当前活动模式的 PM 设置

**测试职责**：验证 M-mode 下设置 `mstatus.MPRV=1` 时，load/store 使用有效特权模式（MPP 指定）的 PM 设置，而非 M-mode 自身的 PM 设置。这是 PM 与特权级交互的关键测试。

#### 8.a：MPRV - S-mode PM（Smnpm）

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZPM-MPRV-01 | MPRV=1 MPP=S 使用 S-mode PM | M-mode，MPRV=1，MPP=S，menvcfg.PMM=PMLEN7，tagged load | load 使用 S-mode PM 变换，成功访问 |
| ZPM-MPRV-02 | MPRV=1 MPP=S 但 S-mode PM 禁用 | MPRV=1，MPP=S，menvcfg.PMM=DISABLED，tagged load | tagged addr 未变换，访问可能 fault |

#### 8.b：MPRV - U-mode PM（Ssnpm）

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZPM-MPRV-03 | MPRV=1 MPP=U 使用 U-mode PM | MPRV=1，MPP=U，senvcfg.PMM=PMLEN7，tagged load | load 使用 U-mode PM 变换 |

#### 8.c：MPRV - M-mode PM（Smmpm）

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZPM-MPRV-04 | MPRV=0 不影响 PM | M-mode，MPRV=0，mseccfg.PMM=PMLEN7，tagged load | 使用 M-mode PM（非 MPP 指定） |
| ZPM-MPRV-05 | MPRV 切换前后 PM 变化 | 先 MPRV=0（M-mode PM），再 MPRV=1（S-mode PM） | PM 行为随 MPRV 变化 |

```c
/* ZPM-MPRV-01 示例：MPRV=1 + MPP=S 使用 S-mode PM */
TEST_REGISTER(test_zpm_mprv_smode_pm);
bool test_zpm_mprv_smode_pm(void) {
    TEST_BEGIN("ZPM-MPRV-01: MPRV=1 MPP=S uses S-mode PM");
    if (!detect_smnpm()) TEST_SKIP("Smnpm not implemented");
    pm_set_smode(PMM_PMLEN7);
    if (pm_get_smode() != PMM_PMLEN7) TEST_SKIP("PMLEN=7 not supported");

    /* Disable M-mode PM to isolate MPRV effect */
    if (detect_smmpm()) pm_set_mmode(PMM_DISABLED);

    static volatile uint64_t mprv_data = 0xAAAABBBBCCCCDDDDULL;
    uintptr_t base = (uintptr_t)&mprv_data;
    uintptr_t tagged = pm_tag_address(base, 0x7F, 7);

    /* Set MPRV=1, MPP=S */
    uintptr_t mstatus = CSRR(mstatus);
    uintptr_t new_mstatus = mstatus;
    new_mstatus |= MSTATUS_MPRV_BIT;                   /* MPRV=1 */
    new_mstatus = (new_mstatus & ~MSTATUS_MPP_MASK)
                  | (PRIV_S << MSTATUS_MPP_OFF);        /* MPP=S */
    CSRW(mstatus, new_mstatus);

    /* Load via tagged addr; should use S-mode PM (sign-extend) */
    uint64_t val = *(volatile uint64_t *)tagged;

    /* Restore mstatus */
    CSRW(mstatus, mstatus);

    TEST_ASSERT("MPRV load via S-mode PM correct",
                val == 0xAAAABBBBCCCCDDDDULL);

    pm_set_smode(PMM_DISABLED);
    TEST_END();
}
```

> [!WARNING]
> MPRV 测试在 M-mode 下直接操作 mstatus，修改 MPRV/MPP 后必须立即执行 tagged load，然后立即恢复 mstatus。如果在 MPRV=1 期间触发异常，trap handler 也会受到 MPRV 影响，可能导致不可预期的行为。

---

### Group 9：PM 与 MXR 交互

**规范依据**：
- `norm:pm_mxr_exception`：当 MXR 在有效特权模式生效时，PM 不应用
- 注意：即使页表虚拟内存未启用（Bare 模式），MXR 仍用于判定是否应用 PM

**测试职责**：验证 `mstatus.MXR=1` 时 PM 不应用（即使 PMM 非零），以及 MXR=0 时 PM 正常应用。这是 PM 规范中一个容易被忽略的交互规则。

#### 9.a：MXR - S-mode（Smnpm）

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZPM-MXR-01 | MXR=1 时 PM 不应用 | S-mode，MXR=1，PMM=PMLEN7，tagged load | PM 不变换，tagged addr 直接用于访问（可能 fault） |
| ZPM-MXR-02 | MXR=0 时 PM 正常 | S-mode，MXR=0，PMM=PMLEN7，tagged load | PM 正常变换，访问成功 |

#### 9.b：MXR - M-mode（Smmpm）

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZPM-MXR-03 | MXR=1 Bare 模式也禁用 PM | M-mode Bare，MXR=1，mseccfg.PMM=PMLEN7，tagged load | PM 不变换 |
| ZPM-MXR-04 | 动态切换 MXR | 先 MXR=0 tagged load 成功，再 MXR=1 tagged load fault | PM 行为随 MXR 变化 |

```c
/* ZPM-MXR-01 示例：MXR=1 时 PM 不应用 */
TEST_REGISTER(test_zpm_mxr_disables_pm);
bool test_zpm_mxr_disables_pm(void) {
    TEST_BEGIN("ZPM-MXR-01: MXR=1 disables PM");
    if (!detect_smnpm()) TEST_SKIP("Smnpm not implemented");
    pm_set_smode(PMM_PMLEN7);
    if (pm_get_smode() != PMM_PMLEN7) TEST_SKIP("PMLEN=7 not supported");

    /* Enable MXR */
    uintptr_t mstatus = CSRR(mstatus);
    CSRW(mstatus, mstatus | MSTATUS_MXR_BIT);

    /* In S-mode with MXR=1, PM should NOT apply.
     * A tagged load should use the raw tagged address,
     * which likely points to an invalid location -> fault expected.
     *
     * This test verifies PM is suppressed when MXR is active. */

    /* ... setup VM, attempt tagged load, expect fault ... */

    CSRW(mstatus, mstatus);  /* restore */
    pm_set_smode(PMM_DISABLED);
    TEST_END();
}
```

> [!NOTE]
> MXR 规范原文："When MXR is in effect at the effective privilege mode where explicit memory access is performed, pointer masking does not apply." 注意这包括非 VM 模式（Bare）下 MXR 虽然对权限检查无效，但仍影响 PM 是否应用。

---

### Group 10：stval/mtval 硬件写入变换 + trap handler 地址

**规范依据**：
- `norm:pm_csr_hw_apply`：硬件写 CSR 时应用 PM 变换（如异常时写入 stval/mtval 的地址为变换后地址）
- `norm:pm_no_trap_vector_mask`：trap 投递时，trap handler 地址（stvec/mtvec）不受 PM 影响

**测试职责**：验证异常发生时硬件写入 stval/mtval 的地址是否经过 PM 变换，以及 trap 投递时 stvec/mtvec 指向的 handler 地址不受 PM 影响。

#### 10.a：Trap - U-mode stval（Ssnpm）

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZPM-TRAP-01 | stval 包含变换后地址 | U-mode PM=PMLEN7，tagged addr 触发 load page-fault | stval 中的地址为 PM 变换后地址（sign-extended） |
| ZPM-TRAP-05 | stval sign-extend 验证 | PMLEN7 VA 变换，bit56=1，触发 fault | stval bits[63:57] 全为 1 |

#### 10.b：Trap - M-mode mtval（Smmpm）

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZPM-TRAP-02 | mtval 包含变换后地址 | M-mode PM=PMLEN7，tagged addr 触发 access fault | mtval 中的地址为 PM 变换后地址（zero-extended for PA） |

#### 10.c：Trap - S-mode（Smnpm）

| 测试 ID | 测试名称 | 测试描述 | 预期结果 |
|---------|----------|----------|----------|
| ZPM-TRAP-03 | trap handler 不受 PM | PM=PMLEN7，stvec 设为正常地址 | trap 正常投递到 stvec 指定地址（未被 PM 变换） |
| ZPM-TRAP-04 | tagged stvec 仍正常投递 | 将 tagged addr 写入 stvec，触发 trap | trap 投递使用 stvec 原始值（PM 不应用于 trap delivery） |

```c
/* ZPM-TRAP-01 示例：stval 包含变换后地址 */
TEST_REGISTER(test_zpm_trap_stval_transformed);
bool test_zpm_trap_stval_transformed(void) {
    TEST_BEGIN("ZPM-TRAP-01: stval contains transformed address");
    if (!detect_ssnpm()) TEST_SKIP("Ssnpm not implemented");
    pm_set_umode(PMM_PMLEN7);
    if (pm_get_umode() != PMM_PMLEN7) TEST_SKIP("PMLEN=7 not supported");

    /* Setup VM with identity mapping, but leave a page unmapped */
    pt_context_t ctx;
    pt_pool_reset();
    pt_init(&ctx, SATP_MODE_SV39);
    pt_setup_identity_mapping(&ctx, PLATFORM_MEM_BASE, 0x4000000,
        PTE_V|PTE_R|PTE_W|PTE_X|PTE_U|PTE_A|PTE_D, PT_LEVEL_2M);

    /* Use a VA that, after PM transform, maps to a valid address
     * but the raw tagged VA itself is not in any valid mapping.
     * When the fault occurs, stval should contain the transformed addr. */
    uintptr_t unmapped_va = 0x0000004000000000ULL;  /* outside mapping */
    uintptr_t tagged = pm_tag_address(unmapped_va, 0x55, 7);

    /* Expected transformed addr (sign-extend from bit 56) */
    uintptr_t expected_stval = pm_transform_va(tagged, 7);

    /* Execute in U-mode, expect page-fault */
    /* ... check stval == expected_stval after fault ... */

    pm_set_umode(PMM_DISABLED);
    pt_pool_reset();
    TEST_END();
}

/* ZPM-TRAP-03 示例：trap handler 地址不受 PM */
TEST_REGISTER(test_zpm_trap_handler_no_pm);
bool test_zpm_trap_handler_no_pm(void) {
    TEST_BEGIN("ZPM-TRAP-03: trap handler addr not masked");
    if (!detect_smnpm()) TEST_SKIP("Smnpm not implemented");

    /* Enable S-mode PM */
    pm_set_smode(PMM_PMLEN7);

    /* Set stvec to a known handler address */
    uintptr_t handler = (uintptr_t)s_trap_entry;
    CSRW(stvec, handler);

    /* Trigger a trap in S-mode; verify trap is delivered to
     * the exact stvec address (not PM-transformed).
     * If PM were applied to stvec, the handler would be at
     * a different (wrong) address and likely crash. */

    /* ... trigger ecall in S-mode, verify handler executed ... */

    pm_set_smode(PMM_DISABLED);
    TEST_END();
}
```

> [!IMPORTANT]
> ZPM-TRAP-01/02 是验证 PM 对硬件 CSR 写入行为的关键测试。规范明确区分了"软件写 CSR"（不变换）和"硬件写 CSR"（变换）的语义。stval/mtval 中记录的异常地址必须是变换后的地址，这对调试器和异常处理程序至关重要。

---

### 测试用例汇总

| Group | 子组 | 测试 ID 范围 | 用例数 | 覆盖扩展 |
|-------|------|-------------|--------|----------|
| 1 | 1.a | ZPM-CAP-01, 04 | 2 | Ssnpm |
| | 1.b | ZPM-CAP-02, 05 | 2 | Smnpm |
| | 1.c | ZPM-CAP-03, 06 | 2 | Smmpm |
| 2 | 2a | ZPM-CSR-01~05 | 5 | Ssnpm |
| | 2b | ZPM-CSR-06~10 | 5 | Smnpm |
| | 2c | ZPM-CSR-11~15 | 5 | Smmpm |
| 3 | - | ZPM-UVA-01~08 | 8 | Ssnpm |
| 4 | - | ZPM-UAMO-01~04 | 4 | Ssnpm |
| 5 | - | ZPM-SVA-01~09 | 9 | Smnpm |
| 6 | - | ZPM-MPA-01~09 | 9 | Smmpm |
| 7 | 7.a | ZPM-NEG-01~02 | 2 | Ssnpm |
| | 7.b | ZPM-NEG-03~05 | 3 | Smnpm |
| 8 | 8.a | ZPM-MPRV-01~02 | 2 | Smnpm |
| | 8.b | ZPM-MPRV-03 | 1 | Ssnpm |
| | 8.c | ZPM-MPRV-04~05 | 2 | Smmpm |
| 9 | 9.a | ZPM-MXR-01~02 | 2 | Smnpm |
| | 9.b | ZPM-MXR-03~04 | 2 | Smmpm |
| 10 | 10.a | ZPM-TRAP-01, 05 | 2 | Ssnpm |
| | 10.b | ZPM-TRAP-02 | 1 | Smmpm |
| | 10.c | ZPM-TRAP-03~04 | 2 | Smnpm |
| **合计** | | | **70** | Ssnpm 24 / Smnpm 25 / Smmpm 21 |

---

### 依赖的框架组件

| 组件 | 文件 | 用途 |
|------|------|------|
| PM 控制 API | `common/pm/pm_cfg.h` + `pm_cfg.c` | `pm_set_umode()` / `detect_ssnpm()` 等 |
| Tagged Address 工具 | `common/pm/pm_addr.h` | `pm_tag_address()` / `pm_transform_va()` 等 |
| CSR 定义 | `common/encoding.h` | `SENVCFG_PMM_MASK` / `PMM_PMLEN7` 等 |
| U-mode VM 执行 | `common/vm/satp.c` | `vm_run_in_umode()` |
| S-mode VM 执行 | `common/vm/satp.c` | `vm_run_in_smode()` |
| 动态 CSR 访问 | `common/csr_accessors.c` | `senvcfg(0x10A)` / `menvcfg(0x30A)` |
