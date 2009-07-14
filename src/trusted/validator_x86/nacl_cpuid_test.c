/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
  if (fv.f_386) printf("This is a 386 compatible computer\n");

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
  if (fv.f_CFLUSH) printf("        CFLUSH\n");
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
