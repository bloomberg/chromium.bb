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
    if (ExprMemOffset == node->kind && (node->flags & ExprFlag(ExprSet))) {
      int base_reg_index;
      OperandKind base_reg;
      int index_reg_index;
      ExprNode* index_reg_node;
      OperandKind index_reg;
      int scale_index;
      int disp_index;

      DEBUG(printf("found assign at node %"PRIu32"\n", i));

      base_reg_index = i + 1;
      base_reg = GetNodeVectorRegister(vector, base_reg_index);
      DEBUG(printf("base reg = %s\n", OperandKindName(base_reg)));
      if (base_reg != state->base_register &&
          base_reg != RegRSP &&
          base_reg != RegRBP &&
          base_reg != RegRIP) {
        NcValidatorInstMessage(LOG_ERROR, state, inst,
                               "Invalid base register in memory store\n");
        break;
      }
      index_reg_index = base_reg_index + ExprNodeWidth(vector, base_reg_index);
      index_reg_node = &vector->node[index_reg_index];
      index_reg = GetNodeRegister(index_reg_node);
      DEBUG(printf("index reg = %s\n", OperandKindName(index_reg)));
      if (RegUnknown != index_reg) {
        Bool index_reg_is_good = FALSE;
        if ((base_reg != RegRIP) &&
            (index_reg_node->flags & ExprFlag(ExprSize64)) &&
            NcInstIterHasLookbackState(iter, 1)) {
          NcInstState* prev_inst = NcInstIterGetLookbackState(iter, 1);
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
          NcValidatorInstMessage(LOG_ERROR, state, inst,
                                 "Invalid index register in memory store\n");
        }
        break;
      }
      scale_index = index_reg_index + ExprNodeWidth(vector, index_reg_index);
      disp_index = scale_index + ExprNodeWidth(vector, scale_index);
      DEBUG(printf("disp index = %d\n", disp_index));
      if (ExprConstant != vector->node[disp_index].kind) {
        if ((base_reg != RegRIP) ||
            (ExprConstant64 == vector->node[disp_index].kind)) {
          NcValidatorInstMessage(LOG_ERROR, state, inst,
                                 "Invalid displacement in memory store\n");
        }
      }
      break;
    } else if (ExprFlag(ExprSet) &&
               (UndefinedExp == node->kind ||
                (ExprRegister == node->kind &&
                 (node->flags & ExprFlag(ExprSet)) &&
                 RegUnknown == GetNodeRegister(node)))) {
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
  DEBUG(printf("<- Validating store\n"));
}
