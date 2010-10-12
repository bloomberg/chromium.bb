/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Defines two byte opcodes beginning with OF.
 */

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

#include "native_client/src/trusted/validator_x86/ncdecode_forms.h"
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

void NaClDef0FInsts(struct NaClSymbolTable* st) {
  int i;
  NaClDefDefaultInstPrefix(Prefix0F);

  NaClDefine("0f00/0: Sldt $Mw/Rv", NACLi_SYSTEM, st, UnarySet);
  NaClDefine("0f00/1: Str $Mw/Rv", NACLi_SYSTEM, st, UnarySet);
  NaClDefine("0f00/2: Lldt $Ew", NACLi_SYSTEM, st, Uses);
  NaClDefine("0f00/3: Ltr $Ew", NACLi_SYSTEM, st, Uses);
  NaClDefine("0f00/4: Verr $Ew", NACLi_SYSTEM, st, Other);
  NaClDefine("0f00/5: Verw $Ew", NACLi_SYSTEM, st, Other);
  NaClDefine("0f00/6: Invalid", NACLi_INVALID, st, Other);
  NaClDefine("0f00/7: Invalid", NACLi_INVALID, st, Other);

  NaClDefine("0f01/0: Sgdt $Ms", NACLi_SYSTEM, st, UnarySet);
  NaClDefPrefixInstMrmChoices(Prefix0F, 0x01, Opcode1, 2);
  NaClDefine("0f01/1: Sidt $Ms", NACLi_SYSTEM, st, UnarySet);
  /* Disallows Monitor/mwait.*/
  NaClDefine("0f01/1: Invalid", NACLi_INVALID, st, Other);
  NaClDefine("0f01/2: Lgdt $Ms", NACLi_SYSTEM, st, Uses);
  NaClDefPrefixInstMrmChoices(Prefix0F, 0x01, Opcode3, 2);
  NaClDefine("0f01/3: Lidt $Ms", NACLi_SYSTEM, st, Uses);
  /* Disallows Vmrun, Vmmcall, Vmload, Vmsave, Stgi, Clgi, Skinit, Invlpga */
  NaClDefine("0f01/3: Invalid", NACLi_INVALID, st, Other);
  NaClDefine("0f01/4: Smsw $Mw/Rv", NACLi_SYSTEM, st, UnarySet);
  NaClDefine("0f01/5: Invalid", NACLi_INVALID, st, Other);
  NaClDefine("0f01/6: Lmsw $Ew", NACLi_INVALID, st, Uses);
  NaClDefPrefixInstMrmChoices(Prefix0F, 0x01, Opcode7, 2);
  NaClDefine("0f01/7: Invlpg $Mb", NACLi_SYSTEM, st, Uses);
  /* Disallows Swapgs, Rdtscp. */
  NaClDefine("0f01/7: Invalid", NACLi_INVALID, st, Other);

  NaClDefine("0f02: Lar $Gv, $Ew", NACLi_SYSTEM, st, Other);
  NaClDefine("0f03: Lsl $Gv, $Ew", NACLi_SYSTEM, st, Other);
  NaClDefine("0f04: Invalid", NACLi_INVALID, st, Other);
  /* Intel manual that this only applies in 64-bit mode. */
  NaClDef_64("0f05: Syscall {%rip}, {%rcx}", NACLi_SYSCALL, st, SysCall);
  NaClDefine("0f06: Clts", NACLi_SYSTEM, st, Other);
  /* Intel manual that this only applies in 64-bit mode. */
  NaClDef_64("0f07: Sysret {%rip}, {%rcx}", NACLi_ILLEGAL, st, SysReturn);
  NaClDefine("0f08: Invd", NACLi_SYSTEM, st, Other);
  NaClDefine("0f09: Wbinvd", NACLi_SYSTEM, st, Other);
  NaClDefine("0f0a: Invalid", NACLi_INVALID, st, Other);
  /* Note: ud2 with no prefix bytes is currently understood as a NOP sequence.
   * The definition here only applies to cases where prefix bytes are added.
   */
  NaClDefine("0f0b: Ud2", NACLi_386, st, Other);
  NaClDefine("0f0c: Invalid", NACLi_INVALID, st, Other);
  NaClDefine("0f0d/0: Prefetch", NACLi_3DNOW, st, Other);   /* exclusive */
  NaClDefine("0f0d/1: Prefetch", NACLi_3DNOW, st, Other);   /* modified */
  NaClDefine("0f0d/2: Prefetch", NACLi_3DNOW, st, Other); /* reserved */
  NaClDefine("0f0d/3: Prefetch", NACLi_3DNOW, st, Other); /* modified */
  NaClDefine("0f0d/4: Prefetch", NACLi_3DNOW, st, Other); /* reserved */
  NaClDefine("0f0d/5: Prefetch", NACLi_3DNOW, st, Other); /* reserved */
  NaClDefine("0f0d/6: Prefetch", NACLi_3DNOW, st, Other); /* reserved */
  NaClDefine("0f0d/7: Prefetch", NACLi_3DNOW, st, Other); /* reserved */
  NaClDefine("0f0e: Femms", NACLi_3DNOW, st, Other);

  /* All 3DNOW instructions of form: 0f 0f [modrm] [sib] [displacement]
   *      imm8_opcode
   *
   * These instructions encode into "OP Pq, Qq", based on the value of
   * imm8_opcode. We will (poorly) decode these for now, making them
   * illegal. However, by decoding this approximation, we at least
   * recognize the instruction length.
   * TODO(karl): Decide what (if anything) we should do besides this.
   */
  NaClDefine("0f0f: 3DNow $Pq, $Qq, $Ib", NACLi_3DNOW, st, Other);

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

  NaClDefInvalid(0x24);
  NaClDefInvalid(0x25);
  NaClDefInvalid(0x26);
  NaClDefInvalid(0x27);

  NaClDefInst(0x30, NACLi_RDMSR, NACL_IFLAG(NaClIllegal), InstWrmsr);
  NaClDefOp(RegECX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegEDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));
  NaClSetInstCat(Other);

  NaClDefInst(0x31, NACLi_RDTSC, NACL_EMPTY_IFLAGS, InstRdtsc);
  NaClDefOp(RegEDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegEAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClSetInstCat(Other);

  NaClDefInst(0x32, NACLi_RDMSR, NACL_IFLAG(NaClIllegal), InstRdmsr);
  NaClDefOp(RegECX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegEDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegEAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClSetInstCat(Other);

  NaClDefInvalid(0x36);
  NaClDefInvalid(0x37);
  NaClDefInvalid(0x38);
  NaClDefInvalid(0x39);
  NaClDefInvalid(0x3a);
  NaClDefInvalid(0x3b);
  NaClDefInvalid(0x3c);
  NaClDefInvalid(0x3d);
  NaClDefInvalid(0x3e);
  NaClDefInvalid(0x3f);

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

  NaClDefInvalidIcode(Prefix0F, 0xa0);
  NaClDefInvalidIcode(Prefix0F, 0xa1);

  /* CPUID */
  NaClDefInst(0xa2, NACLi_386, 0, InstCpuid);
  NaClDefOp(RegEAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegEBX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegECX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegEDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));

  /* ISE reviewers suggested omitting bt. */
  DEF_BINST(Ev_, Gv_)(NACLi_386, 0xa3, Prefix0F, InstBt, Compare);
  NaClAddIFlags(NACL_IFLAG(NaClIllegal));

  /* ISE reviewers suggested omitting shld. */
  DEF_BINST(Ev_, Gv_)(NACLi_386, 0xa4, Prefix0F, InstShld, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OperandSize_w) |
                NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  DEF_BINST(Ev_, Gv_)(NACLi_386, 0xa5, Prefix0F, InstShld, Binary);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
                NACL_IFLAG(OperandSize_o));
  NaClDefOp(RegCL, NACL_OPFLAG(OpUse));

  NaClDefInvalidIcode(Prefix0F, 0xa6);
  NaClDefInvalidIcode(Prefix0F, 0xa7);

  /* ISE reviewers suggested omitting btr. */
  DEF_BINST(Ev_, Gv_)(NACLi_386, 0xab, Prefix0F, InstBts, Binary);
  NaClAddIFlags(NACL_IFLAG(NaClIllegal));

  /* ISE reviewers suggested omitting shrd. */
  DEF_BINST(Ev_, Gv_)(NACLi_386, 0xac, Prefix0F, InstShrd, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  DEF_BINST(Ev_, Gv_)(NACLi_386, 0xad, Prefix0F, InstShrd, Binary);
  NaClDefOp(RegCL, NACL_OPFLAG(OpUse));

  NaClDefInvalidIcode(Prefix660F, 0xae);
  NaClDefInvalidIcode(PrefixF20F, 0xae);
  NaClDefInvalidIcode(PrefixF30F, 0xae);

  NaClDefInst(0xae, NACLi_SSE2,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3),
              InstLfence);
  NaClDefOp(Opcode5, NACL_OPFLAG(OperandExtendsOpcode));
  NaClAddIFlags(NACL_IFLAG(ModRmModIs0x3));

  NaClDefInst(0xae, NACLi_SSE2,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3),
              InstMfence);
  NaClDefOp(Opcode6, NACL_OPFLAG(OperandExtendsOpcode));
  NaClAddIFlags(NACL_IFLAG(ModRmModIs0x3));

  NaClDefInstMrmChoices(0xae, Opcode7, 2);
  NaClDefInst(0xae, NACLi_SFENCE_CLFLUSH,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIs0x3),
              InstSfence);
  NaClDefOp(Opcode7, NACL_OPFLAG(OperandExtendsOpcode));
  NaClAddIFlags(NACL_IFLAG(ModRmModIs0x3));

  DEF_USUBO_INST(Mb_)(NACLi_SFENCE_CLFLUSH, 0xae, Prefix0F,
                      Opcode7, InstClflush, Uses);
  NaClAddIFlags(NACL_IFLAG(OpcodeLtC0InModRm));
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
  NaClDefOp(RegAL, NACL_OPFLAG(OpImplicit));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest));
  NaClDefOp(G_Operand, NACL_EMPTY_OPFLAGS);
  NaClSetInstCat(Exchange);

  NaClDefInstChoices_32_64(0xb1, 1, 2);
  NaClDefInst(0xB1, NACLi_386L,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_w) |
              NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OpcodeLockable),
              InstCmpxchg);
  NaClDefOp(RegREAX, NACL_OPFLAG(OpImplicit));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest));
  NaClDefOp(G_Operand, NACL_EMPTY_OPFLAGS);
  NaClSetInstCat(Exchange);

  NaClDefInst(0xB1, NACLi_386L,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(Opcode64Only) |
              NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OpcodeLockable),
              InstCmpxchg);
  NaClDefOp(RegRAX, NACL_OPFLAG(OpImplicit));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpDest));
  NaClDefOp(G_Operand, NACL_EMPTY_OPFLAGS);
  NaClSetInstCat(Exchange);

  /* ISE reviewers suggested omitting btr */
  DEF_BINST(Ev_, Gv_)(NACLi_386, 0xb3, Prefix0F, InstBtr, Binary);
  NaClAddIFlags(NACL_IFLAG(NaClIllegal));

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

  NaClDefInvalid(0xb8);

  /* ISE reviewers suggested omitting bt, btc, btr and bts, but must
   * be kept in 64-bit mode, because the compiler needs it to access
   * the top 32-bits of a 64-bit value.
   */
  DEF_OINST(Ev_, Ib_)(NACLi_386, 0xba, Prefix0F, Opcode4, InstBt, Compare);
  NaClAddIFlags(NACL_IFLAG(NaClIllegal) | NACL_IFLAG(OpcodeHasImmed_b) |
                NACL_IFLAG(Opcode32Only));

  DEF_OINST(Ev_, Ib_)(NACLi_386, 0xba, Prefix0F, Opcode4, InstBt, Compare);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(Opcode64Only));

  DEF_OINST(Ev_, Ib_)(NACLi_386, 0xba, Prefix0F, Opcode5, InstBts, Binary);
  NaClAddIFlags(NACL_IFLAG(NaClIllegal) | NACL_IFLAG(OpcodeHasImmed_b) |
                NACL_IFLAG(Opcode32Only));

  DEF_OINST(Ev_, Ib_)(NACLi_386, 0xba, Prefix0F, Opcode5, InstBts, Binary);
  NaClAddIFlags(NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OpcodeHasImmed_b));

  DEF_OINST(Ev_, Ib_)(NACLi_386, 0xba, Prefix0F, Opcode6, InstBtr, Binary);
  NaClAddIFlags(NACL_IFLAG(NaClIllegal) | NACL_IFLAG(OpcodeHasImmed_b) |
                NACL_IFLAG(Opcode32Only));

  DEF_OINST(Ev_, Ib_)(NACLi_386, 0xba, Prefix0F, Opcode6, InstBtr, Binary);
  NaClAddIFlags(NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OpcodeHasImmed_b));

  DEF_OINST(Ev_, Ib_)(NACLi_386, 0xba, Prefix0F, Opcode7, InstBtc, Binary);
  NaClAddIFlags(NACL_IFLAG(NaClIllegal) | NACL_IFLAG(OpcodeHasImmed_b) |
                NACL_IFLAG(Opcode32Only));

  DEF_OINST(Ev_, Ib_)(NACLi_386, 0xba, Prefix0F, Opcode7, InstBtc, Binary);
  NaClAddIFlags(NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OpcodeHasImmed_b));

  /* ISE reviewers suggested omitting btc */
  DEF_BINST(Ev_, Gv_)(NACLi_386, 0xbb, Prefix0F, InstBtc, Binary);
  NaClAddIFlags(NACL_IFLAG(NaClIllegal));

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

  DEF_BINST(Eb_, Gb_)(NACLi_386L, 0xc0, Prefix0F, InstXadd, Exchange);
  NaClAddIFlags(NACL_IFLAG(OperandSize_b) | NACL_IFLAG(OpcodeLockable)),
  DEF_BINST(Ev_, Gv_)(NACLi_386L, 0xc1, Prefix0F, InstXadd, Exchange);
  NaClAddIFlags(NACL_IFLAG(OpcodeLockable));

  /* NaClDefs C8 throught CF */
  NaClDefBswap();
}
