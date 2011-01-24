/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
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
#include "native_client/src/trusted/validator_x86/nacl_cpuid.h"

int main(int argc, char *argv[]) {
  CPUFeatures fv;

  GetCPUFeatures(&fv);
  if (NaClArchSupported()) {
    printf("This is a native client %d-bit %s compatible computer\n",
           NACL_TARGET_SUBARCH, GetCPUIDString());
  } else {
    if (!fv.arch_features.f_cpuid_supported) {
      printf("Computer doesn't support CPUID\n");
    }
    if (!fv.arch_features.f_cpu_supported) {
      printf("Computer id %s is not supported\n", GetCPUIDString());
    }
  }

  printf("This processor has:\n");
  if (fv.f_x87)    printf("        x87\n");
  if (fv.f_MMX)    printf("        MMX\n");
  if (fv.f_SSE)    printf("        SSE\n");
  if (fv.f_SSE2)   printf("        SSE2\n");
  if (fv.f_SSE3)   printf("        SSE3\n");
  if (fv.f_SSSE3)  printf("        SSSE3\n");
  if (fv.f_SSE41)  printf("        SSE41\n");
  if (fv.f_SSE42)  printf("        SSE42\n");
  if (fv.f_MOVBE)  printf("        MOVBE\n");
  if (fv.f_POPCNT) printf("        POPCNT\n");
  if (fv.f_CX8)    printf("        CX8\n");
  if (fv.f_CX16)   printf("        CX16\n");
  if (fv.f_CMOV)   printf("        CMOV\n");
  if (fv.f_MON)    printf("        MON\n");
  if (fv.f_FXSR)   printf("        FXSR\n");
  if (fv.f_CLFLUSH) printf("        CLFLUSH\n");
  if (fv.f_TSC)    printf("        TSC\n");
  /* These instructions are illegal but included for completeness */
  if (fv.f_MSR)    printf("        MSR\n");
  if (fv.f_VME)    printf("        VME\n");
  if (fv.f_PSN)    printf("        PSN\n");
  if (fv.f_VMX)    printf("        VMX\n");
  /* AMD-specific features */
  if (fv.f_3DNOW)  printf("        3DNOW\n");
  if (fv.f_EMMX)   printf("        EMMX\n");
  if (fv.f_E3DNOW) printf("        E3DNOW\n");
  if (fv.f_LZCNT)  printf("        LZCNT\n");
  if (fv.f_SSE4A)  printf("        SSE4A\n");
  if (fv.f_LM)     printf("        LM\n");
  if (fv.f_SVM)    printf("        SVM\n");

  return 0;
}
