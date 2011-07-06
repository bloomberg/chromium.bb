/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVAL_SEG_SFI_NCVALIDATE_INTERNALTYPES_H__
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVAL_SEG_SFI_NCVALIDATE_INTERNALTYPES_H__

/*
 * ncvalidate_internaltypes.h
 * Type declarations intimate to ncvalidate.h, exposed for testing.
 *
 */
#include "native_client/src/trusted/validator/x86/nacl_cpuid.h"
#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncdecode.h"

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

/* We track instructions in a three-entry circular buffer,
 * allowing us to see the two previous instructions and to
 * check the safe call sequence. I rounded up to
 * four so we can use a mask, even though we only need to
 * remember three instructions.
 * This is #defined rather than const int because it is used
 * as an array dimension
 */
#define kNCValidatorInstBufferSize 4

/* put all formerly global data into a struct */
typedef struct NCValidatorState {
  /* NOTE: Decoder state (dstate) must appear first so that we can use it like
   * C++ inheritance, where a pointer to a validator state will be the
   * same as a pointer to a decoder state.
   */
  NCDecoderState dstate;
  NCDecoderInst inst_buffer[kNCValidatorInstBufferSize];
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

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVAL_SEG_SFI_NCVALIDATE_INTERNALTYPES_H__ */
