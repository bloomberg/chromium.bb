/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

/*
 * nacl_cpuid_test.c
 * test main and subroutines for nacl_cpuid
 */
#include "native_client/src/include/portability.h"
#include <stdio.h>
#include "native_client/src/trusted/validator/x86/nacl_cpuid.h"

int main(int argc, char *argv[]) {
  NaClCPUFeaturesX86 fv;
  NaClCPUData cpu_data;
  int feature_id;
  NaClCPUDataGet(&cpu_data);

  NaClGetCurrentCPUFeatures(&fv);
  if (NaClArchSupported(&fv)) {
    printf("This is a native client %d-bit %s compatible computer\n",
           NACL_TARGET_SUBARCH, GetCPUIDString(&cpu_data));
  } else {
    if (!fv.arch_features.f_cpuid_supported) {
      printf("Computer doesn't support CPUID\n");
    }
    if (!fv.arch_features.f_cpu_supported) {
      printf("Computer id %s is not supported\n", GetCPUIDString(&cpu_data));
    }
  }

  printf("This processor has:\n");
  for (feature_id = 0; feature_id < NaClCPUFeature_Max; ++feature_id) {
    if (NaClGetCPUFeature(&fv, feature_id))
      printf("        %s\n", NaClGetCPUFeatureName(feature_id));
  }
  return 0;
}
