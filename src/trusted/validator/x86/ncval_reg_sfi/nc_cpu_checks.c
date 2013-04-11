/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
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
  NaClClearCPUFeaturesX86(&state->cpu_checks.cpu_features);
  state->cpu_checks.f_CMOV_and_x87 = FALSE;
  state->cpu_checks.f_MMX_or_SSE2 = FALSE;
}

/* Helper function to report unsupported features */
static INLINE void NaClCheckFeature(NaClCPUFeatureX86ID feature,
                                    struct NaClValidatorState* vstate,
                                    Bool* squash_me) {
  if (!NaClGetCPUFeatureX86(&vstate->cpu_features, feature)) {
    if (!NaClGetCPUFeatureX86(&vstate->cpu_checks.cpu_features, feature)) {
      NaClValidatorInstMessage(
          LOG_WARNING, vstate, vstate->cur_inst_state,
          "CPU model does not support %s instructions.\n",
          NaClGetCPUFeatureX86Name(feature));
      NaClSetCPUFeatureX86(&vstate->cpu_checks.cpu_features, feature, TRUE);
    }
    *squash_me = TRUE;
  }
}

void NaClCpuCheck(struct NaClValidatorState* state,
                  struct NaClInstIter* iter) {
  Bool squash_me = FALSE;
  switch (state->cur_inst->insttype) {
    case NACLi_X87:
    case NACLi_X87_FSINCOS:
      NaClCheckFeature(NaClCPUFeatureX86_x87, state, &squash_me);
      break;
    case NACLi_SFENCE_CLFLUSH:
      /* TODO(bradchen): distinguish between SFENCE and CLFLUSH */
      NaClCheckFeature(NaClCPUFeatureX86_CLFLUSH, state, &squash_me);
      NaClCheckFeature(NaClCPUFeatureX86_FXSR, state, &squash_me);
      break;
    case NACLi_LAHF:
      if (NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_TARGET_SUBARCH == 64)
        NaClCheckFeature(NaClCPUFeatureX86_LAHF, state, &squash_me);
      break;
    case NACLi_CMPXCHG8B:
      NaClCheckFeature(NaClCPUFeatureX86_CX8, state, &squash_me);
      break;
    case NACLi_CMPXCHG16B:
      NaClCheckFeature(NaClCPUFeatureX86_CX16, state, &squash_me);
      break;
    case NACLi_CMOV:
      NaClCheckFeature(NaClCPUFeatureX86_CMOV, state, &squash_me);
      break;
    case NACLi_FCMOV:
      if (!(NaClGetCPUFeatureX86(&state->cpu_features,
                                 NaClCPUFeatureX86_CMOV) &&
            NaClGetCPUFeatureX86(&state->cpu_features,
                                 NaClCPUFeatureX86_x87))) {
        if (!state->cpu_checks.f_CMOV_and_x87) {
          NaClValidatorInstMessage(
              LOG_WARNING, state, state->cur_inst_state,
              "CPU model does not support CMOV and x87 instructions.\n");
          state->cpu_checks.f_CMOV_and_x87 = TRUE;
        }
        squash_me = TRUE;
      }
      break;
    case NACLi_RDTSC:
      NaClCheckFeature(NaClCPUFeatureX86_TSC, state, &squash_me);
      break;
    case NACLi_MMX:
      NaClCheckFeature(NaClCPUFeatureX86_MMX, state, &squash_me);
      break;
    case NACLi_MMXSSE2:
      /* Note: We accept these instructions if either MMX or SSE2 bits */
      /* are set, in case MMX instructions go away someday...          */
      if (!(NaClGetCPUFeatureX86(&state->cpu_features,
                                 NaClCPUFeatureX86_MMX) ||
            NaClGetCPUFeatureX86(&state->cpu_features,
                                 NaClCPUFeatureX86_SSE2))) {
        if (!state->cpu_checks.f_MMX_or_SSE2) {
          NaClValidatorInstMessage(
              LOG_WARNING, state, state->cur_inst_state,
              "CPU model does not support MMX or SSE2 instructions.\n");
          state->cpu_checks.f_MMX_or_SSE2 = TRUE;
        }
      }
      squash_me = TRUE;
      break;
    case NACLi_SSE:
      NaClCheckFeature(NaClCPUFeatureX86_SSE, state, &squash_me);
      break;
    case NACLi_SSE2:
      NaClCheckFeature(NaClCPUFeatureX86_SSE2, state, &squash_me);
      break;
    case NACLi_SSE3:
      NaClCheckFeature(NaClCPUFeatureX86_SSE3, state, &squash_me);
      break;
    case NACLi_SSE4A:
      NaClCheckFeature(NaClCPUFeatureX86_SSE4A, state, &squash_me);
      break;
    case NACLi_SSE41:
      NaClCheckFeature(NaClCPUFeatureX86_SSE41, state, &squash_me);
      break;
    case NACLi_SSE42:
      NaClCheckFeature(NaClCPUFeatureX86_SSE42, state, &squash_me);
      break;
    case NACLi_MOVBE:
      NaClCheckFeature(NaClCPUFeatureX86_MOVBE, state, &squash_me);
      break;
    case NACLi_POPCNT:
      NaClCheckFeature(NaClCPUFeatureX86_POPCNT, state, &squash_me);
      break;
    case NACLi_LZCNT:
      NaClCheckFeature(NaClCPUFeatureX86_LZCNT, state, &squash_me);
      break;
    case NACLi_SSSE3:
      NaClCheckFeature(NaClCPUFeatureX86_SSSE3, state, &squash_me);
      break;
    case NACLi_3DNOW:
      NaClCheckFeature(NaClCPUFeatureX86_3DNOW, state, &squash_me);
      break;
    case NACLi_E3DNOW:
      NaClCheckFeature(NaClCPUFeatureX86_E3DNOW, state, &squash_me);
      break;
    case NACLi_LONGMODE:
      /* TODO(karl): Remove this when NACLi_LONGMODE is no longer needed */
      NaClCheckFeature(NaClCPUFeatureX86_LM, state, &squash_me);
      break;
    case NACLi_SSE2x:
      /* This case requires CPUID checking code */
      /* DATA16 prefix required */
      if (!(state->cur_inst_state->prefix_mask & kPrefixDATA16)) {
        NaClValidatorInstMessage(
            LOG_ERROR, state, state->cur_inst_state,
            "SSEx instruction must use prefix 0x66.\n");
      }
      NaClCheckFeature(NaClCPUFeatureX86_SSE2, state, &squash_me);
      break;
    default:
      /* This instruction could be either legal or illegal, but if we
       * get here it is not CPU-dependent.
       */
      break;
  }
  if (state->cur_inst->flags & NACL_IFLAG(LongMode)) {
    NaClCheckFeature(NaClCPUFeatureX86_LM, state, &squash_me);
  }
  if (squash_me) {
    if (state->readonly_text) {
      NaClValidatorInstMessage(
          LOG_ERROR, state, state->cur_inst_state,
          "Read-only text: cannot squash unsupported instruction.\n");
    } else {
      /* Replace all bytes of the instruction with the HLT instruction. */
      NCStubOutMem(state, NaClInstIterGetInstMemoryInline(iter),
                   NaClInstStateLength(state->cur_inst_state));
    }
  }
}
