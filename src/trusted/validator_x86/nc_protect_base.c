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

/* To turn on debugging of instruction decoding, change value of
 * DEBUGGING to 1.
 */
#define DEBUGGING 0

#include "native_client/src/shared/utils/debugging.h"

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
  /* Points to the previous instruction that contains an
   * assignment to register EBP, or NULL if the previous
   * instruction doesn't set EBP. This is done so that
   * for such instructions we can check if the next instruciton
   * uses the value of EBP to update RBP (if not, we need ot
   * report that EBP is incorrectly assigned).
   */
  NcInstState* ebp_set_inst;
} NcBaseRegisterLocals;

static void ReportIllegalChangeToRsp(NcValidatorState* state,
                                     NcInstState* inst) {
  NcValidatorInstMessage(LOG_ERROR, state, inst,
                         "Illegal assignment to RSP\n");
}

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
  if (NULL != locals->ebp_set_inst) {
    NcValidatorInstMessage(LOG_ERROR,
                           state,
                           locals->ebp_set_inst,
                           "Illegal assignment to EBP\n");
    locals->ebp_set_inst = NULL;
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
  locals->ebp_set_inst = NULL;
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

  DEBUG(NcValidatorInstMessage(
      LOG_INFO, state, inst, "Checking base registers...\n"));

  /* Look for assignments to registers. */
  for (i = 0; i < vector->number_expr_nodes; ++i) {
    ExprNode* node = &vector->node[i];
    if (ExprRegister == node->kind) {
      if (node->flags & ExprFlag(ExprSet)) {
        OperandKind reg_name = GetNodeRegister(node);

        /* If reached, found an assignment to a register.
         * Check if its one that we care about (i.e.
         * the base register (RBASE), RSP, or RBP).
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
               *
               *     Note: maintains RSP/RBP invariant, since RBP was already
               *     meeting the invariant.
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
               *        add %rsp, %rbase
               *     where OP is in { add , sub }, and C is a constant.
               * (5) Allow "and $rsp, 0xXX" where 0xXX is an immediate 8 bit
               *     value that is negative. Used to realign the stack pointer.
               * (6) mov %esp, ...
               *     add %rsp, %rbase
               *
               *     Note: The code here allows any operation that zero extends
               *     ebp, not just a move. The rationale is that the MOV does
               *     a zero extend of RBP, and is the only property that is
               *     needed to maintain the invariant on ESP.
               *
               * Note: Cases 2, 4, 5, and 6 are maintaining the invariant that
               * the top half of RSP is the same as RBASE, and the lower half
               * of RBASE is zero. Case (2) does this by seting the bottom 32
               * bits with the first instruction (zeroing out the top 32 bits),
               * and then copies (via or) the top 32 bits of RBASE into RSP
               * (since the bottom 32 bits of RBASE are zero).
               * Case (4) maintains this by first clearing the top half
               * of RSP, and then setting the top half to match RBASE. Case (5)
               * maintains the variant because the constant is small
               * (-1 to -128) to that the invariant for $RSP (top half
               * is unchanged). Case (6) is simular to case 2 (by filling
               * the lower 32 bits and zero extending), followed by an add to
               * set the top 32 bits.
               */
              switch (inst_name) {
                case InstPush:
                case InstCall:
                  /* case 3 (simple). */
                  return;
                case InstPop: {
                    /* case (3) pop -- see above */
                    int reg_operand_index;
                    ExprNode* reg_operand;
                    reg_operand_index = GetExprNodeParentIndex(vector, i);
                    reg_operand = &vector->node[reg_operand_index];
                    if (OperandReference == reg_operand->kind &&
                        (inst_opcode->operands[reg_operand->value].flags &
                         OpFlag(OpImplicit))) {
                      MaybeReportPreviousBad(state, locals);
                      return;
                    }
                  }
                  break;
                case InstOr:
                case InstAdd: {
                    /* case 2/4/6 (depending on instruction name). */
                    if (NcIsBinarySetUsingRegisters(
                            inst_opcode, inst_name, vector, RegRSP,
                            state->base_register) &&
                        NcInstIterHasLookbackState(iter, 1)) {
                      NcInstState* prev_inst =
                          NcInstIterGetLookbackState(iter, 1);
                      if (NcAssignsRegisterWithZeroExtends(prev_inst, RegESP) ||
                          (inst_name == InstAdd &&
                           NcIsAddOrSubBoundedConstFromEsp(prev_inst))) {
                        DEBUG(printf("nc protect base for or/add/or\n"));
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
                    /* case (1) -- see above, matching
                     *    mov %rsp, %rbp
                     */
                    MaybeReportPreviousBad(state, locals);
                    return;
                  }
                  break;
              }
              /* If reached, assume that not a special case. */
              ReportIllegalChangeToRsp(state, inst);
              MaybeReportPreviousBad(state, locals);
              break;
            case RegRBP:
              /* (1) mov %rbp, %rsp
               *
               *     Note: maintains RSP/RBP invariant, since RSP was already
               *     meeting the invariant.
               *
               * (2) mov %ebp, ...
               *     add %rbp, %r15
               *
               *     Typical use in the exit from a fucntion, restoring RBP.
               *     The ... in the MOV is gotten from a stack pop in such
               *     cases. However, for long jumps etc., the value may
               *     be gotten from memory, or even a register.
               *
               *     Note: The code here allows any operation that zero extends
               *     ebp, not just a move. The rationale is that the MOV does
               *     a zero extend of rbp, and is the only property that is
               *     needed to maintain the invarinat on ebp.
               */
              if (!NcIsMovUsingRegisters(inst_opcode, vector,
                                         RegRBP, RegRSP)) {
                NcInstState* prev_inst = NcInstIterGetLookbackState(iter, 1);
                if (NcIsBinarySetUsingRegisters(inst_opcode, InstAdd, vector,
                                                RegRBP, state->base_register) &&
                    NcAssignsRegisterWithZeroExtends(prev_inst, RegEBP)) {
                  /* case 6. */
                  NcMarkInstructionJumpIllegal(state, inst);
                  locals->ebp_set_inst = NULL;
                  MaybeReportPreviousBad(state, locals);
                  return;
                }
                /* If reached, not valid. */
                NcValidatorInstMessage(
                    LOG_ERROR, state, inst, "Illegal change to register RBP\n");
              }
              break;
            case RegESP:
              /* Record that we must recheck this after we have
               * moved to the next instruction.
               */
              MaybeReportPreviousBad(state, locals);
              locals->esp_set_inst = inst;
              return;
            case RegEBP:
              /* Record that we must recheck this after we have
               * moved to the next instruction.
               */
              MaybeReportPreviousBad(state, locals);
              locals->ebp_set_inst = inst;
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
