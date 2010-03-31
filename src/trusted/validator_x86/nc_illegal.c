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
  NaClInstState* inst_state = NaClInstIterGetState(iter);
  NaClDisallowsFlags disallows_flags = NaClInstStateDisallowsFlags(inst_state);
  Bool is_legal = NaClInstStateIsNaClLegal(inst_state);
  DEBUG({
      printf("->NaClValidateInstructionLegal\n");
      NaClInstPrint(stdout, NaClInstStateInst(inst_state));
      NaClExpVectorPrint(stdout, NaClInstStateExpVector(inst_state));
    });
  is_legal = NaClInstStateIsNaClLegal(inst_state);
  DEBUG(printf("is_legal = %"NACL_PRIdBool"\n", is_legal));
  if (is_legal) {
    /* Check other forms to disallow. */
    NaClInst* inst = NaClInstStateInst(inst_state);
    switch (inst->insttype) {
      case NACLi_RETURN:
      case NACLi_EMMX:
        /* EMMX needs to be supported someday but isn't ready yet. */
      case NACLi_INVALID:
      case NACLi_ILLEGAL:
      case NACLi_SYSTEM:
      case NACLi_RDMSR:
      case NACLi_RDTSCP:
      case NACLi_SYSCALL:
      case NACLi_SYSENTER:
      case NACLi_LONGMODE:
      case NACLi_SVM:
      case NACLi_3BYTE:
      case NACLi_CMPXCHG16B:
      case NACLi_UNDEFINED:
        is_legal = FALSE;
        DEBUG(printf("is_legal = %"NACL_PRIdBool"\n", is_legal));
        break;
      default:
        break;
    }
  }
  if (inst_state->prefix_mask & kPrefixADDR16) {
    is_legal = FALSE;
    disallows_flags |= NACL_DISALLOWS_FLAG(NaClCantUsePrefix67);
    DEBUG(printf("use of 67 (ADDR16) prefix not allowed\n"));
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
  DEBUG(printf("<-NaClValidateInstructionLegal: is_legal = %"NACL_PRIdBool"\n",
               is_legal));
}
