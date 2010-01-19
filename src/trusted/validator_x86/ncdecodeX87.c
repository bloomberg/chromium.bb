/* Copyright (c) 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * ncdecodex87.c - Handles x87 instructions.
 */

#include "native_client/src/trusted/validator_x86/ncdecode_tablegen.h"

static void DefineD9Opcodes() {
  int i;
  DefineOpcodePrefix(PrefixD9);
  for (i = 0; i < 8; ++i) {
    DefineOpcode(0xC0 + i, NACLi_X87, InstFlag(OpcodePlusR), InstFld);
    DefineOperand(OpcodeBaseMinus0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(St_Operand, OpFlag(OpUse));
  }
}

void DefineX87Opcodes() {
  DefineOpcodePrefix(NoPrefix);

  DefineOpcode(0xd9, NACLi_X87, InstFlag(OpcodeInModRm), InstFld);
  DefineOperand(Opcode0, OpFlag(OperandExtendsOpcode));
  DefineOperand(E_Operand, OpFlag(OpUse));

  DefineOpcode(0xdb, NACLi_X87, InstFlag(OpcodeInModRm), InstFld);
  DefineOperand(Opcode5, OpFlag(OperandExtendsOpcode));
  DefineOperand(E_Operand, OpFlag(OpUse));

  DefineOpcode(0xdd, NACLi_X87, InstFlag(OpcodeInModRm), InstFld);
  DefineOperand(Opcode0, OpFlag(OperandExtendsOpcode));
  DefineOperand(E_Operand, OpFlag(OpUse));

  DefineD9Opcodes();
}
