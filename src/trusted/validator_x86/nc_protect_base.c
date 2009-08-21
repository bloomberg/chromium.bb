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

/* nc_protect_base.h - For 64-bit mode, verifies that no instruction
 * changes the value of the base register.
 */

#include <assert.h>

#include "native_client/src/trusted/validator_x86/nc_protect_base.h"

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator_x86/nc_inst_trans.h"
#include "native_client/src/trusted/validator_x86/nc_jumps.h"
#include "native_client/src/trusted/validator_x86/ncop_exps.h"
#include "native_client/src/trusted/validator_x86/ncvalidate_iter.h"
#include "native_client/src/trusted/validator_x86/ncvalidate_iter_internal.h"
#include "native_client/src/trusted/validator_x86/ncvalidate_utils.h"

/* Defines locals used by the NcBaseRegisterValidator. */
typedef struct NcBaseRegisterLocals {
  /* Points to previous instruction that contains an
   * assignment to register ESP, or NULL if the previous
   * instruction doesn't set ESP. This is done so that
   * for such instructions we can check if the next instruction
   * uses the value of ESP to update RSP (if not, we need to
   * report that ESP is incorrectly assigned).
   */
  NcInstState* esp_set_inst;
} NcBaseRegisterLocals;

/* Checks flags in the given locals, and reports any
 * previous instructions that were marked as bad.
 *
 * Parameters:
 *   state - The state of the validator.
 *   iter - The instruction iterator being used by the validator.
 *   locals - The locals used by validator NcBaseRegisterValidator.
 */
static void MaybeReportPreviousBad(NcValidatorState* state,
                                   NcBaseRegisterLocals* locals) {
  if (NULL != locals->esp_set_inst) {
    NcValidatorInstMessage(LOG_ERROR,
                           state,
                           locals->esp_set_inst,
                           "Illegal assignment to ESP\n");
    locals->esp_set_inst = NULL;
  }
}

NcBaseRegisterLocals* NcBaseRegisterMemoryCreate(NcValidatorState* state) {
  NcBaseRegisterLocals* locals = (NcBaseRegisterLocals*)
      malloc(sizeof(NcBaseRegisterLocals));
  if (NULL == locals) {
    NcValidatorMessage(LOG_FATAL, state,
                       "Out of memory, can't allocate NcBaseRegisterLocals\n");
  }
  locals->esp_set_inst = NULL;
  return locals;
}

void NcBaseRegisterMemoryDestroy(NcValidatorState* state,
                                 NcBaseRegisterLocals* locals) {
  free(locals);
}

void NcBaseRegisterValidator(struct NcValidatorState* state,
                             struct NcInstIter* iter,
                             NcBaseRegisterLocals* locals) {
  uint32_t i;
  NcInstState* inst = NcInstIterGetState(iter);
  Opcode* inst_opcode = NcInstStateOpcode(inst);
  ExprNodeVector* vector = NcInstStateNodeVector(inst);

  /* Look for assignments to registers. */
  for (i = 0; i < vector->number_expr_nodes; ++i) {
    ExprNode* node = &vector->node[i];
    if (ExprRegister == node->kind) {
      if (node->flags & ExprFlag(ExprSet)) {
        OperandKind reg_name = GetNodeRegister(node);

        /* If reached, found an assignment to a register.
         * Check if its one that we care about (i.e.
         * the base register (r15), RSP, or RBP).
         */
        if (reg_name == state->base_register) {
          NcValidatorInstMessage(
              LOG_ERROR, state, inst,
              "Illegal to change the value of register %s\n",
              OperandKindName(state->base_register));
        } else {
          InstMnemonic inst_name = inst_opcode->name;
          switch (reg_name) {
            case RegRSP:
              /* Only allow one of:
               * (1) mov %rsp, %rbp
               * (2) %esp = zero extend 32-bit value
               *     or %rsp, %rbase
               * (3) One of the following instructions:
               *     Push or Call.
               *     Pop and the operand is the implicit stack update.
               */
              if (inst_name != InstPush && inst_name != InstCall) {
                if (inst_name == InstPop) {
                  /* case (3) pop -- see above */
                  int reg_operand_index = GetExprNodeParentIndex(vector, i);
                  ExprNode* reg_operand = &vector->node[reg_operand_index];
                  if (OperandReference == reg_operand->kind &&
                      (inst_opcode->operands[reg_operand->value].flags &
                       OpFlag(OpImplicit))) {
                      MaybeReportPreviousBad(state, locals);
                      return;
                  }
                } else if (InstOr == inst_name) {
                  /* case (2) -- see above */
                  if (NcIsOrUsingRegister(inst_opcode, vector, RegRSP,
                                          state->base_register) &&
                      NcInstIterHasLookbackState(iter, 1)) {
                    NcInstState* prev_inst =
                        NcInstIterGetLookbackState(iter, 1);
                    if (NcAssignsRegisterWithZeroExtends(prev_inst, RegESP)) {
                      /* Matches, but we need to add that one can't branch into
                       * the middle of this pattern. Then mark the assignment to
                       * ESP as legal, and report any other found problems on
                       * previous instructions.
                       */
                      NcMarkInstructionJumpIllegal(state, inst);
                      locals->esp_set_inst = NULL;
                      MaybeReportPreviousBad(state, locals);
                      return;
                    }
                  }
                } if (NcIsMovUsingRegisters(inst_opcode, vector,
                                            RegRSP, RegRBP)) {
                  /* case (1) -- see above */
                  MaybeReportPreviousBad(state, locals);
                  return;
                }
                /* If reached, assume that not a special case. */
                NcValidatorInstMessage(
                    LOG_ERROR, state, inst,
                    "Illegal change to register RSP\n");
                MaybeReportPreviousBad(state, locals);
              }
              break;
            case RegRBP:
              /* Only allow: mov %rbp, %rsp. */
              if (!NcIsMovUsingRegisters(inst_opcode, vector,
                                         RegRBP, RegRSP)) {
                NcValidatorInstMessage(
                    LOG_ERROR, state, inst,
                    "Illegal change to register RBP\n");
              }
              break;
            case RegESP:
              /* Record that we must recheck this after we have
               * moved to the next instruction.
               */
              MaybeReportPreviousBad(state, locals);
              locals->esp_set_inst = inst;
              return;
            default:
              /* Don't allow any subregister assignments of the
               * base register (R15), RSP, or RBP.
               */
              if (NcIs64Subregister(reg_name, state->base_register)) {
                NcValidatorInstMessage(
                    LOG_ERROR, state, inst,
                    "Changing %s changes the value of register %s\n",
                    OperandKindName(reg_name),
                    OperandKindName(state->base_register));
              } else if (NcIs64Subregister(reg_name, RegRSP)) {
                NcValidatorInstMessage(
                    LOG_ERROR, state, inst,
                    "Changing %s changes the value of register %s\n",
                    OperandKindName(reg_name),
                    OperandKindName(RegRSP));
              } else if (NcIs64Subregister(reg_name, RegRBP)) {
                NcValidatorInstMessage(
                    LOG_ERROR, state, inst,
                    "Changing %s changes the value of register %s\n",
                    OperandKindName(reg_name),
                    OperandKindName(RegRBP));
              }
              break;
          }
        }
      }
    }
  }
  /* Before moving to the next instruction, see if we need to report
   * problems with the previous instruction.
   */
  MaybeReportPreviousBad(state, locals);
}

void NcBaseRegisterSummarize(FILE* f,
                             struct NcValidatorState* state,
                             struct NcBaseRegisterLocals* locals) {
  /* Check if problems in last instruction of segment. */
  MaybeReportPreviousBad(state, locals);
}
