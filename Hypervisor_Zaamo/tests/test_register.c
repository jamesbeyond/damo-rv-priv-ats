/*
 * Copyright (c) 2026 Alibaba Group.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * test_register.c - Hypervisor x Zaamo Cross Test Registration
 *
 * All test cases are organized by Group, matching
 * DOCS/testplan/Hypervisor_Za_test_plan.md Group 2 (HZAMO-01~31).
 * SUITE_VSATP_MODE / SUITE_HGATP_MODE are provided by the Makefile.
 *
 * Execution order:
 *   Group 2.1 (normal exec):        HZAMO-01~04
 *   Group 2.2 (store/AMO class):    HZAMO-05~09
 *   Group 2.3 (G-stage/trap ctx):   HZAMO-10~14
 *   Group 2.4 (htinst):             HZAMO-15~20
 *   Group 2.5 (misaligned/MAG):     HZAMO-21~24
 *   Group 2.6 (FIOM/ADUE):          HZAMO-25~29
 *   Group 2.7 (boundary):           HZAMO-30~31
 */

#include "test_helpers.h"

/* --- Group 2.1: HS/VS/VU-mode normal execution + cause=22 exclusion --- */
#include "test_hzamo_exec.c"

/* --- Group 2.2: AMO -> store/AMO class + VS-stage delegation --- */
#include "test_hzamo_deleg.c"

/* --- Group 2.3: G-stage faults forced to HS-mode + trap context --- */
#include "test_hzamo_gstage.c"

/* --- Group 2.4: htinst transformed atomic vs pseudoinstruction --- */
#include "test_hzamo_htinst.c"

/* --- Group 2.5: misaligned AMO and MAG relaxation --- */
#include "test_hzamo_align_mag.c"

/* --- Group 2.6: henvcfg.FIOM / ADUE interaction --- */
#include "test_hzamo_fiom_adue.c"

/* --- Group 2.7: architectural boundary (no AMO guest equivalent) --- */
#include "test_hzamo_boundary.c"
