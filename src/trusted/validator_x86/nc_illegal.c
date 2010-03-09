/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/* Validator to check that instructions are in the legal subset. */

#include "native_client/src/trusted/validator_x86/nc_illegal.h"

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator_x86/nc_inst_state.h"
#include "native_client/src/trusted/validator_x86/nc_inst_iter.h"
#include "native_client/src/trusted/validator_x86/ncop_exps.h"
#include "native_client/src/trusted/validator_x86/ncvalidate_iter.h"

/* To turn on debugging of instruction decoding, change value of
 * DEBUGGING to 1.
 */
#define DEBUGGING 0

#include "native_client/src/shared/utils/debugging.h"

void NcValidateInstructionLegal(NcValidatorState* state,
                                NcInstIter* iter,
                                void* ignore) {
  NcInstState* inst = NcInstIterGetState(iter);
  Bool is_legal = NcInstStateIsNaclLegal(inst);
  DEBUG({
      printf("->NcValidateInstructionLegal\n");
      PrintOpcode(stdout, NcInstStateOpcode(inst));
      PrintExprNodeVector(stdout, NcInstStateNodeVector(inst));
    });
  is_legal = NcInstStateIsNaclLegal(inst);
  DEBUG(printf("is_legal = %"NACL_PRIdBool"\n", is_legal));
  if (is_legal) {
    /* Check other forms to disallow. */
    Opcode* opcode = NcInstStateOpcode(inst);
    switch (opcode->insttype) {
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
  if (!is_legal) {
    NcValidatorInstMessage(LOG_ERROR, state, inst,
                           "Illegal native client instruction\n");
  }
  DEBUG(printf("<-NcValidateInstructionLegal: is_legal = %"NACL_PRIdBool"\n",
               is_legal));
}
