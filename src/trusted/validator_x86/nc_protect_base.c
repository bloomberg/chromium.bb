/* Copyright (c) 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
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

/* Returns true if the instruction is of the form
 *   OP %esp, C
 * where OP in { add , sub } and C is a 32 bit constant.
 */
static Bool NcIsAddOrSubBoundedConstFromEsp(NcInstState* inst) {
  Opcode* opcode = NcInstStateOpcode(inst);
  ExprNodeVector* vector = NcInstStateNodeVector(inst);
  return (InstAdd == opcode->name || InstSub == opcode->name) &&
      2 == NcGetOpcodeNumberOperands(opcode) &&
      /* Note: Since the vector contains a list of operand expressions, the
       * first operand reference is always at index zero, and its first child
       * (where the register would be defined) is at index 1.
       */
      ExprRegister == vector->node[1].kind &&
      RegESP == GetNodeRegister(&vector->node[1]) &&
      /* Note: Since the first subtree is a register operand, it uses
       * nodes 0 and 1 in the vector (node 0 is the operand reference, and
       * node 1 is its child defining a register value). The second operand
       * reference therefore lies at node 2, and if the operand is defined by
       * a 32 bit constant, it is the first kid of node 2, which is node 3.
       */
      ExprConstant == vector->node[3].kind;
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
               *     Note: this is part of an exit from a function, and
               *     is used in the pattern:
               *        mov %rsp, %rbp
               *        pop %rbp
               * (2) %esp = zero extend 32-bit value
               *     or %rsp, %rbase
               * (3) One of the following instructions:
               *     Push or Call.
               *     Pop and the operand is the implicit stack update.
               *
               *     Note that entering a function corresponds to the pattern:
               *        push %rpb
               *        mov %rbp, %rsp
               * (4) Allow stack updates of the form:
               *        OP %esp, C
               *        add %rsp, %r15
               *     where OP is in { add , sub }, and C is a constant.
               * (5) Allow "and $rsp, 0xXX" where 0xXX is an immediate 8 bit
               *     value that is negative. Used to realign the stack pointer.
               *
               * Note: Cases 4 and 5 are maintaining the invariant that the top
               * half of RSP is the same as R15, and the lower half of R15 is
               * zero. Case (4) maintains this by first clearing the top half
               * of RSP, and then setting the top half to match R15. Case (5)
               * maintains the variant becaus the constant is small (-1 to -128)
               * to that the invariant for $RSP (top half is unchanged).
               */
              switch (inst_name) {
                case InstPush:
                case InstCall:
                  /* case 3 (simple). */
                  return;
                case InstPop:
                  { /* case (3) pop -- see above */
                    int reg_operand_index = GetExprNodeParentIndex(vector, i);
                    ExprNode* reg_operand = &vector->node[reg_operand_index];
                    if (OperandReference == reg_operand->kind &&
                        (inst_opcode->operands[reg_operand->value].flags &
                         OpFlag(OpImplicit))) {
                      MaybeReportPreviousBad(state, locals);
                      return;
                    }
                  }
                  break;
                case InstOr:
                case InstAdd:
                case InstSub: {
                    /* case 2/4 (depending on instruction name). */
                    Bool or_case = (inst_name == InstOr);
                    if (NcIsBinarySetUsingRegisters(
                            inst_opcode, inst_name, vector, RegRSP,
                            (or_case ? state->base_register : RegR15)) &&
                        NcInstIterHasLookbackState(iter, 1)) {
                      NcInstState* prev_inst =
                          NcInstIterGetLookbackState(iter, 1);
                      if (or_case
                          ? NcAssignsRegisterWithZeroExtends(prev_inst, RegESP)
                          : NcIsAddOrSubBoundedConstFromEsp(prev_inst)) {
                        /* Matches, but we need to add that one can't branch
                         * into the middle of this pattern. Then mark the
                         * assignment as legal, and report any other found
                         * problems on previous instructions.
                         */
                        NcMarkInstructionJumpIllegal(state, inst);
                        locals->esp_set_inst = NULL;
                        MaybeReportPreviousBad(state, locals);
                        return;
                      }
                    }
                  }
                  break;
                case InstAnd:
                  /* See if case 5: and $rsp, 0xXX */
                  if (NcInstStateLength(inst) == 4 &&
                      NcInstStateByte(inst, 0) == 0x48 &&
                      NcInstStateByte(inst, 1) == 0x83 &&
                      NcInstStateByte(inst, 2) == 0xe4 &&
                      /* negative byte test: check if leftmost bit set. */
                      (NcInstStateByte(inst, 3) & 0x80)) {
                    MaybeReportPreviousBad(state, locals);
                    return;
                  }
                  /* Intentionally fall to the next case. */
                default:
                  if (NcIsMovUsingRegisters(inst_opcode, vector,
                                                 RegRSP, RegRBP)) {
                    /* case (1) -- see above */
                    /* Matches, but we need to add that one can't branch into
                     * the middle of this pattern. Then mark the assignment to
                     * RSP as legal, and report any other found problems on
                     * previous instructions.
                     */
                    NcMarkInstructionJumpIllegal(state, inst);
                    MaybeReportPreviousBad(state, locals);
                    return;
                  }
                  break;
              }
              /* If reached, assume that not a special case. */
              NcValidatorInstMessage(
                  LOG_ERROR, state, inst,
                  "Illegal change to register RSP\n");
              MaybeReportPreviousBad(state, locals);
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
              if (NcIs64Subregister(inst, reg_name, state->base_register)) {
                NcValidatorInstMessage(
                    LOG_ERROR, state, inst,
                    "Changing %s changes the value of register %s\n",
                    OperandKindName(reg_name),
                    OperandKindName(state->base_register));
              } else if (NcIs64Subregister(inst, reg_name, RegRSP)) {
                NcValidatorInstMessage(
                    LOG_ERROR, state, inst,
                    "Changing %s changes the value of register %s\n",
                    OperandKindName(reg_name),
                    OperandKindName(RegRSP));
              } else if (NcIs64Subregister(inst, reg_name, RegRBP)) {
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
