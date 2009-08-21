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
  DEBUG(printf("is_legal = %"PRIdBool"\n", is_legal));
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
        DEBUG(printf("is_legal = %"PRIdBool"\n", is_legal));
        break;
      default:
        break;
    }
  }
  if (!is_legal) {
    NcValidatorInstMessage(LOG_ERROR, state, inst,
                           "Illegal native client instruction\n");
  }
  DEBUG(printf("<-NcValidateInstructionLegal: is_legal = %"PRIdBool"\n",
               is_legal));
}
