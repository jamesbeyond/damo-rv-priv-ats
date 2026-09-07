/*
 * Copyright (c) 2026 Alibaba Group.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * test_register.c - Hypervisor x Zalasr Cross Test Registration
 *
 * All test cases are organized by Group, matching
 * DOCS/testplan/Hypervisor_Za_test_plan.md Group 5 (HZLASR-01~36).
 * SUITE_VSATP_MODE / SUITE_HGATP_MODE are provided by the Makefile.
 *
 * Execution order:
 *   Group 5.1 (normal exec):        HZLASR-01~04
 *   Group 5.2 (class split / perm): HZLASR-05~11
 *   Group 5.3 (G-stage/trap ctx):   HZLASR-12~17
 *   Group 5.4 (htinst):             HZLASR-18~24
 *   Group 5.5 (misaligned/MAG):     HZLASR-25~28
 *   Group 5.6 (FIOM/ADUE):          HZLASR-29~33
 *   Group 5.7 (reserved encoding):  HZLASR-34
 *   Group 5.8 (boundary):           HZLASR-35~36
 */

#include "test_helpers.h"

/* --- Group 5.1: HS/VS/VU-mode normal execution + cause=22 exclusion --- */
#include "test_hzlasr_exec.c"

/* --- Group 5.2: exception-class split + permission requirements --- */
#include "test_hzlasr_deleg.c"

/* --- Group 5.3: G-stage faults forced to HS-mode + trap context --- */
#include "test_hzlasr_gstage.c"

/* --- Group 5.4: htinst transformed atomic (opcode 0x2F) vs pseudoinst --- */
#include "test_hzlasr_htinst.c"

/* --- Group 5.5: misaligned load-acquire/store-release + MAG relaxation --- */
#include "test_hzlasr_align.c"

/* --- Group 5.6: henvcfg.FIOM / ADUE interaction --- */
#include "test_hzlasr_fiom_adue.c"

/* --- Group 5.7: reserved encodings -> illegal-instruction (cause=2) --- */
#include "test_hzlasr_reserved.c"

/* --- Group 5.8: architectural boundary (no atomic-ordered guest equiv) --- */
#include "test_hzlasr_boundary.c"
