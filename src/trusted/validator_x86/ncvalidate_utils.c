/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/* Some useful utilities for validator patterns. */

#include "native_client/src/trusted/validator_x86/ncvalidate_utils.h"

#include "native_client/src/trusted/validator_x86/nc_inst_state.h"
#include "native_client/src/trusted/validator_x86/ncop_exps.h"

/* To turn on debugging of instruction decoding, change value of
 * DEBUGGING to 1.
 */
#define DEBUGGING 0

#include "native_client/src/shared/utils/debugging.h"

const OperandFlags NcOpSetOrUse = OpFlag(OpSet) | OpFlag(OpUse);

Bool NcIsBinaryUsingRegisters(Opcode* opcode,
                              InstMnemonic name,
                              ExprNodeVector* vector,
                              OperandKind reg_1,
                              OperandKind reg_2) {
  return name == opcode->name &&
      2 == NcGetOpcodeNumberOperands(opcode) &&
      /* Note: Since the vector contains a list of operand expressions, the
       * first operand reference is always at index zero, and its first child
       * (where the register would be defined) is at index 1.
       */
      ExprRegister == vector->node[1].kind &&
      reg_1 == GetNodeRegister(&vector->node[1]) &&
      /* Note: Since the first subtree is a register operand, it uses
       * nodes 0 and 1 in the vector (node 0 is the operand reference, and
       * node 1 is its child defining a register value). The second operand
       * reference therefore lies at node 2, and if the operand is defined by
       * a register, it is the first kid of node 2, which is node 3.
       */
      ExprRegister == vector->node[3].kind &&
      reg_2 == GetNodeRegister(&vector->node[3]);
}

Bool NcIsMovUsingRegisters(Opcode* opcode,
                           ExprNodeVector* vector,
                           OperandKind reg_set,
                           OperandKind reg_use) {
  return NcIsBinaryUsingRegisters(opcode, InstMov, vector, reg_set, reg_use) &&
      OpFlag(OpSet) == (NcGetOpcodeOperand(opcode, 0)->flags & NcOpSetOrUse) &&
      OpFlag(OpUse) == (NcGetOpcodeOperand(opcode, 1)->flags & NcOpSetOrUse);
}

Bool NcIsBinarySetUsingRegisters(Opcode* opcode,
                                 InstMnemonic name,
                                 ExprNodeVector* vector,
                                 OperandKind reg_1,
                                 OperandKind reg_2) {
  return NcIsBinaryUsingRegisters(opcode, name, vector, reg_1, reg_2) &&
      NcOpSetOrUse == (NcGetOpcodeOperand(opcode, 0)->flags & NcOpSetOrUse) &&
      OpFlag(OpUse) == (NcGetOpcodeOperand(opcode, 1)->flags & NcOpSetOrUse);
}

Bool NcOperandOneIsRegisterSet(NcInstState* inst,
                               OperandKind reg_name) {
  /* Note: Since the vector contains a list of operand expressions, the
   * first operand reference is always at index zero, and its first child
   * (where the register would be defined) is at index 1.
   */
  Bool result = FALSE;
  ExprNodeVector* vector = NcInstStateNodeVector(inst);
  DEBUG(printf("->NcOperandOneIsRegisterSet %s\n", OperandKindName(reg_name)));
  DEBUG(PrintExprNodeVector(stdout, vector));
  if (vector->number_expr_nodes >= 2) {
    ExprNode* op_reg = &vector->node[1];
    result = (ExprRegister == op_reg->kind &&
              reg_name == GetNodeRegister(op_reg) &&
              (op_reg->flags & ExprFlag(ExprSet)));
  }
  DEBUG(printf("<-NcOperandOneIsRegisterSet = %"NACL_PRIdBool"\n", result));
  return result;
}

Bool NcOperandOneZeroExtends(NcInstState* inst) {
  Bool result = FALSE;
  Opcode* opcode = NcInstStateOpcode(inst);
  DEBUG(printf("->NcOperandOneZeroExtends\n"));
  DEBUG(PrintOpcode(stdout, opcode));
  result = (1 <= NcGetOpcodeNumberOperands(opcode) &&
            (NcGetOpcodeOperand(opcode, 0)->flags &
             OpFlag(OperandZeroExtends_v)) &&
            4 == NcInstStateOperandSize(inst));
  DEBUG(printf("<-NcOPerandOneZeroExtends = %"NACL_PRIdBool"\n", result));
  return result;
}

Bool NcAssignsRegisterWithZeroExtends(NcInstState* inst,
                                      OperandKind reg_name) {
  return NcOperandOneIsRegisterSet(inst, reg_name) &&
      NcOperandOneZeroExtends(inst);
}
