/*
 * Copyright (c) 2026 Alibaba Group.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * test_register.c - Hypervisor x Zabha Cross Test Registration
 *
 * All test cases are organized by Group, matching
 * DOCS/testplan/Hypervisor_Za_test_plan.md Group 4 (HZABHA-01~39).
 * SUITE_VSATP_MODE / SUITE_HGATP_MODE are provided by the Makefile.
 *
 * Execution order:
 *   Group 4.1 (normal exec):        HZABHA-01~04
 *   Group 4.2 (store/AMO class):    HZABHA-05~09
 *   Group 4.3 (G-stage/trap ctx):   HZABHA-10~14
 *   Group 4.4 (htinst):             HZABHA-15~21
 *   Group 4.5 (alignment/MAG):      HZABHA-22~26
 *   Group 4.6 (FIOM/ADUE):          HZABHA-27~31
 *   Group 4.7 (amocas.b/h):         HZABHA-32~36
 *   Group 4.8 (reserved lr/sc):     HZABHA-37
 *   Group 4.9 (boundary):           HZABHA-38~39
 */

#include "test_helpers.h"

/* --- Group 4.1: HS/VS/VU-mode normal execution + cause=22 exclusion --- */
#include "test_hzabha_exec.c"

/* --- Group 4.2: byte/half AMO -> store/AMO class + width-indep perm --- */
#include "test_hzabha_deleg.c"

/* --- Group 4.3: G-stage faults forced to HS-mode + trap context --- */
#include "test_hzabha_gstage.c"

/* --- Group 4.4: htinst transformed atomic (funct3 width retention) --- */
#include "test_hzabha_htinst.c"

/* --- Group 4.5: byte AMO never misaligned; half AMO misaligned/MAG --- */
#include "test_hzabha_align.c"

/* --- Group 4.6: henvcfg.FIOM / ADUE interaction --- */
#include "test_hzabha_fiom_adue.c"

/* --- Group 4.7: amocas.b/h (Zabha x Zacas, conditional) --- */
#include "test_hzabha_amocas_bh.c"

/* --- Group 4.8: reserved byte/half lr/sc -> illegal-instruction --- */
#include "test_hzabha_reserved.c"

/* --- Group 4.9: architectural boundary (no byte/half guest atomic) --- */
#include "test_hzabha_boundary.c"
