/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Defines two byte opcodes beginning with OF.
 */

#include "native_client/src/trusted/validator_x86/ncdecode_tablegen.h"

static void NaClDefJmp0FPair(uint8_t opcode, NaClMnemonic name) {
  NaClDefInstChoices_32_64(opcode, 2, 1);
  NaClDefInst(opcode, NACLi_JMPZ,
              NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OpcodeHasImmed) |
              NACL_IFLAG(Opcode32Only),
              name);
  NaClDefOp(RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(J_Operand,
            NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) |
            NACL_OPFLAG(OperandRelative));

  NaClDefInst(opcode, NACLi_JMPZ,
              NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OpcodeHasImmed),
              name);
  NaClDefOp(RegREIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(J_Operand,
            NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) |
            NACL_OPFLAG(OperandRelative));
}

static void NaClDefSetCC(uint8_t opcode, NaClMnemonic name) {
  NaClDefInstChoices_32_64(opcode, 1, 2);
  NaClDefInst(opcode, NACLi_386,
              NACL_IFLAG(OperandSize_b) | NACL_IFLAG(OpcodeUsesModRm),
              name);
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet));

  NaClDefInst(opcode, NACLi_386,
              NACL_IFLAG(OperandSize_b) | NACL_IFLAG(OpcodeUsesModRm) |
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OpcodeRex),
              name);
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet));
}

static void NaClDefCmovCC(uint8_t opcode, NaClMnemonic name) {
  NaClDefInstChoices_32_64(opcode, 1, 2);
  NaClDefInst(opcode, NACLi_CMOV,
              NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
              NACL_IFLAG(OpcodeUsesModRm), name);
  NaClDefOp(G_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(opcode, NACLi_CMOV,
              NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OpcodeUsesModRm) |
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OpcodeUsesRexW),
              name);
  NaClDefOp(G_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));
}

static void NaClDefBswap() {
  uint8_t i;
  for (i = 0; i < 8; ++i) {
    NaClDefInstChoices_32_64(0xc8 + i, 1, 2);
    NaClDefInst(0xC8 + i, NACLi_386,
                NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OpcodePlusR),
                InstBswap);
    NaClDefOp(OpcodeBaseMinus0 + i, NACL_OPFLAG(OperandExtendsOpcode));
    NaClDefOp(G_OpcodeBase, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));

    NaClDefInst(0xC8 + i, NACLi_386,
                NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OperandSize_o) |
                NACL_IFLAG(OpcodePlusR),
                InstBswap);
    NaClDefOp(OpcodeBaseMinus0 + i, NACL_OPFLAG(OperandExtendsOpcode));
    NaClDefOp(G_OpcodeBase, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
  }
}

void NaClDef0FInsts() {
  int i;
  NaClDefInstPrefix(Prefix0F);

  /* Note: The SSE instructions that begin with 0F are not defined here. Look
   * at ncdecode_sse.c for the definitions of SSE instruction.
   */

  NaClDefInst(0x18, NACLi_SSE,
              NACL_IFLAG(OpcodeInModRm),
              InstPrefetchnta);
  NaClDefOp(Opcode0, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(Mb_Operand, NACL_EMPTY_OPFLAGS);


  NaClDefInst(0x18, NACLi_SSE,
              NACL_IFLAG(OpcodeInModRm),
              InstPrefetcht0);
  NaClDefOp(Opcode1, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(Mb_Operand, NACL_EMPTY_OPFLAGS);


  NaClDefInst(0x18, NACLi_SSE,
              NACL_IFLAG(OpcodeInModRm),
              InstPrefetcht1);
  NaClDefOp(Opcode2, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(Mb_Operand, NACL_EMPTY_OPFLAGS);


  NaClDefInst(0x18, NACLi_SSE,
              NACL_IFLAG(OpcodeInModRm),
              InstPrefetcht2);
  NaClDefOp(Opcode3, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(Mb_Operand, NACL_EMPTY_OPFLAGS);

  for (i = 4; i < 8; ++i) {
    NaClDefInst(0x18, NACLi_386, NACL_IFLAG(OpcodeInModRm), InstNop);
    NaClDefOp(Opcode0 + i, NACL_OPFLAG(OperandExtendsOpcode));
  }

  /* TODO(karl) Should we verify the contents of the nop matches table 4.1
   * in Intel manual? (i.e. only allow valid forms of modrm data and
   * displacements).
   */
  NaClDefInst(0x1F, NACLi_386,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_w) |
              NACL_IFLAG(OperandSize_v) | NACL_IFLAG(IgnorePrefixDATA16) |
              NACL_IFLAG(IgnorePrefixSEGCS),
              InstNop);
  NaClDefOp(Opcode0, NACL_OPFLAG(OperandExtendsOpcode));

  /* CMOVcc */
  NaClDefCmovCC(0x40, InstCmovo);
  NaClDefCmovCC(0x41, InstCmovno);
  NaClDefCmovCC(0x42, InstCmovb);
  NaClDefCmovCC(0x43, InstCmovnb);
  NaClDefCmovCC(0x44, InstCmovz);
  NaClDefCmovCC(0x45, InstCmovnz);
  NaClDefCmovCC(0x46, InstCmovbe);
  NaClDefCmovCC(0x47, InstCmovnbe);
  NaClDefCmovCC(0x48, InstCmovs);
  NaClDefCmovCC(0x49, InstCmovns);
  NaClDefCmovCC(0x4a, InstCmovp);
  NaClDefCmovCC(0x4b, InstCmovnp);
  NaClDefCmovCC(0x4c, InstCmovl);
  NaClDefCmovCC(0x4d, InstCmovnl);
  NaClDefCmovCC(0x4e, InstCmovle);
  NaClDefCmovCC(0x4f, InstCmovnle);

  NaClDefInst(0x05, NACLi_SYSCALL, NACL_IFLAG(Opcode64Only), InstSyscall);
  NaClDefOp(RegRCX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegRIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse) |
            NACL_OPFLAG(OpImplicit));

  /* JMPcc */
  NaClDefJmp0FPair(0x80, InstJo);
  NaClDefJmp0FPair(0x81, InstJno);
  NaClDefJmp0FPair(0x82, InstJb);
  NaClDefJmp0FPair(0x83, InstJnb);
  NaClDefJmp0FPair(0x84, InstJz);
  NaClDefJmp0FPair(0x85, InstJnz);
  NaClDefJmp0FPair(0x86, InstJbe);
  NaClDefJmp0FPair(0x87, InstJnbe);
  NaClDefJmp0FPair(0x88, InstJs);
  NaClDefJmp0FPair(0x89, InstJns);
  NaClDefJmp0FPair(0x8a, InstJp);
  NaClDefJmp0FPair(0x8b, InstJnp);
  NaClDefJmp0FPair(0x8c, InstJl);
  NaClDefJmp0FPair(0x8d, InstJnl);
  NaClDefJmp0FPair(0x8e, InstJle);
  NaClDefJmp0FPair(0x8f, InstJnle);

  /* SETcc */
  NaClDefSetCC(0x90, InstSeto);
  NaClDefSetCC(0x91, InstSetno);
  NaClDefSetCC(0x92, InstSetb);
  NaClDefSetCC(0x93, InstSetnb);
  NaClDefSetCC(0x94, InstSetz);
  NaClDefSetCC(0x95, InstSetnz);
  NaClDefSetCC(0x96, InstSetbe);
  NaClDefSetCC(0x97, InstSetnbe);
  NaClDefSetCC(0x98, InstSets);
  NaClDefSetCC(0x99, InstSetns);
  NaClDefSetCC(0x9a, InstSetp);
  NaClDefSetCC(0x9b, InstSetnp);
  NaClDefSetCC(0x9c, InstSetl);
  NaClDefSetCC(0x9d, InstSetnl);
  NaClDefSetCC(0x9e, InstSetle);
  NaClDefSetCC(0x9f, InstSetnle);

  /* CPUID */
  NaClDefInst(0xa2, NACLi_386, 0, InstCpuid);
  NaClDefOp(RegEAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegEBX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegECX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegEDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));

  /* ISE reviewers suggested omitting bt. */
  NaClDefInstChoices_32_64(0xa3, 1, 2);
  NaClDefInst(0xa3, NACLi_ILLEGAL,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_w) |
              NACL_IFLAG(OperandSize_v) | NACL_IFLAG(AddressSizeDefaultIs32),
              InstBt);
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(G_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xa3, NACLi_ILLEGAL,
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OpcodeUsesModRm) |
              NACL_IFLAG(OperandSize_o) | NACL_IFLAG(AddressSizeDefaultIs32),
              InstBt);
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(G_Operand, NACL_OPFLAG(OpUse));

  /* ISE reviewers suggested omitting btr. */
  NaClDefInstChoices_32_64(0xab, 1, 2);
  NaClDefInst(0xab, NACLi_ILLEGAL,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_w) |
              NACL_IFLAG(OperandSize_v) | NACL_IFLAG(AddressSizeDefaultIs32),
              InstBts);
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
  NaClDefOp(G_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xab, NACLi_ILLEGAL,
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OpcodeUsesModRm) |
              NACL_IFLAG(OperandSize_o) | NACL_IFLAG(AddressSizeDefaultIs32),
              InstBts);
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
  NaClDefOp(G_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xae, NACLi_SSE2,
              NACL_IFLAG(OpcodeInModRm),
              InstLfence);
  NaClDefOp(Opcode5, NACL_OPFLAG(OperandExtendsOpcode));

  NaClDefInst(0xae, NACLi_SSE2,
              NACL_IFLAG(OpcodeInModRm),
              InstMfence);
  NaClDefOp(Opcode6, NACL_OPFLAG(OperandExtendsOpcode));

  NaClDefInst(0xae, NACLi_SFENCE_CLFLUSH,
              NACL_IFLAG(OpcodeInModRm),
              InstSfence);
  NaClDefOp(Opcode7, NACL_OPFLAG(OperandExtendsOpcode));

  NaClDefInstChoices_32_64(0xaf, 1, 2);
  NaClDefInst(0xaf, NACLi_386,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_w) |
              NACL_IFLAG(OperandSize_v),
              InstImul);
  NaClDefOp(G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xaf, NACLi_386,
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OpcodeUsesModRm) |
              NACL_IFLAG(OperandSize_o),
              InstImul);
  NaClDefOp(G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xB0, NACLi_386L,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b) |
              NACL_IFLAG(OpcodeLockable),
              InstCmpxchg);
  NaClDefOp(RegAL, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
  NaClDefOp(G_Operand, NACL_OPFLAG(OpUse));

  NaClDefInstChoices_32_64(0xb1, 1, 2);
  NaClDefInst(0xB1, NACLi_386L,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_w) |
              NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OpcodeLockable),
              InstCmpxchg);
  NaClDefOp(RegREAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
  NaClDefOp(G_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xB1, NACLi_386L,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(Opcode64Only) |
              NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OpcodeLockable),
               InstCmpxchg);
  NaClDefOp(RegRAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
  NaClDefOp(G_Operand, NACL_OPFLAG(OpUse));

  /* ISE reviewers suggested omitting btc */
  NaClDefInstChoices_32_64(0xb3, 1, 2);
  NaClDefInst(0xb3, NACLi_ILLEGAL,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_w) |
              NACL_IFLAG(OperandSize_v) | NACL_IFLAG(AddressSizeDefaultIs32),
              InstBtr);
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
  NaClDefOp(G_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xb3, NACLi_ILLEGAL,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_o) |
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(AddressSizeDefaultIs32),
              InstBtr);
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
  NaClDefOp(G_Operand, NACL_OPFLAG(OpUse));

  /* MOVZX */
  NaClDefInstChoices_32_64(0xb6, 1, 2);
  NaClDefInst(0xb6, NACLi_386,
              NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
              NACL_IFLAG(OpcodeUsesModRm),
              InstMovzx);
  NaClDefOp(G_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(Eb_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xb6, NACLi_386,
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OperandSize_o) |
              NACL_IFLAG(OpcodeUsesRexW) | NACL_IFLAG(OpcodeUsesModRm),
              InstMovzx);
  NaClDefOp(G_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(Eb_Operand, NACL_OPFLAG(OpUse));

  NaClDefInstChoices_32_64(0xb7, 1, 2);
  NaClDefInst(0xb7, NACLi_386,
              NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OpcodeUsesModRm),
              InstMovzx);
  NaClDefOp(G_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(Ew_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xb7, NACLi_386,
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OperandSize_o) |
              NACL_IFLAG(OpcodeUsesRexW) | NACL_IFLAG(OpcodeUsesModRm),
              InstMovzx);
  NaClDefOp(G_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(Ew_Operand, NACL_OPFLAG(OpUse));

  /* ISE reviewers suggested omitting bt, btc, btr and bts, but must
   * be kept in 64-bit mode, because the compiler needs it to access
   * the top 32-bits of a 64-bit value.
   */
  NaClDefInstMrmChoices_32_64(0xba, Opcode4, 1, 2);
  NaClDefInst(0xba, NACLi_386,
              NACL_IFLAG(Opcode32Only) | NACL_IFLAG(NaClIllegal) |
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_w) |
              NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OpcodeHasImmed_b),
              InstBtr);
  NaClDefOp(Opcode4, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xba, NACLi_386,
              NACL_IFLAG(Opcode64Only) |
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_w) |
              NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OpcodeHasImmed_b),
              InstBtr);
  NaClDefOp(Opcode4, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xba, NACLi_386,
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OpcodeInModRm) |
              NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OpcodeHasImmed_b),
              InstBt);
  NaClDefOp(Opcode4, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInstMrmChoices_32_64(0xba, Opcode5, 1, 2);
  NaClDefInst(0xba, NACLi_386,
              NACL_IFLAG(Opcode32Only) | NACL_IFLAG(NaClIllegal) |
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_w) |
              NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OpcodeHasImmed_b),
              InstBts);
  NaClDefOp(Opcode5, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xba, NACLi_386,
              NACL_IFLAG(Opcode64Only) |
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_w) |
              NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OpcodeHasImmed_b),
              InstBts);
  NaClDefOp(Opcode5, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xba, NACLi_386,
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OpcodeInModRm) |
              NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OpcodeHasImmed_b),
              InstBts);
  NaClDefOp(Opcode5, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInstMrmChoices_32_64(0xba, Opcode6, 1, 2);
  NaClDefInst(0xba, NACLi_386,
              NACL_IFLAG(Opcode32Only) | NACL_IFLAG(NaClIllegal) |
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_w) |
              NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OpcodeHasImmed_b),
              InstBtr);
  NaClDefOp(Opcode6, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xba, NACLi_386,
              NACL_IFLAG(Opcode64Only) |
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_w) |
              NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OpcodeHasImmed_b),
              InstBtr);
  NaClDefOp(Opcode6, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xba, NACLi_386,
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OpcodeInModRm) |
              NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OpcodeHasImmed_b),
              InstBtr);
  NaClDefOp(Opcode6, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInstMrmChoices_32_64(0xba, Opcode7, 1, 2);
  NaClDefInst(0xba, NACLi_386,
              NACL_IFLAG(Opcode32Only) | NACL_IFLAG(NaClIllegal) |
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_w) |
              NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OpcodeHasImmed_b),
              InstBtc);
  NaClDefOp(Opcode7, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xba, NACLi_386,
              NACL_IFLAG(Opcode64Only) |
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_w) |
              NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OpcodeHasImmed_b),
              InstBtc);
  NaClDefOp(Opcode7, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xba, NACLi_386,
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OpcodeInModRm) |
              NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OpcodeHasImmed_b),
              InstBtc);
  NaClDefOp(Opcode7, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  /* ISE reviewers suggested omitting btc */
  NaClDefInstChoices_32_64(0xbb, 1, 2);
  NaClDefInst(0xbb, NACLi_ILLEGAL,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_w) |
              NACL_IFLAG(OperandSize_v) | NACL_IFLAG(AddressSizeDefaultIs32),
              InstBtc);
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
  NaClDefOp(G_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xbb, NACLi_ILLEGAL,
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OpcodeUsesModRm) |
              NACL_IFLAG(OperandSize_o) | NACL_IFLAG(AddressSizeDefaultIs32),
              InstBtc);
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
  NaClDefOp(G_Operand, NACL_OPFLAG(OpUse));

  NaClDefInstChoices_32_64(0xbc, 1, 2);
  NaClDefInst(0xbc, NACLi_386,
              NACL_IFLAG(OpcodeUsesModRm) |
              NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
              InstBsf);
  NaClDefOp(G_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet));

  NaClDefInst(0xbc, NACLi_386,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(Opcode64Only) |
              NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OpcodeUsesRexW),
              InstBsf);
  NaClDefOp(G_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet));

  NaClDefInstChoices_32_64(0xbd, 1, 2);
  NaClDefInst(0xbd, NACLi_386,
              NACL_IFLAG(OpcodeUsesModRm) |
              NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
              InstBsr);
  NaClDefOp(G_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet));

  NaClDefInst(0xbd, NACLi_386,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(Opcode64Only) |
              NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OpcodeUsesRexW),
              InstBsr);
  NaClDefOp(G_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet));

  /* MOVSX */
  NaClDefInstChoices_32_64(0xbe, 1, 2);
  NaClDefInst(0xbe, NACLi_386,
              NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
              NACL_IFLAG(OpcodeUsesModRm),
              InstMovsx);
  NaClDefOp(G_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(Eb_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xbe, NACLi_386,
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OperandSize_o) |
              NACL_IFLAG(OpcodeRex) | NACL_IFLAG(OpcodeUsesModRm),
              InstMovsx);
  NaClDefOp(G_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(Eb_Operand, NACL_OPFLAG(OpUse));

  NaClDefInstChoices_32_64(0xbf, 1, 2);
  NaClDefInst(0xbf, NACLi_386,
              NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
              NACL_IFLAG(OpcodeUsesModRm),
              InstMovsx);
  NaClDefOp(G_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(Ew_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xbf, NACLi_386,
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OperandSize_o) |
              NACL_IFLAG(OpcodeUsesRexW) | NACL_IFLAG(OpcodeUsesModRm),
              InstMovsx);
  NaClDefOp(G_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(Ew_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xC0, NACLi_386L,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b) |
              NACL_IFLAG(OpcodeLockable),
              InstXadd);
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
  NaClDefOp(G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));

  NaClDefInstChoices_32_64(0xc1, 1, 2);
  NaClDefInst(0xC1, NACLi_386L,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_w) |
              NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OpcodeLockable),
              InstXadd);
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
  NaClDefOp(G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));

  NaClDefInst(0xC1, NACLi_386L,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(Opcode64Only) |
              NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OpcodeLockable),
              InstXadd);
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
  NaClDefOp(G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));

  NaClDefInstChoices_32_64(0xc3, 1, 2);
  NaClDefInst(0xc3, NACLi_SSE2,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_v),
              InstMovnti);
  NaClDefOp(M_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(G_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xc3, NACLi_SSE2,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_o) |
              NACL_IFLAG(Opcode64Only),
              InstMovnti);
  NaClDefOp(M_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(G_Operand, NACL_OPFLAG(OpUse));

  /* NaClDefs C8 throught CF */
  NaClDefBswap();
}
