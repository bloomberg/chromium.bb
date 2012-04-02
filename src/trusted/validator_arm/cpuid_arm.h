/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_CPUID_ARM_H
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_CPUID_ARM_H


typedef struct {
  /* Add a dummy field because a zero-sized structure causes skew between C and
   * C++.
   */
  int bogus;
} NaClCPUFeaturesArm;

static INLINE void NaClGetCurrentCPUFeatures(NaClCPUFeaturesArm *cpu_features) {
  UNREFERENCED_PARAMETER(cpu_features);
}

static INLINE void NaClSetAllCPUFeatures(NaClCPUFeaturesArm *cpu_features) {
  UNREFERENCED_PARAMETER(cpu_features);
}

static INLINE void NaClClearCPUFeatures(NaClCPUFeaturesArm *cpu_features) {
  UNREFERENCED_PARAMETER(cpu_features);
}

static INLINE int NaClFixCPUFeatures(NaClCPUFeaturesArm *cpu_features) {
  UNREFERENCED_PARAMETER(cpu_features);
  return 1;
}

#endif /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_CPUID_ARM_H */
