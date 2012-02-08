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
  CPUFeatures fv;
  NaClCPUData cpu_data;
  NaClCPUDataGet(&cpu_data);

  GetCPUFeatures(&cpu_data, &fv);
  if (NaClArchSupported(&cpu_data)) {
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
  if (NaClGetCPUFeature(&fv, NaClCPUFeature_x87))    printf("        x87\n");
  if (NaClGetCPUFeature(&fv, NaClCPUFeature_MMX))    printf("        MMX\n");
  if (NaClGetCPUFeature(&fv, NaClCPUFeature_SSE))    printf("        SSE\n");
  if (NaClGetCPUFeature(&fv, NaClCPUFeature_SSE2))   printf("        SSE2\n");
  if (NaClGetCPUFeature(&fv, NaClCPUFeature_SSE3))   printf("        SSE3\n");
  if (NaClGetCPUFeature(&fv, NaClCPUFeature_SSSE3))  printf("        SSSE3\n");
  if (NaClGetCPUFeature(&fv, NaClCPUFeature_SSE41))  printf("        SSE41\n");
  if (NaClGetCPUFeature(&fv, NaClCPUFeature_SSE42))  printf("        SSE42\n");
  if (NaClGetCPUFeature(&fv, NaClCPUFeature_MOVBE))  printf("        MOVBE\n");
  if (NaClGetCPUFeature(&fv, NaClCPUFeature_POPCNT)) printf("        POPCNT\n");
  if (NaClGetCPUFeature(&fv, NaClCPUFeature_CX8))    printf("        CX8\n");
  if (NaClGetCPUFeature(&fv, NaClCPUFeature_CX16))   printf("        CX16\n");
  if (NaClGetCPUFeature(&fv, NaClCPUFeature_CMOV))   printf("        CMOV\n");
  if (NaClGetCPUFeature(&fv, NaClCPUFeature_MON))    printf("        MON\n");
  if (NaClGetCPUFeature(&fv, NaClCPUFeature_FXSR))   printf("        FXSR\n");
  if (NaClGetCPUFeature(&fv, NaClCPUFeature_CLFLUSH))
    printf("        CLFLUSH\n");
  if (NaClGetCPUFeature(&fv, NaClCPUFeature_TSC))    printf("        TSC\n");
  if (NaClGetCPUFeature(&fv, NaClCPUFeature_MSR))    printf("        MSR\n");
  if (NaClGetCPUFeature(&fv, NaClCPUFeature_VME))    printf("        VME\n");
  if (NaClGetCPUFeature(&fv, NaClCPUFeature_PSN))    printf("        PSN\n");
  if (NaClGetCPUFeature(&fv, NaClCPUFeature_VMX))    printf("        VMX\n");
  if (NaClGetCPUFeature(&fv, NaClCPUFeature_3DNOW))  printf("        3DNOW\n");
  if (NaClGetCPUFeature(&fv, NaClCPUFeature_EMMX))   printf("        EMMX\n");
  if (NaClGetCPUFeature(&fv, NaClCPUFeature_E3DNOW)) printf("        E3DNOW\n");
  if (NaClGetCPUFeature(&fv, NaClCPUFeature_LZCNT))  printf("        LZCNT\n");
  if (NaClGetCPUFeature(&fv, NaClCPUFeature_SSE4A))  printf("        SSE4A\n");
  if (NaClGetCPUFeature(&fv, NaClCPUFeature_LM))     printf("        LM\n");
  if (NaClGetCPUFeature(&fv, NaClCPUFeature_SVM))    printf("        SVM\n");

  return 0;
}
