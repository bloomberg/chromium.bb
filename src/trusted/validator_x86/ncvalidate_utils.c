/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Some useful utilities for validator patterns. */

#include "native_client/src/trusted/validator_x86/ncvalidate_utils.h"

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator_x86/nc_decode_tables.h"
#include "native_client/src/trusted/validator_x86/nc_inst_state_internal.h"
#include "native_client/src/trusted/validator_x86/ncop_exps.h"

/* To turn on debugging of instruction decoding, change value of
 * DEBUGGING to 1.
 */
#define DEBUGGING 0

#include "native_client/src/shared/utils/debugging.h"

const NaClOpFlags NaClOpSetOrUse = NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse);

Bool NaClIsBinaryUsingRegisters(const NaClInst* inst,
                                NaClMnemonic name,
                                NaClExpVector* vector,
                                NaClOpKind reg_1,
                                NaClOpKind reg_2) {
  return name == inst->name &&
      2 == NaClGetInstNumberOperands(inst) &&
      /* Note: Since the vector contains a list of operand expressions, the
       * first operand reference is always at index zero, and its first child
       * (where the register would be defined) is at index 1.
       */
      ExprRegister == vector->node[1].kind &&
      reg_1 == NaClGetExpRegister(&vector->node[1]) &&
      /* Note: Since the first subtree is a register operand, it uses
       * nodes 0 and 1 in the vector (node 0 is the operand reference, and
       * node 1 is its child defining a register value). The second operand
       * reference therefore lies at node 2, and if the operand is defined by
       * a register, it is the first kid of node 2, which is node 3.
       */
      ExprRegister == vector->node[3].kind &&
      reg_2 == NaClGetExpRegister(&vector->node[3]);
}

Bool NaClIsMovUsingRegisters(const NaClDecodeTables* tables,
                             const NaClInst* inst,
                             NaClExpVector* vector,
                             NaClOpKind reg_set,
                             NaClOpKind reg_use) {
  return NaClIsBinaryUsingRegisters(inst, InstMov, vector, reg_set, reg_use) &&
      NACL_OPFLAG(OpSet) ==
      (NaClGetInstOperand(tables, inst, 0)->flags & NaClOpSetOrUse) &&
      NACL_OPFLAG(OpUse) ==
      (NaClGetInstOperand(tables, inst, 1)->flags & NaClOpSetOrUse);
}

Bool NaClIsBinarySetUsingRegisters(const NaClDecodeTables* tables,
                                   const NaClInst* inst,
                                   NaClMnemonic name,
                                   NaClExpVector* vector,
                                   NaClOpKind reg_1,
                                   NaClOpKind reg_2) {
  return NaClIsBinaryUsingRegisters(inst, name, vector, reg_1, reg_2) &&
      NaClOpSetOrUse ==
      (NaClGetInstOperand(tables, inst, 0)->flags & NaClOpSetOrUse) &&
      NACL_OPFLAG(OpUse) ==
      (NaClGetInstOperand(tables, inst, 1)->flags & NaClOpSetOrUse);
}

Bool NaClOperandOneIsRegisterSet(NaClInstState* inst,
                                 NaClOpKind reg_name) {
  /* Note: Since the vector contains a list of operand expressions, the
   * first operand reference is always at index zero, and its first child
   * (where the register would be defined) is at index 1.
   */
  Bool result = FALSE;
  NaClExpVector* vector = NaClInstStateExpVector(inst);
  DEBUG(NaClLog(LOG_INFO,
                "->NaClOperandOneIsRegisterSet %s\n",
                NaClOpKindName(reg_name)));
  DEBUG(NaClExpVectorPrint(NaClLogGetGio(), vector));
  if (vector->number_expr_nodes >= 2) {
    NaClExp* op_reg = &vector->node[1];
    result = (ExprRegister == op_reg->kind &&
              reg_name == NaClGetExpRegister(op_reg) &&
              (op_reg->flags & NACL_EFLAG(ExprSet)));
  }
  DEBUG(NaClLog(LOG_INFO,
                "<-NaClOperandOneIsRegisterSet = %"NACL_PRIdBool"\n", result));
  return result;
}

Bool NaClOperandOneZeroExtends(NaClInstState* state) {
  Bool result = FALSE;
  const NaClInst* inst = NaClInstStateInst(state);
  DEBUG(NaClLog(LOG_INFO, "->NaClOperandOneZeroExtends\n"));
  DEBUG(NaClInstPrint(NaClLogGetGio(), state->decoder_tables, inst));
  result = (1 <= NaClGetInstNumberOperands(inst) &&
            (NaClGetInstOperand(state->decoder_tables, inst, 0)->flags &
             NACL_OPFLAG(OperandZeroExtends_v)) &&
            4 == NaClInstStateOperandSize(state));
  DEBUG(NaClLog(LOG_INFO,
                "<-NcOPerandOneZeroExtends = %"NACL_PRIdBool"\n", result));
  return result;
}

Bool NaClAssignsRegisterWithZeroExtends(NaClInstState* state,
                                        NaClOpKind reg_name) {
  return NaClOperandOneIsRegisterSet(state, reg_name) &&
      NaClOperandOneZeroExtends(state);
}
