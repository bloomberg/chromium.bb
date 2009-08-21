/*
 * Copyright 2009, Google Inc.
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
 * Checks that CPU ID features match instructions found in executable.
 */

#include <stdlib.h>
#include <string.h>

#include "native_client/src/trusted/validator_x86/nc_cpu_checks.h"

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator_x86/nc_inst_iter.h"
#include "native_client/src/trusted/validator_x86/nc_inst_state.h"
#include "native_client/src/trusted/validator_x86/nc_inst_state_internal.h"
#include "native_client/src/trusted/validator_x86/nc_segment.h"
#include "native_client/src/trusted/validator_x86/nacl_cpuid.h"
#include "native_client/src/trusted/validator_x86/ncvalidate_iter.h"
#include "native_client/src/trusted/validator_x86/ncvalidate_iter_internal.h"

/* Define the stop instruction. */
static const uint8_t kNaClFullStop = 0xf4;   /* x86 HALT opcode */

typedef struct NcCpuCheckState {
  /* The standard CPU features. */
  CPUFeatures cpu_features;
  /* Check that both f_CMOV and f_x87 is defined. */
  Bool f_CMOV_and_x87;
  /* Check that either f_MMX or f_SSE2 is defined. */
  Bool f_MMX_or_SSE2;
} NcCpuCheckState;

NcCpuCheckState* NcCpuCheckMemoryCreate(NcValidatorState* state) {
  return (NcCpuCheckState*) calloc(1, sizeof(NcCpuCheckState));
}

void NcCpuCheckMemoryDestroy(NcValidatorState* state,
                             NcCpuCheckState* checked_features) {
  free(checked_features);
}

/* Helper macro to report unsupported features */
#define CHECK_FEATURE(feature, feature_name) \
  if (!cpu_features->feature) { \
    if (!checked_features->cpu_features.feature) { \
      NcValidatorInstMessage( \
          LOG_WARNING, state, inst, \
          "Does not support " feature_name "feature, removing usage(s).\n"); \
      checked_features->cpu_features.feature = TRUE; \
    } \
    squash_me = TRUE; \
  }

void NcCpuCheck(struct NcValidatorState* state,
                struct NcInstIter* iter,
                NcCpuCheckState* checked_features) {
  NcInstState* inst = NcInstIterGetState(iter);
  Bool squash_me = FALSE;
  Opcode* opcode = NcInstStateOpcode(inst);
  CPUFeatures *cpu_features = &state->cpu_features;
  switch (opcode->insttype) {
    case NACLi_X87:
      CHECK_FEATURE(f_x87, "x87");
      break;
    case NACLi_SFENCE_CLFLUSH:
      /* TODO(bradchen): distinguish between SFENCE and CLFLUSH */
      CHECK_FEATURE(f_CLFLUSH, "CLFLUSH");
      CHECK_FEATURE(f_FXSR, "SFENCE");
      break;
    case NACLi_CMPXCHG8B:
      CHECK_FEATURE(f_CX8, "CX8");
      break;
    case NACLi_CMOV:
      CHECK_FEATURE(f_CMOV, "CMOV");
      break;
    case NACLi_FCMOV:
      if (!(cpu_features->f_CMOV && cpu_features->f_x87)) {
        if (!checked_features->f_CMOV_and_x87) {
          NcValidatorInstMessage(
              LOG_WARNING, state, inst,
              "Does not support CMOV and x87 features, "
              "removing corresponding CMOV usage(s).\n");
          checked_features->f_CMOV_and_x87 = TRUE;
        }
        squash_me = TRUE;
      }
      break;
    case NACLi_RDTSC:
      CHECK_FEATURE(f_TSC, "TSC");
      break;
    case NACLi_MMX:
      CHECK_FEATURE(f_MMX, "MMX");
      break;
    case NACLi_MMXSSE2:
      /* Note: We accept these instructions if either MMX or SSE2 bits */
      /* are set, in case MMX instructions go away someday...          */
      if (!(cpu_features->f_MMX || cpu_features->f_SSE2)) {
        if (!checked_features->f_MMX_or_SSE2) {
          NcValidatorInstMessage(
              LOG_WARNING, state, inst,
              "Does not support MMX or SSE2 features, "
              "removing corresponding usage(s).\n");
          checked_features->f_MMX_or_SSE2 = TRUE;
        }
      }
      squash_me = TRUE;
      break;
    case NACLi_SSE:
      CHECK_FEATURE(f_SSE, "SSE");
      break;
    case NACLi_SSE2:
      CHECK_FEATURE(f_SSE2, "SSE");
      break;
    case NACLi_SSE3:
      CHECK_FEATURE(f_SSE3, "SSE3");
      break;
    case NACLi_SSE4A:
      CHECK_FEATURE(f_SSE4A, "SSE4A");
      break;
    case NACLi_SSE41:
      CHECK_FEATURE(f_SSE41, "SSE41");
      break;
    case NACLi_SSE42:
      CHECK_FEATURE(f_SSE42, "SSE42");
      break;
    case NACLi_MOVBE:
      CHECK_FEATURE(f_MOVBE, "MOVBE");
      break;
    case NACLi_POPCNT:
      CHECK_FEATURE(f_POPCNT, "POPCNT");
      break;
    case NACLi_LZCNT:
      CHECK_FEATURE(f_LZCNT, "LZCNT");
      break;
    case NACLi_SSSE3:
      CHECK_FEATURE(f_SSSE3, "SSSE3");
      break;
    case NACLi_3DNOW:
      CHECK_FEATURE(f_3DNOW, "3DNOW");
      break;
    case NACLi_E3DNOW:
      CHECK_FEATURE(f_E3DNOW, "E3DNOW");
      break;
    case NACLi_SSE2x:
      /* This case requires CPUID checking code */
      /* DATA16 prefix required */
      if (!(inst->prefix_mask & kPrefixDATA16)) {
        NcValidatorInstMessage(
            LOG_ERROR, state, inst,
            "SSEx instruction must use prefix 0x66.\n");
      }
      CHECK_FEATURE(f_SSE2, "SSE2");
      break;
    default:
      break;
  }
  if (squash_me) {
    /* Replace all bytes with the stop instruction. */
    memset(iter->segment->mbase, kNaClFullStop, NcInstStateLength(inst));
  }
}

void NcCpuCheckSummary(FILE* file,
                       NcValidatorState* state,
                       NcCpuCheckState* checked_features) {
  /* The name of the flag is misleading; f_386 requires not just    */
  /* 386 instructions but also the CPUID instruction is supported.  */
  if (!state->cpu_features.f_386) {
    NcValidatorMessage(LOG_ERROR, state, "CPU does not support CPUID");
  }
}
