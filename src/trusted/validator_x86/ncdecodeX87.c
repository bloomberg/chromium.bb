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
static void DefineX87NoOperands(const OpcodePrefix prefix,
                                const uint8_t opcode,
                                const InstMnemonic mnemonic) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opcode, NACLi_X87, EmptyInstFlags, mnemonic);
  ResetToDefaultOpcodePrefix();
}

/* Define an x87 instruction that uses AX. */
static void DefineX87Ax(const OpcodePrefix  prefix,
                        const uint8_t opcode,
                        const InstMnemonic mnemonic,
                        const OperandFlags ax_flags) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opcode, NACLi_X87, EmptyInstFlags, mnemonic);
  DefineOperand(RegAX, ax_flags);
  ResetToDefaultOpcodePrefix();
}

/* Define an x87 instruction with no operands, but is extended using
 * the modrm byte.
 */
static void DefineX87MrmNoOperands(const OpcodePrefix prefix,
                                   const uint8_t opcode,
                                   const OperandKind opcode_in_modrm,
                                   const InstMnemonic mnemonic) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opcode, NACLi_X87, InstFlag(OpcodeInModRm), mnemonic);
  DefineOperand(opcode_in_modrm, OpFlag(OperandExtendsOpcode));
  ResetToDefaultOpcodePrefix();
}

/* Define an x87 instruction that uses a single argument st0 */
static void DefineX87St0(const OpcodePrefix prefix,
                         const uint8_t opcode,
                         const OperandKind opcode_in_modrm,
                         InstMnemonic mnemonic,
                         const OperandFlags st0_flags) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opcode, NACLi_X87, InstFlag(OpcodeInModRm), mnemonic);
  DefineOperand(opcode_in_modrm, OpFlag(OperandExtendsOpcode));
  DefineOperand(RegST0, st0_flags);
  ResetToDefaultOpcodePrefix();
}

/* Define an x87 instruction that uses a single memory pointer. */
static void DefineX87LtC0Mem(const OpcodePrefix prefix,
                             const uint8_t opcode,
                             const OperandKind opcode_in_modrm,
                             const InstMnemonic mnemonic,
                             const OperandFlags mfp_flags) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opcode, NACLi_X87, InstFlag(OpcodeLtC0InModRm), mnemonic);
  DefineOperand(opcode_in_modrm, OpFlag(OperandExtendsOpcode));
  DefineOperand(M_Operand, mfp_flags);
  ResetToDefaultOpcodePrefix();
}

/* Define an x87 instruction that has two operands, a memory pointer
 * and st0, and the memory pointer is defined by the value of st0.
 */
static void DefineX87LtC0MoveMemSt0(const OpcodePrefix prefix,
                                    const uint8_t opcode,
                                    const OperandKind opcode_in_modrm,
                                    const InstMnemonic mnemonic) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opcode, NACLi_X87, InstFlag(OpcodeLtC0InModRm), mnemonic);
  DefineOperand(opcode_in_modrm, OpFlag(OperandExtendsOpcode));
  DefineOperand(M_Operand, OpFlag(OpSet));
  DefineOperand(RegST0, OpFlag(OpUse) | OpFlag(OpImplicit));
  ResetToDefaultOpcodePrefix();
}

/* Define an x87 binary instruction that has St0 as its first
 * argument, and a memory pointer as its second argument.
 */
static void DefineX87LtC0BinopSt0Mem(const OpcodePrefix prefix,
                                     const uint8_t opcode,
                                     const OperandKind opcode_in_modrm,
                                     const InstMnemonic mnemonic) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opcode, NACLi_X87, InstFlag(OpcodeLtC0InModRm), mnemonic);
  DefineOperand(opcode_in_modrm, OpFlag(OperandExtendsOpcode));
  DefineOperand(RegST0, OpFlag(OpSet) | OpFlag(OpUse) | OpFlag(OpImplicit));
  DefineOperand(M_Operand, OpFlag(OpUse));
  ResetToDefaultOpcodePrefix();
}

/* Define an x87 binary instruction that has St0 as its first
 * argument, and sti as its second argument.
 */
static void DefineX87BinopSt0Sti(const OpcodePrefix prefix,
                                 const uint8_t opcode,
                                 const int base_offset,
                                 const InstMnemonic mnemonic) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opcode + base_offset, NACLi_X87,
               InstFlag(OpcodePlusR), mnemonic);
  DefineOperand(OpcodeBaseMinus0 + base_offset, OpFlag(OperandExtendsOpcode));
  DefineOperand(RegST0, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(St_Operand, OpFlag(OpUse));
  ResetToDefaultOpcodePrefix();
}


/* Define the group of x87 binary instructions that have st0 as its first
 * argument, and sti (for all i, 1<=i<=8) as the second.
 */
static void DefineX87BinopSt0StiGroup(const OpcodePrefix prefix,
                                      const uint8_t opcode,
                                      const InstMnemonic mnemonic) {
  int i;
  for (i = 0; i < 8; ++i) {
    DefineX87BinopSt0Sti(prefix, opcode, i, mnemonic);
  }
}

/* Define an x87 compare instruction that has st0 and its first
 * argument, and sti as its second argument.
 */
static void DefineX87CompareSt0Sti(const OpcodePrefix prefix,
                                   const uint8_t opcode,
                                   const int base_offset,
                                   const InstMnemonic mnemonic) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opcode + base_offset, NACLi_X87,
               InstFlag(OpcodePlusR), mnemonic);
  DefineOperand(OpcodeBaseMinus0 + base_offset, OpFlag(OperandExtendsOpcode));
  DefineOperand(RegST0, OpFlag(OpUse) | OpFlag(OpImplicit));
  DefineOperand(St_Operand, OpFlag(OpUse));
  ResetToDefaultOpcodePrefix();
}

/* Define the group of x87 compare instructions that have st0 as its
 * first argument, and sti (for all i, 1<=i<=8) as the second.
 */
static void DefineX87CompareSt0StiGroup(const OpcodePrefix prefix,
                                        const uint8_t opcode,
                                        const InstMnemonic mnemonic) {
  int i;
  for (i = 0; i < 8; ++i) {
    DefineX87CompareSt0Sti(prefix, opcode, i, mnemonic);
  }
}

/* Define an x87 compare instruction that has st0 as its first
 * argument, and a Memory pointer as its second.
 */
static void DefineX87LtC0CompareSt0Mem(const OpcodePrefix prefix,
                                       const uint8_t opcode,
                                       const OperandKind opcode_in_modrm,
                                       const InstMnemonic mnemonic) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opcode, NACLi_X87, InstFlag(OpcodeLtC0InModRm), mnemonic);
  DefineOperand(opcode_in_modrm, OpFlag(OperandExtendsOpcode));
  DefineOperand(RegST0, OpFlag(OpUse) | OpFlag(OpImplicit));
  DefineOperand(M_Operand, OpFlag(OpUse));
  ResetToDefaultOpcodePrefix();
}

/* Define an x87 exchange instruction that has st0 as its first
 * argument, and sti as its second argument.
 */
static void DefineX87ExchangeSt0Sti(const OpcodePrefix prefix,
                                    const uint8_t opcode,
                                    const int base_offset,
                                    const InstMnemonic mnemonic) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opcode + base_offset, NACLi_X87,
               InstFlag(OpcodePlusR), mnemonic);
  DefineOperand(OpcodeBaseMinus0 + base_offset, OpFlag(OperandExtendsOpcode));
  DefineOperand(RegST0, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(St_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  ResetToDefaultOpcodePrefix();
}

/* Define a group of x87 exchange instructions that have st0 as thier first
 * argument, and sti (for all i, 1<=i<=8) for the second argument.
 */
static void DefineX87ExchangeSt0StiGroup(const OpcodePrefix prefix,
                                         const uint8_t opcode,
                                         const InstMnemonic mnemonic) {
  int i;
  for (i = 0; i < 8; ++i) {
    DefineX87ExchangeSt0Sti(prefix, opcode, i, mnemonic);
  }
}

/* Define an x87 instruction that has two operands, sti and st0,
 * and the value of sti is defined by the value of st0.
 */
static void DefineX87MoveStiSt0(const OpcodePrefix prefix,
                             const uint8_t opcode,
                             const int base_offset,
                             const InstMnemonic mnemonic) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opcode + base_offset, NACLi_X87, InstFlag(OpcodePlusR), mnemonic);
  DefineOperand(OpcodeBaseMinus0 + base_offset, OpFlag(OperandExtendsOpcode));
  DefineOperand(St_Operand, OpFlag(OpSet));
  DefineOperand(RegST0, OpFlag(OpUse));
  ResetToDefaultOpcodePrefix();
}

/* Define a group of x87 instructions that have two operands, sti and st0,
 * and the value of sti (for all i, 1<=i<=8) is defined by the value of st0.
 */
static void DefineX87MoveStiSt0Group(const OpcodePrefix prefix,
                                     const uint8_t opcode,
                                     const InstMnemonic mnemonic) {
  int i;
  for (i = 0; i < 8; ++i) {
    DefineX87MoveStiSt0(prefix, opcode, i, mnemonic);
  }
}

/* Define an x87 binary instruction that has sti as its first argument,
 * and st0 as its second argument.
 */
static void DefineX87BinopStiSt0(const OpcodePrefix prefix,
                                 const uint8_t opcode,
                                 const int base_offset,
                                 const InstMnemonic mnemonic) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opcode + base_offset, NACLi_X87, InstFlag(OpcodePlusR), mnemonic);
  DefineOperand(OpcodeBaseMinus0 + base_offset, OpFlag(OperandExtendsOpcode));
  DefineOperand(St_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(RegST0, OpFlag(OpUse));
  ResetToDefaultOpcodePrefix();
}

/* Define a group of x87 binary instructions that have sti as the
 * first argument (for all i, 1<=i<=8), and st0 as the second argument.
 */
static void DefineX87BinopStiSt0Group(const OpcodePrefix prefix,
                                      const uint8_t opcode,
                                      const InstMnemonic mnemonic) {
  int i;
  for (i = 0; i < 8; ++i) {
    DefineX87BinopStiSt0(prefix, opcode, i, mnemonic);
  }
}

/* Define an x87 binary instruction that has st1 as the first argument,
 * and st0 as the second argument.
 */
static void DefineX87BinopSt1St0(const OpcodePrefix prefix,
                                 const uint8_t opcode,
                                 const InstMnemonic mnemonic) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opcode, NACLi_X87, EmptyInstFlags, mnemonic);
  DefineOperand(RegST1, OpFlag(OpSet) | OpFlag(OpUse) | OpFlag(OpImplicit));
  DefineOperand(RegST0, OpFlag(OpUse) | OpFlag(OpImplicit));
  ResetToDefaultOpcodePrefix();
}

/* Define an x87 binary instruction that has st0 as the first argument,
 * and st1 as the second argument.
 */
static void DefineX87BinopSt0St1(const OpcodePrefix prefix,
                                 const uint8_t opcode,
                                 const InstMnemonic mnemonic) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opcode, NACLi_X87, EmptyInstFlags, mnemonic);
  DefineOperand(RegST0, OpFlag(OpSet) | OpFlag(OpUse) | OpFlag(OpImplicit));
  DefineOperand(RegST1, OpFlag(OpUse) | OpFlag(OpImplicit));
  ResetToDefaultOpcodePrefix();
}

/* Define an x87 comparison instruction that has st0 as the first
 * argument, and st1 as the second argument.
 */
static void DefineX87CompareSt0St1(const OpcodePrefix prefix,
                                   const uint8_t opcode,
                                   const InstMnemonic mnemonic) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opcode, NACLi_X87, EmptyInstFlags, mnemonic);
  DefineOperand(RegST0, OpFlag(OpUse) | OpFlag(OpImplicit));
  DefineOperand(RegST1, OpFlag(OpUse) | OpFlag(OpImplicit));
  ResetToDefaultOpcodePrefix();
}

/* Define an x87 instruction that updates st0, based on its previous
 * value.
 */
static void DefineX87ModifySt0(const OpcodePrefix prefix,
                               const uint8_t opcode,
                               const InstMnemonic mnemonic) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opcode, NACLi_X87, EmptyInstFlags, mnemonic);
  DefineOperand(RegST0, OpFlag(OpSet) | OpFlag(OpUse) | OpFlag(OpImplicit));
  ResetToDefaultOpcodePrefix();
}

/* Define an x87 instruction that updates sti, based on its previous value. */
static void DefineX87Sti(const OpcodePrefix prefix,
                         const uint8_t opcode,
                         const int base_offset,
                         const InstMnemonic mnemonic,
                         const OperandFlags sti_flags) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opcode + base_offset, NACLi_X87, InstFlag(OpcodePlusR), mnemonic);
  DefineOperand(OpcodeBaseMinus0 + base_offset, OpFlag(OperandExtendsOpcode));
  DefineOperand(St_Operand, sti_flags);
  ResetToDefaultOpcodePrefix();
}

/* Define a group of x87 instructions that updates sti (for all i, 1<=i<=8)
 * based on its previous value.
 */
static void DefineX87StiGroup(const OpcodePrefix prefix,
                              const uint8_t opcode,
                              const InstMnemonic mnemonic,
                              const OperandFlags sti_flags) {
  int i;
  for (i = 0; i < 8; ++i) {
    DefineX87Sti(prefix, opcode, i, mnemonic, sti_flags);
  }
}

void DefineX87Opcodes() {
  DefineDefaultOpcodePrefix(NoPrefix);

  /* Define D8 x87 instructions. */

  DefineX87LtC0BinopSt0Mem(NoPrefix, 0xd8, Opcode0, InstFadd);
  DefineX87LtC0BinopSt0Mem(NoPrefix, 0xd8, Opcode1, InstFmul);
  DefineX87LtC0BinopSt0Mem(NoPrefix, 0xd8, Opcode2, InstFcom);
  DefineX87LtC0BinopSt0Mem(NoPrefix, 0xd8, Opcode3, InstFcomp);
  DefineX87LtC0BinopSt0Mem(NoPrefix, 0xd8, Opcode4, InstSub);
  DefineX87LtC0BinopSt0Mem(NoPrefix, 0xd8, Opcode5, InstFsubr);
  DefineX87LtC0BinopSt0Mem(NoPrefix, 0xd8, Opcode6, InstFdiv);
  DefineX87LtC0BinopSt0Mem(NoPrefix, 0xd8, Opcode7, InstFdivr);
  DefineX87BinopSt0StiGroup(PrefixD8, 0xc0, InstFadd);
  DefineX87BinopSt0StiGroup(PrefixD8, 0xc8, InstFmul);
  DefineX87BinopSt0StiGroup(PrefixD8, 0xd0, InstFcom);
  DefineX87BinopSt0StiGroup(PrefixD8, 0xd8, InstFcomp);
  DefineX87BinopSt0StiGroup(PrefixD8, 0xe0, InstFsub);
  DefineX87BinopSt0StiGroup(PrefixD8, 0xe8,  InstFsubr);
  DefineX87BinopSt0StiGroup(PrefixD8, 0xf0, InstFdiv);
  DefineX87BinopSt0StiGroup(PrefixD8, 0xf8, InstFdivr);

  /* Define D9 x87 instructions. */

  DefineX87LtC0Mem(NoPrefix, 0xd9, Opcode0, InstFld, OpFlag(OpUse));
  DefineX87LtC0MoveMemSt0(NoPrefix, 0xd9, Opcode2, InstFst);
  DefineX87LtC0MoveMemSt0(NoPrefix, 0xd9, Opcode3, InstFstp);
  DefineX87LtC0Mem(NoPrefix, 0xd9, Opcode4, InstFldenv, OpFlag(OpUse));
  DefineX87LtC0Mem(NoPrefix, 0xd9, Opcode5, InstFldcw, OpFlag(OpUse));
  DefineX87LtC0Mem(NoPrefix, 0xd9, Opcode6, InstFnstenv,
            OpFlag(OpSet));
  DefineX87LtC0Mem(NoPrefix, 0xd9, Opcode7, InstFnstcw, OpFlag(OpSet));

  DefineX87StiGroup(PrefixD9, 0xc0, InstFld, OpFlag(OpUse));
  DefineX87ExchangeSt0StiGroup(PrefixD9, 0xc8, InstFxch);
  DefineX87MrmNoOperands(PrefixD9, 0xd0, Opcode0, InstFnop);
  DefineX87St0(PrefixD9, 0xe0,Opcode0, InstFchs,
            OpFlag(OpSet) | OpFlag(OpUse));
  DefineX87St0(PrefixD9, 0xe0, Opcode1, InstFabs,
            OpFlag(OpSet) | OpFlag(OpUse));
  DefineX87St0(PrefixD9, 0xe0, Opcode4, InstFtst, OpFlag(OpUse));
  DefineX87St0(PrefixD9, 0xe0, Opcode5, InstFxam, OpFlag(OpUse));
  DefineX87MrmNoOperands(PrefixD9, 0xe8, Opcode0, InstFld1);
  DefineX87MrmNoOperands(PrefixD9, 0xe8, Opcode1, InstFldl2t);
  DefineX87MrmNoOperands(PrefixD9, 0xe8, Opcode2, InstFldl2e);
  DefineX87MrmNoOperands(PrefixD9, 0xe8, Opcode3, InstFldpi);
  DefineX87MrmNoOperands(PrefixD9, 0xe8, Opcode4, InstFldlg2);
  DefineX87MrmNoOperands(PrefixD9, 0xe8, Opcode5, InstFldln2);
  DefineX87MrmNoOperands(PrefixD9, 0xe8, Opcode6, InstFldz);
  DefineX87ModifySt0(PrefixD9, 0xf0, InstF2m1);
  DefineX87BinopSt1St0(PrefixD9, 0xf1, InstFyl2x);
  DefineX87ModifySt0(PrefixD9, 0xf2, InstFptan);
  DefineX87BinopSt1St0(PrefixD9, 0xf3, InstFpatan);
  DefineX87ModifySt0(PrefixD9, 0xf4, InstFxtract);
  DefineX87BinopSt0St1(PrefixD9, 0xf5, InstFprem1);
  DefineX87NoOperands(PrefixD9, 0xf6, InstFdecstp);
  DefineX87NoOperands(PrefixD9, 0xf7, InstFincstp);
  DefineX87BinopSt0St1(PrefixD9, 0xf8, InstFprem);
  DefineX87BinopSt1St0(PrefixD9, 0xf9, InstFyl2xp1);
  DefineX87ModifySt0(PrefixD9, 0xfa, InstFsqrt);
  DefineX87ModifySt0(PrefixD9, 0xfb, InstFsincos);
  DefineX87ModifySt0(PrefixD9, 0xfc, InstFrndint);
  DefineX87BinopSt0St1(PrefixD9, 0xfd, InstFscale);
  DefineX87ModifySt0(PrefixD9, 0xfe, InstFsin);
  DefineX87ModifySt0(PrefixD9, 0xff, InstFcos);

  /* Define DA x87 instructions. */

  DefineX87LtC0BinopSt0Mem(NoPrefix, 0xda, Opcode0, InstFiadd);
  DefineX87LtC0BinopSt0Mem(NoPrefix, 0xda, Opcode1, InstFimul);
  DefineX87LtC0BinopSt0Mem(NoPrefix, 0xda, Opcode2, InstFicom);
  DefineX87LtC0BinopSt0Mem(NoPrefix, 0xda, Opcode3, InstFicomp);
  DefineX87LtC0BinopSt0Mem(NoPrefix, 0xda, Opcode4, InstFisub);
  DefineX87LtC0BinopSt0Mem(NoPrefix, 0xda, Opcode5, InstFisubr);
  DefineX87LtC0BinopSt0Mem(NoPrefix, 0xda, Opcode6, InstFidiv);
  DefineX87LtC0BinopSt0Mem(NoPrefix, 0xda, Opcode7, InstFidivr);
  DefineX87BinopSt0StiGroup(PrefixDA, 0xc0, InstFcmovb);
  DefineX87BinopSt0StiGroup(PrefixDA, 0xc8, InstFcmove);
  DefineX87BinopSt0StiGroup(PrefixDA, 0xd0, InstFcmovbe);
  DefineX87BinopSt0StiGroup(PrefixDA, 0xd8, InstFcmovu);
  DefineX87CompareSt0St1(PrefixDA, 0xe9, InstFucompp);

  /* Define DB x87 instructions. */

  DefineX87LtC0Mem(NoPrefix, 0xdb, Opcode0, InstFild, OpFlag(OpUse));
  DefineX87LtC0MoveMemSt0(NoPrefix, 0xdb, Opcode1, InstFisttp);
  DefineX87LtC0MoveMemSt0(NoPrefix, 0xdb, Opcode2, InstFist);
  DefineX87LtC0MoveMemSt0(NoPrefix, 0xdb, Opcode3, InstFistp);
  DefineX87LtC0Mem(NoPrefix, 0xdb, Opcode5, InstFld, OpFlag(OpUse));
  DefineX87LtC0MoveMemSt0(NoPrefix, 0xdb, Opcode7, InstFstp);
  DefineX87BinopSt0StiGroup(PrefixDB, 0xc0, InstFcmovnb);
  DefineX87BinopSt0StiGroup(PrefixDB, 0xc8, InstFcmovne);
  DefineX87BinopSt0StiGroup(PrefixDB, 0xd0, InstFcmovnbe);
  DefineX87BinopSt0StiGroup(PrefixDB, 0xd8, InstFcmovnu);
  DefineX87NoOperands(PrefixDB, 0xe2, InstFnclex);
  DefineX87NoOperands(PrefixDB, 0xe3, InstFninit);
  DefineX87CompareSt0StiGroup(PrefixDB, 0xe8, InstFucomi);
  DefineX87CompareSt0StiGroup(PrefixDB, 0xf0, InstFcomi);

  /* Define DC x87 instructions. */

  DefineX87LtC0BinopSt0Mem(NoPrefix, 0xdc, Opcode0, InstFadd);
  DefineX87LtC0BinopSt0Mem(NoPrefix, 0xdc, Opcode1, InstFmul);
  DefineX87LtC0BinopSt0Mem(NoPrefix, 0xdc, Opcode2, InstFcom);
  DefineX87LtC0BinopSt0Mem(NoPrefix, 0xdc, Opcode3, InstFcomp);
  DefineX87LtC0BinopSt0Mem(NoPrefix, 0xdc, Opcode4, InstSub);
  DefineX87LtC0BinopSt0Mem(NoPrefix, 0xdc, Opcode5, InstFsubr);
  DefineX87LtC0BinopSt0Mem(NoPrefix, 0xdc, Opcode6, InstFdiv);
  DefineX87LtC0BinopSt0Mem(NoPrefix, 0xdc, Opcode7, InstFdivr);
  DefineX87BinopStiSt0Group(PrefixDC, 0xc0, InstFadd);
  DefineX87BinopStiSt0Group(PrefixDC, 0xc8, InstFmul);
  DefineX87BinopStiSt0Group(PrefixDC, 0xe0, InstFsubr);
  DefineX87BinopStiSt0Group(PrefixDC, 0xe8, InstFsub);
  DefineX87BinopStiSt0Group(PrefixDC, 0xf0, InstFdivr);
  DefineX87BinopStiSt0Group(PrefixDC, 0xf8, InstFdiv);

  /* Define DD x87 instructions. */

  DefineX87LtC0Mem(NoPrefix, 0xdd, Opcode0, InstFld, OpFlag(OpUse));
  DefineX87LtC0MoveMemSt0(NoPrefix, 0xdd, Opcode1, InstFisttp);
  DefineX87LtC0MoveMemSt0(NoPrefix, 0xdd, Opcode2, InstFst);
  DefineX87LtC0MoveMemSt0(NoPrefix, 0xdd, Opcode3, InstFstp);
  DefineX87LtC0Mem(NoPrefix, 0xdd, Opcode4, InstFrstor, OpFlag(OpUse));
  DefineX87LtC0Mem(NoPrefix, 0xdd, Opcode6, InstFnsave, OpFlag(OpSet));
  DefineX87LtC0Mem(NoPrefix, 0xdd, Opcode7, InstFnstsw, OpFlag(OpSet));
  DefineX87StiGroup(PrefixDD, 0xc0, InstFfree, EmptyOpFlags);
  DefineX87MoveStiSt0Group(PrefixDD, 0xd0, InstFst);
  DefineX87MoveStiSt0Group(PrefixDD, 0xd8, InstFstp);
  DefineX87CompareSt0StiGroup(PrefixDD, 0xe0, InstFucom);
  DefineX87CompareSt0StiGroup(PrefixDD, 0xe8, InstFucomp);

  /* Define DE x87 instructions. */

  DefineX87LtC0BinopSt0Mem(NoPrefix, 0xde, Opcode0, InstFiadd);
  DefineX87LtC0BinopSt0Mem(NoPrefix, 0xde, Opcode1, InstFimul);
  DefineX87LtC0CompareSt0Mem(NoPrefix, 0xde, Opcode2, InstFicom);
  DefineX87LtC0CompareSt0Mem(NoPrefix, 0xde, Opcode3, InstFicomp);
  DefineX87LtC0BinopSt0Mem(NoPrefix, 0xde, Opcode4, InstFisub);
  DefineX87LtC0BinopSt0Mem(NoPrefix, 0xde, Opcode5, InstFisubr);
  DefineX87LtC0BinopSt0Mem(NoPrefix, 0xde, Opcode6, InstFidiv);
  DefineX87LtC0BinopSt0Mem(NoPrefix, 0xde, Opcode7, InstFidivr);
  DefineX87BinopStiSt0Group(PrefixDE, 0xc0, InstFaddp);
  DefineX87BinopStiSt0Group(PrefixDE, 0xc8, InstFmulp);
  DefineX87CompareSt0St1(PrefixDE, 0xd9, InstFcompp);
  DefineX87BinopStiSt0Group(PrefixDE, 0xe0, InstFsubrp);
  DefineX87BinopStiSt0Group(PrefixDE, 0xe8, InstFsubp);
  DefineX87BinopStiSt0Group(PrefixDE, 0xf0, InstFdivrp);
  DefineX87BinopStiSt0Group(PrefixDE, 0xf8, InstFdivp);

  /* Define DF x87 instructions. */

  DefineX87LtC0Mem(NoPrefix, 0xdf, Opcode0, InstFild, OpFlag(OpUse));
  DefineX87LtC0MoveMemSt0(NoPrefix, 0xdf, Opcode1, InstFisttp);
  DefineX87LtC0MoveMemSt0(NoPrefix, 0xdf, Opcode2, InstFist);
  DefineX87LtC0MoveMemSt0(NoPrefix, 0xdf, Opcode3, InstFistp);
  DefineX87LtC0Mem(NoPrefix, 0xdf, Opcode4, InstFbld, OpFlag(OpUse));
  DefineX87LtC0Mem(NoPrefix, 0xdf, Opcode5, InstFild, OpFlag(OpUse));
  DefineX87LtC0MoveMemSt0(NoPrefix, 0xdf, Opcode6, InstFbstp);
  DefineX87LtC0MoveMemSt0(NoPrefix, 0xdf, Opcode7, InstFistp);

  DefineX87Ax(PrefixDF, 0xe0, InstFnstsw, OpFlag(OpSet));
  DefineX87BinopSt0StiGroup(PrefixDF, 0xe8, InstFucomip);
  DefineX87BinopSt0StiGroup(PrefixDF, 0xf0, InstFcomip);

  /* todo(karl) What about "9b db e2 Fclex" ? */
  /* todo(karl) What about "9b db e3 finit" ? */
  /* todo(karl) What about "9b d9 /6 Fstenv" ? */
  /* todo(karl) What about "9b d9 /7 Fstcw" ? */
  /* todo(karl) What about "9b dd /6 Fsave" ? */
  /* todo(karl) What about "9b dd /7 Fstsw" ? */
  /* todo(karl) What about "9b df e0 Fstsw" ? */
}
