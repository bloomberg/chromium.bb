/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVALIDATE_INTERNALTYPES_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVALIDATE_INTERNALTYPES_H_
/*
 * ncvalidate_internaltypes.h
 * Type declarations intimate to ncvalidate.h, exposed for testing.
 *
 */
#include "native_client/src/trusted/validator_x86/nacl_cpuid.h"
#include "native_client/src/trusted/validator_x86/ncdecode.h"

/* statistics */
typedef struct SummaryStats {
  /* these are just information */
  uint32_t instructions;
  uint32_t checktarget;
  uint32_t targetindirect;
  uint32_t segments;
  /* the following indicate safety defects */
  uint32_t badtarget;
  uint32_t unsafeindirect;
  uint32_t returns;
  uint32_t illegalinst;
  uint32_t badalignment;
  uint32_t segfaults;
  uint32_t badprefix;
  uint32_t badinstlength;
  uint32_t internalerrors;
  int sawfailure;          /* boolean */
} SummaryStats;

/* put all formerly global data into a struct */
typedef struct NCValidatorState {
  /* NOTE: Decoder state (dstate) must appear first so that we can use it like
   * C++ inheritance, where a pointer to a validator state will be the
   * same as a pointer to a decoder state.
   */
  NCDecoderState dstate;
  CPUFeatures cpufeatures;  /* from CPUID bit masks; see nacl_cpuid.c */
  NaClPcAddress iadrbase;
  NaClPcAddress iadrlimit;
  uint8_t alignment;
  uint32_t alignmask;
  SummaryStats stats;
  uint32_t opcodehisto[256];
  uint8_t *vttable;
  uint8_t *kttable;
  int do_stub_out;  /* boolean */
  int num_diagnostics; /* How many error messages to print. */
} NCValidatorState;

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVALIDATE_INTERNALTYPES_H_*/
