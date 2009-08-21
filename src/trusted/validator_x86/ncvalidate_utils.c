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

Bool NcIsMovUsingRegisters(Opcode* opcode,
                           ExprNodeVector* vector,
                           OperandKind reg_set,
                           OperandKind reg_use) {
  return InstMov == opcode->name &&
      2 == NcGetOpcodeNumberOperands(opcode) &&
      OpFlag(OpSet) == (NcGetOpcodeOperand(opcode, 0)->flags & NcOpSetOrUse) &&
      OpFlag(OpUse) == (NcGetOpcodeOperand(opcode, 1)->flags & NcOpSetOrUse) &&
      /* Note: Since the vector contains a list of operand expressions, the
       * first operand reference is always at index zero, and its first child
       * (where the register would be defined) is at index 1.
       */
      ExprRegister == vector->node[1].kind &&
      reg_set == GetNodeRegister(&vector->node[1]) &&
      /* Note: Since the first subtree is a register operand, it uses
       * nodes 0 and 1 in the vector (node 0 is the operand reference, and
       * node 1 is its child defining a register value). The second operand
       * reference therefore lies at node 2, and if the operand is defined by
       * a register, it is the first kid of node 2, which is node 3.
       */
      ExprRegister == vector->node[3].kind &&
      reg_use == GetNodeRegister(&vector->node[3]);
}

Bool NcIsOrUsingRegister(Opcode* opcode,
                         ExprNodeVector* vector,
                         OperandKind reg_set,
                         OperandKind reg_use) {
  return InstOr == opcode->name &&
      2 == NcGetOpcodeNumberOperands(opcode) &&
      NcOpSetOrUse == (NcGetOpcodeOperand(opcode, 0)->flags & NcOpSetOrUse) &&
      OpFlag(OpUse) == (NcGetOpcodeOperand(opcode, 1)->flags & NcOpSetOrUse) &&
      /* Note: Since the vector contains a list of operand expressions, the
       * first operand reference is always at index zero, and its first child
       * (where the register would be defined) is at index 1.
       */
      ExprRegister == vector->node[1].kind &&
      reg_set == GetNodeRegister(&vector->node[1]) &&
      /* Note: Since the first subtree is a register operand, it uses
       * nodes 0 and 1 in the vector (node 0 is the operand reference, and
       * node 1 is its child defining a register value). The second operand
       * reference therefore lies at node 2, and if the operand is defined by
       * a register, it is the first kid of node 2, which is node 3.
       */
      ExprRegister == vector->node[3].kind &&
      reg_use == GetNodeRegister(&vector->node[3]);
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
  DEBUG(printf("<-NcOperandOneIsRegisterSet = %"PRIdBool"\n", result));
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
  DEBUG(printf("<-NcOPerandOneZeroExtends = %"PRIdBool"\n", result));
  return result;
}

Bool NcAssignsRegisterWithZeroExtends(NcInstState* inst,
                                      OperandKind reg_name) {
  return NcOperandOneIsRegisterSet(inst, reg_name) &&
      NcOperandOneZeroExtends(inst);
}
