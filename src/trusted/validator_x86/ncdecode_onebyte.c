/* Copyright (c) 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Defines the one byte x86 opcodes.
 */

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/trusted/validator_x86/ncdecode_tablegen.h"

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

static const InstMnemonic Group1CompName[1] = {
  InstCmp
};

static Bool IsGroup1CompName(InstMnemonic name) {
  int i;
  for (i = 0; i < NACL_ARRAY_SIZE(Group1CompName); ++i) {
    if (name == Group1CompName[i]) return TRUE;
  }
  return FALSE;
}

static OperandFlags GetGroup1DestOpUsage(InstMnemonic name) {
  return (IsGroup1CompName(name)
          ? InstFlag(OpUse)
          : (InstFlag(OpSet) | InstFlag(OpUse)));
}

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
  OperandFlags dest_op_usage = GetGroup1DestOpUsage(name);
  DefineOpcode(
      base,
      itype,
      InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeRex) |
      InstFlag(OperandSize_b) | extra_flags,
      name);
  DefineOperand(E_Operand, dest_op_usage);
  DefineOperand(G_Operand, OpFlag(OpUse));

  DefineOpcodeChoices_32_64(base+1, 1, 2);
  DefineOpcode(
      base+1,
      itype,
      InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_v) |
      InstFlag(OperandSize_w) | extra_flags,
      name);
  DefineOperand(E_Operand, dest_op_usage | OpFlag(OperandZeroExtends_v));
  DefineOperand(G_Operand, OpFlag(OpUse));

  DefineOpcode(
      base+1,
      itype,
      InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeUsesRexW) |
      InstFlag(OperandSize_o) | InstFlag(Opcode64Only) | extra_flags,
      name);
  DefineOperand(E_Operand, dest_op_usage);
  DefineOperand(G_Operand, OpFlag(OpUse));

  DefineOpcode(
      base+2,
      itype,
      InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeRex) |
      InstFlag(OperandSize_b) | extra_flags,
      name);
  DefineOperand(G_Operand, dest_op_usage);
  DefineOperand(E_Operand, OpFlag(OpUse));

  DefineOpcodeChoices_32_64(base+3, 1, 2);
  DefineOpcode(
      base+3,
      itype,
      InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_w) |
      InstFlag(OperandSize_v) | extra_flags,
      name);
  DefineOperand(G_Operand, dest_op_usage | OpFlag(OperandZeroExtends_v));
  DefineOperand(E_Operand, OpFlag(OpUse));

  DefineOpcode(
      base+3,
      itype,
      InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeUsesRexW) |
      InstFlag(OperandSize_o) | InstFlag(Opcode64Only) | extra_flags,
      name);
  DefineOperand(G_Operand, dest_op_usage);
  DefineOperand(E_Operand, OpFlag(OpUse));

  DefineOpcode(
      base+4,
      itype,
      InstFlag(OperandSize_b) | InstFlag(OpcodeHasImmed) | extra_flags,
      name);
  DefineOperand(RegAL, dest_op_usage);
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodeChoices_32_64(base+5, 2, 3);
  DefineOpcode(
      base+5,
      itype,
      InstFlag(OpcodeHasImmed) | InstFlag(OperandSize_v) | extra_flags,
      name);
  DefineOperand(RegEAX, dest_op_usage | OpFlag(OperandZeroExtends_v));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcode(
      base+5,
      itype,
      InstFlag(OpcodeHasImmed_v) | InstFlag(OpcodeUsesRexW) |
      InstFlag(OperandSize_o) | InstFlag(Opcode64Only) | extra_flags,
      name);
  DefineOperand(RegRAX, dest_op_usage);
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcode(
      base+5,
      itype,
      InstFlag(OperandSize_w) | InstFlag(OpcodeHasImmed) | extra_flags,
      name);
  DefineOperand(RegAX, dest_op_usage);
  DefineOperand(I_Operand, OpFlag(OpUse));
}

/* Define the Xchg operator where the register is embedded in the opcode */
static void DefineXchgRegister() {
  /* Note: xchg is commutative, so order of operands is unimportant. */
  int i;
  /* Note: skip 0, implies a nop */
  for (i = 1; i < 8; ++i) {
    DefineOpcodeChoices(0x90 + i, 2);
    DefineOpcode(0x90 + i, NACLi_386L,
                 InstFlag(OpcodePlusR) | InstFlag(OperandSize_w) |
                 InstFlag(OperandSize_v) | InstFlag(OpcodeLockable),
                 InstXchg);
    DefineOperand(OpcodeBaseMinus0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(G_OpcodeBase, OpFlag(OpSet) | OpFlag(OpUse));
    DefineOperand(RegREAX, OpFlag(OpSet) | OpFlag(OpUse));

    DefineOpcode(0x90 + i, NACLi_386L,
                 InstFlag(OpcodePlusR) | InstFlag(OperandSize_o) |
                 InstFlag(OpcodeUsesRexW) | InstFlag(OpcodeLockable),
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
        InstFlag(OperandSize_w) | InstFlag(OperandSize_v) |
        InstFlag(OpcodeLockable),
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
  OperandFlags op_access = (name == InstPush ? OpFlag(OpUse) : OpFlag(OpSet));
  for (i = 0; i < 8; ++i) {
    DefineOpcodeChoices(base + i, 2);
    DefineOpcode(
        base + i,
        NACLi_386,
        InstFlag(OpcodePlusR) | InstFlag(OperandSize_w),
        name);
    DefineOperand(OpcodeBaseMinus0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(RegRESP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
    DefineOperand(G_OpcodeBase, op_access);

    DefineOpcode(
        base + i,
        NACLi_386,
        InstFlag(OpcodePlusR) | InstFlag(OperandSize_v) |
        InstFlag(Opcode32Only),
        name);
    DefineOperand(OpcodeBaseMinus0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(RegESP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
    DefineOperand(G_OpcodeBase, op_access);

    DefineOpcode(
        base + i,
        NACLi_386,
        InstFlag(OpcodePlusR) | InstFlag(OperandSize_o) |
        InstFlag(Opcode64Only) | InstFlag(OperandSizeDefaultIs64),
        name);
    DefineOperand(OpcodeBaseMinus0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(RegRSP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
    DefineOperand(G_OpcodeBase, op_access);
  }
}

static void DefineGroup1OpcodesInModRm() {
  int i;
  for (i = 0; i < NACL_ARRAY_SIZE(Group1OpcodeName); ++i) {
    OperandFlags dest_op_usage = GetGroup1DestOpUsage(Group1OpcodeName[i]);
    DefineOpcode(
        0x80,
        NACLi_386L,
        InstFlag(OpcodeInModRm) | InstFlag(OperandSize_b) |
        InstFlag(OpcodeHasImmed) |
        InstFlag(OpcodeLockable) | InstFlag(OpcodeRex),
        Group1OpcodeName[i]);
    DefineOperand(Opcode0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(E_Operand, dest_op_usage);
    DefineOperand(I_Operand, OpFlag(OpUse));

    DefineOpcodeMrmChoices_32_64(0x81, Opcode0 + i, 2, 3);
    DefineOpcode(
        0x81,
        NACLi_386L,
        InstFlag(OpcodeInModRm) | InstFlag(OperandSize_w) |
        InstFlag(OpcodeHasImmed) | InstFlag(OpcodeLockable),
        Group1OpcodeName[i]);
    DefineOperand(Opcode0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(E_Operand, dest_op_usage);
    DefineOperand(I_Operand, OpFlag(OpUse));

    DefineOpcode(
        0x81,
        NACLi_386L,
        InstFlag(OpcodeInModRm) | InstFlag(OperandSize_v) |
        InstFlag(OpcodeHasImmed) | InstFlag(OpcodeLockable),
        Group1OpcodeName[i]);
    DefineOperand(Opcode0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(E_Operand, dest_op_usage | OpFlag(OperandZeroExtends_v));
    DefineOperand(I_Operand, OpFlag(OpUse));

    DefineOpcode(
        0x81,
        NACLi_386L,
        InstFlag(OpcodeInModRm) | InstFlag(Opcode64Only) |
        InstFlag(OpcodeUsesRexW) | InstFlag(OperandSize_o) |
        InstFlag(OpcodeHasImmed_v) | InstFlag(OpcodeLockable),
        Group1OpcodeName[i]);
    DefineOperand(Opcode0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(E_Operand, dest_op_usage);
    DefineOperand(I_Operand, OpFlag(OpUse));

    DefineOpcodeMrmChoices_32_64(0x83, Opcode0 + i, 2, 3);
    DefineOpcode(
        0x83,
        NACLi_386L,
        InstFlag(OpcodeInModRm) | InstFlag(OperandSize_w) |
        InstFlag(OpcodeHasImmed_b) | InstFlag(OpcodeLockable),
        Group1OpcodeName[i]);
    DefineOperand(Opcode0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(E_Operand, dest_op_usage);
    DefineOperand(I_Operand, OpFlag(OpUse));

    DefineOpcode(
        0x83,
        NACLi_386L,
        InstFlag(OpcodeInModRm) | InstFlag(OperandSize_v) |
        InstFlag(OpcodeHasImmed_b) | InstFlag(OpcodeLockable),
        Group1OpcodeName[i]);
    DefineOperand(Opcode0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(E_Operand, dest_op_usage | OpFlag(OperandZeroExtends_v));
    DefineOperand(I_Operand, OpFlag(OpUse));

    DefineOpcode(
        0x83,
        NACLi_386L,
        InstFlag(OpcodeInModRm) | InstFlag(OperandSize_o) |
        InstFlag(Opcode64Only) | InstFlag(OpcodeUsesRexW) |
        InstFlag(OpcodeHasImmed_b) | InstFlag(OpcodeLockable),
        Group1OpcodeName[i]);
    DefineOperand(Opcode0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(E_Operand, dest_op_usage);
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

static void DefineGroup2OpcodesInModRm() {
  int i;
  for (i = 0; i < NACL_ARRAY_SIZE(Group2OpcodeName); i++) {
    if (Group2OpcodeName[i] == InstMnemonicEnumSize) continue;
    DefineOpcode(0xC0, NACLi_OPINMRM,
                 InstFlag(OpcodeInModRm) | InstFlag(OperandSize_b) |
                 InstFlag(OpcodeHasImmed) | InstFlag(OpcodeRex),
                 Group2OpcodeName[i]);
    DefineOperand(Opcode0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
    DefineOperand(I_Operand, 0);

    DefineOpcodeMrmChoices_32_64(0xc1, Opcode0 + i, 1, 2);
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

    DefineOpcodeMrmChoices_32_64(0xd1, Opcode0 + i, 1, 2);
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

    DefineOpcodeMrmChoices_32_64(0xd3, Opcode0 + i, 1, 2);
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
  DefineOperand(RegAL, OpFlag(OpSet) | OpFlag(OpUse) | OpFlag(OpImplicit));

  BuildBinaryOps_00_05(0x38, NACLi_386, InstCmp, 0);

  /* Ox3e is segment ds prefix */

  DefineOpcode(0x3f, NACLi_ILLEGAL, InstFlag(Opcode32Only), InstAas);

  DefineIncOrDec_00_07(0x40, InstInc);
  DefineIncOrDec_00_07(0x48, InstDec);
  DefinePushOrPop_00_07(0x50, InstPush);
  DefinePushOrPop_00_07(0x58, InstPop);

  /* Note: pushd shares opcode (uses different operand size). */
  DefineOpcodeChoices_32_64(0x60, 2, 0);
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

  DefineOpcodeChoices_32_64(0x61, 2, 0);
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

  DefineOpcodeChoices_32_64(0x63, 1, 2);
  DefineOpcode(0x63, NACLi_SYSTEM,
               InstFlag(Opcode32Only) | InstFlag(OperandSize_w) |
               InstFlag(OpcodeUsesModRm),
               InstArpl);
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(G_Operand, OpFlag(OpUse));


  DefineOpcode(0x63, NACLi_386,
               InstFlag(Opcode64Only) | InstFlag(OperandSize_o) |
               InstFlag(OpcodeUsesRexW) | InstFlag(OpcodeUsesModRm),
               InstMovsxd);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(Ev_Operand, OpFlag(OpUse));

  /* NOTE: this form of movsxd should be discouraged. */
  DefineOpcode(0x63, /* NACLi_SYSTEM, */ NACLi_ILLEGAL,
               InstFlag(Opcode64Only) | InstFlag(OperandSize_v) |
               InstFlag(OpcodeUsesModRm),
               InstMovsxd);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(E_Operand, OpFlag(OpUse));

  DefineOpcodeChoices_32_64(0x68, 1, 2);
  DefineOpcode(0x68, NACLi_386,
               InstFlag(OperandSize_w) | InstFlag(OpcodeHasImmed),
               InstPush);
  DefineOperand(RegRESP, OpFlag(OpImplicit) | OpFlag(OpUse) | OpFlag(OpSet));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcode(0x68, NACLi_386,
               InstFlag(OperandSize_o) | InstFlag(OpcodeHasImmed_v) |
               InstFlag(OperandSizeDefaultIs64) | InstFlag(Opcode64Only),
               InstPush);
  DefineOperand(RegRESP, OpFlag(OpImplicit) | OpFlag(OpUse) | OpFlag(OpSet));
  DefineOperand(I_Operand, OpFlag(OpUse));

  /* NOTE: The two argument form appears to be the same as the three
   * argument form, where the first argument is duplicated.
   */
  DefineOpcodeChoices_32_64(0x69, 1, 2);
  DefineOpcode(0x69, NACLi_386,
               InstFlag(OperandSize_w) | InstFlag(OperandSize_v) |
               InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeHasImmed),
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

  /* NOTE: The two argument form appears to be the same as the three
   * argument form, where the first argument is duplicated.
   */
  DefineOpcodeChoices_32_64(0x6b, 1, 2);
  DefineOpcode(0x6b, NACLi_386,
               InstFlag(OperandSize_w) | InstFlag(OperandSize_v) |
               InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeHasImmed_b),
               InstImul);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(E_Operand, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcode(0x6b, NACLi_386,
               InstFlag(Opcode64Only) | InstFlag(OperandSize_o) |
               InstFlag(OpcodeUsesRexW) |
               InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeHasImmed_b),
               InstImul);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(E_Operand, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcode(0x6c, NACLi_ILLEGAL, InstFlag(OperandSize_b), InstInsb);
  DefineOperand(RegES_EDI, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegDX, OpFlag(OpUse) | OpFlag(OpImplicit));

  DefineOpcodeChoices(0x6D, 2);
  DefineOpcode(0x6D, NACLi_ILLEGAL, InstFlag(OperandSize_w), InstInsw);
  DefineOperand(RegES_EDI, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegDX, OpFlag(OpUse) | OpFlag(OpImplicit));

  DefineOpcode(0x6D, NACLi_ILLEGAL, InstFlag(OperandSize_v), InstInsd);
  DefineOperand(RegES_EDI, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegDX, OpFlag(OpUse) | OpFlag(OpImplicit));

  DefineOpcode(0x6E, NACLi_ILLEGAL, InstFlag(OperandSize_b), InstOutsb);
  DefineOperand(RegDX, OpFlag(OpUse) | OpFlag(OpImplicit));
  DefineOperand(RegES_EDI, OpFlag(OpUse) | OpFlag(OpImplicit));

  DefineOpcodeChoices(0x6F, 2);
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

  DefineOpcodeChoices_32_64(0x85, 1, 2);
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
               InstFlag(Opcode32Only) | InstFlag(OpcodeLockable),
               InstXchg);
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(G_Operand, OpFlag(OpSet) | OpFlag(OpUse));

  DefineOpcode(0x86, NACLi_386L,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_b) |
               InstFlag(OpcodeRex) | InstFlag(Opcode64Only) |
               InstFlag(OpcodeLockable),
               InstXchg);
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(G_Operand, OpFlag(OpSet) | OpFlag(OpUse));

  DefineOpcodeChoices_32_64(0x87, 1, 2);
  DefineOpcode(0x87, NACLi_386L,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_w) |
               InstFlag(OperandSize_v) | InstFlag(OpcodeLockable),
               InstXchg);
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(G_Operand, OpFlag(OpSet) | OpFlag(OpUse));

  DefineOpcode(0x87, NACLi_386L,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_o) |
               InstFlag(Opcode64Only) | InstFlag(OpcodeUsesRexW) |
               InstFlag(OpcodeLockable),
               InstXchg);
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(G_Operand, OpFlag(OpSet) | OpFlag(OpUse));

  DefineOpcode(0x88, NACLi_386,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_b) |
               InstFlag(OpcodeRex),
               InstMov);
  DefineOperand(E_Operand, OpFlag(OpSet));
  DefineOperand(G_Operand, OpFlag(OpUse));

  DefineOpcodeChoices_32_64(0x89, 1, 2);
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

  DefineOpcodeChoices_32_64(0x8b, 1, 2);
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
  DefineOpcodeChoices_32_64(0x8c, 1, 2);
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

  DefineOpcodeChoices_32_64(0x8d, 1, 2);
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
  DefineOpcodeChoices_32_64(0x8e, 1, 2);
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

  DefineOpcodeMrmChoices(0x8f, Opcode0, 2);
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

  DefineOpcodeChoices_32_64(0x98, 2, 3);
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

  DefineOpcodeChoices_32_64(0x99, 2, 3);
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

  DefineOpcodeChoices_32_64(0x9a, 2, 0);
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

  DefineOpcodeChoices(0x9c, 2);
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

  DefineOpcodeChoices(0x9d, 2);
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

  DefineOpcodeChoices_32_64(0xa1, 1, 2);
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

  DefineOpcodeChoices_32_64(0xa3, 1, 2);
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

  DefineOpcodeChoices_32_64(0xa5, 2, 3);
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

  DefineOpcodeChoices_32_64(0xa7, 2, 3);
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

  DefineOpcodeChoices_32_64(0xA9, 2, 3);
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

  DefineOpcodeChoices_32_64(0xab, 2, 3);
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

  DefineOpcodeChoices_32_64(0xad, 2, 3);
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

  DefineOpcodeChoices_32_64(0xaf, 2, 3);
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
    DefineOpcodeChoices_32_64(0xB0 + i, 1, 2);
    DefineOpcode(0xB0 + i, NACLi_386,
                 InstFlag(Opcode64Only) | InstFlag(OpcodePlusR) |
                 InstFlag(OperandSize_b) | InstFlag(OpcodeHasImmed) |
                 InstFlag(OpcodeRex),
                 InstMov);
    DefineOperand(OpcodeBaseMinus0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(G_OpcodeBase, OpFlag(OpSet));
    DefineOperand(I_Operand, OpFlag(OpUse));

    DefineOpcode(0xB0 + i, NACLi_386,
                 InstFlag(OpcodePlusR) | InstFlag(OperandSize_b) |
                 InstFlag(OpcodeHasImmed),
                 InstMov);
    DefineOperand(OpcodeBaseMinus0 + i, OpFlag(OperandExtendsOpcode));
    DefineOperand(G_OpcodeBase, OpFlag(OpSet));
    DefineOperand(I_Operand, OpFlag(OpUse));
  }

  for (i = 0; i < 8; ++i) {
    DefineOpcodeChoices_32_64(0xB8 + i, 2, 3);
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

  DefineOpcode(0xc2, NACLi_RETURN, InstFlag(OpcodeHasImmed_w), InstRet);
  DefineOperand(RegREIP, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegRESP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(I_Operand, OpFlag(OpUse) | OpFlag(OperandNear));

  DefineOpcode(0xc3, NACLi_RETURN, 0, InstRet);
  DefineOperand(RegREIP, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegRESP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));

  DefineOpcodeChoices_32_64(0xc4, 2, 0);
  DefineOpcode(0xc4, NACLi_ILLEGAL,
               InstFlag(Opcode32Only) | InstFlag(OperandSize_w) |
               InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeHasImmed_v),
               InstLes);
  DefineOperand(ES_G_Operand, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(G_Operand, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcode(0xc4, NACLi_ILLEGAL,
               InstFlag(Opcode32Only) | InstFlag(OperandSize_v) |
               InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeHasImmed_p),
               InstLes);
  DefineOperand(ES_G_Operand, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(G_Operand, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodeChoices_32_64(0xc5, 2, 0);
  DefineOpcode(0xc5, NACLi_ILLEGAL,
               InstFlag(Opcode32Only) | InstFlag(OperandSize_w) |
               InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeHasImmed_v),
               InstLds);
  DefineOperand(G_Operand, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcode(0xc5, NACLi_ILLEGAL,
               InstFlag(Opcode32Only) | InstFlag(OperandSize_v) |
               InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeHasImmed_p),
               InstLes);
  DefineOperand(ES_G_Operand, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(G_Operand, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  /* GROUP 11 */
  DefineOpcode(0xC6, NACLi_386,
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_b) |
               InstFlag(OpcodeRex) | InstFlag(OpcodeHasImmed),
               InstMov);
  DefineOperand(Opcode0, OpFlag(OperandExtendsOpcode));
  DefineOperand(E_Operand, OpFlag(OpSet));
  DefineOperand(I_Operand, OpFlag(OpUse));

  /* GROUP 11 */
  DefineOpcodeMrmChoices_32_64(0xc7, Opcode0, 2, 3);
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

  DefineOpcode(0xC8, NACLi_ILLEGAL,
               InstFlag(OpcodeHasImmed_w) | InstFlag(OpcodeHasImmed2_b),
               InstEnter);
  DefineOperand(I_Operand, OpFlag(OpUse));
  DefineOperand(I2_Operand, OpFlag(OpUse));

  DefineOpcode(0xc9, NACLi_386, 0, InstLeave);
  DefineOperand(RegREBP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegRESP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));

  DefineOpcode(0xca, NACLi_RETURN, InstFlag(OpcodeHasImmed_w), InstRet);
  DefineOperand(RegREIP, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegRESP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(I_Operand, OpFlag(OpUse) | OpFlag(OperandFar));

  DefineOpcode(0xcb, NACLi_RETURN, 0, InstRet);
  DefineOperand(RegREIP, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegRESP, OpFlag(OpUse) | OpFlag(OpSet) | OpFlag(OpImplicit));

  DefineOpcode(0xcc, NACLi_ILLEGAL, 0, InstInt3);

  DefineOpcode(0xcd, NACLi_ILLEGAL,
               InstFlag(OperandSize_b) | InstFlag(OpcodeHasImmed),
               InstInt);
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcode(0xce, NACLi_ILLEGAL, InstFlag(Opcode32Only), InstInt0);

  DefineOpcode(0xcf, NACLi_SYSTEM, 0, InstIret);

  /* Group 2 - 0xC0, 0xC1, 0xD0, 0XD1, 0xD2, 0xD3 */
  DefineGroup2OpcodesInModRm();

  DefineOpcode(0xD4, NACLi_ILLEGAL,
               InstFlag(Opcode32Only) | InstFlag(OpcodeHasImmed_b),
               InstAam);
  DefineOperand(RegAX, OpFlag(OpSet) | OpFlag(OpUse) | OpFlag(OpImplicit));
  DefineOperand(I_Operand, OpFlag(OpUse) | OpFlag(OpImplicit));

  DefineOpcode(0xd5, NACLi_ILLEGAL,
               InstFlag(Opcode32Only) | InstFlag(OpcodeHasImmed_b),
               InstAad);
  DefineOperand(RegAX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegAL, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegAH, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(I_Operand, OpFlag(OpUse) | OpFlag(OpImplicit));

  /* Note: 0xdd /4 is already defined in ncdecodeX87.c */

  /* ISE reviewers suggested making loopne, loope, loop, jcxz illegal */
  DefineJump8Opcode(0xE0, InstJcxz);

  DefineOpcode(0xE4, NACLi_ILLEGAL,
               InstFlag(OpcodeHasImmed_b),
               InstIn);
  DefineOperand(RegAL, OpFlag(OpSet));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcode(0xE5, NACLi_ILLEGAL,
               InstFlag(OperandSize_w) | InstFlag(OperandSize_v) |
               InstFlag(OpcodeHasImmed_b),
               InstIn);
  DefineOperand(RegREAX, OpFlag(OpSet));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodeChoices(0xe8, 2);
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

  DefineOpcodeChoices_32_64(0xe9, 3, 2);
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

  DefineOpcode(0xec, NACLi_ILLEGAL, 0, InstIn);
  DefineOperand(RegAL, OpFlag(OpSet));
  DefineOperand(RegDX, OpFlag(OpUse));

  DefineOpcode(0xed, NACLi_ILLEGAL,
               InstFlag(OperandSize_w) | InstFlag(OperandSize_v),
               InstIn);
  DefineOperand(RegREAX, OpFlag(OpSet));
  DefineOperand(RegDX, OpFlag(OpUse));

  DefineOpcode(0xf4, NACLi_386, 0, InstHlt);

  /* Group3 opcode. */
  DefineOpcode(0xF6, NACLi_386,
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_b) |
               InstFlag(OpcodeHasImmed) | InstFlag(OpcodeRex),
               InstTest);
  DefineOperand(Opcode0, OpFlag(OperandExtendsOpcode));
  DefineOperand(E_Operand, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcode(0xF6, NACLi_386L,
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_b) |
               InstFlag(OpcodeRex) | InstFlag(OpcodeLockable),
               InstNot);
  DefineOperand(Opcode2, OpFlag(OperandExtendsOpcode));
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));

  DefineOpcode(0xF6, NACLi_386L,
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_b) |
               InstFlag(OpcodeRex) | InstFlag(OpcodeLockable),
               InstNeg);
  DefineOperand(Opcode3, OpFlag(OperandExtendsOpcode));
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));

  DefineOpcode(0xF6, NACLi_386,
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_b) |
               InstFlag(OpcodeRex),
               InstMul);
  DefineOperand(Opcode4, OpFlag(OperandExtendsOpcode));
  DefineOperand(RegAX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegAL, OpFlag(OpUse) | OpFlag(OpImplicit));
  DefineOperand(E_Operand, OpFlag(OpUse));

  DefineOpcode(0xF6, NACLi_386,
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_b),
               InstImul);
  DefineOperand(Opcode5, OpFlag(OperandExtendsOpcode));
  DefineOperand(RegAX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegAL, OpFlag(OpUse) | OpFlag(OpImplicit));
  DefineOperand(E_Operand, OpFlag(OpUse));

  DefineOpcode(0xF6, NACLi_386,
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_b) |
               InstFlag(OpcodeRex),
               InstDiv);
  DefineOperand(Opcode6, OpFlag(OperandExtendsOpcode));
  DefineOperand(RegAX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegAL, OpFlag(OpUse) | OpFlag(OpImplicit));
  DefineOperand(E_Operand, OpFlag(OpUse));

  DefineOpcode(0xF6, NACLi_386,
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_b) |
               InstFlag(OpcodeRex),
               InstIdiv);
  DefineOperand(Opcode7, OpFlag(OperandExtendsOpcode));
  DefineOperand(RegAL, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegAH, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegAX, OpFlag(OpUse) | OpFlag(OpImplicit));
  DefineOperand(E_Operand, OpFlag(OpUse));

  DefineOpcodeMrmChoices_32_64(0xf7, Opcode0, 2, 3);
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

  DefineOpcodeMrmChoices_32_64(0xf7, Opcode2, 1, 2);
  DefineOpcode(0xF7, NACLi_386L,
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_w) |
               InstFlag(OperandSize_v) | InstFlag(OpcodeLockable),
               InstNot);
  DefineOperand(Opcode2, OpFlag(OperandExtendsOpcode));
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));

  DefineOpcode(0xF7, NACLi_386L,
               InstFlag(Opcode64Only) | InstFlag(OpcodeInModRm) |
               InstFlag(OperandSize_o) | InstFlag(OpcodeUsesRexW) |
               InstFlag(OpcodeLockable),
               InstNot);
  DefineOperand(Opcode2, OpFlag(OperandExtendsOpcode));
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));

  DefineOpcodeMrmChoices_32_64(0xf7, Opcode3, 1, 2);
  DefineOpcode(0xF7, NACLi_386L,
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_w) |
               InstFlag(OperandSize_v) | InstFlag(OpcodeLockable),
               InstNeg);
  DefineOperand(Opcode3, OpFlag(OperandExtendsOpcode));
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));

  DefineOpcode(0xF7, NACLi_386L,
               InstFlag(Opcode64Only) | InstFlag(OpcodeInModRm) |
               InstFlag(OperandSize_o) | InstFlag(OpcodeUsesRexW) |
               InstFlag(OpcodeLockable),
               InstNeg);
  DefineOperand(Opcode3, OpFlag(OperandExtendsOpcode));
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));

  DefineOpcodeMrmChoices_32_64(0xF7, Opcode4, 1, 2);
  DefineOpcode(0xF7, NACLi_386,
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_w) |
               InstFlag(OperandSize_v),
               InstMul);
  DefineOperand(Opcode4, OpFlag(OperandExtendsOpcode));
  DefineOperand(SegmentDX_AX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegREAX, OpFlag(OpUse) | OpFlag(OpImplicit));
  DefineOperand(E_Operand, OpFlag(OpUse));

  DefineOpcode(0xF7, NACLi_386,
               InstFlag(Opcode64Only) | InstFlag(OpcodeInModRm) |
               InstFlag(OperandSize_o) | InstFlag(OpcodeUsesRexW),
               InstMul);
  DefineOperand(Opcode4, OpFlag(OperandExtendsOpcode));
  DefineOperand(SegmentDX_AX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegRAX, OpFlag(OpUse) | OpFlag(OpImplicit));
  DefineOperand(E_Operand, OpFlag(OpUse));

  DefineOpcodeMrmChoices_32_64(0xf7, Opcode5, 1, 2);
  DefineOpcode(0xF7, NACLi_386,
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_w) |
               InstFlag(OperandSize_v),
               InstImul);
  DefineOperand(Opcode5, OpFlag(OperandExtendsOpcode));
  DefineOperand(SegmentDX_AX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegREAX, OpFlag(OpUse) | OpFlag(OpImplicit));
  DefineOperand(E_Operand, OpFlag(OpUse));

  DefineOpcode(0xF7, NACLi_386,
               InstFlag(Opcode64Only) | InstFlag(OpcodeInModRm) |
               InstFlag(OperandSize_o),
               InstImul);
  DefineOperand(Opcode5, OpFlag(OperandExtendsOpcode));
  DefineOperand(SegmentDX_AX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegREAX, OpFlag(OpUse) | OpFlag(OpImplicit));
  DefineOperand(E_Operand, OpFlag(OpUse));

  DefineOpcodeMrmChoices_32_64(0xf7, Opcode6, 1, 2);
  DefineOpcode(0xF7, NACLi_386,
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_w) |
               InstFlag(OperandSize_v),
               InstDiv);
  DefineOperand(Opcode6, OpFlag(OperandExtendsOpcode));
  DefineOperand(RegREAX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegREDX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(SegmentDX_AX, OpFlag(OpUse) | OpFlag(OpImplicit));
  DefineOperand(E_Operand, OpFlag(OpUse));

  DefineOpcode(0xF7, NACLi_386,
               InstFlag(Opcode64Only) | InstFlag(OpcodeInModRm) |
               InstFlag(OperandSize_o) | InstFlag(OpcodeUsesRexW),
               InstDiv);
  DefineOperand(Opcode6, OpFlag(OperandExtendsOpcode));
  DefineOperand(RegRAX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegRDX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(SegmentDX_AX, OpFlag(OpUse) | OpFlag(OpImplicit));
  DefineOperand(E_Operand, OpFlag(OpUse));

  DefineOpcodeMrmChoices_32_64(0xF7, Opcode7, 1, 2);
  DefineOpcode(0xF7, NACLi_386,
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_w) |
               InstFlag(OperandSize_v),
               InstIdiv);
  DefineOperand(Opcode7, OpFlag(OperandExtendsOpcode));
  DefineOperand(RegREAX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegREDX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(SegmentDX_AX, OpFlag(OpUse) | OpFlag(OpImplicit));
  DefineOperand(E_Operand, OpFlag(OpUse));

  DefineOpcode(0xF7, NACLi_386,
               InstFlag(Opcode64Only) | InstFlag(OpcodeInModRm) |
               InstFlag(OperandSize_o) | InstFlag(OpcodeUsesRexW),
               InstIdiv);
  DefineOperand(Opcode7, OpFlag(OperandExtendsOpcode));
  DefineOperand(RegRAX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegRDX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(SegmentDX_AX, OpFlag(OpUse) | OpFlag(OpImplicit));
  DefineOperand(E_Operand, OpFlag(OpUse));

  DefineOpcode(0xFC, NACLi_386, 0, InstCld);

  DefineOpcode(0xFE, NACLi_386L,
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_b) |
               InstFlag(OpcodeLockable),
               InstInc);
  DefineOperand(Opcode0, OpFlag(OperandExtendsOpcode));
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));

  DefineOpcode(0xFE, NACLi_386L,
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_b) |
               InstFlag(OpcodeLockable),
               InstDec);
  DefineOperand(Opcode1, OpFlag(OperandExtendsOpcode));
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));

  /* Group5 opcode. */
  DefineOpcodeMrmChoices_32_64(0xff, Opcode0, 1, 2);
  DefineOpcode(0xff, NACLi_386L,
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_w) |
               InstFlag(OperandSize_v) | InstFlag(OpcodeLockable),
               InstInc);
  DefineOperand(Opcode0, OpFlag(OperandExtendsOpcode));
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));

  DefineOpcode(0xff, NACLi_386L,
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_o) |
               InstFlag(Opcode64Only) | InstFlag(OpcodeUsesRexW) |
               InstFlag(OpcodeLockable),
               InstInc);
  DefineOperand(Opcode0, OpFlag(OperandExtendsOpcode));
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));

  DefineOpcodeMrmChoices_32_64(0xff, Opcode1, 1, 2);
  DefineOpcode(0xff, NACLi_386L,
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_w) |
               InstFlag(OperandSize_v) | InstFlag(OpcodeLockable),
               InstDec);
  DefineOperand(Opcode1, OpFlag(OperandExtendsOpcode));
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));

  DefineOpcode(0xff, NACLi_386L,
               InstFlag(OpcodeInModRm) | InstFlag(OperandSize_o) |
               InstFlag(Opcode64Only) | InstFlag(OpcodeUsesRexW) |
               InstFlag(OpcodeLockable),
               InstDec);
  DefineOperand(Opcode1, OpFlag(OperandExtendsOpcode));
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OpUse));

  DefineOpcodeMrmChoices_32_64(0xff, Opcode2, 2, 1);
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

  DefineOpcodeMrmChoices_32_64(0xff, Opcode4, 2, 1);
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

  DefineOpcodeMrmChoices_32_64(0xff, Opcode5, 2, 3);
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

  DefineOpcodeMrmChoices(0xff, Opcode6, 2);
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
