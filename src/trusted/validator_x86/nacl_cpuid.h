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
 * cpuid.c
 * This module provides a simple abstraction for using the CPUID
 * instruction to determine instruction set extensions supported by
 * the current processor.
 */
#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NACL_CPUID_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NACL_CPUID_H_

typedef unsigned char bool;

typedef struct cpu_feature_struct {
  bool f_386;  /* a CPU that doesn't have f_386 shouldn't be trusted. */
  bool f_x87;
  bool f_MMX;
  bool f_SSE;
  bool f_SSE2;
  bool f_SSE3;
  bool f_SSSE3;
  bool f_SSE41;
  bool f_SSE42;
  bool f_MOVBE;
  bool f_POPCNT;
  bool f_CX8;
  bool f_CX16;
  bool f_CMOV;
  bool f_MON;
  bool f_FXSR;
  bool f_CFLUSH;
  bool f_TSC;
  /* These instructions are illegal but included for completeness */
  bool f_MSR;
  bool f_VME;
  bool f_PSN;
  bool f_VMX;
  /* AMD-specific features */
  bool f_3DNOW;
  bool f_EMMX;
  bool f_E3DNOW;
  bool f_LZCNT;
  bool f_SSE4A;
  bool f_LM;
  bool f_SVM;
} CPUFeatures;

#define /* static const int */ kCPUIDStringLength 21

/* Fills in cpuf with feature vector for this CPU. */
extern void GetCPUFeatures(CPUFeatures *cpuf);
/* This returns a string of length kCPUIDStringLength */
extern char *GetCPUIDString();

#endif /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NACL_CPUID_H_ */
