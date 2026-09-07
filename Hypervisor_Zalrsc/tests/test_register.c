/*
 * Copyright (c) 2026 Alibaba Group.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * test_register.c - Hypervisor x Zalrsc Cross Test Registration
 *
 * All test cases are organized by Group, matching
 * DOCS/testplan/Hypervisor_Za_test_plan.md Group 1 (HZLRSC-01~40).
 *
 * Each test file is #included here so TEST_REGISTER macros place
 * function pointers into the .test_table section for auto-execution.
 * SUITE_VSATP_MODE / SUITE_HGATP_MODE are provided by the Makefile.
 *
 * Execution order:
 *   Group 1.1 (normal exec):        HZLRSC-01~04
 *   Group 1.2 (class split/deleg):  HZLRSC-05~08
 *   Group 1.3 (G-stage/trap ctx):   HZLRSC-09~14
 *   Group 1.4 (htinst):             HZLRSC-15~21
 *   Group 1.5 (SC permission):      HZLRSC-22~26
 *   Group 1.6 (misaligned):         HZLRSC-27~29
 *   Group 1.7 (FIOM/ADUE):          HZLRSC-30~34
 *   Group 1.8 (forward progress):   HZLRSC-35~38
 *   Group 1.9 (boundary):           HZLRSC-39~40
 */

#include "test_helpers.h"

/* --- Group 1.1: HS/VS/VU-mode normal execution + cause=22 exclusion --- */
#include "test_hzlrsc_exec.c"

/* --- Group 1.2: exception-class split + VS-stage delegation path --- */
#include "test_hzlrsc_deleg.c"

/* --- Group 1.3: G-stage faults forced to HS-mode + trap context --- */
#include "test_hzlrsc_gstage.c"

/* --- Group 1.4: htinst transformed atomic vs pseudoinstruction --- */
#include "test_hzlrsc_htinst.c"

/* --- Group 1.5: SC retire permission (failed SC still store-class) --- */
#include "test_hzlrsc_scperm.c"

/* --- Group 1.6: misaligned LR/SC exception path (no MAG relaxation) --- */
#include "test_hzlrsc_align.c"

/* --- Group 1.7: henvcfg.FIOM / ADUE interaction --- */
#include "test_hzlrsc_fiom_adue.c"

/* --- Group 1.8: constrained-loop forward progress + virt trap --- */
#include "test_hzlrsc_progress.c"

/* --- Group 1.9: architectural boundary (no HLR/HSC equivalent) --- */
#include "test_hzlrsc_boundary.c"
