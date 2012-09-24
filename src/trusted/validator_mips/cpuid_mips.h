/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_MIPS_CPUID_MIPS_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_MIPS_CPUID_MIPS_H_


typedef struct {
  /* Add a dummy field because a zero-sized structure causes skew between C and
   * C++.
   */
  int bogus;
} NaClCPUFeaturesMips;

static INLINE void NaClGetCurrentCPUFeatures(
     NaClCPUFeaturesMips *cpu_features) {
  UNREFERENCED_PARAMETER(cpu_features);
}

static INLINE void NaClSetAllCPUFeatures(NaClCPUFeaturesMips *cpu_features) {
  UNREFERENCED_PARAMETER(cpu_features);
}

static INLINE void NaClClearCPUFeatures(NaClCPUFeaturesMips *cpu_features) {
  UNREFERENCED_PARAMETER(cpu_features);
}

static INLINE int NaClFixCPUFeatures(NaClCPUFeaturesMips *cpu_features) {
  UNREFERENCED_PARAMETER(cpu_features);
  return 1;
}

#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_MIPS_CPUID_MIPS_H_
