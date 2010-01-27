/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
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
  bool f_CLFLUSH;
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
