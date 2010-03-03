/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Defines two byte opcodes beginning with OF.
 */

#include "native_client/src/trusted/validator_x86/ncdecode_tablegen.h"

static void DefineJmp0FPair(uint8_t opcode, InstMnemonic name) {
  DefineOpcodeChoices_32_64(opcode, 2, 1);
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
  DefineOpcodeChoices_32_64(opcode, 1, 2);
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
  DefineOpcodeChoices_32_64(opcode, 1, 2);
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
    DefineOpcodeChoices_32_64(0xc8 + i, 1, 2);
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
  int i;
  DefineOpcodePrefix(Prefix0F);

  /* Note: The SSE instructions that begin with 0F are not defined here. Look
   * at ncdecode_sse.c for the definitions of SSE instruction.
   */

  DefineOpcode(0x18, NACLi_SSE,
               InstFlag(OpcodeInModRm),
               InstPrefetchnta);
  DefineOperand(Opcode0, OpFlag(OperandExtendsOpcode));
  DefineOperand(Mb_Operand, (OperandFlags) 0);


  DefineOpcode(0x18, NACLi_SSE,
               InstFlag(OpcodeInModRm),
               InstPrefetcht0);
  DefineOperand(Opcode1, OpFlag(OperandExtendsOpcode));
  DefineOperand(Mb_Operand, (OperandFlags) 0);


  DefineOpcode(0x18, NACLi_SSE,
               InstFlag(OpcodeInModRm),
               InstPrefetcht1);
  DefineOperand(Opcode2, OpFlag(OperandExtendsOpcode));
  DefineOperand(Mb_Operand, (OperandFlags) 0);


  DefineOpcode(0x18, NACLi_SSE,
               InstFlag(OpcodeInModRm),
               InstPrefetcht2);
  DefineOperand(Opcode3, OpFlag(OperandExtendsOpcode));
  DefineOperand(Mb_Operand, (OperandFlags) 0);

  for (i = 4; i < 8; ++i) {
    DefineOpcode(0x18, NACLi_386, InstFlag(OpcodeInModRm), InstNop);
    DefineOperand(Opcode0 + i, OpFlag(OperandExtendsOpcode));
  }

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

  /* ISE reviewers suggested omitting bt. */
  DefineOpcodeChoices_32_64(0xa3, 1, 2);
  DefineOpcode(0xa3, NACLi_ILLEGAL,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_w) |
               InstFlag(OperandSize_v) | InstFlag(AddressSizeDefaultIs32),
               InstBt);
  DefineOperand(E_Operand, OpFlag(OpUse));
  DefineOperand(G_Operand, OpFlag(OpUse));

  DefineOpcode(0xa3, NACLi_ILLEGAL,
               InstFlag(Opcode64Only) | InstFlag(OpcodeUsesModRm) |
               InstFlag(OperandSize_o) | InstFlag(AddressSizeDefaultIs32),
               InstBt);
  DefineOperand(E_Operand, OpFlag(OpUse));
  DefineOperand(G_Operand, OpFlag(OpUse));

  /* ISE reviewers suggested omitting btr. */
  DefineOpcodeChoices_32_64(0xab, 1, 2);
  DefineOpcode(0xab, NACLi_ILLEGAL,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_w) |
               InstFlag(OperandSize_v) | InstFlag(AddressSizeDefaultIs32),
               InstBts);
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(G_Operand, OpFlag(OpUse));

  DefineOpcode(0xab, NACLi_ILLEGAL,
               InstFlag(Opcode64Only) | InstFlag(OpcodeUsesModRm) |
               InstFlag(OperandSize_o) | InstFlag(AddressSizeDefaultIs32),
               InstBts);
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(G_Operand, OpFlag(OpUse));

  DefineOpcode(0xae, NACLi_SSE2,
               InstFlag(OpcodeInModRm),
               InstLfence);
  DefineOperand(Opcode5, OpFlag(OperandExtendsOpcode));

  DefineOpcode(0xae, NACLi_SSE2,
               InstFlag(OpcodeInModRm),
               InstMfence);
  DefineOperand(Opcode6, OpFlag(OperandExtendsOpcode));

  DefineOpcode(0xae, NACLi_SFENCE_CLFLUSH,
               InstFlag(OpcodeInModRm),
               InstSfence);
  DefineOperand(Opcode7, OpFlag(OperandExtendsOpcode));

  DefineOpcodeChoices_32_64(0xaf, 1, 2);
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

  DefineOpcodeChoices_32_64(0xb1, 1, 2);
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

  /* ISE reviewers suggested omitting btc */
  DefineOpcodeChoices_32_64(0xb3, 1, 2);
  DefineOpcode(0xb3, NACLi_ILLEGAL,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_w) |
               InstFlag(OperandSize_v) | InstFlag(AddressSizeDefaultIs32),
               InstBtr);
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(G_Operand, OpFlag(OpUse));

  DefineOpcode(0xb3, NACLi_ILLEGAL,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_o) |
               InstFlag(Opcode64Only) | InstFlag(AddressSizeDefaultIs32),
               InstBtr);
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(G_Operand, OpFlag(OpUse));

  /* MOVZX */
  DefineOpcodeChoices_32_64(0xb6, 1, 2);
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

  DefineOpcodeChoices_32_64(0xb7, 1, 2);
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

  /* ISE reviewers suggested omitting bt, btc, btr and bts, but must
   * be kept in 64-bit mode, because the compiler needs it to access
   * the top 32-bits of a 64-bit value.
   */
  DefineOpcodeMrmChoices_32_64(0xba, Opcode4, 1, 2);
  DefineOpcode(0xba, NACLi_386,
               InstFlag(Opcode32Only) | InstFlag(NaclIllegal) |
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_w) |
               InstFlag(OperandSize_v) | InstFlag(OpcodeHasImmed_b),
               InstBtr);
  DefineOperand(Opcode4, OpFlag(OperandExtendsOpcode));
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcode(0xba, NACLi_386,
               InstFlag(Opcode64Only) |
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_w) |
               InstFlag(OperandSize_v) | InstFlag(OpcodeHasImmed_b),
               InstBtr);
  DefineOperand(Opcode4, OpFlag(OperandExtendsOpcode));
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcode(0xba, NACLi_386,
               InstFlag(Opcode64Only) | InstFlag(OpcodeInModRm) |
               InstFlag(OperandSize_o) | InstFlag(OpcodeHasImmed_b),
               InstBt);
  DefineOperand(Opcode4, OpFlag(OperandExtendsOpcode));
  DefineOperand(E_Operand, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodeMrmChoices_32_64(0xba, Opcode5, 1, 2);
  DefineOpcode(0xba, NACLi_386,
               InstFlag(Opcode32Only) | InstFlag(NaclIllegal) |
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_w) |
               InstFlag(OperandSize_v) | InstFlag(OpcodeHasImmed_b),
               InstBts);
  DefineOperand(Opcode5, OpFlag(OperandExtendsOpcode));
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcode(0xba, NACLi_386,
               InstFlag(Opcode64Only) |
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_w) |
               InstFlag(OperandSize_v) | InstFlag(OpcodeHasImmed_b),
               InstBts);
  DefineOperand(Opcode5, OpFlag(OperandExtendsOpcode));
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcode(0xba, NACLi_386,
               InstFlag(Opcode64Only) | InstFlag(OpcodeInModRm) |
               InstFlag(OperandSize_o) | InstFlag(OpcodeHasImmed_b),
               InstBts);
  DefineOperand(Opcode5, OpFlag(OperandExtendsOpcode));
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodeMrmChoices_32_64(0xba, Opcode6, 1, 2);
  DefineOpcode(0xba, NACLi_386,
               InstFlag(Opcode32Only) | InstFlag(NaclIllegal) |
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_w) |
               InstFlag(OperandSize_v) | InstFlag(OpcodeHasImmed_b),
               InstBtr);
  DefineOperand(Opcode6, OpFlag(OperandExtendsOpcode));
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcode(0xba, NACLi_386,
               InstFlag(Opcode64Only) |
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_w) |
               InstFlag(OperandSize_v) | InstFlag(OpcodeHasImmed_b),
               InstBtr);
  DefineOperand(Opcode6, OpFlag(OperandExtendsOpcode));
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcode(0xba, NACLi_386,
               InstFlag(Opcode64Only) | InstFlag(OpcodeInModRm) |
               InstFlag(OperandSize_o) | InstFlag(OpcodeHasImmed_b),
               InstBtr);
  DefineOperand(Opcode6, OpFlag(OperandExtendsOpcode));
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodeMrmChoices_32_64(0xba, Opcode7, 1, 2);
  DefineOpcode(0xba, NACLi_386,
               InstFlag(Opcode32Only) | InstFlag(NaclIllegal) |
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_w) |
               InstFlag(OperandSize_v) | InstFlag(OpcodeHasImmed_b),
               InstBtc);
  DefineOperand(Opcode7, OpFlag(OperandExtendsOpcode));
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcode(0xba, NACLi_386,
               InstFlag(Opcode64Only) |
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_w) |
               InstFlag(OperandSize_v) | InstFlag(OpcodeHasImmed_b),
               InstBtc);
  DefineOperand(Opcode7, OpFlag(OperandExtendsOpcode));
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcode(0xba, NACLi_386,
               InstFlag(Opcode64Only) | InstFlag(OpcodeInModRm) |
               InstFlag(OperandSize_o) | InstFlag(OpcodeHasImmed_b),
               InstBtc);
  DefineOperand(Opcode7, OpFlag(OperandExtendsOpcode));
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  /* ISE reviewers suggested omitting btc */
  DefineOpcodeChoices_32_64(0xbb, 1, 2);
  DefineOpcode(0xbb, NACLi_ILLEGAL,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_w) |
               InstFlag(OperandSize_v) | InstFlag(AddressSizeDefaultIs32),
               InstBtc);
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(G_Operand, OpFlag(OpUse));

  DefineOpcode(0xbb, NACLi_ILLEGAL,
               InstFlag(Opcode64Only) | InstFlag(OpcodeUsesModRm) |
               InstFlag(OperandSize_o) | InstFlag(AddressSizeDefaultIs32),
               InstBtc);
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(G_Operand, OpFlag(OpUse));

  DefineOpcodeChoices_32_64(0xbc, 1, 2);
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

  DefineOpcodeChoices_32_64(0xbd, 1, 2);
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
  DefineOpcodeChoices_32_64(0xbe, 1, 2);
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

  DefineOpcodeChoices_32_64(0xbf, 1, 2);
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

  DefineOpcodeChoices_32_64(0xc1, 1, 2);
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

  DefineOpcodeChoices_32_64(0xc3, 1, 2);
  DefineOpcode(0xc3, NACLi_SSE2,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_v),
               InstMovnti);
  DefineOperand(M_Operand, OpFlag(OpSet));
  DefineOperand(G_Operand, OpFlag(OpUse));

  DefineOpcode(0xc3, NACLi_SSE2,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_o) |
               InstFlag(Opcode64Only),
               InstMovnti);
  DefineOperand(M_Operand, OpFlag(OpSet));
  DefineOperand(G_Operand, OpFlag(OpUse));

  /* Defines C8 throught CF */
  DefineBswap();
}
