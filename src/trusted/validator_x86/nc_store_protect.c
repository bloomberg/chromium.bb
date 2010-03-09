/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include "native_client/src/trusted/validator_x86/nc_store_protect.h"

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

/* Checks if the given node index for the given instruction is
 * a valid memory offset, based on NACL rules, returning TRUE iff
 * the memory offset is NACL compliant.
 * parameters are:
 *    state - The state of the validator,
 *    iter - The current value of the instruction iterator.
 *    lookback_index - number of instructions to lookback
 *      (within the iterator) in order to retrieve the instruction.
 *    inst - The instruction that holds the memory offset.
 *    vector - The expression vector associated with the instruction.
 *    node_index - The index of the memory offset within the given
 *      instruction vector.
 *    print_messages - True if this routine is responsable for printing
 *      error messages if the memory offset isn't NACL compliant.
 */
static Bool IsValidMemOffset(
    NcValidatorState* state,
    NcInstIter* iter,
    int lookback_index,
    NcInstState* inst,
    ExprNodeVector* vector,
    int node_index,
    Bool print_messages) {
  int base_reg_index;
  OperandKind base_reg;
  int index_reg_index;
  ExprNode* index_reg_node;
  OperandKind index_reg;
  int scale_index;
  int disp_index;
  ExprNode* node = &vector->node[node_index];

  if (ExprMemOffset != node->kind) return FALSE;
  DEBUG(printf("found MemOffset at node %"NACL_PRIu32"\n", node_index));
  base_reg_index = node_index + 1;
  base_reg = GetNodeVectorRegister(vector, base_reg_index);
  DEBUG(printf("base reg = %s\n", OperandKindName(base_reg)));
  if (base_reg != state->base_register &&
      base_reg != RegRSP &&
      base_reg != RegRBP &&
      base_reg != RegRIP) {
    if (print_messages) {
      NcValidatorInstMessage(LOG_ERROR, state, inst,
                             "Invalid base register in memory store\n");
    }
    return FALSE;
  }
  DEBUG(printf("  => base register is valid\n"));
  index_reg_index = base_reg_index + ExprNodeWidth(vector, base_reg_index);
  index_reg_node = &vector->node[index_reg_index];
  index_reg = GetNodeRegister(index_reg_node);
  DEBUG(printf("index reg = %s\n", OperandKindName(index_reg)));
  if (RegUnknown != index_reg) {
    Bool index_reg_is_good = FALSE;
    if ((base_reg != RegRIP) &&
        (index_reg_node->flags & ExprFlag(ExprSize64)) &&
        NcInstIterHasLookbackState(iter, lookback_index + 1)) {
      NcInstState* prev_inst =
          NcInstIterGetLookbackState(iter, lookback_index + 1);
      DEBUG({
          printf("prev inst:\n");
          PrintOpcode(stdout, NcInstStateOpcode(prev_inst));
          PrintExprNodeVector(stdout, NcInstStateNodeVector(prev_inst));
        });
      if (NcAssignsRegisterWithZeroExtends(
              prev_inst, NcGet32For64BitRegister(index_reg))) {
        DEBUG(printf("zero extends - safe!\n"));
        NcMarkInstructionJumpIllegal(state, inst);
        index_reg_is_good = TRUE;
      }
    }
    if (!index_reg_is_good) {
      if (print_messages) {
        NcValidatorInstMessage(LOG_ERROR, state, inst,
                               "Invalid index register in memory store\n");
      }
      return FALSE;
    }
  }
  scale_index = index_reg_index + ExprNodeWidth(vector, index_reg_index);
  disp_index = scale_index + ExprNodeWidth(vector, scale_index);
  DEBUG(printf("disp index = %d\n", disp_index));
  if (ExprConstant != vector->node[disp_index].kind) {
    if ((base_reg != RegRIP) ||
        (ExprConstant64 == vector->node[disp_index].kind)) {
      if (print_messages) {
        NcValidatorInstMessage(LOG_ERROR, state, inst,
                               "Invalid displacement in memory store\n");
      }
      return FALSE;
    }
  }
  return TRUE;
}

void NcStoreValidator(NcValidatorState* state,
                      NcInstIter* iter,
                      void* ignore) {
  uint32_t i;
  NcInstState* inst = NcInstIterGetState(iter);
  ExprNodeVector* vector = NcInstStateNodeVector(inst);

  DEBUG({
      printf("-> Validating store\n");
      PrintNcInstStateInstruction(stdout, inst);
      PrintOpcode(stdout, NcInstStateOpcode(inst));
      PrintExprNodeVector(stdout, NcInstStateNodeVector(inst));
    });

  /* Look for assignments on a memory offset. */
  for (i = 0; i < vector->number_expr_nodes; ++i) {
    ExprNode* node = &vector->node[i];
    if (node->flags & ExprFlag(ExprSet)) {
      if (IsValidMemOffset(state, iter, 0, inst, vector, i, TRUE)) {
        continue;
      } else if (ExprSegmentAddress == node->kind) {
        /* Note that operations like stosb, stosw, stosd, and stoq use
         * segment notation. In 64 bit mode, the second argument must
         * be a register, and be computed (using lea) so that it matches
         * a valid (sandboxed) memory offset. For example:
         *
         *    mov %edi, %edi           ; zero out top half of rdi
         *    lea %rdi, (%r15, %edi, 1); calculate address, put in rdi
         *    stos %eax                ; implicity uses %es:(%rdi)
         *
         * Note: we actually allow any zero-extending operation, not just
         * an identity MOV.
         */
        int seg_prefix_reg_index;
        OperandKind seg_prefix_reg;
        DEBUG(printf("found segment assign at node %"NACL_PRIu32"\n", i));

        /* Only allow segment prefix registers that are treated as
         * null prefixes.
         */
        seg_prefix_reg_index = GetExprNodeKidIndex(vector, i, 0);
        seg_prefix_reg = GetNodeVectorRegister(vector, seg_prefix_reg_index);
        switch (seg_prefix_reg) {
          case RegCS:
          case RegDS:
          case RegES:
          case RegSS:
            /* Check that we match lea constraints. */
            if (NcInstIterHasLookbackState(iter, 1)) {
              NcInstState* prev_inst = NcInstIterGetLookbackState(iter, 1);
              Opcode* prev_opcode = NcInstStateOpcode(prev_inst);
              DEBUG(printf("look at previous\n"));
              if (InstLea == prev_opcode->name) {
                int seg_reg_index = GetExprNodeKidIndex(vector, i, 1);
                OperandKind seg_reg =
                    GetNodeVectorRegister(vector, seg_reg_index);
                DEBUG(printf("seg reg = %s\n", OperandKindName(seg_reg)));
                if (seg_reg_index < (int) vector->number_expr_nodes) {
                  ExprNodeVector* prev_vector =
                      NcInstStateNodeVector(prev_inst);
                  /* Note that first argument of LEA is always a register,
                   * (under an OperandReference), and hence is always at
                   * index 1.
                   */
                  OperandKind lea_reg = GetNodeVectorRegister(prev_vector, 1);
                  DEBUG(printf("lea reg = %s\n", OperandKindName(lea_reg)));
                  if (lea_reg == seg_reg) {
                    /* Move to first argument which should be a memory
                     * address.
                     */
                    int memoff_index =
                        GetNthNodeKind(prev_vector, OperandReference, 2);
                    if (memoff_index < (int) prev_vector->number_expr_nodes) {
                      memoff_index =
                          GetExprNodeKidIndex(prev_vector, memoff_index, 0);
                      if (memoff_index < (int) prev_vector->number_expr_nodes) {
                        DEBUG(printf("check mem offset!\n"));
                        if (IsValidMemOffset(state,
                                             iter,
                                             1,
                                             prev_inst,
                                             prev_vector,
                                             memoff_index,
                                             FALSE)) {
                          NcMarkInstructionJumpIllegal(state, inst);
                          continue;
                        }
                      }
                    }
                  }
                }
              }
            }
            break;
          default:
            break;
        }
        /* If reached, we don't know how to handle the segment assign. */
        NcValidatorInstMessage(LOG_ERROR, state, inst,
                               "Segment store not allowed\n");
      } else if (UndefinedExp == node->kind ||
                 (ExprRegister == node->kind &&
                  RegUnknown == GetNodeRegister(node))) {
        /* This shouldn't happpen, but if it does, its because either:
         * (1) We couldn't translate the expression, and hence complain; or
         * (2) It is an X87 instruction with a register address, which we don't
         *     allow (in case these instructions get generalized in the future).
         */
        NcValidatorInstMessage(
            LOG_ERROR, state, inst,
            "Store not understood, can't verify correctness.\n");
      }
    }
  }
  DEBUG(printf("<- Validating store\n"));
}
