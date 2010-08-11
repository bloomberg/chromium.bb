/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/* Validator to check that instructions are in the legal subset. */

#include "native_client/src/trusted/validator_x86/nc_illegal.h"

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator_x86/nc_inst_state.h"
#include "native_client/src/trusted/validator_x86/nc_inst_state_internal.h"
#include "native_client/src/trusted/validator_x86/nc_inst_iter.h"
#include "native_client/src/trusted/validator_x86/ncop_exps.h"
#include "native_client/src/trusted/validator_x86/ncvalidate_iter.h"

/* To turn on debugging of instruction decoding, change value of
 * DEBUGGING to 1.
 */
#define DEBUGGING 0

#include "native_client/src/shared/utils/debugging.h"

void NaClValidateInstructionLegal(NaClValidatorState* state,
                                  NaClInstIter* iter,
                                  void* ignore) {
  int num_prefix_bytes;
  Bool is_legal = TRUE;
  NaClInstState* inst_state = NaClInstIterGetState(iter);
  NaClInst* inst = NaClInstStateInst(inst_state);
  NaClDisallowsFlags disallows_flags = NACL_EMPTY_DISALLOWS_FLAGS;
  DEBUG({
      struct Gio* g = NaClLogGetGio();
      NaClLog(LOG_INFO, "->NaClValidateInstructionLegal\n");
      NaClInstPrint(g, NaClInstStateInst(inst_state));
      NaClExpVectorPrint(g, NaClInstStateExpVector(inst_state));
    });

  /* Don't allow more than one (non-REX) prefix. */
  num_prefix_bytes = inst_state->num_prefix_bytes;
  if (inst_state->rexprefix) --num_prefix_bytes;

  /* Don't allow an invalid instruction. */
  if (!NaClInstStateIsValid(inst_state)) {
    is_legal = FALSE;
    disallows_flags |= NACL_DISALLOWS_FLAG(NaClMarkedInvalid);
    DEBUG(NaClLog(LOG_INFO, "invalid instruction opcode found\n"));
  }

  /* Note: Explicit NOP sequences that use multiple 66 values are
   * recognized as special cases, and need not be processed here.
   */
  if (num_prefix_bytes > 1) {
    is_legal = FALSE;
    disallows_flags |= NACL_DISALLOWS_FLAG(NaClTooManyPrefixBytes);
    DEBUG(NaClLog(LOG_INFO, "too many prefix bytes\n"));
  }

  /* Check other forms to disallow. */
  switch (inst->insttype) {
    case NACLi_RETURN:
    case NACLi_EMMX:
      /* EMMX needs to be supported someday but isn't ready yet. */
    case NACLi_ILLEGAL:
    case NACLi_SYSTEM:
    case NACLi_RDMSR:
    case NACLi_RDTSCP:
    case NACLi_LONGMODE:
    case NACLi_SVM:
    case NACLi_3BYTE:
    case NACLi_CMPXCHG16B:
    case NACLi_UNDEFINED:
      is_legal = FALSE;
      disallows_flags |= NACL_DISALLOWS_FLAG(NaClMarkedIllegal);
      DEBUG(NaClLog(LOG_INFO, "mark instruction as NaCl illegal\n"));
      break;
    case NACLi_INVALID:
      is_legal = FALSE;
      disallows_flags |= NACL_DISALLOWS_FLAG(NaClMarkedInvalid);
      DEBUG(NaClLog(LOG_INFO, "invalid instruction opcode found\n"));
      break;
    case NACLi_SYSCALL:
    case NACLi_SYSENTER:
      is_legal = FALSE;
      disallows_flags |= NACL_DISALLOWS_FLAG(NaClMarkedSystem);
      DEBUG(NaClLog(LOG_INFO, "system instruction not allowed\n"));
      break;
    default:
      break;
  }
  if (NACL_IFLAG(NaClIllegal) & inst->flags) {
    is_legal = FALSE;
    disallows_flags |= NACL_DISALLOWS_FLAG(NaClMarkedIllegal);
    DEBUG(NaClLog(LOG_INFO, "mark instruction as NaCl illegal\n"));
  }
  if (inst_state->num_rex_prefixes > 1) {
    /* NOTE: does not apply to NOP, since they are parsed using
     * special handling (i.e. explicit byte sequence matches) that
     * doesn't explicitly define prefix bytes.
     *
     * NOTE: We don't disallow this while decoding, since xed doesn't
     * disallow this, and we want to be able to compare our tool
     * to xed.
     */
    is_legal = FALSE;
    disallows_flags |= NACL_DISALLOWS_FLAG(NaClMultipleRexPrefix);
    DEBUG(NaClLog(LOG_INFO, "multiple use of REX prefix not allowed\n"));
  }
  if (inst_state->prefix_mask & kPrefixADDR16) {
    is_legal = FALSE;
    disallows_flags |= NACL_DISALLOWS_FLAG(NaClCantUsePrefix67);
    DEBUG(NaClLog(LOG_INFO, "use of 67 (ADDR16) prefix not allowed\n"));
  }
  if (NACL_TARGET_SUBARCH == 64) {
    /* Don't allow CS, DS, ES, or SS prefix overrides,
     * since it has no effect, unless explicitly stated
     * otherwise.
     */
    if (inst_state->prefix_mask &
        (kPrefixSEGCS | kPrefixSEGSS | kPrefixSEGES |
         kPrefixSEGDS)) {
      if (NACL_EMPTY_IFLAGS ==
          (inst->flags & NACL_IFLAG(IgnorePrefixSEGCS))) {
        is_legal = FALSE;
        disallows_flags |= NACL_DISALLOWS_FLAG(NaClHasBadSegmentPrefix);
        DEBUG(NaClLog(LOG_INFO, "segment prefix disallowed\n"));
      }
    }
  }
  if (!is_legal) {
    /* Print out error message for each reason the instruction is disallowed. */
    if (NACL_EMPTY_DISALLOWS_FLAGS != disallows_flags) {
      int i;
      Bool printed_reason = FALSE;
      for (i = 0; i < NaClDisallowsFlagEnumSize; ++i) {
        if (disallows_flags & NACL_DISALLOWS_FLAG(i)) {
          switch (i) {
            case NaClTooManyPrefixBytes:
              printed_reason = TRUE;
              NaClValidatorInstMessage(
                  LOG_ERROR, state, inst_state,
                  "More than one (non-REX) prefix byte specified\n");
              break;
            case NaClMarkedIllegal:
              printed_reason = TRUE;
              NaClValidatorInstMessage(
                  LOG_ERROR, state, inst_state,
                  "This instruction has been marked illegal "
                  "by Native Client\n");
              break;
            case NaClMarkedInvalid:
              printed_reason = TRUE;
              NaClValidatorInstMessage(
                  LOG_ERROR, state, inst_state,
                  "Opcode sequence doesn't define a valid x86 instruction\n");
              break;
            case NaClMarkedSystem:
              printed_reason = TRUE;
              NaClValidatorInstMessage(
                  LOG_ERROR, state, inst_state,
                  "System instructions are not allowed by Native Client\n");
              break;
            case NaClHasBadSegmentPrefix:
              printed_reason = TRUE;
              NaClValidatorInstMessage(
                  LOG_ERROR, state, inst_state,
                  "Uses a segment prefix byte not allowed by Native Client\n");
              break;
            case NaClCantUsePrefix67:
              printed_reason = TRUE;
              NaClValidatorInstMessage(
                  LOG_ERROR, state, inst_state,
                  "Use of 67 (ADDR16) prefix not allowed by Native Client\n");
              break;
            case NaClMultipleRexPrefix:
              printed_reason = TRUE;
              NaClValidatorInstMessage(
                  LOG_ERROR, state, inst_state,
                  "Multiple use of REX prefix not allowed\n");
              break;
            default:
              /* This shouldn't happen, but if it does, and no errors
               * are printed, this will force the default error message
               * below.
               */
              break;
          }
        }
        /* Stop looking if we should quit reporting errors. */
        if (NaClValidateQuit(state)) break;
      }
      /* Be sure we print a reason (in case the switch isn't complete). */
      if (!printed_reason) {
        disallows_flags = NACL_EMPTY_DISALLOWS_FLAGS;
      }
    }
    if (disallows_flags == NACL_EMPTY_DISALLOWS_FLAGS) {
      NaClValidatorInstMessage(LOG_ERROR, state, inst_state,
                               "Illegal native client instruction\n");
    }
  }
  DEBUG(NaClLog(LOG_INFO,
                "<-NaClValidateInstructionLegal: is_legal = %"NACL_PRIdBool"\n",
                is_legal));
}
