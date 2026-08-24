/*
 * Copyright (c) 2026 Alibaba Group.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef COMMON_MEM_OPS_H
#define COMMON_MEM_OPS_H

#include "types.h"

/* LOAD/STORE macros for use in inline asm (adapt to XLEN) */
#if __riscv_xlen == 64
#define STORE "sd"
#define LOAD  "ld"
#else
#define STORE "sw"
#define LOAD  "lw"
#endif

/* ===================================================================
 * Memory Operation Primitives
 *
 * All load/store operations use .option norvc to ensure non-compressed
 * instructions (4 bytes each), so the trap handler can reliably skip
 * faulting instructions with mepc += 4.
 *
 * The 'volatile' and memory clobber prevent compiler reordering.
 * =================================================================== */

/* ===== Load operations ===== */

static inline uint8_t mem_load8(uintptr_t addr) {
    uint8_t val;
    asm volatile(
        ".option push\n\t"
        ".option norvc\n\t"
        "lb %0, 0(%1)\n\t"
        ".option pop\n\t"
        : "=r"(val) : "r"(addr) : "memory"
    );
    return val;
}

static inline uint16_t mem_load16(uintptr_t addr) {
    uint16_t val;
    asm volatile(
        ".option push\n\t"
        ".option norvc\n\t"
        "lh %0, 0(%1)\n\t"
        ".option pop\n\t"
        : "=r"(val) : "r"(addr) : "memory"
    );
    return val;
}

static inline uint32_t mem_load32(uintptr_t addr) {
    uint32_t val;
    asm volatile(
        ".option push\n\t"
        ".option norvc\n\t"
        "lw %0, 0(%1)\n\t"
        ".option pop\n\t"
        : "=r"(val) : "r"(addr) : "memory"
    );
    return val;
}

#if __riscv_xlen == 64
static inline uint64_t mem_load64(uintptr_t addr) {
    uint64_t val;
    asm volatile(
        ".option push\n\t"
        ".option norvc\n\t"
        "ld %0, 0(%1)\n\t"
        ".option pop\n\t"
        : "=r"(val) : "r"(addr) : "memory"
    );
    return val;
}
#else
static inline uint64_t mem_load64(uintptr_t addr) {
    uint32_t lo = mem_load32(addr);
    uint32_t hi = mem_load32(addr + 4);
    return ((uint64_t)hi << 32) | lo;
}
#endif

/* ===== Store operations ===== */

static inline void mem_store8(uintptr_t addr, uint8_t val) {
    asm volatile(
        ".option push\n\t"
        ".option norvc\n\t"
        "sb %0, 0(%1)\n\t"
        ".option pop\n\t"
        :: "r"(val), "r"(addr) : "memory"
    );
}

static inline void mem_store16(uintptr_t addr, uint16_t val) {
    asm volatile(
        ".option push\n\t"
        ".option norvc\n\t"
        "sh %0, 0(%1)\n\t"
        ".option pop\n\t"
        :: "r"(val), "r"(addr) : "memory"
    );
}

static inline void mem_store32(uintptr_t addr, uint32_t val) {
    asm volatile(
        ".option push\n\t"
        ".option norvc\n\t"
        "sw %0, 0(%1)\n\t"
        ".option pop\n\t"
        :: "r"(val), "r"(addr) : "memory"
    );
}

#if __riscv_xlen == 64
static inline void mem_store64(uintptr_t addr, uint64_t val) {
    asm volatile(
        ".option push\n\t"
        ".option norvc\n\t"
        "sd %0, 0(%1)\n\t"
        ".option pop\n\t"
        :: "r"(val), "r"(addr) : "memory"
    );
}
#else
static inline void mem_store64(uintptr_t addr, uint64_t val) {
    mem_store32(addr, (uint32_t)val);
    mem_store32(addr + 4, (uint32_t)(val >> 32));
}
#endif

/* XLEN-width convenience aliases */
#if __riscv_xlen == 64
#define mem_load_xlen   mem_load64
#define mem_store_xlen  mem_store64
#else
#define mem_load_xlen   mem_load32
#define mem_store_xlen  mem_store32
#endif

/* ===== Execute operation ===== */

/*
 * exec_at - Jump to addr and execute instructions there.
 *
 * The code at addr should be nop;ret (filled by entry.S).
 * Before jumping, we save the return address in trap_record.return_addr
 * so the trap handler can recover if an instruction access fault occurs.
 *
 * Implementation:
 *   1. Save recovery label address to trap_record.return_addr
 *   2. jalr x0, addr (jump without saving ra, since we use recovery label)
 *   3. Recovery label: clear return_addr
 */
static inline void exec_at(uintptr_t addr) {
    asm volatile(
        ".option push\n\t"
        ".option norvc\n\t"
        "la t0, 1f\n\t"                /* t0 = recovery label */
        "la t1, _exec_return_addr\n\t"  /* t1 = &_exec_return_addr */
        STORE " t0, 0(t1)\n\t"         /* save recovery address */
        "mv ra, t0\n\t"                /* ra = recovery (for normal ret) */
        "jr %0\n\t"                     /* jump to test address */
        "1:\n\t"                        /* recovery label */
        STORE " zero, 0(t1)\n\t"        /* clear recovery address */
        ".option pop\n\t"
        :: "r"(addr)
        : "t0", "t1", "ra", "memory"
    );
}

/* ===== AMO operations ===== */

static inline uint32_t mem_amo_swap_w(uintptr_t addr, uint32_t val) {
    uint32_t result;
    asm volatile(
        ".option push\n\t"
        ".option norvc\n\t"
        "amoswap.w %0, %1, (%2)\n\t"
        ".option pop\n\t"
        : "=r"(result) : "r"(val), "r"(addr) : "memory"
    );
    return result;
}

static inline uint32_t mem_lr_w(uintptr_t addr) {
    uint32_t val;
    asm volatile(
        ".option push\n\t"
        ".option norvc\n\t"
        "lr.w %0, (%1)\n\t"
        ".option pop\n\t"
        : "=r"(val) : "r"(addr) : "memory"
    );
    return val;
}

static inline uint32_t mem_sc_w(uintptr_t addr, uint32_t val) {
    uint32_t result;
    asm volatile(
        ".option push\n\t"
        ".option norvc\n\t"
        "sc.w %0, %1, (%2)\n\t"
        ".option pop\n\t"
        : "=r"(result) : "r"(val), "r"(addr) : "memory"
    );
    return result;
}

#if __riscv_xlen == 64
static inline uint64_t mem_lr_d(uintptr_t addr) {
    uint64_t val;
    asm volatile(
        ".option push\n\t"
        ".option norvc\n\t"
        "lr.d %0, (%1)\n\t"
        ".option pop\n\t"
        : "=r"(val) : "r"(addr) : "memory"
    );
    return val;
}

static inline uint64_t mem_sc_d(uintptr_t addr, uint64_t val) {
    uint64_t result;
    asm volatile(
        ".option push\n\t"
        ".option norvc\n\t"
        "sc.d %0, %1, (%2)\n\t"
        ".option pop\n\t"
        : "=r"(result) : "r"(val), "r"(addr) : "memory"
    );
    return result;
}
#endif /* __riscv_xlen == 64 */

/* ===== AMO operations (full A-extension AMO set) =====
 *
 * Every helper returns the value loaded from memory *before* the
 * read-modify-write took place (the AMO rd semantics). All forms
 * use .option norvc so the trap handler can skip faulting AMOs with
 * sepc += 4. The doubleword (.d) forms exist only on RV64.
 */

#define _MEM_AMO_OP_W(op, addr, val) ({ \
    uint32_t _r; \
    uint32_t _v = (uint32_t)(val); \
    asm volatile( \
        ".option push\n\t" \
        ".option norvc\n\t" \
        op " %0, %1, (%2)\n\t" \
        ".option pop\n\t" \
        : "=r"(_r) : "r"(_v), "r"(addr) : "memory" \
    ); \
    _r; \
})

#if __riscv_xlen == 64
#define _MEM_AMO_OP_D(op, addr, val) ({ \
    uint64_t _r; \
    uint64_t _v = (uint64_t)(val); \
    asm volatile( \
        ".option push\n\t" \
        ".option norvc\n\t" \
        op " %0, %1, (%2)\n\t" \
        ".option pop\n\t" \
        : "=r"(_r) : "r"(_v), "r"(addr) : "memory" \
    ); \
    _r; \
})
#endif

static inline uint32_t mem_amo_add_w(uintptr_t addr, uint32_t val) {
    return _MEM_AMO_OP_W("amoadd.w", addr, val);
}

static inline uint32_t mem_amo_and_w(uintptr_t addr, uint32_t val) {
    return _MEM_AMO_OP_W("amoand.w", addr, val);
}

static inline uint32_t mem_amo_or_w(uintptr_t addr, uint32_t val) {
    return _MEM_AMO_OP_W("amoor.w", addr, val);
}

static inline uint32_t mem_amo_xor_w(uintptr_t addr, uint32_t val) {
    return _MEM_AMO_OP_W("amoxor.w", addr, val);
}

static inline uint32_t mem_amo_min_w(uintptr_t addr, uint32_t val) {
    return _MEM_AMO_OP_W("amomin.w", addr, val);
}

static inline uint32_t mem_amo_max_w(uintptr_t addr, uint32_t val) {
    return _MEM_AMO_OP_W("amomax.w", addr, val);
}

static inline uint32_t mem_amo_minu_w(uintptr_t addr, uint32_t val) {
    return _MEM_AMO_OP_W("amominu.w", addr, val);
}

static inline uint32_t mem_amo_maxu_w(uintptr_t addr, uint32_t val) {
    return _MEM_AMO_OP_W("amomaxu.w", addr, val);
}

#if __riscv_xlen == 64
static inline uint64_t mem_amo_swap_d(uintptr_t addr, uint64_t val) {
    return _MEM_AMO_OP_D("amoswap.d", addr, val);
}

static inline uint64_t mem_amo_add_d(uintptr_t addr, uint64_t val) {
    return _MEM_AMO_OP_D("amoadd.d", addr, val);
}

static inline uint64_t mem_amo_and_d(uintptr_t addr, uint64_t val) {
    return _MEM_AMO_OP_D("amoand.d", addr, val);
}

static inline uint64_t mem_amo_or_d(uintptr_t addr, uint64_t val) {
    return _MEM_AMO_OP_D("amoor.d", addr, val);
}

static inline uint64_t mem_amo_xor_d(uintptr_t addr, uint64_t val) {
    return _MEM_AMO_OP_D("amoxor.d", addr, val);
}

static inline uint64_t mem_amo_min_d(uintptr_t addr, uint64_t val) {
    return _MEM_AMO_OP_D("amomin.d", addr, val);
}

static inline uint64_t mem_amo_max_d(uintptr_t addr, uint64_t val) {
    return _MEM_AMO_OP_D("amomax.d", addr, val);
}

static inline uint64_t mem_amo_minu_d(uintptr_t addr, uint64_t val) {
    return _MEM_AMO_OP_D("amominu.d", addr, val);
}

static inline uint64_t mem_amo_maxu_d(uintptr_t addr, uint64_t val) {
    return _MEM_AMO_OP_D("amomaxu.d", addr, val);
}
#endif /* __riscv_xlen == 64 */

#endif /* COMMON_MEM_OPS_H */
