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

/*
 * Defines two byte opcodes beginning with OF.
 */

#include "native_client/src/trusted/validator_x86/ncdecode_tablegen.h"

static void DefineJmp0FPair(uint8_t opcode, InstMnemonic name) {
  DefineOpcode(opcode, NACLi_JMPZ,
               InstFlag(OperandSize_w) | InstFlag(OpcodeHasImmed) |
               InstFlag(Opcode32Only),
               name);
  DefineOperand(RegEIP, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(J_Operand,
                OpFlag(OpUse) | OpFlag(OperandNear) | OpFlag(OperandRelative));

  DefineOpcode(opcode, NACLi_JMPZ,
               InstFlag(OperandSize_v) | InstFlag(OpcodeHasImmed),
               name);
  DefineOperand(RegREIP, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(J_Operand,
                OpFlag(OpUse) | OpFlag(OperandNear) | OpFlag(OperandRelative));
}

void Define0FOpcodes() {
  DefineOpcodePrefix(Prefix0F);

  /* Note: The SSE instructions that begin with 0F are not defined here. Look
   * at ncdecode_sse.c for the definitions of SSE instruction.
   */

  /* TODO(karl) Should we verify the contents of the nop matches table 4.1
   * in Intel manual? (i.e. only allow valid forms of modrm data and
   * displacements).
   */
  DefineOpcode(0x1F, NACLi_386,
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_w) |
               InstFlag(OperandSize_v),
               InstNop);
  DefineOperand(Opcode0, OpFlag(OperandExtendsOpcode));

  DefineJmp0FPair(0x80, InstJo);
  DefineJmp0FPair(0x81, InstJno);
  DefineJmp0FPair(0x82, InstJb);
  DefineJmp0FPair(0x83, InstJnb);
  DefineJmp0FPair(0x84, InstJz);
  DefineJmp0FPair(0x85, InstJnz);
  DefineJmp0FPair(0x86, InstJbe);
  DefineJmp0FPair(0x87, InstJnbe);
  DefineJmp0FPair(0x88, InstJs);
  DefineJmp0FPair(0x89, InstJns);
  DefineJmp0FPair(0x8a, InstJp);
  DefineJmp0FPair(0x8b, InstJnp);
  DefineJmp0FPair(0x8c, InstJl);
  DefineJmp0FPair(0x8d, InstJge);
  DefineJmp0FPair(0x8e, InstJle);
  DefineJmp0FPair(0x8f, InstJg);

  DefineOpcode(0xbe, NACLi_386,
               InstFlag(OperandSize_w) | InstFlag(OperandSize_v) |
               InstFlag(OpcodeUsesModRm),
               InstMovsx);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(Eb_Operand, OpFlag(OpUse));

  DefineOpcode(0xbe, NACLi_386,
               InstFlag(Opcode64Only) | InstFlag(OperandSize_o) |
               InstFlag(OpcodeRex) | InstFlag(OpcodeUsesModRm),
               InstMovsx);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(Eb_Operand, OpFlag(OpUse));

  DefineOpcode(0xbf, NACLi_386,
               InstFlag(OperandSize_w) | InstFlag(OperandSize_v) |
               InstFlag(OpcodeUsesModRm),
               InstMovsx);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(Ew_Operand, OpFlag(OpUse));

  DefineOpcode(0xbf, NACLi_386,
               InstFlag(Opcode64Only) | InstFlag(OperandSize_o) |
               InstFlag(OpcodeUsesRexW) | InstFlag(OpcodeUsesModRm),
               InstMovsx);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(Ew_Operand, OpFlag(OpUse));
}
