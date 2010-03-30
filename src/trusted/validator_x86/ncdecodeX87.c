/* Copyright (c) 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * ncdecodex87.c - Handles x87 instructions.
 *
 * Note: This modeling code doesn't handle several aspects of floating point
 * operations. This includes:
 *
 * 1) Doesn't model condition flags.
 * 2) Doesn't model floating point stack adjustments.
 * 3) Doesn't model differences in size of pointed to memory.
 */

#include "native_client/src/trusted/validator_x86/ncdecode_tablegen.h"

/* Define an x87 instruction with no operands. */
static void NaClDefX87NoOperands(const NaClInstPrefix prefix,
                                 const uint8_t opcode,
                                 const NaClMnemonic mnemonic) {
  NaClDefInstPrefix(prefix);
  NaClDefInst(opcode, NACLi_X87, NACL_EMPTY_IFLAGS, mnemonic);
  NaClResetToDefaultInstPrefix();
}

/* Define an x87 instruction that uses AX. */
static void NaClDefX87Ax(const NaClInstPrefix  prefix,
                         const uint8_t opcode,
                         const NaClMnemonic mnemonic,
                         const NaClOpFlags ax_flags) {
  NaClDefInstPrefix(prefix);
  NaClDefInst(opcode, NACLi_X87, NACL_EMPTY_IFLAGS, mnemonic);
  NaClDefOp(RegAX, ax_flags);
  NaClResetToDefaultInstPrefix();
}

/* Define an x87 instruction with no operands, but is extended using
 * the modrm byte.
 */
#if (0 == 1)
/* TODO(kschimpf): Delete this function if unneeded. */
static void NaClDefX87MrmNoOperands(const NaClInstPrefix prefix,
                                    const uint8_t opcode,
                                    const NaClOpKind opcode_in_modrm,
                                    const NaClMnemonic mnemonic) {
  NaClDefInstPrefix(prefix);
  NaClDefInst(opcode, NACLi_X87, NACL_IFLAG(OpcodeInModRm), mnemonic);
  NaClDefOp(opcode_in_modrm, NACL_OPFLAG(OperandExtendsOpcode));
  NaClResetToDefaultInstPrefix();
}
#endif

/* Define an x87 instruction that uses a single argument st0 */
static void NaClDefX87St0(const NaClInstPrefix prefix,
                          const uint8_t opcode,
                          const NaClOpKind opcode_in_modrm,
                          NaClMnemonic mnemonic,
                          const NaClOpFlags st0_flags) {
  NaClDefInstPrefix(prefix);
  NaClDefInst(opcode, NACLi_X87, NACL_EMPTY_IFLAGS, mnemonic);
  NaClDefOp(RegST0, st0_flags);
  NaClResetToDefaultInstPrefix();
}

#if (0 == 1)
/* TODO(kschimpf): Delete this function if unneeded. */
static void BrokenOldDefineX87St0(const NaClInstPrefix prefix,
                                  const uint8_t opcode,
                                  const NaClOpKind opcode_in_modrm,
                                  NaClMnemonic mnemonic,
                                  const NaClOpFlags st0_flags) {
  NaClDefInstPrefix(prefix);
  NaClDefInst(opcode, NACLi_X87, NACL_IFLAG(OpcodeInModRm), mnemonic);
  NaClDefOp(opcode_in_modrm, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(RegST0, st0_flags);
  NaClResetToDefaultInstPrefix();
}
#endif

/* Define an x87 instruction that uses a single memory pointer. */
static void NaClDefX87LtC0Mem(const NaClInstPrefix prefix,
                              const uint8_t opcode,
                              const NaClOpKind opcode_in_modrm,
                              const NaClMnemonic mnemonic,
                              const NaClOpFlags mfp_flags) {
  NaClDefInstPrefix(prefix);
  NaClDefInst(opcode, NACLi_X87, NACL_IFLAG(OpcodeLtC0InModRm), mnemonic);
  NaClDefOp(opcode_in_modrm, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(M_Operand, mfp_flags);
  NaClResetToDefaultInstPrefix();
}

/* Define an x87 instruction that has two operands, a memory pointer
 * and st0, and the memory pointer is defined by the value of st0.
 */
static void NaClDefX87LtC0MoveMemSt0(const NaClInstPrefix prefix,
                                     const uint8_t opcode,
                                     const NaClOpKind opcode_in_modrm,
                                     const NaClMnemonic mnemonic) {
  NaClDefInstPrefix(prefix);
  NaClDefInst(opcode, NACLi_X87, NACL_IFLAG(OpcodeLtC0InModRm), mnemonic);
  NaClDefOp(opcode_in_modrm, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(M_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(RegST0, NACL_OPFLAG(OpUse));
  NaClResetToDefaultInstPrefix();
}

/* Define an x87 binary instruction that has St0 as its first
 * argument, and a memory pointer as its second argument.
 */
static void NaClDefX87LtC0BinopSt0Mem(const NaClInstPrefix prefix,
                                      const uint8_t opcode,
                                      const NaClOpKind opcode_in_modrm,
                                      const NaClMnemonic mnemonic) {
  NaClDefInstPrefix(prefix);
  NaClDefInst(opcode, NACLi_X87, NACL_IFLAG(OpcodeLtC0InModRm), mnemonic);
  NaClDefOp(opcode_in_modrm, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
  NaClDefOp(M_Operand, NACL_OPFLAG(OpUse));
  NaClResetToDefaultInstPrefix();
}

/* Define an x87 binary instruction that has St0 as its first
 * argument, and sti as its second argument.
 */
static void NaClDefX87BinopSt0Sti(const NaClInstPrefix prefix,
                                  const uint8_t opcode,
                                  const int base_offset,
                                  const NaClMnemonic mnemonic) {
  NaClDefInstPrefix(prefix);
  NaClDefInst(opcode + base_offset, NACLi_X87,
               NACL_IFLAG(OpcodePlusR), mnemonic);
  NaClDefOp(OpcodeBaseMinus0 + base_offset, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
  NaClDefOp(St_Operand, NACL_OPFLAG(OpUse));
  NaClResetToDefaultInstPrefix();
}


/* Define the group of x87 binary instructions that have st0 as its first
 * argument, and sti (for all i, 1<=i<=8) as the second.
 */
static void NaClDefX87BinopSt0StiGroup(const NaClInstPrefix prefix,
                                       const uint8_t opcode,
                                       const NaClMnemonic mnemonic) {
  int i;
  for (i = 0; i < 8; ++i) {
    NaClDefX87BinopSt0Sti(prefix, opcode, i, mnemonic);
  }
}

/* Define an x87 compare instruction that has st0 and its first
 * argument, and sti as its second argument.
 */
static void NaClDefX87CompareSt0Sti(const NaClInstPrefix prefix,
                                    const uint8_t opcode,
                                    const int base_offset,
                                    const NaClMnemonic mnemonic) {
  NaClDefInstPrefix(prefix);
  NaClDefInst(opcode + base_offset, NACLi_X87,
               NACL_IFLAG(OpcodePlusR), mnemonic);
  NaClDefOp(OpcodeBaseMinus0 + base_offset, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(RegST0, NACL_OPFLAG(OpUse));
  NaClDefOp(St_Operand, NACL_OPFLAG(OpUse));
  NaClResetToDefaultInstPrefix();
}

/* Define the group of x87 compare instructions that have st0 as its
 * first argument, and sti (for all i, 1<=i<=8) as the second.
 */
static void NaClDefX87CompareSt0StiGroup(const NaClInstPrefix prefix,
                                         const uint8_t opcode,
                                         const NaClMnemonic mnemonic) {
  int i;
  for (i = 0; i < 8; ++i) {
    NaClDefX87CompareSt0Sti(prefix, opcode, i, mnemonic);
  }
}

/* Define an x87 compare instruction that has st0 as its first
 * argument, and a Memory pointer as its second.
 */
static void NaClDefX87LtC0CompareSt0Mem(const NaClInstPrefix prefix,
                                        const uint8_t opcode,
                                        const NaClOpKind opcode_in_modrm,
                                        const NaClMnemonic mnemonic) {
  NaClDefInstPrefix(prefix);
  NaClDefInst(opcode, NACLi_X87, NACL_IFLAG(OpcodeLtC0InModRm), mnemonic);
  NaClDefOp(opcode_in_modrm, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(RegST0, NACL_OPFLAG(OpUse));
  NaClDefOp(M_Operand, NACL_OPFLAG(OpUse));
  NaClResetToDefaultInstPrefix();
}

/* Define an x87 exchange instruction that has st0 as its first
 * argument, and sti as its second argument.
 */
static void NaClDefX87ExchangeSt0Sti(const NaClInstPrefix prefix,
                                     const uint8_t opcode,
                                     const int base_offset,
                                     const NaClMnemonic mnemonic) {
  NaClDefInstPrefix(prefix);
  NaClDefInst(opcode + base_offset, NACLi_X87,
               NACL_IFLAG(OpcodePlusR), mnemonic);
  NaClDefOp(OpcodeBaseMinus0 + base_offset, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
  NaClDefOp(St_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
  NaClResetToDefaultInstPrefix();
}

/* Define a group of x87 exchange instructions that have st0 as thier first
 * argument, and sti (for all i, 1<=i<=8) for the second argument.
 */
static void NaClDefX87ExchangeSt0StiGroup(const NaClInstPrefix prefix,
                                          const uint8_t opcode,
                                          const NaClMnemonic mnemonic) {
  int i;
  for (i = 0; i < 8; ++i) {
    NaClDefX87ExchangeSt0Sti(prefix, opcode, i, mnemonic);
  }
}

/* Define an x87 instruction that has two operands, sti and st0,
 * and the value of sti is defined by the value of st0.
 */
static void NaClDefX87MoveStiSt0(const NaClInstPrefix prefix,
                                 const uint8_t opcode,
                                 const int base_offset,
                                 const NaClMnemonic mnemonic) {
  NaClDefInstPrefix(prefix);
  NaClDefInst(opcode + base_offset, NACLi_X87, NACL_IFLAG(OpcodePlusR),
              mnemonic);
  NaClDefOp(OpcodeBaseMinus0 + base_offset, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(St_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(RegST0, NACL_OPFLAG(OpUse));
  NaClResetToDefaultInstPrefix();
}

/* Define a group of x87 instructions that have two operands, sti and st0,
 * and the value of sti (for all i, 1<=i<=8) is defined by the value of st0.
 */
static void NaClDefX87MoveStiSt0Group(const NaClInstPrefix prefix,
                                      const uint8_t opcode,
                                      const NaClMnemonic mnemonic) {
  int i;
  for (i = 0; i < 8; ++i) {
    NaClDefX87MoveStiSt0(prefix, opcode, i, mnemonic);
  }
}

/* Define an x87 binary instruction that has sti as its first argument,
 * and st0 as its second argument.
 */
static void NaClDefX87BinopStiSt0(const NaClInstPrefix prefix,
                                  const uint8_t opcode,
                                  const int base_offset,
                                  const NaClMnemonic mnemonic) {
  NaClDefInstPrefix(prefix);
  NaClDefInst(opcode + base_offset, NACLi_X87, NACL_IFLAG(OpcodePlusR),
              mnemonic);
  NaClDefOp(OpcodeBaseMinus0 + base_offset, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(St_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
  NaClDefOp(RegST0, NACL_OPFLAG(OpUse));
  NaClResetToDefaultInstPrefix();
}

/* Define a group of x87 binary instructions that have sti as the
 * first argument (for all i, 1<=i<=8), and st0 as the second argument.
 */
static void NaClDefX87BinopStiSt0Group(const NaClInstPrefix prefix,
                                       const uint8_t opcode,
                                       const NaClMnemonic mnemonic) {
  int i;
  for (i = 0; i < 8; ++i) {
    NaClDefX87BinopStiSt0(prefix, opcode, i, mnemonic);
  }
}

/* Define an x87 binary instruction that has st1 as the first argument,
 * and st0 as the second argument.
 */
static void NaClDefX87BinopSt1St0(const NaClInstPrefix prefix,
                                  const uint8_t opcode,
                                  const NaClMnemonic mnemonic) {
  NaClDefInstPrefix(prefix);
  NaClDefInst(opcode, NACLi_X87, NACL_EMPTY_IFLAGS, mnemonic);
  NaClDefOp(RegST1, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
  NaClDefOp(RegST0, NACL_OPFLAG(OpUse));
  NaClResetToDefaultInstPrefix();
}

/* Define an x87 binary instruction that has st0 as the first argument,
 * and st1 as the second argument.
 */
static void NaClDefX87BinopSt0St1(const NaClInstPrefix prefix,
                                  const uint8_t opcode,
                                  const NaClMnemonic mnemonic) {
  NaClDefInstPrefix(prefix);
  NaClDefInst(opcode, NACLi_X87, NACL_EMPTY_IFLAGS, mnemonic);
  NaClDefOp(RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
  NaClDefOp(RegST1, NACL_OPFLAG(OpUse));
  NaClResetToDefaultInstPrefix();
}

/* Define an x87 comparison instruction that has st0 as the first
 * argument, and st1 as the second argument.
 */
static void NaClDefX87CompareSt0St1(const NaClInstPrefix prefix,
                                    const uint8_t opcode,
                                    const NaClMnemonic mnemonic) {
  NaClDefInstPrefix(prefix);
  NaClDefInst(opcode, NACLi_X87, NACL_EMPTY_IFLAGS, mnemonic);
  NaClDefOp(RegST0, NACL_OPFLAG(OpUse));
  NaClDefOp(RegST1, NACL_OPFLAG(OpUse));
  NaClResetToDefaultInstPrefix();
}

/* Define an x87 instruction that updates st0, based on its previous
 * value.
 */
static void NaClDefX87ModifySt0(const NaClInstPrefix prefix,
                                const uint8_t opcode,
                                const NaClMnemonic mnemonic) {
  NaClDefInstPrefix(prefix);
  NaClDefInst(opcode, NACLi_X87, NACL_EMPTY_IFLAGS, mnemonic);
  NaClDefOp(RegST0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
  NaClResetToDefaultInstPrefix();
}

/* Define an x87 instruction that updates sti, based on its previous value. */
static void NaClDefX87Sti(const NaClInstPrefix prefix,
                          const uint8_t opcode,
                          const int base_offset,
                          const NaClMnemonic mnemonic,
                          const NaClOpFlags sti_flags) {
  NaClDefInstPrefix(prefix);
  NaClDefInst(opcode + base_offset, NACLi_X87, NACL_IFLAG(OpcodePlusR),
              mnemonic);
  NaClDefOp(OpcodeBaseMinus0 + base_offset, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(St_Operand, sti_flags);
  NaClResetToDefaultInstPrefix();
}

/* Define a group of x87 instructions that updates sti (for all i, 1<=i<=8)
 * based on its previous value.
 */
static void NaClDefX87StiGroup(const NaClInstPrefix prefix,
                               const uint8_t opcode,
                               const NaClMnemonic mnemonic,
                               const NaClOpFlags sti_flags) {
  int i;
  for (i = 0; i < 8; ++i) {
    NaClDefX87Sti(prefix, opcode, i, mnemonic, sti_flags);
  }
}

void NaClDefX87Insts() {
  NaClDefDefaultInstPrefix(NoPrefix);

  /* Define D8 x87 instructions. */

  NaClDefX87LtC0BinopSt0Mem(NoPrefix, 0xd8, Opcode0, InstFadd);
  NaClDefX87LtC0BinopSt0Mem(NoPrefix, 0xd8, Opcode1, InstFmul);
  NaClDefX87LtC0BinopSt0Mem(NoPrefix, 0xd8, Opcode2, InstFcom);
  NaClDefX87LtC0BinopSt0Mem(NoPrefix, 0xd8, Opcode3, InstFcomp);
  NaClDefX87LtC0BinopSt0Mem(NoPrefix, 0xd8, Opcode4, InstFsub);
  NaClDefX87LtC0BinopSt0Mem(NoPrefix, 0xd8, Opcode5, InstFsubr);
  NaClDefX87LtC0BinopSt0Mem(NoPrefix, 0xd8, Opcode6, InstFdiv);
  NaClDefX87LtC0BinopSt0Mem(NoPrefix, 0xd8, Opcode7, InstFdivr);
  NaClDefX87BinopSt0StiGroup(PrefixD8, 0xc0, InstFadd);
  NaClDefX87BinopSt0StiGroup(PrefixD8, 0xc8, InstFmul);
  NaClDefX87BinopSt0StiGroup(PrefixD8, 0xd0, InstFcom);
  NaClDefX87BinopSt0StiGroup(PrefixD8, 0xd8, InstFcomp);
  NaClDefX87BinopSt0StiGroup(PrefixD8, 0xe0, InstFsub);
  NaClDefX87BinopSt0StiGroup(PrefixD8, 0xe8,  InstFsubr);
  NaClDefX87BinopSt0StiGroup(PrefixD8, 0xf0, InstFdiv);
  NaClDefX87BinopSt0StiGroup(PrefixD8, 0xf8, InstFdivr);

  /* Define D9 x87 instructions. */

  NaClDefX87LtC0Mem(NoPrefix, 0xd9, Opcode0, InstFld, NACL_OPFLAG(OpUse));
  NaClDefX87LtC0MoveMemSt0(NoPrefix, 0xd9, Opcode2, InstFst);
  NaClDefX87LtC0MoveMemSt0(NoPrefix, 0xd9, Opcode3, InstFstp);
  NaClDefX87LtC0Mem(NoPrefix, 0xd9, Opcode4, InstFldenv, NACL_OPFLAG(OpUse));
  NaClDefX87LtC0Mem(NoPrefix, 0xd9, Opcode5, InstFldcw, NACL_OPFLAG(OpUse));
  NaClDefX87LtC0Mem(NoPrefix, 0xd9, Opcode6, InstFnstenv,
                    NACL_OPFLAG(OpSet));
  NaClDefX87LtC0Mem(NoPrefix, 0xd9, Opcode7, InstFnstcw, NACL_OPFLAG(OpSet));

  NaClDefX87StiGroup(PrefixD9, 0xc0, InstFld, NACL_OPFLAG(OpUse));
  NaClDefX87ExchangeSt0StiGroup(PrefixD9, 0xc8, InstFxch);
  NaClDefX87NoOperands(PrefixD9, 0xd0, InstFnop);
  NaClDefX87St0(PrefixD9, 0xe0, Opcode0, InstFchs,
                NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
  NaClDefX87St0(PrefixD9, 0xe1, Opcode1, InstFabs,
                NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
  /* 0xe2 and 0xe3 are not defined */
  NaClDefX87St0(PrefixD9, 0xe4, Opcode4, InstFtst, NACL_OPFLAG(OpUse));
  NaClDefX87St0(PrefixD9, 0xe5, Opcode5, InstFxam, NACL_OPFLAG(OpUse));
  /* 0xe6 and 0xe7 are not defined */
  NaClDefX87ModifySt0(PrefixD9, 0xe8, InstFld1);
  NaClDefX87ModifySt0(PrefixD9, 0xe9, InstFldl2t);
  NaClDefX87ModifySt0(PrefixD9, 0xea, InstFldl2e);
  NaClDefX87ModifySt0(PrefixD9, 0xeb, InstFldpi);
  NaClDefX87ModifySt0(PrefixD9, 0xec, InstFldlg2);
  NaClDefX87ModifySt0(PrefixD9, 0xed, InstFldln2);
  NaClDefX87ModifySt0(PrefixD9, 0xee, InstFldz);
  /* 0xef is not defined */
  NaClDefX87ModifySt0(PrefixD9, 0xf0, InstF2m1);
  NaClDefX87BinopSt1St0(PrefixD9, 0xf1, InstFyl2x);
  NaClDefX87ModifySt0(PrefixD9, 0xf2, InstFptan);
  NaClDefX87BinopSt1St0(PrefixD9, 0xf3, InstFpatan);
  NaClDefX87ModifySt0(PrefixD9, 0xf4, InstFxtract);
  NaClDefX87BinopSt0St1(PrefixD9, 0xf5, InstFprem1);
  NaClDefX87NoOperands(PrefixD9, 0xf6, InstFdecstp);
  NaClDefX87NoOperands(PrefixD9, 0xf7, InstFincstp);
  NaClDefX87BinopSt0St1(PrefixD9, 0xf8, InstFprem);
  NaClDefX87BinopSt1St0(PrefixD9, 0xf9, InstFyl2xp1);
  NaClDefX87ModifySt0(PrefixD9, 0xfa, InstFsqrt);
  NaClDefX87ModifySt0(PrefixD9, 0xfb, InstFsincos);
  NaClDefX87ModifySt0(PrefixD9, 0xfc, InstFrndint);
  NaClDefX87BinopSt0St1(PrefixD9, 0xfd, InstFscale);
  NaClDefX87ModifySt0(PrefixD9, 0xfe, InstFsin);
  NaClDefX87ModifySt0(PrefixD9, 0xff, InstFcos);

  /* Define DA x87 instructions. */

  NaClDefX87LtC0BinopSt0Mem(NoPrefix, 0xda, Opcode0, InstFiadd);
  NaClDefX87LtC0BinopSt0Mem(NoPrefix, 0xda, Opcode1, InstFimul);
  NaClDefX87LtC0BinopSt0Mem(NoPrefix, 0xda, Opcode2, InstFicom);
  NaClDefX87LtC0BinopSt0Mem(NoPrefix, 0xda, Opcode3, InstFicomp);
  NaClDefX87LtC0BinopSt0Mem(NoPrefix, 0xda, Opcode4, InstFisub);
  NaClDefX87LtC0BinopSt0Mem(NoPrefix, 0xda, Opcode5, InstFisubr);
  NaClDefX87LtC0BinopSt0Mem(NoPrefix, 0xda, Opcode6, InstFidiv);
  NaClDefX87LtC0BinopSt0Mem(NoPrefix, 0xda, Opcode7, InstFidivr);
  NaClDefX87BinopSt0StiGroup(PrefixDA, 0xc0, InstFcmovb);
  NaClDefX87BinopSt0StiGroup(PrefixDA, 0xc8, InstFcmove);
  NaClDefX87BinopSt0StiGroup(PrefixDA, 0xd0, InstFcmovbe);
  NaClDefX87BinopSt0StiGroup(PrefixDA, 0xd8, InstFcmovu);
  NaClDefX87CompareSt0St1(PrefixDA, 0xe9, InstFucompp);

  /* Define DB x87 instructions. */

  NaClDefX87LtC0Mem(NoPrefix, 0xdb, Opcode0, InstFild, NACL_OPFLAG(OpUse));
  NaClDefX87LtC0MoveMemSt0(NoPrefix, 0xdb, Opcode1, InstFisttp);
  NaClDefX87LtC0MoveMemSt0(NoPrefix, 0xdb, Opcode2, InstFist);
  NaClDefX87LtC0MoveMemSt0(NoPrefix, 0xdb, Opcode3, InstFistp);
  NaClDefX87LtC0Mem(NoPrefix, 0xdb, Opcode5, InstFld, NACL_OPFLAG(OpUse));
  NaClDefX87LtC0MoveMemSt0(NoPrefix, 0xdb, Opcode7, InstFstp);
  NaClDefX87BinopSt0StiGroup(PrefixDB, 0xc0, InstFcmovnb);
  NaClDefX87BinopSt0StiGroup(PrefixDB, 0xc8, InstFcmovne);
  NaClDefX87BinopSt0StiGroup(PrefixDB, 0xd0, InstFcmovnbe);
  NaClDefX87BinopSt0StiGroup(PrefixDB, 0xd8, InstFcmovnu);
  NaClDefX87NoOperands(PrefixDB, 0xe2, InstFnclex);
  NaClDefX87NoOperands(PrefixDB, 0xe3, InstFninit);
  NaClDefX87CompareSt0StiGroup(PrefixDB, 0xe8, InstFucomi);
  NaClDefX87CompareSt0StiGroup(PrefixDB, 0xf0, InstFcomi);

  /* Define DC x87 instructions. */

  NaClDefX87LtC0BinopSt0Mem(NoPrefix, 0xdc, Opcode0, InstFadd);
  NaClDefX87LtC0BinopSt0Mem(NoPrefix, 0xdc, Opcode1, InstFmul);
  NaClDefX87LtC0BinopSt0Mem(NoPrefix, 0xdc, Opcode2, InstFcom);
  NaClDefX87LtC0BinopSt0Mem(NoPrefix, 0xdc, Opcode3, InstFcomp);
  NaClDefX87LtC0BinopSt0Mem(NoPrefix, 0xdc, Opcode4, InstFsub);
  NaClDefX87LtC0BinopSt0Mem(NoPrefix, 0xdc, Opcode5, InstFsubr);
  NaClDefX87LtC0BinopSt0Mem(NoPrefix, 0xdc, Opcode6, InstFdiv);
  NaClDefX87LtC0BinopSt0Mem(NoPrefix, 0xdc, Opcode7, InstFdivr);
  NaClDefX87BinopStiSt0Group(PrefixDC, 0xc0, InstFadd);
  NaClDefX87BinopStiSt0Group(PrefixDC, 0xc8, InstFmul);
  NaClDefX87BinopStiSt0Group(PrefixDC, 0xe0, InstFsubr);
  NaClDefX87BinopStiSt0Group(PrefixDC, 0xe8, InstFsub);
  NaClDefX87BinopStiSt0Group(PrefixDC, 0xf0, InstFdivr);
  NaClDefX87BinopStiSt0Group(PrefixDC, 0xf8, InstFdiv);

  /* Define DD x87 instructions. */

  NaClDefX87LtC0Mem(NoPrefix, 0xdd, Opcode0, InstFld, NACL_OPFLAG(OpUse));
  NaClDefX87LtC0MoveMemSt0(NoPrefix, 0xdd, Opcode1, InstFisttp);
  NaClDefX87LtC0MoveMemSt0(NoPrefix, 0xdd, Opcode2, InstFst);
  NaClDefX87LtC0MoveMemSt0(NoPrefix, 0xdd, Opcode3, InstFstp);
  NaClDefX87LtC0Mem(NoPrefix, 0xdd, Opcode4, InstFrstor, NACL_OPFLAG(OpUse));
  NaClDefX87LtC0Mem(NoPrefix, 0xdd, Opcode6, InstFnsave, NACL_OPFLAG(OpSet));
  NaClDefX87LtC0Mem(NoPrefix, 0xdd, Opcode7, InstFnstsw, NACL_OPFLAG(OpSet));
  NaClDefX87StiGroup(PrefixDD, 0xc0, InstFfree, NACL_EMPTY_IFLAGS);
  NaClDefX87MoveStiSt0Group(PrefixDD, 0xd0, InstFst);
  NaClDefX87MoveStiSt0Group(PrefixDD, 0xd8, InstFstp);
  NaClDefX87CompareSt0StiGroup(PrefixDD, 0xe0, InstFucom);
  NaClDefX87CompareSt0StiGroup(PrefixDD, 0xe8, InstFucomp);

  /* Define DE x87 instructions. */

  NaClDefX87LtC0BinopSt0Mem(NoPrefix, 0xde, Opcode0, InstFiadd);
  NaClDefX87LtC0BinopSt0Mem(NoPrefix, 0xde, Opcode1, InstFimul);
  NaClDefX87LtC0CompareSt0Mem(NoPrefix, 0xde, Opcode2, InstFicom);
  NaClDefX87LtC0CompareSt0Mem(NoPrefix, 0xde, Opcode3, InstFicomp);
  NaClDefX87LtC0BinopSt0Mem(NoPrefix, 0xde, Opcode4, InstFisub);
  NaClDefX87LtC0BinopSt0Mem(NoPrefix, 0xde, Opcode5, InstFisubr);
  NaClDefX87LtC0BinopSt0Mem(NoPrefix, 0xde, Opcode6, InstFidiv);
  NaClDefX87LtC0BinopSt0Mem(NoPrefix, 0xde, Opcode7, InstFidivr);
  NaClDefX87BinopStiSt0Group(PrefixDE, 0xc0, InstFaddp);
  NaClDefX87BinopStiSt0Group(PrefixDE, 0xc8, InstFmulp);
  NaClDefX87CompareSt0St1(PrefixDE, 0xd9, InstFcompp);
  NaClDefX87BinopStiSt0Group(PrefixDE, 0xe0, InstFsubrp);
  NaClDefX87BinopStiSt0Group(PrefixDE, 0xe8, InstFsubp);
  NaClDefX87BinopStiSt0Group(PrefixDE, 0xf0, InstFdivrp);
  NaClDefX87BinopStiSt0Group(PrefixDE, 0xf8, InstFdivp);

  /* Define DF x87 instructions. */

  NaClDefX87LtC0Mem(NoPrefix, 0xdf, Opcode0, InstFild, NACL_OPFLAG(OpUse));
  NaClDefX87LtC0MoveMemSt0(NoPrefix, 0xdf, Opcode1, InstFisttp);
  NaClDefX87LtC0MoveMemSt0(NoPrefix, 0xdf, Opcode2, InstFist);
  NaClDefX87LtC0MoveMemSt0(NoPrefix, 0xdf, Opcode3, InstFistp);
  NaClDefX87LtC0Mem(NoPrefix, 0xdf, Opcode4, InstFbld, NACL_OPFLAG(OpUse));
  NaClDefX87LtC0Mem(NoPrefix, 0xdf, Opcode5, InstFild, NACL_OPFLAG(OpUse));
  NaClDefX87LtC0MoveMemSt0(NoPrefix, 0xdf, Opcode6, InstFbstp);
  NaClDefX87LtC0MoveMemSt0(NoPrefix, 0xdf, Opcode7, InstFistp);

  NaClDefX87Ax(PrefixDF, 0xe0, InstFnstsw, NACL_OPFLAG(OpSet));
  NaClDefX87BinopSt0StiGroup(PrefixDF, 0xe8, InstFucomip);
  NaClDefX87BinopSt0StiGroup(PrefixDF, 0xf0, InstFcomip);

  /* todo(karl) What about "9b db e2 Fclex" ? */
  /* todo(karl) What about "9b db e3 finit" ? */
  /* todo(karl) What about "9b d9 /6 Fstenv" ? */
  /* todo(karl) What about "9b d9 /7 Fstcw" ? */
  /* todo(karl) What about "9b dd /6 Fsave" ? */
  /* todo(karl) What about "9b dd /7 Fstsw" ? */
  /* todo(karl) What about "9b df e0 Fstsw" ? */
}
