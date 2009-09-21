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
 * Defines the one byte x86 opcodes.
 */

#include "native_client/src/trusted/validator_x86/ncdecode_tablegen.h"

/* Define binary operation XX+00 to XX+05, for the binary operators
 * add, or, adc, sbb, and, sub, xor, and cmp. Base is the value XX.
 * Name is the name of the operation. Extra flags are any additional
 * flags that are true to a specific binary operator, rather than
 * all binary operators.
 */
static void BuildBinaryOps_00_05(const uint8_t base,
                                 const NaClInstType itype,
                                 const InstMnemonic name,
                                 const OperandFlags extra_flags) {
  DefineOpcode(
      base,
      itype,
      InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeRex) |
      InstFlag(OperandSize_b) | extra_flags,
      name);
  DefineOperand(E_Operand, OpFlag(OpUse) | OpFlag(OpSet));
  DefineOperand(G_Operand, OpFlag(OpUse));

  DefineOpcode(
      base+1,
      itype,
      InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_v) |
      InstFlag(OperandSize_w) | extra_flags,
      name);
  DefineOperand(E_Operand,
                OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OperandZeroExtends_v));
  DefineOperand(G_Operand, OpFlag(OpUse));

  DefineOpcode(
      base+1,
      itype,
      InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeUsesRexW) |
      InstFlag(OperandSize_o) | InstFlag(Opcode64Only) | extra_flags,
      name);
  DefineOperand(E_Operand, OpFlag(OpUse) | OpFlag(OpSet));
  DefineOperand(G_Operand, OpFlag(OpUse));

  DefineOpcode(
      base+2,
      itype,
      InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeRex) |
      InstFlag(OperandSize_b) | extra_flags,
      name);
  DefineOperand(G_Operand, OpFlag(OpUse) | OpFlag(OpSet));
  DefineOperand(E_Operand, OpFlag(OpUse));

  DefineOpcode(
      base+3,
      itype,
      InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_w) |
      InstFlag(OperandSize_v) | extra_flags,
      name);
  DefineOperand(G_Operand,
                OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OperandZeroExtends_v));
  DefineOperand(E_Operand, OpFlag(OpUse));

  DefineOpcode(
      base+3,
      itype,
      InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeUsesRexW) |
      InstFlag(OperandSize_o) | InstFlag(Opcode64Only) | extra_flags,
      name);
  DefineOperand(G_Operand, OpFlag(OpUse) | OpFlag(OpSet));
  DefineOperand(E_Operand, OpFlag(OpUse));

  DefineOpcode(
      base+4,
      itype,
      InstFlag(OperandSize_b) | InstFlag(OpcodeHasImmed) | extra_flags,
      name);
  DefineOperand(RegAL, OpFlag(OpUse) | OpFlag(OpSet));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcode(
      base+5,
      itype,
      InstFlag(OpcodeHasImmed) | InstFlag(OperandSize_v) | extra_flags,
      name);
  DefineOperand(RegEAX,
                OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OperandZeroExtends_v));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcode(
      base+5,
      itype,
      InstFlag(OpcodeHasImmed_v) | InstFlag(OpcodeUsesRexW) |
      InstFlag(OperandSize_o) | InstFlag(Opcode64Only) | extra_flags,
      name);
  DefineOperand(RegRAX, OpFlag(OpUse) | OpFlag(OpSet));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcode(
      base+5,
      itype,
      InstFlag(OperandSize_w) | InstFlag(OpcodeHasImmed) | extra_flags,
      name);
  DefineOperand(RegAX, OpFlag(OpUse) | OpFlag(OpSet));
  DefineOperand(I_Operand, OpFlag(OpUse));
}

/* Define the Xchg operator where the register is embedded in the opcode */
static void DefineXchgRegister() {
  /* Note: xchg is commutative, so order of operands is unimportant. */
  int i;
  /* Note: skip 0, implies a nop */
  for (i = 1; i < 8; ++i) {
    DefineOpcode(0x90 + i, NACLi_386L,
                 InstFlag(OpcodePlusR) | InstFlag(OperandSize_w) |
                 InstFlag(OperandSize_v),
                 InstXchg);
    DefineOperand(OpcodeBaseMinus0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(G_OpcodeBase, OpFlag(OpSet) | OpFlag(OpUse));
    DefineOperand(RegREAX, OpFlag(OpSet) | OpFlag(OpUse));

    DefineOpcode(0x90 + i, NACLi_386L,
                 InstFlag(OpcodePlusR) | InstFlag(OperandSize_o) |
                 InstFlag(OpcodeUsesRexW),
                 InstXchg);
    DefineOperand(OpcodeBaseMinus0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(G_OpcodeBase, OpFlag(OpSet) | OpFlag(OpUse));
    DefineOperand(RegRAX, OpFlag(OpSet) | OpFlag(OpUse));
  }
}

/* Define the increment and descrement operators XX+00 to XX+07. Base is
 * the value XX. Name is the name of the increment/decrement operator.
 */
static void DefineIncOrDec_00_07(const uint8_t base,
                                 const InstMnemonic name) {
  int i;
  for (i = 0; i < 8; ++i) {
    DefineOpcode(
        base + i,
        NACLi_386L,
        InstFlag(Opcode32Only) | InstFlag(OpcodePlusR) |
        InstFlag(OperandSize_w) | InstFlag(OperandSize_v),
        name);
    DefineOperand(OpcodeBaseMinus0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(G_OpcodeBase, OpFlag(OpUse) | OpFlag(OpSet));
  }
}

/* Define the push and pop operators XX+00 to XX+17. Base is
 * the value of XX. Name is the name of the push/pop operator.
 */
static void DefinePushOrPop_00_07(const uint8_t base,
                                  const InstMnemonic name) {
  int i;
  for (i = 0; i < 8; ++i) {
    DefineOpcode(
        base+i,
        NACLi_386,
        InstFlag(OpcodePlusR) | InstFlag(OperandSize_w),
        name);
    DefineOperand(OpcodeBaseMinus0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(RegRESP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
    DefineOperand(G_OpcodeBase, OpFlag(OpSet));

    DefineOpcode(
        base + i,
        NACLi_386,
        InstFlag(OpcodePlusR) | InstFlag(OperandSize_v) |
        InstFlag(Opcode32Only),
        name);
    DefineOperand(OpcodeBaseMinus0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(RegESP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
    DefineOperand(G_OpcodeBase, OpFlag(OpSet));

    DefineOpcode(
        base + i,
        NACLi_386,
        InstFlag(OpcodePlusR) | InstFlag(OperandSize_o) |
        InstFlag(Opcode64Only) | InstFlag(OperandSizeDefaultIs64),
        name);
    DefineOperand(OpcodeBaseMinus0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(RegRSP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
    DefineOperand(G_OpcodeBase, OpFlag(OpSet));
  }
}

static const InstMnemonic Group1OpcodeName[8] = {
  InstAdd,
  InstOr,
  InstAdc,
  InstSbb,
  InstAnd,
  InstSub,
  InstXor,
  InstCmp
};

/* Define the number of elements in Group2OpcodeName. */
static const int kNumGroup1OpcodeNames =
    sizeof(Group1OpcodeName) / sizeof(InstMnemonic);

static void DefineGroup1OpcodesInModRm() {
  int i;
  /* TODO(karl) verify this pattern is correct for instructions besides add and
   * sub.
   */
  for (i = 0; i < kNumGroup1OpcodeNames; ++i) {
    DefineOpcode(
        0x80,
        NACLi_386L,
        InstFlag(OpcodeInModRm) | InstFlag(OperandSize_b) |
        InstFlag(OpcodeHasImmed) |
        InstFlag(OpcodeLockable) | InstFlag(OpcodeRex),
        Group1OpcodeName[i]);
    DefineOperand(Opcode0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(E_Operand, OpFlag(OpUse) | OpFlag(OpSet));
    DefineOperand(I_Operand, OpFlag(OpUse));

    DefineOpcode(
        0x81,
        NACLi_386L,
        InstFlag(OpcodeInModRm) | InstFlag(OperandSize_w) |
        InstFlag(OpcodeHasImmed) | InstFlag(OpcodeLockable),
        Group1OpcodeName[i]);
    DefineOperand(Opcode0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(E_Operand, OpFlag(OpUse) | OpFlag(OpSet));
    DefineOperand(I_Operand, OpFlag(OpUse));

    DefineOpcode(
        0x81,
        NACLi_386L,
        InstFlag(OpcodeInModRm) | InstFlag(OperandSize_v) |
        InstFlag(OpcodeHasImmed) | InstFlag(OpcodeLockable),
        Group1OpcodeName[i]);
    DefineOperand(Opcode0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(E_Operand,
                  OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OperandZeroExtends_v));
    DefineOperand(I_Operand, OpFlag(OpUse));

    DefineOpcode(
        0x81,
        NACLi_386L,
        InstFlag(OpcodeInModRm) | InstFlag(Opcode64Only) |
        InstFlag(OpcodeUsesRexW) | InstFlag(OperandSize_o) |
        InstFlag(OpcodeHasImmed_v) | InstFlag(OpcodeLockable),
        Group1OpcodeName[i]);
    DefineOperand(Opcode0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(E_Operand, OpFlag(OpUse) | OpFlag(OpSet));
    DefineOperand(I_Operand, OpFlag(OpUse));

    DefineOpcode(
        0x83,
        NACLi_386L,
        InstFlag(OpcodeInModRm) | InstFlag(OperandSize_w) |
        InstFlag(OpcodeHasImmed_b) | InstFlag(OpcodeLockable),
        Group1OpcodeName[i]);
    DefineOperand(Opcode0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(E_Operand, OpFlag(OpUse) | OpFlag(OpSet));
    DefineOperand(I_Operand, OpFlag(OpUse));

    DefineOpcode(
        0x83,
        NACLi_386L,
        InstFlag(OpcodeInModRm) | InstFlag(OperandSize_v) |
        InstFlag(OpcodeHasImmed_b) | InstFlag(OpcodeLockable),
        Group1OpcodeName[i]);
    DefineOperand(Opcode0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(E_Operand,
                  OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OperandZeroExtends_v));
    DefineOperand(I_Operand, OpFlag(OpUse));

    DefineOpcode(
        0x83,
        NACLi_386L,
        InstFlag(OpcodeInModRm) | InstFlag(OperandSize_o) |
        InstFlag(Opcode64Only) | InstFlag(OpcodeUsesRexW) |
        InstFlag(OpcodeHasImmed_b) | InstFlag(OpcodeLockable),
        Group1OpcodeName[i]);
    DefineOperand(Opcode0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(E_Operand, OpFlag(OpUse) | OpFlag(OpSet));
    DefineOperand(I_Operand, OpFlag(OpUse));
  }
}

static void DefineJump8Opcode(uint8_t opcode, InstMnemonic name) {
  DefineOpcode(opcode, NACLi_JMP8,
               InstFlag(OperandSize_b) | InstFlag(OpcodeHasImmed),
               name);
  DefineOperand(RegREIP, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(J_Operand,
                OpFlag(OpUse) | OpFlag(OperandNear) | OpFlag(OperandRelative));
}

static const InstMnemonic Group2OpcodeName[8] = {
  InstRol,
  InstRor,
  InstRcl,
  InstRcr,
  InstShl,
  InstShr,
  InstMnemonicEnumSize, /* Denote no instruction defined. */
  InstSar
};

/* Define the number of elements in Group2OpcodeName */
static const int kNumGroup2OpcodeNames =
    sizeof(Group2OpcodeName) / sizeof(InstMnemonic);

static void DefineGroup2OpcodesInModRm() {
  int i;
  for (i = 0; i < kNumGroup2OpcodeNames; i++) {
    if (Group2OpcodeName[i] == InstMnemonicEnumSize) continue;
    DefineOpcode(0xC0, NACLi_OPINMRM,
                 InstFlag(OpcodeInModRm) | InstFlag(OperandSize_b) |
                 InstFlag(OpcodeHasImmed) | InstFlag(OpcodeRex),
                 Group2OpcodeName[i]);
    DefineOperand(Opcode0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
    DefineOperand(I_Operand, 0);

    DefineOpcode(0xC1, NACLi_OPINMRM,
                 InstFlag(OpcodeInModRm) | InstFlag(OperandSize_w) |
                 InstFlag(OperandSize_v) | InstFlag(OpcodeHasImmed_b),
                 Group2OpcodeName[i]);
    DefineOperand(Opcode0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(E_Operand,
                  OpFlag(OpSet) | OpFlag(OpUse) | OpFlag(OperandZeroExtends_v));
    DefineOperand(I_Operand, 0);

    DefineOpcode(0xC1, NACLi_OPINMRM,
                 InstFlag(OpcodeInModRm) | InstFlag(OperandSize_o) |
                 InstFlag(OpcodeUsesRexW) | InstFlag(Opcode64Only) |
                 InstFlag(OpcodeHasImmed_b),
                 Group2OpcodeName[i]);
    DefineOperand(Opcode0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
    DefineOperand(I_Operand, 0);

    DefineOpcode(0xD0, NACLi_OPINMRM,
                 InstFlag(OpcodeInModRm) | InstFlag(OperandSize_b) |
                 InstFlag(OpcodeRex),
                 Group2OpcodeName[i]);
    DefineOperand(Opcode0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(E_Operand,  OpFlag(OpSet) | OpFlag(OpUse));
    DefineOperand(Const_1, 0);

    DefineOpcode(0xD1, NACLi_OPINMRM,
                 InstFlag(OpcodeInModRm) | InstFlag(OperandSize_w) |
                 InstFlag(OperandSize_v),
                 Group2OpcodeName[i]);
    DefineOperand(Opcode0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
    DefineOperand(Const_1, 0);

    DefineOpcode(0xD1, NACLi_OPINMRM,
                 InstFlag(OpcodeInModRm) | InstFlag(OperandSize_o) |
                 InstFlag(OpcodeUsesRexW) | InstFlag(Opcode64Only),
                 Group2OpcodeName[i]);
    DefineOperand(Opcode0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
    DefineOperand(Const_1, 0);

    DefineOpcode(0xD2, NACLi_OPINMRM,
                 InstFlag(OpcodeInModRm) | InstFlag(OperandSize_b) |
                 InstFlag(OpcodeRex),
                 Group2OpcodeName[i]);
    DefineOperand(Opcode0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
    DefineOperand(RegCL, OpFlag(OpUse));

    DefineOpcode(0xD3, NACLi_OPINMRM,
                 InstFlag(OpcodeInModRm) | InstFlag(OperandSize_v),
                 Group2OpcodeName[i]);
    DefineOperand(Opcode0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
    DefineOperand(RegCL, OpFlag(OpUse));

    DefineOpcode(0xD3, NACLi_OPINMRM,
                 InstFlag(OpcodeInModRm) | InstFlag(OperandSize_o) |
                 InstFlag(OpcodeUsesRexW) | InstFlag(Opcode64Only),
                 Group2OpcodeName[i]);
    DefineOperand(Opcode0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
    DefineOperand(RegCL, OpFlag(OpUse));

    DefineOpcode(0xd5, NACLi_ILLEGAL,
                 InstFlag(Opcode32Only) | InstFlag(OpcodeHasImmed_b),
                 InstAad);
    DefineOperand(RegAL, OpFlag(OpSet) | OpFlag(OpImplicit));
    DefineOperand(RegAH, OpFlag(OpSet) | OpFlag(OpImplicit));
  }
}

static const int kFirstByteGroupsForX87Inst = 2;
static const int kFirstByteGroupSizeForX87Inst = 2;
static const int kNumInstForX87Inst = 8;

/*
 * Group the first byte of the X87 opcodes by the set of instructions
 * they define.
 *
 * kFirstByteGroupsForX87Inst x kFirstByteGroupSizeForX87Inst
 */
static const uint8_t FirstByteOpcodeForX87Inst[2][2] = {
  {0xD8, 0xDC},
  {0xDA, 0xDE}
};

/*
 * kFirstByteGroupsForX87Inst x kNumInstForX87Inst
 */
static const InstMnemonic OneByteOpcodeNameForX87Inst[2][8] = {
  { InstFadd, /* 0xD8 and 0xDC */
    InstFmul,
    InstFcom,
    InstFcomp,
    InstFsub,
    InstFsubr,
    InstFdiv,
    InstFdivr },
  { InstFiadd, /* 0xDA and 0xDE */
    InstFimul,
    InstFicom,
    InstFicomp,
    InstFisub,
    InstFisubr,
    InstFidiv,
    InstFidivr }
};

/*
 *
 * The X87 opcode maps are grouped by the first byte of the opcode,
 * from D8-DF. Some opcodes refer to the same instruction sets.
 *
 * 0xD8 {InstFadd, .. InstFdivr}
 * 0xDC {InstFadd, .. InstFdivr}
 * 0xDA {InstFiadd, .. InstFidivr}
 * 0xDE {InstFiadd, .. InstFidivr}
 */
static void DefineModRmOpcodesForX87Inst() {
  int byte_group;
  int byte_idx;
  int i;
  for (byte_group = 0; byte_group < kFirstByteGroupsForX87Inst; ++byte_group)
    for (byte_idx = 0; byte_idx < kFirstByteGroupSizeForX87Inst; ++byte_idx)
      for (i = 0; i < kNumInstForX87Inst; ++i) {
        DefineOpcode(
            FirstByteOpcodeForX87Inst[byte_group][byte_idx],
            NACLi_X87,
            InstFlag(OpcodeInModRm),
            OneByteOpcodeNameForX87Inst[byte_group][i]);
        DefineOperand(Opcode0 + i, OpFlag(OperandExtendsOpcode));
        DefineOperand(RegST0,
                      OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
        DefineOperand(MemOffset_E_Operand, OpFlag(OpUse));
      }
}

void DefineOneByteOpcodes() {
  uint8_t i;

  DefineOpcodePrefix(NoPrefix);

  BuildBinaryOps_00_05(0x00, NACLi_386L, InstAdd, InstFlag(OpcodeLockable));

  DefineOpcode(0x06, NACLi_ILLEGAL, InstFlag(Opcode32Only), InstPush);
  DefineOperand(RegESP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegES, OpFlag(OpUse));

  DefineOpcode(0x07, NACLi_ILLEGAL, InstFlag(Opcode32Only), InstPop);
  DefineOperand(RegESP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegES, OpFlag(OpSet));

  BuildBinaryOps_00_05(0x08, NACLi_386L, InstOr, InstFlag(OpcodeLockable));

  DefineOpcode(0x0e, NACLi_ILLEGAL, InstFlag(Opcode32Only), InstPush);
  DefineOperand(RegESP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegCS, OpFlag(OpUse));

  /* 0x0F is a two-byte opcode prefix. */

  BuildBinaryOps_00_05(0x10, NACLi_386L, InstAdc, InstFlag(OpcodeLockable));

  DefineOpcode(0x16, NACLi_ILLEGAL, InstFlag(Opcode32Only), InstPush);
  DefineOperand(RegRESP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegSS, OpFlag(OpUse));

  DefineOpcode(0x17, NACLi_ILLEGAL, InstFlag(Opcode32Only), InstPop);
  DefineOperand(RegRESP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegSS, OpFlag(OpSet));

  BuildBinaryOps_00_05(0x18, NACLi_386L, InstSbb, InstFlag(OpcodeLockable));

  DefineOpcode(0x1e, NACLi_ILLEGAL, InstFlag(Opcode32Only), InstPush);
  DefineOperand(RegESP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegDS, OpFlag(OpUse));

  DefineOpcode(0x1f, NACLi_ILLEGAL, InstFlag(Opcode32Only), InstPop);
  DefineOperand(RegESP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegDS, OpFlag(OpSet));

  BuildBinaryOps_00_05(0x20, NACLi_386L, InstAnd, InstFlag(OpcodeLockable));

  DefineOpcode(0x27, NACLi_ILLEGAL, InstFlag(Opcode32Only), InstDaa);

  BuildBinaryOps_00_05(0x28, NACLi_386L, InstSub, InstFlag(OpcodeLockable));

  DefineOpcode(0x2f, NACLi_ILLEGAL, InstFlag(Opcode32Only), InstDas);

  BuildBinaryOps_00_05(0x30, NACLi_386L, InstXor, InstFlag(OpcodeLockable));

 /*deprecated */
  DefineOpcode(0x37, NACLi_ILLEGAL, InstFlag(Opcode32Only), InstAaa);

  BuildBinaryOps_00_05(0x38, NACLi_386, InstCmp, 0);

  /* Ox3e is segment ds prefix */

  DefineOpcode(0x3f, NACLi_ILLEGAL, InstFlag(Opcode32Only), InstAas);

  DefineIncOrDec_00_07(0x40, InstInc);
  DefineIncOrDec_00_07(0x48, InstDec);
  DefinePushOrPop_00_07(0x50, InstPush);
  DefinePushOrPop_00_07(0x58, InstPop);

  DefineOpcode(
      0x60,
      NACLi_ILLEGAL,
      InstFlag(Opcode32Only) | InstFlag(OperandSize_w),
      InstPusha);
  DefineOperand(RegESP, OpFlag(OpImplicit) | OpFlag(OpUse) | OpFlag(OpSet));
  DefineOperand(RegGP7, OpFlag(OpImplicit) | OpFlag(OpUse));

  DefineOpcode(
      0x60,
      NACLi_ILLEGAL,
      InstFlag(Opcode32Only) | InstFlag(OperandSize_v),
      InstPushad);
  DefineOperand(RegESP, OpFlag(OpImplicit) | OpFlag(OpUse) | OpFlag(OpSet));
  DefineOperand(RegGP7, OpFlag(OpImplicit) | OpFlag(OpUse));

  DefineOpcode(
      0x61,
      NACLi_ILLEGAL,
      InstFlag(Opcode32Only) | InstFlag(OperandSize_w),
      InstPopa);
  DefineOperand(RegESP, OpFlag(OpImplicit) | OpFlag(OpUse) | OpFlag(OpSet));
  DefineOperand(RegGP7, OpFlag(OpImplicit) | OpFlag(OpSet));

  DefineOpcode(
     0x61,
     NACLi_ILLEGAL,
     InstFlag(Opcode32Only) | InstFlag(OperandSize_v),
     InstPopad);
  DefineOperand(RegESP, OpFlag(OpImplicit) | OpFlag(OpUse) | OpFlag(OpSet));
  DefineOperand(RegGP7, OpFlag(OpImplicit) | OpFlag(OpSet));

  /* TODO(karl) FIX this instruction -- It isn't consistent.
  DefineOpcode(
      0x62,
      NACLi_ILLEGAL,
      InstFlag(Opcode32Only) | InstFlag(OpcodeInModRm) |
      InstFlag(OperandSize_v),
      InstBound);
  DefineOperand(G_Operand, OpFlag(OpUse));
  DefineOperand(E_Operand, OpFlag(OpUse));
  */

  DefineOpcode(0x63, NACLi_SYSTEM,
               InstFlag(Opcode32Only) | InstFlag(OperandSize_w) |
               InstFlag(OpcodeUsesModRm),
               InstArpl);
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(G_Operand, OpFlag(OpUse));


  DefineOpcode(0x63, NACLi_SYSTEM,
               InstFlag(Opcode64Only) | InstFlag(OperandSize_o) |
               InstFlag(OpcodeUsesRexW) | InstFlag(OpcodeUsesModRm),
               InstMovsxd);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(Ev_Operand, OpFlag(OpUse));

  /* NOTE: this form of movsxd should be discourages. */
  DefineOpcode(0x63, /* NACLi_SYSTEM, */ NACLi_ILLEGAL,
               InstFlag(Opcode64Only) | InstFlag(OperandSize_v) |
               InstFlag(OpcodeUsesModRm),
               InstMovsxd);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(E_Operand, OpFlag(OpUse));

  DefineOpcode(0x68, NACLi_386,
               InstFlag(OperandSize_w) | InstFlag(OpcodeHasImmed),
               InstPush);
  DefineOperand(RegRESP, OpFlag(OpImplicit) | OpFlag(OpUse) | OpFlag(OpSet));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcode(0x68, NACLi_386,
               InstFlag(OperandSize_o) | InstFlag(OpcodeHasImmed_v) |
               InstFlag(OperandSizeDefaultIs64),
               InstPush);
  DefineOperand(RegRESP, OpFlag(OpImplicit) | OpFlag(OpUse) | OpFlag(OpSet));
  DefineOperand(I_Operand, OpFlag(OpUse));

  /* TODO(karl) Figure out how the two and three argument versions are
   * differentiated (two argument form not described)?
   */
  DefineOpcode(0x69, NACLi_386,
               InstFlag(OperandSize_w) | InstFlag(OpcodeUsesModRm) |
               InstFlag(OpcodeHasImmed),
               InstImul);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(E_Operand, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcode(0x69, NACLi_386,
               InstFlag(OperandSize_v) | InstFlag(OpcodeUsesModRm) |
               InstFlag(OpcodeHasImmed),
               InstImul);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(E_Operand, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcode(0x69, NACLi_386,
               InstFlag(Opcode64Only) | InstFlag(OperandSize_o) |
               InstFlag(OpcodeUsesRexW) |
               InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeHasImmed_v),
               InstImul);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(E_Operand, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcode(0x6a, NACLi_386,
               InstFlag(OperandSize_b) | InstFlag(OpcodeHasImmed),
               InstPush);
  DefineOperand(RegRESP, OpFlag(OpImplicit) | OpFlag(OpUse) | OpFlag(OpSet));
  DefineOperand(I_Operand, OpFlag(OpUse));

  /* TODO(karl) Figure out how the two and three argument versions are
   * differentiated (two argument form not described)?
   */
  DefineOpcode(0x6b, NACLi_386,
               InstFlag(OperandSize_w) | InstFlag(OpcodeUsesModRm) |
               InstFlag(OpcodeHasImmed),
               InstImul);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(E_Operand, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcode(0x6b, NACLi_386,
               InstFlag(OperandSize_v) | InstFlag(OpcodeUsesModRm) |
               InstFlag(OpcodeHasImmed),
               InstImul);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(E_Operand, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcode(0x6b, NACLi_386,
               InstFlag(Opcode64Only) | InstFlag(OperandSize_o) |
               InstFlag(OpcodeUsesRexW) |
               InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeHasImmed_v),
               InstImul);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(E_Operand, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcode(0x6c, NACLi_ILLEGAL, InstFlag(OperandSize_b), InstInsb);
  DefineOperand(RegES_EDI, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegDX, OpFlag(OpUse) | OpFlag(OpImplicit));

  DefineOpcode(0x6D, NACLi_ILLEGAL, InstFlag(OperandSize_w), InstInsw);
  DefineOperand(RegES_EDI, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegDX, OpFlag(OpUse) | OpFlag(OpImplicit));

  DefineOpcode(0x6D, NACLi_ILLEGAL, InstFlag(OperandSize_v), InstInsd);
  DefineOperand(RegES_EDI, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegDX, OpFlag(OpUse) | OpFlag(OpImplicit));

  DefineOpcode(0x6E, NACLi_ILLEGAL, InstFlag(OperandSize_b), InstOutsb);
  DefineOperand(RegDX, OpFlag(OpUse) | OpFlag(OpImplicit));
  DefineOperand(RegES_EDI, OpFlag(OpUse) | OpFlag(OpImplicit));

  DefineOpcode(0x6F, NACLi_ILLEGAL, InstFlag(OperandSize_w), InstOutsw);
  DefineOperand(RegDX, OpFlag(OpUse) | OpFlag(OpImplicit));
  DefineOperand(RegES_EDI, OpFlag(OpUse) | OpFlag(OpImplicit));

  DefineOpcode(0x6F, NACLi_ILLEGAL, InstFlag(OperandSize_v), InstOutsd);
  DefineOperand(RegDX, OpFlag(OpUse) | OpFlag(OpImplicit));
  DefineOperand(RegES_EDI, OpFlag(OpUse) | OpFlag(OpImplicit));

  DefineJump8Opcode(0x70, InstJo);
  DefineJump8Opcode(0x71, InstJno);
  DefineJump8Opcode(0x72, InstJb);
  DefineJump8Opcode(0x73, InstJnb);
  DefineJump8Opcode(0x74, InstJz);
  DefineJump8Opcode(0x75, InstJnz);
  DefineJump8Opcode(0x76, InstJbe);
  DefineJump8Opcode(0x77, InstJnbe);
  DefineJump8Opcode(0x78, InstJs);
  DefineJump8Opcode(0x79, InstJns);
  DefineJump8Opcode(0x7a, InstJp);
  DefineJump8Opcode(0x7b, InstJnp);
  DefineJump8Opcode(0x7c, InstJl);
  DefineJump8Opcode(0x7d, InstJnl);
  DefineJump8Opcode(0x7e, InstJle);
  DefineJump8Opcode(0x7f, InstJnle);

  /* For the moment, show some examples of Opcodes in Mod/Rm. */
  DefineGroup1OpcodesInModRm();

  /* The AMD manual shows 0x82 as a synonym for 0x80,
   * however these are all illegal in 64-bit mode so we omit them here
   * too. (note: by omission, the opcode is automatically undefined).
   */

  DefineOpcode(0x84, NACLi_386,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_b) |
               InstFlag(OpcodeRex),
               InstTest);
  DefineOperand(E_Operand, OpFlag(OpUse));
  DefineOperand(G_Operand, OpFlag(OpUse));

  DefineOpcode(0x85, NACLi_386,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_w) |
               InstFlag(OperandSize_v),
               InstTest);
  DefineOperand(E_Operand, OpFlag(OpUse));
  DefineOperand(G_Operand, OpFlag(OpUse));

  DefineOpcode(0x85, NACLi_386,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_o) |
               InstFlag(OpcodeUsesRexW) | InstFlag(Opcode64Only),
               InstTest);
  DefineOperand(E_Operand, OpFlag(OpUse));
  DefineOperand(G_Operand, OpFlag(OpUse));

  /* Note: for xchg, order of operands are commutative, in terms of
   * opcode used.
   */
  DefineOpcode(0x86, NACLi_386L,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_b) |
               InstFlag(Opcode32Only),
               InstXchg);
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(G_Operand, OpFlag(OpSet) | OpFlag(OpUse));

  DefineOpcode(0x86, NACLi_386L,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_b) |
               InstFlag(OpcodeRex) | InstFlag(Opcode64Only),
               InstXchg);
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(G_Operand, OpFlag(OpSet) | OpFlag(OpUse));

  DefineOpcode(0x87, NACLi_386L,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_w) |
               InstFlag(OperandSize_v),
               InstXchg);
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(G_Operand, OpFlag(OpSet) | OpFlag(OpUse));

  DefineOpcode(0x87, NACLi_386L,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_o) |
               InstFlag(Opcode64Only) | InstFlag(OpcodeUsesRexW),
               InstXchg);
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(G_Operand, OpFlag(OpSet) | OpFlag(OpUse));

  DefineOpcode(0x88, NACLi_386,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_b) |
               InstFlag(OpcodeRex),
               InstMov);
  DefineOperand(E_Operand, OpFlag(OpSet));
  DefineOperand(G_Operand, OpFlag(OpUse));

  DefineOpcode(0x89, NACLi_386,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_w) |
               InstFlag(OperandSize_v),
               InstMov);
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OperandZeroExtends_v));
  DefineOperand(G_Operand, OpFlag(OpUse));

  DefineOpcode(0x89, NACLi_386,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_o) |
               InstFlag(Opcode64Only) | InstFlag(OpcodeUsesRexW),
               InstMov);
  DefineOperand(E_Operand, OpFlag(OpSet));
  DefineOperand(G_Operand, OpFlag(OpUse));

  DefineOpcode(0x8A, NACLi_386,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_b) |
               InstFlag(OpcodeRex),
               InstMov);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(E_Operand, OpFlag(OpUse));

  DefineOpcode(0x8B, NACLi_386,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_w) |
               InstFlag(OperandSize_v),
               InstMov);
  DefineOperand(G_Operand, OpFlag(OpSet) | OpFlag(OperandZeroExtends_v));
  DefineOperand(E_Operand, OpFlag(OpUse));

  DefineOpcode(0x8B, NACLi_386,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_o) |
               InstFlag(Opcode64Only) | InstFlag(OpcodeUsesRexW),
               InstMov);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(E_Operand, OpFlag(OpUse));

  /* TODO(karl) what is SReg (second argument) in 0x8c*/
  DefineOpcode(0x8c, NACLi_ILLEGAL,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_w),
               InstMov);
  DefineOperand(E_Operand, OpFlag(OpSet));

  /* TODO(karl) what is SReg (second argument) in 0x8c*/
  DefineOpcode(0x8c, NACLi_ILLEGAL,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_o) |
               InstFlag(Opcode64Only) | InstFlag(OpcodeUsesRexW),
               InstMov);
  DefineOperand(E_Operand, OpFlag(OpSet));

  DefineOpcode(0x8d, NACLi_386,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_w) |
               InstFlag(OperandSize_v),
               InstLea);
  DefineOperand(G_Operand, OpFlag(OpSet) | OpFlag(OperandZeroExtends_v));
  DefineOperand(M_Operand, OpFlag(OpAddress));

  DefineOpcode(0x8d, NACLi_386,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_o) |
               InstFlag(Opcode64Only) | InstFlag(OpcodeUsesRexW),
               InstLea);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(M_Operand, OpFlag(OpAddress));

  /* TODO(karl) what is SReg (first argument) in 0x8e*/
  DefineOpcode(0x8e, NACLi_ILLEGAL,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_w),
               InstMov);
  DefineOperand(E_Operand, OpFlag(OpSet));

  /* TODO(karl) what is SReg (first argument) in 0x8e*/
  DefineOpcode(0x8e, NACLi_ILLEGAL,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_o) |
               InstFlag(Opcode64Only) | InstFlag(OpcodeUsesRexW),
               InstMov);
  DefineOperand(E_Operand, OpFlag(OpSet));

  DefineOpcode(0x8f, NACLi_386,
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_w),
               InstPop);
  DefineOperand(Opcode0, OpFlag(OperandExtendsOpcode));
  DefineOperand(RegRESP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(E_Operand, OpFlag(OpSet));

  DefineOpcode(0x8f, NACLi_386,
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_v) |
               InstFlag(Opcode32Only),
               InstPop);
  DefineOperand(Opcode0, OpFlag(OperandExtendsOpcode));
  DefineOperand(RegESP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(E_Operand, OpFlag(OpSet));

  DefineOpcode(0x8f, NACLi_386,
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_o) |
               InstFlag(Opcode64Only),
               InstPop);
  DefineOperand(Opcode0, OpFlag(OperandExtendsOpcode));
  DefineOperand(RegRSP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(E_Operand, OpFlag(OpSet));

  DefineOpcode(0x90, NACLi_386R, 0, InstNop);

  DefineXchgRegister();

  DefineOpcode(0x98, NACLi_386,
               InstFlag(OperandSize_w),
               InstCbw);
  DefineOperand(RegAX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegAL, OpFlag(OpUse) | OpFlag(OpImplicit));

  DefineOpcode(0x98, NACLi_386,
               InstFlag(OperandSize_v),
               InstCwde);
  DefineOperand(RegEAX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegAX, OpFlag(OpUse) | OpFlag(OpImplicit));

  DefineOpcode(0x98, NACLi_386,
               InstFlag(OperandSize_o) | InstFlag(OpcodeUsesRexW) |
               InstFlag(Opcode64Only),
               InstCdqe);
  DefineOperand(RegRAX,
                OpFlag(OpSet) | OpFlag(OpImplicit) |
                OpFlag(OperandSignExtends_v));
  DefineOperand(RegEAX, OpFlag(OpUse) | OpFlag(OpImplicit));

  DefineOpcode(0x99, NACLi_386,
               InstFlag(OperandSize_w),
               InstCwd);
  DefineOperand(SegmentDX_AX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegAX, OpFlag(OpUse) | OpFlag(OpImplicit));

  DefineOpcode(0x99, NACLi_386,
               InstFlag(OperandSize_v),
               InstCdq);
  DefineOperand(SegmentDX_AX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegEAX, OpFlag(OpUse) | OpFlag(OpImplicit));

  DefineOpcode(0x99, NACLi_386,
               InstFlag(OperandSize_o) | InstFlag(OpcodeUsesRexW) |
               InstFlag(Opcode64Only),
               InstCqo);
  DefineOperand(SegmentDX_AX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegRAX, OpFlag(OpUse) | OpFlag(OpImplicit));

  DefineOpcode(0x9a, NACLi_ILLEGAL,
               InstFlag(OperandSize_w) | InstFlag(OpcodeHasImmed_v) |
               InstFlag(Opcode32Only),
               InstCall);
  DefineOperand(RegEIP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegESP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(J_Operand, OpFlag(OpUse) | OpFlag(OperandFar) |
                OpFlag(OperandRelative));

  DefineOpcode(0x9a, NACLi_ILLEGAL,
               InstFlag(OperandSize_v) | InstFlag(OpcodeHasImmed_p) |
               InstFlag(Opcode32Only),
               InstCall);
  DefineOperand(RegEIP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegESP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(J_Operand, OpFlag(OpUse) | OpFlag(OperandFar) |
                OpFlag(OperandRelative));

  DefineOpcode(0x9b, NACLi_X87, 0, InstWait);

  DefineOpcode(0x9c, NACLi_ILLEGAL, InstFlag(OperandSize_w), InstPushf);
  DefineOperand(RegRESP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));

  DefineOpcode(0x9c, NACLi_ILLEGAL,
               InstFlag(Opcode32Only) | InstFlag(OperandSize_v),
               InstPushfd);
  DefineOperand(RegESP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));

  DefineOpcode(0x9c, NACLi_ILLEGAL,
               InstFlag(Opcode64Only) | InstFlag(OperandSize_o) |
               InstFlag(OperandSizeDefaultIs64),
               InstPushfq);
  DefineOperand(RegRSP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));

  DefineOpcode(0x9d, NACLi_ILLEGAL, InstFlag(OperandSize_w), InstPopf);
  DefineOperand(RegRESP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));

  DefineOpcode(0x9d, NACLi_ILLEGAL,
               InstFlag(Opcode32Only) | InstFlag(OperandSize_v),
               InstPopfd);
  DefineOperand(RegESP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));

  DefineOpcode(0x9d, NACLi_ILLEGAL,
               InstFlag(Opcode64Only) | InstFlag(OperandSize_v),
               InstPopfq);
  DefineOperand(RegRSP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));

  DefineOpcode(0x9e, NACLi_386, InstFlag(Opcode32Only), InstSahf);
  DefineOperand(RegAH, OpFlag(OpUse) | OpFlag(OpImplicit));

  DefineOpcode(0x9f, NACLi_386, InstFlag(Opcode32Only), InstLahf);
  DefineOperand(RegAH, OpFlag(OpSet) | OpFlag(OpImplicit));

  DefineOpcode(0xa0, NACLi_386,
               InstFlag(OperandSize_b) | InstFlag(OpcodeHasImmed_Addr),
               InstMov);
  DefineOperand(RegAL, OpFlag(OpSet));
  DefineOperand(O_Operand, OpFlag(OpUse));

  DefineOpcode(0xa1, NACLi_386,
               InstFlag(OperandSize_w) | InstFlag(OperandSize_v) |
               InstFlag(OpcodeHasImmed_Addr),
               InstMov);
  DefineOperand(RegREAX, OpFlag(OpSet) | OpFlag(OperandZeroExtends_v));
  DefineOperand(O_Operand, OpFlag(OpUse));

  DefineOpcode(0xa1, NACLi_386,
               InstFlag(OperandSize_o) | InstFlag(OpcodeHasImmed_Addr) |
               InstFlag(Opcode64Only) | InstFlag(OpcodeUsesRexW),
               InstMov);
  DefineOperand(RegRAX, OpFlag(OpSet));
  DefineOperand(O_Operand, OpFlag(OpUse));

  DefineOpcode(0xa2, NACLi_386,
               InstFlag(OperandSize_b) | InstFlag(OpcodeHasImmed_Addr),
               InstMov);
  DefineOperand(O_Operand, OpFlag(OpSet));
  DefineOperand(RegAL, OpFlag(OpUse));

  DefineOpcode(0xa3, NACLi_386,
               InstFlag(OperandSize_w) | InstFlag(OperandSize_v) |
               InstFlag(OpcodeHasImmed_Addr),
               InstMov);
  DefineOperand(O_Operand, OpFlag(OpSet) | OpFlag(OperandZeroExtends_v));
  DefineOperand(RegREAX, OpFlag(OpUse));

  DefineOpcode(0xa3, NACLi_386,
               InstFlag(OperandSize_o) | InstFlag(OpcodeHasImmed_Addr) |
               InstFlag(Opcode64Only) | InstFlag(OpcodeUsesRexW),
               InstMov);
  DefineOperand(O_Operand, OpFlag(OpSet));
  DefineOperand(RegRAX, OpFlag(OpUse));

  DefineOpcode(0xa4, NACLi_386R,
               InstFlag(OperandSize_b),
               InstMovsb);
  DefineOperand(RegES_EDI, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegDS_EDI, OpFlag(OpUse) | OpFlag(OpImplicit));

  DefineOpcode(0xa5, NACLi_386R,
               InstFlag(OperandSize_w),
               InstMovsw);
  DefineOperand(RegES_EDI, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegDS_EDI, OpFlag(OpUse) | OpFlag(OpImplicit));

  DefineOpcode(0xa5, NACLi_386R,
               InstFlag(OperandSize_v),
               InstMovsd);
  DefineOperand(RegES_EDI, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegDS_EDI, OpFlag(OpUse) | OpFlag(OpImplicit));

  DefineOpcode(0xa5, NACLi_386R,
               InstFlag(Opcode64Only) | InstFlag(OperandSize_o) |
               InstFlag(OpcodeUsesRexW),
               InstMovsq);
  DefineOperand(RegES_EDI, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegDS_EDI, OpFlag(OpUse) | OpFlag(OpImplicit));

  DefineOpcode(0xa6, NACLi_386RE,
               InstFlag(OperandSize_b),
               InstCmpsb);
  DefineOperand(RegES_EDI, OpFlag(OpUse) | OpFlag(OpImplicit));
  DefineOperand(RegDS_EDI, OpFlag(OpUse) | OpFlag(OpImplicit));

  DefineOpcode(0xa7, NACLi_386RE,
               InstFlag(OperandSize_w),
               InstCmpsw);
  DefineOperand(RegES_EDI, OpFlag(OpUse) | OpFlag(OpImplicit));
  DefineOperand(RegDS_EDI, OpFlag(OpUse) | OpFlag(OpImplicit));

  DefineOpcode(0xa7, NACLi_386RE,
               InstFlag(OperandSize_v),
               InstCmpsd);
  DefineOperand(RegES_EDI, OpFlag(OpUse) | OpFlag(OpImplicit));
  DefineOperand(RegDS_EDI, OpFlag(OpUse) | OpFlag(OpImplicit));

  DefineOpcode(0xa7, NACLi_386RE,
               InstFlag(Opcode64Only) | InstFlag(OperandSize_o) |
               InstFlag(OpcodeUsesRexW),
               InstCmpsq);
  DefineOperand(RegES_EDI, OpFlag(OpUse) | OpFlag(OpImplicit));
  DefineOperand(RegDS_EDI, OpFlag(OpUse) | OpFlag(OpImplicit));

  DefineOpcode(0xA8, NACLi_386,
               InstFlag(OperandSize_b) | InstFlag(OpcodeHasImmed),
               InstTest);
  DefineOperand(RegAL, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcode(0xA9, NACLi_386,
               InstFlag(OperandSize_w) | InstFlag(OpcodeHasImmed),
               InstTest);
  DefineOperand(RegAX, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcode(0xA9, NACLi_386,
               InstFlag(OperandSize_v) | InstFlag(OpcodeHasImmed),
               InstTest);
  DefineOperand(RegEAX, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcode(0xA9, NACLi_386,
               InstFlag(Opcode64Only) | InstFlag(OperandSize_o) |
               InstFlag(OpcodeHasImmed_v),
               InstTest);
  DefineOperand(RegRAX, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcode(0xaa, NACLi_386R,
               InstFlag(OperandSize_b),
               InstStosb);
  DefineOperand(RegES_EDI, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegAL, OpFlag(OpUse) | OpFlag(OpImplicit));

  DefineOpcode(0xab, NACLi_386R,
               InstFlag(OperandSize_w),
               InstStosw);
  DefineOperand(RegES_EDI, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegAX, OpFlag(OpUse) | OpFlag(OpImplicit));

  DefineOpcode(0xab, NACLi_386R,
               InstFlag(OperandSize_v),
               InstStosd);
  DefineOperand(RegES_EDI, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegEAX, OpFlag(OpUse) | OpFlag(OpImplicit));

  DefineOpcode(0xab, NACLi_386R,
               InstFlag(Opcode64Only) | InstFlag(OperandSize_o) |
               InstFlag(OpcodeUsesRexW),
               InstStosq);
  DefineOperand(RegES_EDI, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegRAX, OpFlag(OpUse) | OpFlag(OpImplicit));

  /* ISE reviewers suggested omitting lods */
  DefineOpcode(0xac, NACLi_ILLEGAL,
               InstFlag(OperandSize_b),
               InstLodsb);
  DefineOperand(RegAL, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegDS_EDI, OpFlag(OpUse) | OpFlag(OpImplicit));

  DefineOpcode(0xad, NACLi_ILLEGAL,
               InstFlag(OperandSize_w),
               InstLodsw);
  DefineOperand(RegAX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegDS_EDI, OpFlag(OpUse) | OpFlag(OpImplicit));

  DefineOpcode(0xad, NACLi_ILLEGAL,
               InstFlag(OperandSize_v),
               InstLodsd);
  DefineOperand(RegEAX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegDS_EDI, OpFlag(OpUse) | OpFlag(OpImplicit));

  DefineOpcode(0xad, NACLi_ILLEGAL,
               InstFlag(Opcode64Only) | InstFlag(OperandSize_o) |
               InstFlag(OpcodeUsesRexW),
               InstLodsq);
  DefineOperand(RegRAX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegDS_EDI, OpFlag(OpUse) | OpFlag(OpImplicit));

  /* ISE reviewers suggested omitting scas */
  DefineOpcode(0xae, NACLi_386RE,
               InstFlag(OperandSize_b),
               InstScasb);
  DefineOperand(RegAL, OpFlag(OpUse) | OpFlag(OpImplicit));
  DefineOperand(RegDS_EDI, OpFlag(OpUse) | OpFlag(OpImplicit));

  DefineOpcode(0xaf, NACLi_386RE,
               InstFlag(OperandSize_w),
               InstScasw);
  DefineOperand(RegAX, OpFlag(OpUse) | OpFlag(OpImplicit));
  DefineOperand(RegDS_EDI, OpFlag(OpUse) | OpFlag(OpImplicit));

  DefineOpcode(0xaf, NACLi_386RE,
               InstFlag(OperandSize_v),
               InstScasd);
  DefineOperand(RegEAX, OpFlag(OpUse) | OpFlag(OpImplicit));
  DefineOperand(RegDS_EDI, OpFlag(OpUse) | OpFlag(OpImplicit));

  DefineOpcode(0xaf, NACLi_386RE,
               InstFlag(Opcode64Only) | InstFlag(OperandSize_o) |
               InstFlag(OpcodeUsesRexW),
               InstScasq);
  DefineOperand(RegRAX, OpFlag(OpUse) | OpFlag(OpImplicit));
  DefineOperand(RegDS_EDI, OpFlag(OpUse) | OpFlag(OpImplicit));

  for (i = 0; i < 8; ++i) {
    DefineOpcode(0xB8 + i, NACLi_386,
                 InstFlag(OpcodePlusR) | InstFlag(OperandSize_v) |
                 InstFlag(OpcodeHasImmed),
                 InstMov);
    DefineOperand(OpcodeBaseMinus0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(G_OpcodeBase, OpFlag(OpSet) | OpFlag(OperandZeroExtends_v));
    DefineOperand(I_Operand, OpFlag(OpUse));

    DefineOpcode(0xB8 + i, NACLi_386,
                 InstFlag(OpcodePlusR) | InstFlag(OperandSize_w) |
                 InstFlag(OpcodeHasImmed),
                 InstMov);
    DefineOperand(OpcodeBaseMinus0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(G_OpcodeBase, OpFlag(OpSet));
    DefineOperand(I_Operand, OpFlag(OpUse));

    DefineOpcode(0xB8 + i, NACLi_386,
                 InstFlag(OpcodePlusR) | InstFlag(Opcode64Only) |
                 InstFlag(OperandSize_o) | InstFlag(OpcodeHasImmed) |
                 InstFlag(OpcodeUsesRexW),
                 InstMov);
    DefineOperand(OpcodeBaseMinus0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(G_OpcodeBase, OpFlag(OpSet));
    DefineOperand(I_Operand, OpFlag(OpUse));
  }

  /* 0xC0 and 0xC1 defined by DefineGroup2OpcodesInModRm */

  DefineOpcode(0xc3, NACLi_RETURN, 0, InstRet);
  DefineOperand(RegREIP, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegRESP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));

  /* GROUP 11 */
  DefineOpcode(0xC6, NACLi_386,
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_b) |
               InstFlag(OpcodeRex) | InstFlag(OpcodeHasImmed),
               InstMov);
  DefineOperand(Opcode0, OpFlag(OperandExtendsOpcode));
  DefineOperand(E_Operand, OpFlag(OpSet));
  DefineOperand(I_Operand, OpFlag(OpUse));

  /* GROUP 11 */
  DefineOpcode(0xC7, NACLi_386,
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_w) |
               InstFlag(OpcodeHasImmed),
               InstMov);
  DefineOperand(Opcode0, OpFlag(OperandExtendsOpcode));
  DefineOperand(E_Operand, OpFlag(OpSet));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcode(0xC7, NACLi_386,
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_v) |
               InstFlag(OpcodeHasImmed),
               InstMov);
  DefineOperand(Opcode0, OpFlag(OperandExtendsOpcode));
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OperandZeroExtends_v));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcode(0xC7, NACLi_386,
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_o) |
               InstFlag(Opcode64Only) | InstFlag(OpcodeUsesRexW) |
               InstFlag(OpcodeHasImmed_v),
               InstMov);
  DefineOperand(Opcode0, OpFlag(OperandExtendsOpcode));
  DefineOperand(E_Operand, OpFlag(OpSet));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcode(0xc9, NACLi_386, 0, InstLeave);
  DefineOperand(RegREBP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegRESP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));

  /* Group 2 - 0xC0, 0xC1, 0xD0, 0XD1, 0xD2, 0xD3 */
  DefineGroup2OpcodesInModRm();

  /* Maps for escape instruction opcodes (x87 FP) when the ModR/M byte
   * is within the range of 00H-BFH. The maps are grouped by the first
   * byte of the opcode, from D8-DF. */
  DefineModRmOpcodesForX87Inst();

  DefineOpcode(0xDD,
               NACLi_X87,
               InstFlag(OpcodeInModRm),
               InstFrstor);
  DefineOperand(Opcode0 + 4, OpFlag(OperandExtendsOpcode));
  DefineOperand(E_Operand, OpFlag(OpUse) | OpFlag(OpAddress));

  /* ISE reviewers suggested making loopne, loope, loop, jcxz illegal */
  DefineJump8Opcode(0xE0, InstJcxz);

  DefineOpcode(0xe8, NACLi_JMPZ,
               InstFlag(OperandSize_w) | InstFlag(OpcodeHasImmed) |
               InstFlag(Opcode32Only),
               InstCall);
  DefineOperand(RegEIP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegESP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(J_Operand, OpFlag(OpUse) | OpFlag(OperandNear) |
                OpFlag(OperandRelative));

  /* Not supported */
  DefineOpcode(0xe8, NACLi_ILLEGAL,
               InstFlag(OperandSize_w) | InstFlag(OpcodeHasImmed) |
               InstFlag(Opcode64Only),
               InstCall);
  DefineOperand(RegRIP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegRSP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(J_Operand, OpFlag(OpUse) | OpFlag(OperandNear) |
                OpFlag(OperandRelative));

  DefineOpcode(0xe8, NACLi_JMPZ,
               InstFlag(OperandSize_v) | InstFlag(OpcodeHasImmed),
               InstCall);
  DefineOperand(RegREIP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegRESP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(J_Operand, OpFlag(OpUse) | OpFlag(OperandNear) |
                OpFlag(OperandRelative));

  DefineOpcode(0xe9, NACLi_JMPZ,
               InstFlag(Opcode32Only) | InstFlag(OpcodeHasImmed) |
               InstFlag(OperandSize_w),
               InstJmp);
  DefineOperand(RegEIP, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(J_Operand, OpFlag(OpUse) | OpFlag(OperandNear) |
                OpFlag(OperandRelative));

  DefineOpcode(0xe9, NACLi_JMPZ,
               InstFlag(OpcodeHasImmed) | InstFlag(OperandSize_v),
               InstJmp);
  DefineOperand(RegREIP, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(J_Operand, OpFlag(OpUse) | OpFlag(OperandNear) |
                OpFlag(OperandRelative));

  DefineOpcode(0xe9, NACLi_JMPZ,
               InstFlag(OpcodeHasImmed) | InstFlag(OperandSize_w),
               InstJmp);
  DefineOperand(RegREIP, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(J_Operand, OpFlag(OpUse) | OpFlag(OperandNear) |
                OpFlag(OperandRelative));

  DefineOpcode(0xeb, NACLi_JMP8,
               InstFlag(OpcodeHasImmed) | InstFlag(OperandSize_b),
               InstJmp);
  DefineOperand(RegREIP, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(J_Operand, OpFlag(OpUse) | OpFlag(OperandNear) |
                OpFlag(OperandRelative));

  DefineOpcode(0xf4, NACLi_386, 0, InstHlt);

  /* Group3 opcode. */
  DefineOpcode(0xF6, NACLi_386,
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_b) |
               InstFlag(OpcodeHasImmed) | InstFlag(OpcodeRex),
               InstTest);
  DefineOperand(Opcode0, OpFlag(OperandExtendsOpcode));
  DefineOperand(E_Operand, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcode(0xF7, NACLi_386,
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_w) |
               InstFlag(OpcodeHasImmed),
               InstTest);
  DefineOperand(Opcode0, OpFlag(OperandExtendsOpcode));
  DefineOperand(E_Operand, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcode(0xF7, NACLi_386,
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_v) |
               InstFlag(OpcodeHasImmed),
               InstTest);
  DefineOperand(Opcode0, OpFlag(OperandExtendsOpcode));
  DefineOperand(E_Operand, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcode(0xF7, NACLi_386,
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_o) |
               InstFlag(OpcodeHasImmed_v) | InstFlag(Opcode64Only),
               InstTest);
  DefineOperand(Opcode0, OpFlag(OperandExtendsOpcode));
  DefineOperand(E_Operand, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  /* Group5 opcode. */
  DefineOpcode(0xff, NACLi_INDIRECT,
               InstFlag(OpcodeInModRm) | InstFlag(Opcode32Only) |
               InstFlag(OperandSize_w),
               InstCall);
  DefineOperand(Opcode2, OpFlag(OperandExtendsOpcode));
  DefineOperand(RegEIP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegESP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(E_Operand, OpFlag(OpUse) | OpFlag(OperandNear));

  DefineOpcode(0xff, NACLi_INDIRECT,
               InstFlag(OpcodeInModRm) | InstFlag(Opcode32Only) |
               InstFlag(OperandSize_v),
               InstCall);
  DefineOperand(Opcode2, OpFlag(OperandExtendsOpcode));
  DefineOperand(RegEIP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegESP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(E_Operand, OpFlag(OpUse) | OpFlag(OperandNear));

  DefineOpcode(0xff, NACLi_INDIRECT,
               InstFlag(OpcodeInModRm) | InstFlag(Opcode64Only) |
               InstFlag(OperandSize_o) | InstFlag(OperandSizeForce64),
               InstCall);
  DefineOperand(Opcode2, OpFlag(OperandExtendsOpcode));
  DefineOperand(RegRIP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegRSP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(E_Operand, OpFlag(OpUse) | OpFlag(OperandNear));

  DefineOpcode(0xff, NACLi_INDIRECT,
               InstFlag(OpcodeInModRm) | InstFlag(Opcode32Only) |
               InstFlag(OperandSize_w),
               InstJmp);
  DefineOperand(Opcode4, OpFlag(OperandExtendsOpcode));
  DefineOperand(RegEIP, OpFlag(OpSet) | OpImplicit);
  DefineOperand(E_Operand, OpFlag(OpUse) | OpFlag(OperandNear));

  DefineOpcode(0xff, NACLi_INDIRECT,
               InstFlag(OpcodeInModRm) | InstFlag(Opcode32Only) |
               InstFlag(OperandSize_v),
               InstJmp);
  DefineOperand(Opcode4, OpFlag(OperandExtendsOpcode));
  DefineOperand(RegEIP, OpFlag(OpSet) | OpImplicit);
  DefineOperand(E_Operand, OpFlag(OpUse) | OpFlag(OperandNear));

  DefineOpcode(0xff, NACLi_INDIRECT,
               InstFlag(OpcodeInModRm) | InstFlag(Opcode64Only) |
               InstFlag(OperandSize_o) | InstFlag(OperandSizeForce64),
               InstJmp);
  DefineOperand(Opcode4, OpFlag(OperandExtendsOpcode));
  DefineOperand(RegRIP, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(E_Operand, OpFlag(OpUse) | OpFlag(OperandNear));

  DefineOpcode(0xff, NACLi_ILLEGAL,
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_w),
               InstJmp);
  DefineOperand(Opcode5, OpFlag(OperandExtendsOpcode));
  DefineOperand(RegREIP, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(Mw_Operand, OpFlag(OpUse));

  DefineOpcode(0xff, NACLi_ILLEGAL,
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_v),
               InstJmp);
  DefineOperand(Opcode5, OpFlag(OperandExtendsOpcode));
  DefineOperand(RegREIP, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(Mw_Operand, OpFlag(OpUse));

  DefineOpcode(0xff, NACLi_ILLEGAL,
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_w) |
               InstFlag(Opcode64Only) | InstFlag(OpcodeUsesRexW),
               InstJmp);
  DefineOperand(Opcode5, OpFlag(OperandExtendsOpcode));
  DefineOperand(RegREIP, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(Mw_Operand, OpFlag(OpUse));

  DefineOpcode(0xff, NACLi_386,
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_w),
               InstPush);
  DefineOperand(Opcode6, OpFlag(OperandExtendsOpcode));
  DefineOperand(RegRESP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(E_Operand, OpFlag(OpSet));

  DefineOpcode(0xff, NACLi_386,
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_v) |
               InstFlag(Opcode32Only),
               InstPush);
  DefineOperand(Opcode6, OpFlag(OperandExtendsOpcode));
  DefineOperand(RegESP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(E_Operand, OpFlag(OpSet));

  DefineOpcode(0xff, NACLi_386,
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_o) |
               InstFlag(Opcode64Only) | InstFlag(OperandSizeDefaultIs64),
               InstPush);
  DefineOperand(Opcode6, OpFlag(OperandExtendsOpcode));
  DefineOperand(RegRSP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(E_Operand, OpFlag(OpSet));

}
