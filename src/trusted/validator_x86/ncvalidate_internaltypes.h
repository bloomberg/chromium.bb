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

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVALIDATE_INTERNALTYPES_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVALIDATE_INTERNALTYPES_H_
/*
 * ncvalidate_internaltypes.h
 * Type declarations intimate to ncvalidate.h, exposed for testing.
 *
 */
#include "native_client/src/trusted/validator_x86/nacl_cpuid.h"

/* statistics */
struct SummaryStats {
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
  uint32_t missingfullstop;
  uint32_t internalerrors;
  uint32_t badcpu;
  int sawfailure;          /* boolean */
};

/* put all formerly global data into a struct */
struct NCValidatorState {
  CPUFeatures cpufeatures;  /* from CPUID bit masks; see nacl_cpuid.c */
  uint32_t iadrbase;
  uint32_t iadrlimit;
  uint8_t alignment;
  uint32_t alignmask;
  struct SummaryStats stats;
  uint32_t opcodehisto[256];
  uint8_t *vttable;
  uint8_t *kttable;
};

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVALIDATE_INTERNALTYPES_H_*/
