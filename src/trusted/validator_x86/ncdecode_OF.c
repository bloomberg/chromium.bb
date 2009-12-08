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

static void DefineSetCC(uint8_t opcode, InstMnemonic name) {
  DefineOpcode(opcode, NACLi_386,
               InstFlag(OperandSize_b) | InstFlag(OpcodeUsesModRm),
               name);
  DefineOperand(E_Operand, OpFlag(OpSet));

  DefineOpcode(opcode, NACLi_386,
               InstFlag(OperandSize_b) | InstFlag(OpcodeUsesModRm) |
               InstFlag(Opcode64Only) | InstFlag(OpcodeRex),
               name);
  DefineOperand(E_Operand, OpFlag(OpSet));
}

static void DefineCmovCC(uint8_t opcode, InstMnemonic name) {
  DefineOpcode(opcode, NACLi_CMOV,
               InstFlag(OperandSize_w) | InstFlag(OperandSize_v) |
               InstFlag(OpcodeUsesModRm), name);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(E_Operand, OpFlag(OpUse));

  DefineOpcode(opcode, NACLi_CMOV,
               InstFlag(OperandSize_o) | InstFlag(OpcodeUsesModRm) |
               InstFlag(Opcode64Only) | InstFlag(OpcodeUsesRexW),
               name);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(E_Operand, OpFlag(OpUse));
}

static void DefineBswap() {
  uint8_t i;
  for (i = 0; i < 8; ++i) {
    DefineOpcode(0xC8 + i, NACLi_386,
                 InstFlag(OperandSize_v) | InstFlag(OpcodePlusR),
                 InstBswap);
    DefineOperand(OpcodeBaseMinus0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(G_OpcodeBase, OpFlag(OpSet) | OpFlag(OpUse));

    DefineOpcode(0xC8 + i, NACLi_386,
                 InstFlag(Opcode64Only) | InstFlag(OperandSize_o) |
                 InstFlag(OpcodePlusR),
                 InstBswap);
    DefineOperand(OpcodeBaseMinus0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(G_OpcodeBase, OpFlag(OpSet) | OpFlag(OpUse));
  }
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
               InstFlag(OperandSize_v) | InstFlag(IgnorePrefixDATA16) |
               InstFlag(IgnorePrefixSEGCS),
               InstNop);
  DefineOperand(Opcode0, OpFlag(OperandExtendsOpcode));

  /* CMOVcc */
  DefineCmovCC(0x40, InstCmovo);
  DefineCmovCC(0x41, InstCmovno);
  DefineCmovCC(0x42, InstCmovb);
  DefineCmovCC(0x43, InstCmovnb);
  DefineCmovCC(0x44, InstCmovz);
  DefineCmovCC(0x45, InstCmovnz);
  DefineCmovCC(0x46, InstCmovbe);
  DefineCmovCC(0x47, InstCmovnbe);
  DefineCmovCC(0x48, InstCmovs);
  DefineCmovCC(0x49, InstCmovns);
  DefineCmovCC(0x4a, InstCmovp);
  DefineCmovCC(0x4b, InstCmovnp);
  DefineCmovCC(0x4c, InstCmovl);
  DefineCmovCC(0x4d, InstCmovnl);
  DefineCmovCC(0x4e, InstCmovle);
  DefineCmovCC(0x4f, InstCmovnle);

  DefineOpcode(0x05, NACLi_SYSCALL, InstFlag(Opcode64Only), InstSyscall);
  DefineOperand(RegRCX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegRIP, OpFlag(OpSet) | OpFlag(OpUse) | OpFlag(OpImplicit));

  DefineOpcode(0x77, NACLi_MMX, 0, InstEmms);

  /* JMPcc */
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
  DefineJmp0FPair(0x8d, InstJnl);
  DefineJmp0FPair(0x8e, InstJle);
  DefineJmp0FPair(0x8f, InstJnle);

  /* SETcc */
  DefineSetCC(0x90, InstSeto);
  DefineSetCC(0x91, InstSetno);
  DefineSetCC(0x92, InstSetb);
  DefineSetCC(0x93, InstSetnb);
  DefineSetCC(0x94, InstSetz);
  DefineSetCC(0x95, InstSetnz);
  DefineSetCC(0x96, InstSetbe);
  DefineSetCC(0x97, InstSetnbe);
  DefineSetCC(0x98, InstSets);
  DefineSetCC(0x99, InstSetns);
  DefineSetCC(0x9a, InstSetp);
  DefineSetCC(0x9b, InstSetnp);
  DefineSetCC(0x9c, InstSetl);
  DefineSetCC(0x9d, InstSetnl);
  DefineSetCC(0x9e, InstSetle);
  DefineSetCC(0x9f, InstSetnle);

  /* CPUID */
  DefineOpcode(0xa2, NACLi_386, 0, InstCpuid);
  DefineOperand(RegEAX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegEBX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegECX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegEDX, OpFlag(OpSet) | OpFlag(OpImplicit));

  DefineOpcode(0xaf, NACLi_386,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_w) |
               InstFlag(OperandSize_v),
               InstImul);
  DefineOperand(G_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(E_Operand, OpFlag(OpUse));

  DefineOpcode(0xaf, NACLi_386,
               InstFlag(Opcode64Only) | InstFlag(OpcodeUsesModRm) |
               InstFlag(OperandSize_o),
               InstImul);
  DefineOperand(G_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(E_Operand, OpFlag(OpUse));

  DefineOpcode(0xB0, NACLi_386L,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_b) |
               InstFlag(OpcodeLockable),
               InstCmpxchg);
  DefineOperand(RegAL, OpFlag(OpSet) | OpFlag(OpUse) | OpFlag(OpImplicit));
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(G_Operand, OpFlag(OpUse));

  DefineOpcode(0xB1, NACLi_386L,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_w) |
               InstFlag(OperandSize_v) | InstFlag(OpcodeLockable),
               InstCmpxchg);
  DefineOperand(RegREAX, OpFlag(OpSet) | OpFlag(OpUse) | OpFlag(OpImplicit));
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(G_Operand, OpFlag(OpUse));

  DefineOpcode(0xB1, NACLi_386L,
               InstFlag(OpcodeUsesModRm) | InstFlag(Opcode64Only) |
               InstFlag(OperandSize_o) | InstFlag(OpcodeLockable),
               InstCmpxchg);
  DefineOperand(RegRAX, OpFlag(OpSet) | OpFlag(OpUse) | OpFlag(OpImplicit));
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(G_Operand, OpFlag(OpUse));

  /* MOVZX */
  DefineOpcode(0xb6, NACLi_386,
               InstFlag(OperandSize_w) | InstFlag(OperandSize_v) |
               InstFlag(OpcodeUsesModRm),
               InstMovzx);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(Eb_Operand, OpFlag(OpUse));

  DefineOpcode(0xb6, NACLi_386,
               InstFlag(Opcode64Only) | InstFlag(OperandSize_o) |
               InstFlag(OpcodeUsesRexW) | InstFlag(OpcodeUsesModRm),
               InstMovzx);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(Eb_Operand, OpFlag(OpUse));

  DefineOpcode(0xb7, NACLi_386,
               InstFlag(OperandSize_v) | InstFlag(OpcodeUsesModRm),
               InstMovzx);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(Ew_Operand, OpFlag(OpUse));

  DefineOpcode(0xb7, NACLi_386,
               InstFlag(Opcode64Only) | InstFlag(OperandSize_o) |
               InstFlag(OpcodeUsesRexW) | InstFlag(OpcodeUsesModRm),
               InstMovzx);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(Ew_Operand, OpFlag(OpUse));

  DefineOpcode(0xbc, NACLi_386,
               InstFlag(OpcodeUsesModRm) |
               InstFlag(OperandSize_w) | InstFlag(OperandSize_v),
               InstBsf);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(E_Operand, OpFlag(OpSet));

  DefineOpcode(0xbc, NACLi_386,
               InstFlag(OpcodeUsesModRm) | InstFlag(Opcode64Only) |
               InstFlag(OperandSize_o) | InstFlag(OpcodeUsesRexW),
               InstBsf);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(E_Operand, OpFlag(OpSet));

  DefineOpcode(0xbd, NACLi_386,
               InstFlag(OpcodeUsesModRm) |
               InstFlag(OperandSize_w) | InstFlag(OperandSize_v),
               InstBsr);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(E_Operand, OpFlag(OpSet));

  DefineOpcode(0xbd, NACLi_386,
               InstFlag(OpcodeUsesModRm) | InstFlag(Opcode64Only) |
               InstFlag(OperandSize_o) | InstFlag(OpcodeUsesRexW),
               InstBsr);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(E_Operand, OpFlag(OpSet));

  /* MOVSX */
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

  DefineOpcode(0xC0, NACLi_386L,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_b) |
               InstFlag(OpcodeLockable),
               InstXadd);
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(G_Operand, OpFlag(OpSet) | OpFlag(OpUse));

  DefineOpcode(0xC1, NACLi_386L,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_w) |
               InstFlag(OperandSize_v) | InstFlag(OpcodeLockable),
               InstXadd);
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(G_Operand, OpFlag(OpSet) | OpFlag(OpUse));

  DefineOpcode(0xC1, NACLi_386L,
               InstFlag(OpcodeUsesModRm) | InstFlag(Opcode64Only) |
               InstFlag(OperandSize_o) | InstFlag(OpcodeLockable),
               InstXadd);
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(G_Operand, OpFlag(OpSet) | OpFlag(OpUse));

  /* Defines C8 throught CF */
  DefineBswap();
}
