/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/validator/x86/nacl_cpuid.h"

/* All supported features are enabled */
const NaClCPUFeaturesX86 full_cpuid_features = { { 1, 1 }, {
  1, /* NaClCPUFeature_3DNOW */  /* AMD-specific */
  1, /* NaClCPUFeature_AES */
  1, /* NaClCPUFeature_AVX */
  1, /* NaClCPUFeature_BMI1 */
  1, /* NaClCPUFeature_CLFLUSH */
  1, /* NaClCPUFeature_CLMUL */
  1, /* NaClCPUFeature_CMOV */
  1, /* NaClCPUFeature_CX16 */
  1, /* NaClCPUFeature_CX8 */
  1, /* NaClCPUFeature_E3DNOW */ /* AMD-specific */
  1, /* NaClCPUFeature_EMMX */   /* AMD-specific */
  1, /* NaClCPUFeature_F16C */
  1, /* NaClCPUFeature_FMA */
  1, /* NaClCPUFeature_FMA4 */ /* AMD-specific */
  1, /* NaClCPUFeature_FXSR */
  1, /* NaClCPUFeature_LAHF */
  1, /* NaClCPUFeature_LM */
  1, /* NaClCPUFeature_LWP */ /* AMD-specific */
  1, /* NaClCPUFeature_LZCNT */  /* AMD-specific */
  1, /* NaClCPUFeature_MMX */
  1, /* NaClCPUFeature_MON */
  1, /* NaClCPUFeature_MOVBE */
  1, /* NaClCPUFeature_OSXSAVE */
  1, /* NaClCPUFeature_POPCNT */
  1, /* NaClCPUFeature_PRE */ /* AMD-specific */
  1, /* NaClCPUFeature_SSE */
  1, /* NaClCPUFeature_SSE2 */
  1, /* NaClCPUFeature_SSE3 */
  1, /* NaClCPUFeature_SSE41 */
  1, /* NaClCPUFeature_SSE42 */
  1, /* NaClCPUFeature_SSE4A */  /* AMD-specific */
  1, /* NaClCPUFeature_SSSE3 */
  1, /* NaClCPUFeature_TBM */ /* AMD-specific */
  1, /* NaClCPUFeature_TSC */
  1, /* NaClCPUFeature_x87 */
  1  /* NaClCPUFeature_XOP */ /* AMD-specific */
} };
