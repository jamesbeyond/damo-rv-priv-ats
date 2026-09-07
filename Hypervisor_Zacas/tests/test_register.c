/*
 * Copyright (c) 2026 Alibaba Group.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * test_register.c - Hypervisor x Zacas Cross Test Registration
 *
 * All test cases are organized by Group, matching
 * DOCS/testplan/Hypervisor_Za_test_plan.md Group 3 (HZACAS-01~36).
 * SUITE_VSATP_MODE / SUITE_HGATP_MODE are provided by the Makefile.
 *
 * Execution order:
 *   Group 3.1 (normal exec):        HZACAS-01~04
 *   Group 3.2 (store/AMO class):    HZACAS-05~10
 *   Group 3.3 (G-stage/trap ctx):   HZACAS-11~16
 *   Group 3.4 (htinst):             HZACAS-17~22
 *   Group 3.5 (misaligned/MAG):     HZACAS-23~26
 *   Group 3.6 (FIOM/ADUE):          HZACAS-27~33
 *   Group 3.7 (boundary):           HZACAS-34~35
 *   Group 3.8 (hstateen0):          HZACAS-36
 */

#include "test_helpers.h"

/* --- Group 3.1: HS/VS/VU-mode normal execution + cause=22 exclusion --- */
#include "test_hzacas_exec.c"

/* --- Group 3.2: amocas -> store/AMO class + failed-CAS write perm --- */
#include "test_hzacas_deleg.c"

/* --- Group 3.3: G-stage faults forced to HS-mode + trap context --- */
#include "test_hzacas_gstage.c"

/* --- Group 3.4: htinst transformed atomic vs pseudoinstruction --- */
#include "test_hzacas_htinst.c"

/* --- Group 3.5: misaligned amocas and MAG relaxation --- */
#include "test_hzacas_align_mag.c"

/* --- Group 3.6: henvcfg.FIOM / ADUE interaction --- */
#include "test_hzacas_fiom_adue.c"

/* --- Group 3.7: architectural boundary (no amocas guest equivalent) --- */
#include "test_hzacas_boundary.c"

/* --- Group 3.8: hstateen0 does not gate amocas (HZACAS-36) --- */
#include "test_hzacas_stateen.c"
