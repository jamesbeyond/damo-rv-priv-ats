/*
 * Copyright (c) 2026 Alibaba Group.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * test_register.c - Hypervisor x Zca Cross Test Registration
 *
 * All test cases are organized by Group, matching
 * DOCS/testplan_en/Hypervisor_Zc_test_plan_en.md Group 1 (HZCA-01~29).
 *
 * Each test file is #included here so TEST_REGISTER macros place
 * function pointers into the .test_table section for auto-execution.
 * SUITE_VSATP_MODE / SUITE_HGATP_MODE are provided by the Makefile.
 *
 * Execution order:
 *   Group 1.1 (normal exec):        HZCA-01~06
 *   Group 1.2 (reg-based htinst):   HZCA-07~10
 *   Group 1.3 (sp-based htinst):    HZCA-11~14
 *   Group 1.4 (format distinction): HZCA-15~17
 *   Group 1.5 (Addr. Offset/zero):  HZCA-18~20
 *   Group 1.6 (mtinst M-mode):      HZCA-21~23
 *   Group 1.7 (interrupt zero):     HZCA-24
 *   Group 1.8 (IALIGN=16):          HZCA-25~27
 *   Group 1.9 (fetch-class):        HZCA-28~29
 */

#include "test_helpers.h"

/* --- Group 1.1: HS/VS/VU-mode compressed exec + cause=22 exclusion --- */
#include "test_hzca_exec.c"

/* --- Group 1.2 ~ 1.5: htinst transformed of compressed load/store --- */
#include "test_hzca_htinst.c"

/* --- Group 1.6 ~ 1.7: mtinst on M-mode trap + interrupt writes zero --- */
#include "test_hzca_mtinst.c"

/* --- Group 1.8: IALIGN=16 + instruction-address-misaligned exclusion --- */
#include "test_hzca_ialign.c"

/* --- Group 1.9: fetch-class exceptions do not write a transformed htinst --- */
#include "test_hzca_fetch.c"
