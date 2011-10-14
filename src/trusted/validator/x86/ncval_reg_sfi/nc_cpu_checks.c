/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Checks that CPU ID features match instructions found in executable.
 */

#include <stdlib.h>
#include <string.h>

#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/nc_cpu_checks.h"

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator/x86/decoder/nc_inst_state.h"
#include "native_client/src/trusted/validator/x86/decoder/nc_inst_state_internal.h"
#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/ncvalidate_iter.h"
#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/ncvalidate_iter_internal.h"
#include "native_client/src/trusted/validator/x86/nc_segment.h"

#include "native_client/src/trusted/validator/x86/decoder/nc_inst_iter_inl.c"

void NaClCpuCheckMemoryInitialize(NaClValidatorState* state) {
  NaClClearCPUFeatures(&state->cpu_checks.cpu_features);
  state->cpu_checks.f_CMOV_and_x87 = FALSE;
  state->cpu_checks.f_MMX_or_SSE2 = FALSE;
}

/* Helper macro to report unsupported features */
#define NACL_CHECK_FEATURE(feature, feature_name, vstate, squash_me) \
  if (!vstate->cpu_features.feature) { \
    if (!vstate->cpu_checks.cpu_features.feature) {         \
      NaClValidatorInstMessage( \
          LOG_WARNING, vstate, vstate->cur_inst_state, \
          "Does not support " feature_name " feature, removing usage(s).\n"); \
      vstate->cpu_checks.cpu_features.feature = TRUE; \
    } \
    squash_me = TRUE; \
  }

void NaClCpuCheck(struct NaClValidatorState* state,
                  struct NaClInstIter* iter) {
  Bool squash_me = FALSE;
  switch (state->cur_inst->insttype) {
    case NACLi_X87:
      NACL_CHECK_FEATURE(f_x87, "x87", state, squash_me);
      break;
    case NACLi_SFENCE_CLFLUSH:
      /* TODO(bradchen): distinguish between SFENCE and CLFLUSH */
      NACL_CHECK_FEATURE(f_CLFLUSH, "CLFLUSH", state, squash_me);
      NACL_CHECK_FEATURE(f_FXSR, "SFENCE", state, squash_me);
      break;
    case NACLi_CMPXCHG8B:
      NACL_CHECK_FEATURE(f_CX8, "CX8", state, squash_me);
      break;
    case NACLi_CMPXCHG16B:
      NACL_CHECK_FEATURE(f_CX16, "CX16", state, squash_me);
      break;
    case NACLi_CMOV:
      NACL_CHECK_FEATURE(f_CMOV, "CMOV", state, squash_me);
      break;
    case NACLi_FCMOV:
      if (!(state->cpu_features.f_CMOV &&
            state->cpu_features.f_x87)) {
        if (!state->cpu_checks.f_CMOV_and_x87) {
          NaClValidatorInstMessage(
              LOG_WARNING, state, state->cur_inst_state,
              "Does not support CMOV and x87 features, "
              "removing corresponding CMOV usage(s).\n");
          state->cpu_checks.f_CMOV_and_x87 = TRUE;
        }
        squash_me = TRUE;
      }
      break;
    case NACLi_RDTSC:
      NACL_CHECK_FEATURE(f_TSC, "TSC", state, squash_me);
      break;
    case NACLi_MMX:
      NACL_CHECK_FEATURE(f_MMX, "MMX", state, squash_me);
      break;
    case NACLi_MMXSSE2:
      /* Note: We accept these instructions if either MMX or SSE2 bits */
      /* are set, in case MMX instructions go away someday...          */
      if (!(state->cpu_features.f_MMX ||
            state->cpu_features.f_SSE2)) {
        if (!state->cpu_checks.f_MMX_or_SSE2) {
          NaClValidatorInstMessage(
              LOG_WARNING, state, state->cur_inst_state,
              "Does not support MMX or SSE2 features, "
              "removing corresponding usage(s).\n");
          state->cpu_checks.f_MMX_or_SSE2 = TRUE;
        }
      }
      squash_me = TRUE;
      break;
    case NACLi_SSE:
      NACL_CHECK_FEATURE(f_SSE, "SSE", state, squash_me);
      break;
    case NACLi_SSE2:
      NACL_CHECK_FEATURE(f_SSE2, "SSE2", state, squash_me);
      break;
    case NACLi_SSE3:
      NACL_CHECK_FEATURE(f_SSE3, "SSE3", state, squash_me);
      break;
    case NACLi_SSE4A:
      NACL_CHECK_FEATURE(f_SSE4A, "SSE4A", state, squash_me);
      break;
    case NACLi_SSE41:
      NACL_CHECK_FEATURE(f_SSE41, "SSE41", state, squash_me);
      break;
    case NACLi_SSE42:
      NACL_CHECK_FEATURE(f_SSE42, "SSE42", state, squash_me);
      break;
    case NACLi_MOVBE:
      NACL_CHECK_FEATURE(f_MOVBE, "MOVBE", state, squash_me);
      break;
    case NACLi_POPCNT:
      NACL_CHECK_FEATURE(f_POPCNT, "POPCNT", state, squash_me);
      break;
    case NACLi_LZCNT:
      NACL_CHECK_FEATURE(f_LZCNT, "LZCNT", state, squash_me);
      break;
    case NACLi_SSSE3:
      NACL_CHECK_FEATURE(f_SSSE3, "SSSE3", state, squash_me);
      break;
    case NACLi_3DNOW:
      NACL_CHECK_FEATURE(f_3DNOW, "3DNOW", state, squash_me);
      break;
    case NACLi_E3DNOW:
      NACL_CHECK_FEATURE(f_E3DNOW, "E3DNOW", state, squash_me);
      break;
    case NACLi_LONGMODE:
      /* TODO(karl): Remove this when NACLi_LONGMODE is no longer needed */
      NACL_CHECK_FEATURE(f_LM, "LM", state, squash_me);
      break;
    case NACLi_SSE2x:
      /* This case requires CPUID checking code */
      /* DATA16 prefix required */
      if (!(state->cur_inst_state->prefix_mask & kPrefixDATA16)) {
        NaClValidatorInstMessage(
            LOG_ERROR, state, state->cur_inst_state,
            "SSEx instruction must use prefix 0x66.\n");
      }
      NACL_CHECK_FEATURE(f_SSE2, "SSE2", state, squash_me);
      break;
    default:
      break;
  }
  if (state->cur_inst->flags & NACL_IFLAG(LongMode)) {
    NACL_CHECK_FEATURE(f_LM, "LM", state, squash_me);
  }
  if (squash_me) {
    /* Replace all bytes with the stop instruction. */
    memset(NaClInstIterGetInstMemoryInline(iter), kNaClFullStop,
           NaClInstStateLength(state->cur_inst_state));
  }
}
