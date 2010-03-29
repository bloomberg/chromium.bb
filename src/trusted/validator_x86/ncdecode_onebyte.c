/* Copyright (c) 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Defines the one byte x86 opcodes.
 */

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/trusted/validator_x86/ncdecode_forms.h"
#include "native_client/src/trusted/validator_x86/ncdecode_tablegen.h"

static const NaClMnemonic NaClGroup1OpcodeName[8] = {
  InstAdd,
  InstOr,
  InstAdc,
  InstSbb,
  InstAnd,
  InstSub,
  InstXor,
  InstCmp
};

static const NaClMnemonic NaClGroup1CompName[1] = {
  InstCmp
};

static Bool NaClIsGroup1CompName(NaClMnemonic name) {
  int i;
  for (i = 0; i < NACL_ARRAY_SIZE(NaClGroup1CompName); ++i) {
    if (name == NaClGroup1CompName[i]) return TRUE;
  }
  return FALSE;
}

static NaClOpFlags GetGroup1OpUsage(NaClMnemonic name, int op_index) {
  return NaClGetIcatFlags((NaClIsGroup1CompName(name) ? Compare : Binary),
                          op_index);
}

/* Define binary operation XX+00 to XX+05, for the binary operators
 * add, or, adc, sbb, and, sub, xor, and cmp. Base is the value XX.
 * Name is the name of the operation. Extra flags are any additional
 * flags that are true to a specific binary operator, rather than
 * all binary operators.
 */
static void NaClBuildBinaryOps_00_05(const uint8_t base,
                                     const NaClInstType itype,
                                     const NaClMnemonic name,
                                     const NaClOpFlags extra_flags) {
  NaClOpFlags dest_op_usage = GetGroup1OpUsage(name, 1);
  NaClOpFlags src_op_usage = GetGroup1OpUsage(name, 2);
  NaClDefInst(
      base,
      itype,
      NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeRex) |
      NACL_IFLAG(OperandSize_b) | extra_flags,
      name);
  NaClDefOp(E_Operand, dest_op_usage);
  NaClDefOp(G_Operand, src_op_usage);

  NaClDefInstChoices_32_64(base+1, 1, 2);
  NaClDefInst(
      base+1,
      itype,
      NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_v) |
      NACL_IFLAG(OperandSize_w) | extra_flags,
      name);
  NaClDefOp(E_Operand, dest_op_usage | NACL_OPFLAG(OperandZeroExtends_v));
  NaClDefOp(G_Operand, src_op_usage);

  NaClDefInst(
      base+1,
      itype,
      NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeUsesRexW) |
      NACL_IFLAG(OperandSize_o) | NACL_IFLAG(Opcode64Only) | extra_flags,
      name);
  NaClDefOp(E_Operand, dest_op_usage);
  NaClDefOp(G_Operand, src_op_usage);

  NaClDefInst(
      base+2,
      itype,
      NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeRex) |
      NACL_IFLAG(OperandSize_b) | extra_flags,
      name);
  NaClDefOp(G_Operand, dest_op_usage);
  NaClDefOp(E_Operand, src_op_usage);

  NaClDefInstChoices_32_64(base+3, 1, 2);
  NaClDefInst(
      base+3,
      itype,
      NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_w) |
      NACL_IFLAG(OperandSize_v) | extra_flags,
      name);
  NaClDefOp(G_Operand, dest_op_usage | NACL_OPFLAG(OperandZeroExtends_v));
  NaClDefOp(E_Operand, src_op_usage);

  NaClDefInst(
      base+3,
      itype,
      NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeUsesRexW) |
      NACL_IFLAG(OperandSize_o) | NACL_IFLAG(Opcode64Only) | extra_flags,
      name);
  NaClDefOp(G_Operand, dest_op_usage);
  NaClDefOp(E_Operand, src_op_usage);

  NaClDefInst(
      base+4,
      itype,
      NACL_IFLAG(OperandSize_b) | NACL_IFLAG(OpcodeHasImmed) | extra_flags,
      name);
  NaClDefOp(RegAL, dest_op_usage);
  NaClDefOp(I_Operand, src_op_usage);

  NaClDefInstChoices_32_64(base+5, 2, 3);
  NaClDefInst(
      base+5,
      itype,
      NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_v) | extra_flags,
      name);
  NaClDefOp(RegEAX, dest_op_usage | NACL_OPFLAG(OperandZeroExtends_v));
  NaClDefOp(I_Operand, src_op_usage);

  NaClDefInst(
      base+5,
      itype,
      NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeUsesRexW) |
      NACL_IFLAG(OperandSize_o) | NACL_IFLAG(Opcode64Only) | extra_flags,
      name);
  NaClDefOp(RegRAX, dest_op_usage);
  NaClDefOp(I_Operand, src_op_usage);

  NaClDefInst(
      base+5,
      itype,
      NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OpcodeHasImmed) | extra_flags,
      name);
  NaClDefOp(RegAX, dest_op_usage);
  NaClDefOp(I_Operand, src_op_usage);
}

/* Define the Xchg operator where the register is embedded in the opcode */
static void NaClDefXchgRegister() {
  /* Note: xchg is commutative, so order of operands is unimportant. */
  int i;
  /* Note: skip 0, implies a nop */
  for (i = 1; i < 8; ++i) {
    NaClDefInstChoices(0x90 + i, 2);
    NaClDefInst(0x90 + i, NACLi_386L,
                 NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_w) |
                 NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OpcodeLockable),
                 InstXchg);
    NaClDefOp(OpcodeBaseMinus0 + i, NACL_OPFLAG(OperandExtendsOpcode));
    NaClDefOp(G_OpcodeBase, NaClGetIcatFlags(Exchange, 1));
    NaClDefOp(RegREAX, NaClGetIcatFlags(Exchange, 2));

    NaClDefInst(0x90 + i, NACLi_386L,
                 NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_o) |
                 NACL_IFLAG(OpcodeUsesRexW) | NACL_IFLAG(OpcodeLockable),
                 InstXchg);
    NaClDefOp(OpcodeBaseMinus0 + i, NACL_OPFLAG(OperandExtendsOpcode));
    NaClDefOp(G_OpcodeBase, NaClGetIcatFlags(Exchange, 1));
    NaClDefOp(RegRAX, NaClGetIcatFlags(Exchange, 2));
  }
}

/* Define the increment and descrement operators XX+00 to XX+07. Base is
 * the value XX. Name is the name of the increment/decrement operator.
 */
static void NaClDefIncOrDec_00_07(const uint8_t base,
                                  const NaClMnemonic name) {
  int i;
  for (i = 0; i < 8; ++i) {
    NaClDefInst(
        base + i,
        NACLi_386L,
        NACL_IFLAG(Opcode32Only) | NACL_IFLAG(OpcodePlusR) |
        NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
        NACL_IFLAG(OpcodeLockable),
        name);
    NaClDefOp(OpcodeBaseMinus0 + i, NACL_OPFLAG(OperandExtendsOpcode));
    NaClDefOp(G_OpcodeBase, NaClGetIcatFlags(UnaryUpdate, 1));
  }
}

/* Define the push and pop operators XX+00 to XX+17. Base is
 * the value of XX. Name is the name of the push/pop operator.
 */
static void NaClDefPushOrPop_00_07(const uint8_t base,
                                   const NaClMnemonic name) {
  int i;
  NaClOpFlags op_access =
      (name == InstPush ? NACL_OPFLAG(OpUse) : NACL_OPFLAG(OpSet));
  for (i = 0; i < 8; ++i) {
    NaClDefInstChoices(base + i, 2);
    NaClDefInst(
        base + i,
        NACLi_386,
        NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_w),
        name);
    NaClDefOp(OpcodeBaseMinus0 + i, NACL_OPFLAG(OperandExtendsOpcode));
    NaClDefOp(RegRESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
              NACL_OPFLAG(OpImplicit));
    NaClDefOp(G_OpcodeBase, op_access);

    NaClDefInst(
        base + i,
        NACLi_386,
        NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_v) |
        NACL_IFLAG(Opcode32Only),
        name);
    NaClDefOp(OpcodeBaseMinus0 + i, NACL_OPFLAG(OperandExtendsOpcode));
    NaClDefOp(RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
              NACL_OPFLAG(OpImplicit));
    NaClDefOp(G_OpcodeBase, op_access);

    NaClDefInst(
        base + i,
        NACLi_386,
        NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_o) |
        NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OperandSizeDefaultIs64),
        name);
    NaClDefOp(OpcodeBaseMinus0 + i, NACL_OPFLAG(OperandExtendsOpcode));
    NaClDefOp(RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
              NACL_OPFLAG(OpImplicit));
    NaClDefOp(G_OpcodeBase, op_access);
  }
}

static void NaClDefGroup1OpcodesInModRm() {
  int i;
  for (i = 0; i < NACL_ARRAY_SIZE(NaClGroup1OpcodeName); ++i) {
    NaClOpFlags dest_op_usage = GetGroup1OpUsage(NaClGroup1OpcodeName[i], 1);
    NaClOpFlags src_op_usage = GetGroup1OpUsage(NaClGroup1OpcodeName[i], 2);
    NaClDefInst(
        0x80,
        NACLi_386L,
        NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_b) |
        NACL_IFLAG(OpcodeHasImmed) |
        NACL_IFLAG(OpcodeLockable) | NACL_IFLAG(OpcodeRex),
        NaClGroup1OpcodeName[i]);
    NaClDefOp(Opcode0 + i, NACL_OPFLAG(OperandExtendsOpcode));
    NaClDefOp(E_Operand, dest_op_usage);
    NaClDefOp(I_Operand, src_op_usage);

    NaClDefInstMrmChoices_32_64(0x81, Opcode0 + i, 2, 3);
    NaClDefInst(
        0x81,
        NACLi_386L,
        NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_w) |
        NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable),
        NaClGroup1OpcodeName[i]);
    NaClDefOp(Opcode0 + i, NACL_OPFLAG(OperandExtendsOpcode));
    NaClDefOp(E_Operand, dest_op_usage);
    NaClDefOp(I_Operand, src_op_usage);

    NaClDefInst(
        0x81,
        NACLi_386L,
        NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_v) |
        NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeLockable),
        NaClGroup1OpcodeName[i]);
    NaClDefOp(Opcode0 + i, NACL_OPFLAG(OperandExtendsOpcode));
    NaClDefOp(E_Operand, dest_op_usage | NACL_OPFLAG(OperandZeroExtends_v));
    NaClDefOp(I_Operand, src_op_usage);

    NaClDefInst(
        0x81,
        NACLi_386L,
        NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(Opcode64Only) |
        NACL_IFLAG(OpcodeUsesRexW) | NACL_IFLAG(OperandSize_o) |
        NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(OpcodeLockable),
        NaClGroup1OpcodeName[i]);
    NaClDefOp(Opcode0 + i, NACL_OPFLAG(OperandExtendsOpcode));
    NaClDefOp(E_Operand, dest_op_usage);
    NaClDefOp(I_Operand, src_op_usage);

    NaClDefInstMrmChoices_32_64(0x83, Opcode0 + i, 2, 3);
    NaClDefInst(
        0x83,
        NACLi_386L,
        NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_w) |
        NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable),
        NaClGroup1OpcodeName[i]);
    NaClDefOp(Opcode0 + i, NACL_OPFLAG(OperandExtendsOpcode));
    NaClDefOp(E_Operand, dest_op_usage);
    NaClDefOp(I_Operand, src_op_usage);

    NaClDefInst(
        0x83,
        NACLi_386L,
        NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_v) |
        NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable),
        NaClGroup1OpcodeName[i]);
    NaClDefOp(Opcode0 + i, NACL_OPFLAG(OperandExtendsOpcode));
    NaClDefOp(E_Operand, dest_op_usage | NACL_OPFLAG(OperandZeroExtends_v));
    NaClDefOp(I_Operand, src_op_usage);

    NaClDefInst(
        0x83,
        NACLi_386L,
        NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_o) |
        NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OpcodeUsesRexW) |
        NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeLockable),
        NaClGroup1OpcodeName[i]);
    NaClDefOp(Opcode0 + i, NACL_OPFLAG(OperandExtendsOpcode));
    NaClDefOp(E_Operand, dest_op_usage);
    NaClDefOp(I_Operand, src_op_usage);
  }
}

static void NaClDefJump8Opcode(uint8_t opcode, NaClMnemonic name) {
  NaClDefInst(opcode, NACLi_JMP8,
              NACL_IFLAG(OperandSize_b) | NACL_IFLAG(OpcodeHasImmed),
              name);
  NaClDefOp(RegREIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(J_Operand,
            NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) |
            NACL_OPFLAG(OperandRelative));
}

static const NaClMnemonic NaClGroup2OpcodeName[8] = {
  InstRol,
  InstRor,
  InstRcl,
  InstRcr,
  InstShl,
  InstShr,
  NaClMnemonicEnumSize, /* Denote no instruction defined. */
  InstSar
};

static void NaClDefGroup2OpcodesInModRm() {
  int i;
  for (i = 0; i < NACL_ARRAY_SIZE(NaClGroup2OpcodeName); i++) {
    if (NaClGroup2OpcodeName[i] == NaClMnemonicEnumSize) continue;
    NaClDefInst(0xC0, NACLi_OPINMRM,
                NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_b) |
                NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeRex),
                NaClGroup2OpcodeName[i]);
    NaClDefOp(Opcode0 + i, NACL_OPFLAG(OperandExtendsOpcode));
    NaClDefOp(E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
    NaClDefOp(I_Operand, 0);

    NaClDefInstMrmChoices_32_64(0xc1, Opcode0 + i, 1, 2);
    NaClDefInst(0xC1, NACLi_OPINMRM,
                NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_w) |
                NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OpcodeHasImmed_b),
                NaClGroup2OpcodeName[i]);
    NaClDefOp(Opcode0 + i, NACL_OPFLAG(OperandExtendsOpcode));
    NaClDefOp(E_Operand,
              NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse) |
              NACL_OPFLAG(OperandZeroExtends_v));
    NaClDefOp(I_Operand, 0);

    NaClDefInst(0xC1, NACLi_OPINMRM,
                NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_o) |
                NACL_IFLAG(OpcodeUsesRexW) | NACL_IFLAG(Opcode64Only) |
                NACL_IFLAG(OpcodeHasImmed_b),
                NaClGroup2OpcodeName[i]);
    NaClDefOp(Opcode0 + i, NACL_OPFLAG(OperandExtendsOpcode));
    NaClDefOp(E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
    NaClDefOp(I_Operand, 0);

    NaClDefInst(0xD0, NACLi_OPINMRM,
                NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_b) |
                NACL_IFLAG(OpcodeRex),
                NaClGroup2OpcodeName[i]);
    NaClDefOp(Opcode0 + i, NACL_OPFLAG(OperandExtendsOpcode));
    NaClDefOp(E_Operand,  NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
    NaClDefOp(Const_1, 0);

    NaClDefInstMrmChoices_32_64(0xd1, Opcode0 + i, 1, 2);
    NaClDefInst(0xD1, NACLi_OPINMRM,
                NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_w) |
                NACL_IFLAG(OperandSize_v),
                NaClGroup2OpcodeName[i]);
    NaClDefOp(Opcode0 + i, NACL_OPFLAG(OperandExtendsOpcode));
    NaClDefOp(E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
    NaClDefOp(Const_1, 0);

    NaClDefInst(0xD1, NACLi_OPINMRM,
                NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_o) |
                NACL_IFLAG(OpcodeUsesRexW) | NACL_IFLAG(Opcode64Only),
                NaClGroup2OpcodeName[i]);
    NaClDefOp(Opcode0 + i, NACL_OPFLAG(OperandExtendsOpcode));
    NaClDefOp(E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
    NaClDefOp(Const_1, 0);

    NaClDefInst(0xD2, NACLi_OPINMRM,
                NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_b) |
                NACL_IFLAG(OpcodeRex),
                NaClGroup2OpcodeName[i]);
    NaClDefOp(Opcode0 + i, NACL_OPFLAG(OperandExtendsOpcode));
    NaClDefOp(E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
    NaClDefOp(RegCL, NACL_OPFLAG(OpUse));

    NaClDefInstMrmChoices_32_64(0xd3, Opcode0 + i, 1, 2);
    NaClDefInst(0xD3, NACLi_OPINMRM,
                NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_v),
                NaClGroup2OpcodeName[i]);
    NaClDefOp(Opcode0 + i, NACL_OPFLAG(OperandExtendsOpcode));
    NaClDefOp(E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
    NaClDefOp(RegCL, NACL_OPFLAG(OpUse));

    NaClDefInst(0xD3, NACLi_OPINMRM,
                NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_o) |
                NACL_IFLAG(OpcodeUsesRexW) | NACL_IFLAG(Opcode64Only),
                NaClGroup2OpcodeName[i]);
    NaClDefOp(Opcode0 + i, NACL_OPFLAG(OperandExtendsOpcode));
    NaClDefOp(E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
    NaClDefOp(RegCL, NACL_OPFLAG(OpUse));
  }
}

void NaClDefOneByteInsts() {
  uint8_t i;

  NaClDefInstPrefix(NoPrefix);

  NaClBuildBinaryOps_00_05(0x00, NACLi_386L, InstAdd,
                           NACL_IFLAG(OpcodeLockable));

  NaClDefInst(0x06, NACLi_ILLEGAL, NACL_IFLAG(Opcode32Only), InstPush);
  NaClDefOp(RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegES, NACL_OPFLAG(OpUse));

  NaClDefInst(0x07, NACLi_ILLEGAL, NACL_IFLAG(Opcode32Only), InstPop);
  NaClDefOp(RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegES, NACL_OPFLAG(OpSet));

  NaClBuildBinaryOps_00_05(0x08, NACLi_386L, InstOr,
                           NACL_IFLAG(OpcodeLockable));

  NaClDefInst(0x0e, NACLi_ILLEGAL, NACL_IFLAG(Opcode32Only), InstPush);
  NaClDefOp(RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegCS, NACL_OPFLAG(OpUse));

  /* 0x0F is a two-byte opcode prefix. */

  NaClBuildBinaryOps_00_05(0x10, NACLi_386L, InstAdc,
                           NACL_IFLAG(OpcodeLockable));

  NaClDefInst(0x16, NACLi_ILLEGAL, NACL_IFLAG(Opcode32Only), InstPush);
  NaClDefOp(RegRESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegSS, NACL_OPFLAG(OpUse));

  NaClDefInst(0x17, NACLi_ILLEGAL, NACL_IFLAG(Opcode32Only), InstPop);
  NaClDefOp(RegRESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegSS, NACL_OPFLAG(OpSet));

  NaClBuildBinaryOps_00_05(0x18, NACLi_386L, InstSbb,
                           NACL_IFLAG(OpcodeLockable));

  NaClDefInst(0x1e, NACLi_ILLEGAL, NACL_IFLAG(Opcode32Only), InstPush);
  NaClDefOp(RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegDS, NACL_OPFLAG(OpUse));

  NaClDefInst(0x1f, NACLi_ILLEGAL, NACL_IFLAG(Opcode32Only), InstPop);
  NaClDefOp(RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegDS, NACL_OPFLAG(OpSet));

  NaClBuildBinaryOps_00_05(0x20, NACLi_386L, InstAnd,
                           NACL_IFLAG(OpcodeLockable));

  NaClDefInst(0x27, NACLi_ILLEGAL, NACL_IFLAG(Opcode32Only), InstDaa);

  NaClBuildBinaryOps_00_05(0x28, NACLi_386L, InstSub,
                           NACL_IFLAG(OpcodeLockable));

  NaClDefInst(0x2f, NACLi_ILLEGAL, NACL_IFLAG(Opcode32Only), InstDas);

  NaClBuildBinaryOps_00_05(0x30, NACLi_386L, InstXor,
                           NACL_IFLAG(OpcodeLockable));

 /*deprecated */
  NaClDefInst(0x37, NACLi_ILLEGAL, NACL_IFLAG(Opcode32Only), InstAaa);
  NaClDefOp(RegAL, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse) |
            NACL_OPFLAG(OpImplicit));

  NaClBuildBinaryOps_00_05(0x38, NACLi_386, InstCmp,
                           NACL_IFLAG(OpcodeLockable));

  /* Ox3e is segment ds prefix */

  NaClDefInst(0x3f, NACLi_ILLEGAL, NACL_IFLAG(Opcode32Only), InstAas);

  NaClDefIncOrDec_00_07(0x40, InstInc);
  NaClDefIncOrDec_00_07(0x48, InstDec);
  NaClDefPushOrPop_00_07(0x50, InstPush);
  NaClDefPushOrPop_00_07(0x58, InstPop);

  /* Note: pushd shares opcode (uses different operand size). */
  NaClDefInstChoices_32_64(0x60, 2, 0);
  NaClDefInst(
      0x60,
      NACLi_ILLEGAL,
      NACL_IFLAG(Opcode32Only) | NACL_IFLAG(OperandSize_w),
      InstPusha);
  NaClDefOp(RegESP, NACL_OPFLAG(OpImplicit) | NACL_OPFLAG(OpUse) |
            NACL_OPFLAG(OpSet));
  NaClDefOp(RegGP7, NACL_OPFLAG(OpImplicit) | NACL_OPFLAG(OpUse));

  NaClDefInst(
      0x60,
      NACLi_ILLEGAL,
      NACL_IFLAG(Opcode32Only) | NACL_IFLAG(OperandSize_v),
      InstPushad);
  NaClDefOp(RegESP, NACL_OPFLAG(OpImplicit) | NACL_OPFLAG(OpUse) |
            NACL_OPFLAG(OpSet));
  NaClDefOp(RegGP7, NACL_OPFLAG(OpImplicit) | NACL_OPFLAG(OpUse));

  NaClDefInstChoices_32_64(0x61, 2, 0);
  NaClDefInst(
      0x61,
      NACLi_ILLEGAL,
      NACL_IFLAG(Opcode32Only) | NACL_IFLAG(OperandSize_w),
      InstPopa);
  NaClDefOp(RegESP, NACL_OPFLAG(OpImplicit) | NACL_OPFLAG(OpUse) |
            NACL_OPFLAG(OpSet));
  NaClDefOp(RegGP7, NACL_OPFLAG(OpImplicit) | NACL_OPFLAG(OpSet));

  NaClDefInst(
     0x61,
     NACLi_ILLEGAL,
     NACL_IFLAG(Opcode32Only) | NACL_IFLAG(OperandSize_v),
     InstPopad);
  NaClDefOp(RegESP, NACL_OPFLAG(OpImplicit) | NACL_OPFLAG(OpUse) |
            NACL_OPFLAG(OpSet));
  NaClDefOp(RegGP7, NACL_OPFLAG(OpImplicit) | NACL_OPFLAG(OpSet));

  /* TODO(karl) FIX this instruction -- It isn't consistent.
  NaClDefInst(
      0x62,
      NACLi_ILLEGAL,
      NACL_IFLAG(Opcode32Only) | NACL_IFLAG(OpcodeInModRm) |
      NACL_IFLAG(OperandSize_v),
      InstBound);
  NaClDefOp(G_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));
  */

  NaClDefInstChoices_32_64(0x63, 1, 2);
  NaClDefInst(0x63, NACLi_SYSTEM,
              NACL_IFLAG(Opcode32Only) | NACL_IFLAG(OperandSize_w) |
              NACL_IFLAG(OpcodeUsesModRm),
              InstArpl);
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse));
  NaClDefOp(G_Operand, NACL_OPFLAG(OpUse));


  NaClDefInst(0x63, NACLi_386,
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OperandSize_o) |
              NACL_IFLAG(OpcodeUsesRexW) | NACL_IFLAG(OpcodeUsesModRm),
              InstMovsxd);
  NaClDefOp(G_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(Ev_Operand, NACL_OPFLAG(OpUse));

  /* NOTE: this form of movsxd should be discouraged. */
  NaClDefInst(0x63, /* NACLi_SYSTEM, */ NACLi_ILLEGAL,
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OperandSize_v) |
              NACL_IFLAG(OpcodeUsesModRm),
              InstMovsxd);
  NaClDefOp(G_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));

  NaClDefInstChoices_32_64(0x68, 1, 2);
  NaClDefInst(0x68, NACLi_386,
              NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OpcodeHasImmed),
              InstPush);
  NaClDefOp(RegRESP, NACL_OPFLAG(OpImplicit) | NACL_OPFLAG(OpUse) |
            NACL_OPFLAG(OpSet));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0x68, NACLi_386,
              NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OpcodeHasImmed_v) |
              NACL_IFLAG(OperandSizeDefaultIs64) | NACL_IFLAG(Opcode64Only),
              InstPush);
  NaClDefOp(RegRESP, NACL_OPFLAG(OpImplicit) | NACL_OPFLAG(OpUse) |
            NACL_OPFLAG(OpSet));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  /* NOTE: The two argument form appears to be the same as the three
   * argument form, where the first argument is duplicated.
   */
  NaClDefInstChoices_32_64(0x69, 1, 2);
  NaClDefInst(0x69, NACLi_386,
              NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed),
              InstImul);
  NaClDefOp(G_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0x69, NACLi_386,
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OperandSize_o) |
              NACL_IFLAG(OpcodeUsesRexW) |
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_v),
              InstImul);
  NaClDefOp(G_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0x6a, NACLi_386,
              NACL_IFLAG(OperandSize_b) | NACL_IFLAG(OpcodeHasImmed),
              InstPush);
  NaClDefOp(RegRESP, NACL_OPFLAG(OpImplicit) | NACL_OPFLAG(OpUse) |
            NACL_OPFLAG(OpSet));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  /* NOTE: The two argument form appears to be the same as the three
   * argument form, where the first argument is duplicated.
   */
  NaClDefInstChoices_32_64(0x6b, 1, 2);
  NaClDefInst(0x6b, NACLi_386,
              NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
              InstImul);
  NaClDefOp(G_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0x6b, NACLi_386,
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OperandSize_o) |
              NACL_IFLAG(OpcodeUsesRexW) |
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
              InstImul);
  NaClDefOp(G_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0x6c, NACLi_ILLEGAL, NACL_IFLAG(OperandSize_b), InstInsb);
  NaClDefOp(RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));

  NaClDefInstChoices(0x6D, 2);
  NaClDefInst(0x6D, NACLi_ILLEGAL, NACL_IFLAG(OperandSize_w), InstInsw);
  NaClDefOp(RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));

  NaClDefInst(0x6D, NACLi_ILLEGAL, NACL_IFLAG(OperandSize_v), InstInsd);
  NaClDefOp(RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));

  NaClDefInst(0x6E, NACLi_ILLEGAL, NACL_IFLAG(OperandSize_b), InstOutsb);
  NaClDefOp(RegDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));

  NaClDefInstChoices(0x6F, 2);
  NaClDefInst(0x6F, NACLi_ILLEGAL, NACL_IFLAG(OperandSize_w), InstOutsw);
  NaClDefOp(RegDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));

  NaClDefInst(0x6F, NACLi_ILLEGAL, NACL_IFLAG(OperandSize_v), InstOutsd);
  NaClDefOp(RegDX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));

  NaClDefJump8Opcode(0x70, InstJo);
  NaClDefJump8Opcode(0x71, InstJno);
  NaClDefJump8Opcode(0x72, InstJb);
  NaClDefJump8Opcode(0x73, InstJnb);
  NaClDefJump8Opcode(0x74, InstJz);
  NaClDefJump8Opcode(0x75, InstJnz);
  NaClDefJump8Opcode(0x76, InstJbe);
  NaClDefJump8Opcode(0x77, InstJnbe);
  NaClDefJump8Opcode(0x78, InstJs);
  NaClDefJump8Opcode(0x79, InstJns);
  NaClDefJump8Opcode(0x7a, InstJp);
  NaClDefJump8Opcode(0x7b, InstJnp);
  NaClDefJump8Opcode(0x7c, InstJl);
  NaClDefJump8Opcode(0x7d, InstJnl);
  NaClDefJump8Opcode(0x7e, InstJle);
  NaClDefJump8Opcode(0x7f, InstJnle);

  /* For the moment, show some examples of Opcodes in Mod/Rm. */
  NaClDefGroup1OpcodesInModRm();

  /* The AMD manual shows 0x82 as a synonym for 0x80,
   * however these are all illegal in 64-bit mode so we omit them here
   * too. (note: by omission, the opcode is automatically undefined).
   */

  NaClDefInst(0x84, NACLi_386,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b) |
              NACL_IFLAG(OpcodeRex),
              InstTest);
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(G_Operand, NACL_OPFLAG(OpUse));

  NaClDefInstChoices_32_64(0x85, 1, 2);
  NaClDefInst(0x85, NACLi_386,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_w) |
              NACL_IFLAG(OperandSize_v),
              InstTest);
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(G_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0x85, NACLi_386,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_o) |
              NACL_IFLAG(OpcodeUsesRexW) | NACL_IFLAG(Opcode64Only),
              InstTest);
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(G_Operand, NACL_OPFLAG(OpUse));

  /* Note: for xchg, order of operands are commutative, in terms of
   * opcode used.
   */
  NaClDefInst(0x86, NACLi_386L,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b) |
              NACL_IFLAG(Opcode32Only) | NACL_IFLAG(OpcodeLockable),
              InstXchg);
  NaClDefOp(E_Operand, NaClGetIcatFlags(Exchange, 1));
  NaClDefOp(G_Operand, NaClGetIcatFlags(Exchange, 2));

  NaClDefInst(0x86, NACLi_386L,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b) |
              NACL_IFLAG(OpcodeRex) | NACL_IFLAG(Opcode64Only) |
              NACL_IFLAG(OpcodeLockable),
              InstXchg);
  NaClDefOp(E_Operand, NaClGetIcatFlags(Exchange, 1));
  NaClDefOp(G_Operand, NaClGetIcatFlags(Exchange, 2));

  NaClDefInstChoices_32_64(0x87, 1, 2);
  NaClDefInst(0x87, NACLi_386L,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_w) |
              NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OpcodeLockable),
              InstXchg);
  NaClDefOp(E_Operand, NaClGetIcatFlags(Exchange, 1));
  NaClDefOp(G_Operand, NaClGetIcatFlags(Exchange, 2));

  NaClDefInst(0x87, NACLi_386L,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_o) |
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OpcodeUsesRexW) |
              NACL_IFLAG(OpcodeLockable),
              InstXchg);
  NaClDefOp(E_Operand, NaClGetIcatFlags(Exchange, 1));
  NaClDefOp(G_Operand, NaClGetIcatFlags(Exchange, 2));

  NaClDefInst(0x88, NACLi_386,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b) |
              NACL_IFLAG(OpcodeRex),
              InstMov);
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(G_Operand, NACL_OPFLAG(OpUse));

  NaClDefInstChoices_32_64(0x89, 1, 2);
  NaClDefInst(0x89, NACLi_386,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_w) |
              NACL_IFLAG(OperandSize_v),
              InstMov);
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OperandZeroExtends_v));
  NaClDefOp(G_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0x89, NACLi_386,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_o) |
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OpcodeUsesRexW),
              InstMov);
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(G_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0x8A, NACLi_386,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b) |
              NACL_IFLAG(OpcodeRex),
              InstMov);
  NaClDefOp(G_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));

  NaClDefInstChoices_32_64(0x8b, 1, 2);
  NaClDefInst(0x8B, NACLi_386,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_w) |
              NACL_IFLAG(OperandSize_v),
              InstMov);
  NaClDefOp(G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OperandZeroExtends_v));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0x8B, NACLi_386,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_o) |
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OpcodeUsesRexW),
              InstMov);
  NaClDefOp(G_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));

  /* TODO(karl) what is SReg (second argument) in 0x8c*/
  NaClDefInstChoices_32_64(0x8c, 1, 2);
  NaClDefInst(0x8c, NACLi_ILLEGAL,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_w),
              InstMov);
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet));

  /* TODO(karl) what is SReg (second argument) in 0x8c*/
  NaClDefInst(0x8c, NACLi_ILLEGAL,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_o) |
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OpcodeUsesRexW),
              InstMov);
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet));

  NaClDefInstChoices_32_64(0x8d, 1, 2);
  NaClDefInst(0x8d, NACLi_386,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_w) |
              NACL_IFLAG(OperandSize_v),
              InstLea);
  NaClDefOp(G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OperandZeroExtends_v));
  NaClDefOp(M_Operand, NACL_OPFLAG(OpAddress));

  NaClDefInst(0x8d, NACLi_386,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_o) |
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OpcodeUsesRexW),
              InstLea);
  NaClDefOp(G_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(M_Operand, NACL_OPFLAG(OpAddress));

  /* TODO(karl) what is SReg (first argument) in 0x8e*/
  NaClDefInstChoices_32_64(0x8e, 1, 2);
  NaClDefInst(0x8e, NACLi_ILLEGAL,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_w),
              InstMov);
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet));

  /* TODO(karl) what is SReg (first argument) in 0x8e*/
  NaClDefInst(0x8e, NACLi_ILLEGAL,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_o) |
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OpcodeUsesRexW),
              InstMov);
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet));

  NaClDefInstMrmChoices(0x8f, Opcode0, 2);
  NaClDefInst(0x8f, NACLi_386,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_w),
              InstPop);
  NaClDefOp(Opcode0, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(RegRESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet));

  NaClDefInst(0x8f, NACLi_386,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_v) |
              NACL_IFLAG(Opcode32Only),
              InstPop);
  NaClDefOp(Opcode0, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet));

  NaClDefInst(0x8f, NACLi_386,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_o) |
              NACL_IFLAG(Opcode64Only),
              InstPop);
  NaClDefOp(Opcode0, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet));

  NaClDefInst(0x90, NACLi_386R, 0, InstNop);

  NaClDefXchgRegister();

  NaClDefInstChoices_32_64(0x98, 2, 3);
  NaClDefInst(0x98, NACLi_386,
              NACL_IFLAG(OperandSize_w),
              InstCbw);
  NaClDefOp(RegAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));

  NaClDefInst(0x98, NACLi_386,
              NACL_IFLAG(OperandSize_v),
              InstCwde);
  NaClDefOp(RegEAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));

  NaClDefInst(0x98, NACLi_386,
              NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OpcodeUsesRexW) |
              NACL_IFLAG(Opcode64Only),
              InstCdqe);
  NaClDefOp(RegRAX,
            NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit) |
            NACL_OPFLAG(OperandSignExtends_v));
  NaClDefOp(RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));

  NaClDefInstChoices_32_64(0x99, 2, 3);
  NaClDefInst(0x99, NACLi_386,
              NACL_IFLAG(OperandSize_w),
              InstCwd);
  NaClDefOp(RegDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));

  NaClDefInst(0x99, NACLi_386,
              NACL_IFLAG(OperandSize_v),
              InstCdq);
  NaClDefOp(RegEDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegEAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));

  NaClDefInst(0x99, NACLi_386,
              NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OpcodeUsesRexW) |
              NACL_IFLAG(Opcode64Only),
              InstCqo);
  NaClDefOp(RegRDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegRAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegRAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));

  NaClDefInstChoices_32_64(0x9a, 2, 0);
  NaClDefInst(0x9a, NACLi_ILLEGAL,
              NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OpcodeHasImmed_v) |
              NACL_IFLAG(Opcode32Only),
              InstCall);
  NaClDefOp(RegEIP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandFar) |
                NACL_OPFLAG(OperandRelative));

  NaClDefInst(0x9a, NACLi_ILLEGAL,
              NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OpcodeHasImmed_p) |
              NACL_IFLAG(Opcode32Only),
              InstCall);
  NaClDefOp(RegEIP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandFar) |
            NACL_OPFLAG(OperandRelative));

  NaClDefInst(0x9b, NACLi_X87, 0, InstFwait);

  NaClDefInstChoices(0x9c, 2);
  NaClDefInst(0x9c, NACLi_ILLEGAL, NACL_IFLAG(OperandSize_w), InstPushf);
  NaClDefOp(RegRESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));

  NaClDefInst(0x9c, NACLi_ILLEGAL,
              NACL_IFLAG(Opcode32Only) | NACL_IFLAG(OperandSize_v),
              InstPushfd);
  NaClDefOp(RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));

  NaClDefInst(0x9c, NACLi_ILLEGAL,
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OperandSize_o) |
              NACL_IFLAG(OperandSizeDefaultIs64),
              InstPushfq);
  NaClDefOp(RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));

  NaClDefInstChoices(0x9d, 2);
  NaClDefInst(0x9d, NACLi_ILLEGAL, NACL_IFLAG(OperandSize_w), InstPopf);
  NaClDefOp(RegRESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));

  NaClDefInst(0x9d, NACLi_ILLEGAL,
              NACL_IFLAG(Opcode32Only) | NACL_IFLAG(OperandSize_v),
              InstPopfd);
  NaClDefOp(RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));

  NaClDefInst(0x9d, NACLi_ILLEGAL,
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OperandSize_v),
              InstPopfq);
  NaClDefOp(RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));

  NaClDefInst(0x9e, NACLi_386, NACL_IFLAG(Opcode32Only), InstSahf);
  NaClDefOp(RegAH, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));

  NaClDefInst(0x9f, NACLi_386, NACL_IFLAG(Opcode32Only), InstLahf);
  NaClDefOp(RegAH, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));

  NaClDefInst(0xa0, NACLi_386,
              NACL_IFLAG(OperandSize_b) | NACL_IFLAG(OpcodeHasImmed_Addr),
              InstMov);
  NaClDefOp(RegAL, NACL_OPFLAG(OpSet));
  NaClDefOp(O_Operand, NACL_OPFLAG(OpUse));

  NaClDefInstChoices_32_64(0xa1, 1, 2);
  NaClDefInst(0xa1, NACLi_386,
              NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
              NACL_IFLAG(OpcodeHasImmed_Addr),
              InstMov);
  NaClDefOp(RegREAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OperandZeroExtends_v));
  NaClDefOp(O_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xa1, NACLi_386,
              NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OpcodeHasImmed_Addr) |
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OpcodeUsesRexW),
              InstMov);
  NaClDefOp(RegRAX, NACL_OPFLAG(OpSet));
  NaClDefOp(O_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xa2, NACLi_386,
              NACL_IFLAG(OperandSize_b) | NACL_IFLAG(OpcodeHasImmed_Addr),
              InstMov);
  NaClDefOp(O_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(RegAL, NACL_OPFLAG(OpUse));

  NaClDefInstChoices_32_64(0xa3, 1, 2);
  NaClDefInst(0xa3, NACLi_386,
              NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
              NACL_IFLAG(OpcodeHasImmed_Addr),
              InstMov);
  NaClDefOp(O_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OperandZeroExtends_v));
  NaClDefOp(RegREAX, NACL_OPFLAG(OpUse));

  NaClDefInst(0xa3, NACLi_386,
              NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OpcodeHasImmed_Addr) |
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OpcodeUsesRexW),
              InstMov);
  NaClDefOp(O_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(RegRAX, NACL_OPFLAG(OpUse));

  NaClDefInst(0xa4, NACLi_386R,
              NACL_IFLAG(OperandSize_b),
              InstMovsb);
  NaClDefOp(RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));

  NaClDefInstChoices_32_64(0xa5, 2, 3);
  NaClDefInst(0xa5, NACLi_386R,
              NACL_IFLAG(OperandSize_w),
              InstMovsw);
  NaClDefOp(RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));

  NaClDefInst(0xa5, NACLi_386R,
              NACL_IFLAG(OperandSize_v),
              InstMovsd);
  NaClDefOp(RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));

  NaClDefInst(0xa5, NACLi_386R,
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OperandSize_o) |
              NACL_IFLAG(OpcodeUsesRexW),
              InstMovsq);
  NaClDefOp(RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));

  NaClDefInst(0xa6, NACLi_386RE,
              NACL_IFLAG(OperandSize_b),
              InstCmpsb);
  NaClDefOp(RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));

  NaClDefInstChoices_32_64(0xa7, 2, 3);
  NaClDefInst(0xa7, NACLi_386RE,
              NACL_IFLAG(OperandSize_w),
              InstCmpsw);
  NaClDefOp(RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));

  NaClDefInst(0xa7, NACLi_386RE,
              NACL_IFLAG(OperandSize_v),
              InstCmpsd);
  NaClDefOp(RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));

  NaClDefInst(0xa7, NACLi_386RE,
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OperandSize_o) |
              NACL_IFLAG(OpcodeUsesRexW),
              InstCmpsq);
  NaClDefOp(RegES_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));

  NaClDefInst(0xA8, NACLi_386,
              NACL_IFLAG(OperandSize_b) | NACL_IFLAG(OpcodeHasImmed),
              InstTest);
  NaClDefOp(RegAL, NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInstChoices_32_64(0xA9, 2, 3);
  NaClDefInst(0xA9, NACLi_386,
              NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OpcodeHasImmed),
              InstTest);
  NaClDefOp(RegAX, NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xA9, NACLi_386,
              NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OpcodeHasImmed),
              InstTest);
  NaClDefOp(RegEAX, NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xA9, NACLi_386,
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OperandSize_o) |
              NACL_IFLAG(OpcodeHasImmed_v),
              InstTest);
  NaClDefOp(RegRAX, NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xaa, NACLi_386R,
              NACL_IFLAG(OperandSize_b),
              InstStosb);
  NaClDefOp(RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));

  NaClDefInstChoices_32_64(0xab, 2, 3);
  NaClDefInst(0xab, NACLi_386R,
              NACL_IFLAG(OperandSize_w),
              InstStosw);
  NaClDefOp(RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));

  NaClDefInst(0xab, NACLi_386R,
              NACL_IFLAG(OperandSize_v),
              InstStosd);
  NaClDefOp(RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));

  NaClDefInst(0xab, NACLi_386R,
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OperandSize_o) |
              NACL_IFLAG(OpcodeUsesRexW),
              InstStosq);
  NaClDefOp(RegES_EDI, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegRAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));

  /* ISE reviewers suggested omitting lods */
  NaClDefInst(0xac, NACLi_ILLEGAL,
              NACL_IFLAG(OperandSize_b),
              InstLodsb);
  NaClDefOp(RegAL, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));

  NaClDefInstChoices_32_64(0xad, 2, 3);
  NaClDefInst(0xad, NACLi_ILLEGAL,
              NACL_IFLAG(OperandSize_w),
              InstLodsw);
  NaClDefOp(RegAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));

  NaClDefInst(0xad, NACLi_ILLEGAL,
              NACL_IFLAG(OperandSize_v),
              InstLodsd);
  NaClDefOp(RegEAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));

  NaClDefInst(0xad, NACLi_ILLEGAL,
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OperandSize_o) |
              NACL_IFLAG(OpcodeUsesRexW),
              InstLodsq);
  NaClDefOp(RegRAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));

  /* ISE reviewers suggested omitting scas */
  NaClDefInst(0xae, NACLi_386RE,
              NACL_IFLAG(OperandSize_b),
              InstScasb);
  NaClDefOp(RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));

  NaClDefInstChoices_32_64(0xaf, 2, 3);
  NaClDefInst(0xaf, NACLi_386RE,
              NACL_IFLAG(OperandSize_w),
              InstScasw);
  NaClDefOp(RegAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));

  NaClDefInst(0xaf, NACLi_386RE,
              NACL_IFLAG(OperandSize_v),
              InstScasd);
  NaClDefOp(RegEAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));

  NaClDefInst(0xaf, NACLi_386RE,
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OperandSize_o) |
              NACL_IFLAG(OpcodeUsesRexW),
              InstScasq);
  NaClDefOp(RegRAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));

  for (i = 0; i < 8; ++i) {
    NaClDefInstChoices_32_64(0xB0 + i, 1, 2);
    NaClDefInst(0xB0 + i, NACLi_386,
                NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OpcodePlusR) |
                NACL_IFLAG(OperandSize_b) | NACL_IFLAG(OpcodeHasImmed) |
                NACL_IFLAG(OpcodeRex),
                InstMov);
    NaClDefOp(OpcodeBaseMinus0 + i, NACL_OPFLAG(OperandExtendsOpcode));
    NaClDefOp(G_OpcodeBase, NACL_OPFLAG(OpSet));
    NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

    NaClDefInst(0xB0 + i, NACLi_386,
                NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_b) |
                NACL_IFLAG(OpcodeHasImmed),
                InstMov);
    NaClDefOp(OpcodeBaseMinus0 + i, NACL_OPFLAG(OperandExtendsOpcode));
    NaClDefOp(G_OpcodeBase, NACL_OPFLAG(OpSet));
    NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));
  }

  for (i = 0; i < 8; ++i) {
    NaClDefInstChoices_32_64(0xB8 + i, 2, 3);
    NaClDefInst(0xB8 + i, NACLi_386,
                NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_v) |
                NACL_IFLAG(OpcodeHasImmed),
                InstMov);
    NaClDefOp(OpcodeBaseMinus0 + i, NACL_OPFLAG(OperandExtendsOpcode));
    NaClDefOp(G_OpcodeBase, NACL_OPFLAG(OpSet) |
              NACL_OPFLAG(OperandZeroExtends_v));
    NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

    NaClDefInst(0xB8 + i, NACLi_386,
                NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(OperandSize_w) |
                NACL_IFLAG(OpcodeHasImmed),
                InstMov);
    NaClDefOp(OpcodeBaseMinus0 + i, NACL_OPFLAG(OperandExtendsOpcode));
    NaClDefOp(G_OpcodeBase, NACL_OPFLAG(OpSet));
    NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

    NaClDefInst(0xB8 + i, NACLi_386,
                NACL_IFLAG(OpcodePlusR) | NACL_IFLAG(Opcode64Only) |
                NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OpcodeHasImmed) |
                NACL_IFLAG(OpcodeUsesRexW),
                InstMov);
    NaClDefOp(OpcodeBaseMinus0 + i, NACL_OPFLAG(OperandExtendsOpcode));
    NaClDefOp(G_OpcodeBase, NACL_OPFLAG(OpSet));
    NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));
  }

  /* 0xC0 and 0xC1 defined by NaClDefGroup2OpcodesInModRm */

  NaClDefInst(0xc2, NACLi_RETURN, NACL_IFLAG(OpcodeHasImmed_w), InstRet);
  NaClDefOp(RegREIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegRESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear));

  NaClDefInst(0xc3, NACLi_RETURN, 0, InstRet);
  NaClDefOp(RegREIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegRESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));

  NaClDefInstChoices_32_64(0xc4, 2, 0);
  NaClDefInst(0xc4, NACLi_ILLEGAL,
              NACL_IFLAG(Opcode32Only) | NACL_IFLAG(OperandSize_w) |
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_v),
              InstLes);
  NaClDefOp(ES_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(G_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xc4, NACLi_ILLEGAL,
              NACL_IFLAG(Opcode32Only) | NACL_IFLAG(OperandSize_v) |
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_p),
              InstLes);
  NaClDefOp(ES_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(G_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInstChoices_32_64(0xc5, 2, 0);
  NaClDefInst(0xc5, NACLi_ILLEGAL,
              NACL_IFLAG(Opcode32Only) | NACL_IFLAG(OperandSize_w) |
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_v),
              InstLds);
  NaClDefOp(G_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xc5, NACLi_ILLEGAL,
              NACL_IFLAG(Opcode32Only) | NACL_IFLAG(OperandSize_v) |
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_p),
              InstLes);
  NaClDefOp(ES_G_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(G_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  /* GROUP 11 */
  NaClDefInst(0xC6, NACLi_386,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_b) |
              NACL_IFLAG(OpcodeRex) | NACL_IFLAG(OpcodeHasImmed),
              InstMov);
  NaClDefOp(Opcode0, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  /* GROUP 11 */
  NaClDefInstMrmChoices_32_64(0xc7, Opcode0, 2, 3);
  NaClDefInst(0xC7, NACLi_386,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_w) |
              NACL_IFLAG(OpcodeHasImmed),
              InstMov);
  NaClDefOp(Opcode0, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xC7, NACLi_386,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_v) |
              NACL_IFLAG(OpcodeHasImmed),
              InstMov);
  NaClDefOp(Opcode0, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OperandZeroExtends_v));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xC7, NACLi_386,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_o) |
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OpcodeUsesRexW) |
              NACL_IFLAG(OpcodeHasImmed_v),
              InstMov);
  NaClDefOp(Opcode0, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xC8, NACLi_ILLEGAL,
              NACL_IFLAG(OpcodeHasImmed_w) | NACL_IFLAG(OpcodeHasImmed2_b),
              InstEnter);
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(I2_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xc9, NACLi_386, 0, InstLeave);
  NaClDefOp(RegREBP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegRESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));

  NaClDefInst(0xca, NACLi_RETURN, NACL_IFLAG(OpcodeHasImmed_w), InstRet);
  NaClDefOp(RegREIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegRESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandFar));

  NaClDefInst(0xcb, NACLi_RETURN, 0, InstRet);
  NaClDefOp(RegREIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegRESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));

  NaClDefInst(0xcc, NACLi_ILLEGAL, 0, InstInt3);

  NaClDefInst(0xcd, NACLi_ILLEGAL,
              NACL_IFLAG(OperandSize_b) | NACL_IFLAG(OpcodeHasImmed),
              InstInt);
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xce, NACLi_ILLEGAL, NACL_IFLAG(Opcode32Only), InstInt0);

  NaClDefInst(0xcf, NACLi_SYSTEM, 0, InstIret);

  /* Group 2 - 0xC0, 0xC1, 0xD0, 0XD1, 0xD2, 0xD3 */
  NaClDefGroup2OpcodesInModRm();

  NaClDefInst(0xD4, NACLi_ILLEGAL,
              NACL_IFLAG(Opcode32Only) | NACL_IFLAG(OpcodeHasImmed_b),
              InstAam);
  NaClDefOp(RegAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));

  NaClDefInst(0xd5, NACLi_ILLEGAL,
              NACL_IFLAG(Opcode32Only) | NACL_IFLAG(OpcodeHasImmed_b),
              InstAad);
  NaClDefOp(RegAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegAL, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegAH, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));

  /* Note: 0xdd /4 is already defined in ncdecodeX87.c */

  /* ISE reviewers suggested making loopne, loope, loop, jcxz illegal */
  NaClDefJump8Opcode(0xe0, InstLoopne);
  NaClDefJump8Opcode(0xe1, InstLoope);
  NaClDefJump8Opcode(0xe2, InstLoop);
  NaClDefJump8Opcode(0xe3, InstJcxz);

  NaClDefInst(0xE4, NACLi_ILLEGAL,
              NACL_IFLAG(OpcodeHasImmed_b),
              InstIn);
  NaClDefOp(RegAL, NACL_OPFLAG(OpSet));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xE5, NACLi_ILLEGAL,
              NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
              NACL_IFLAG(OpcodeHasImmed_b),
              InstIn);
  NaClDefOp(RegREAX, NACL_OPFLAG(OpSet));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInstChoices(0xe8, 2);
  NaClDefInst(0xe8, NACLi_JMPZ,
              NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OpcodeHasImmed) |
              NACL_IFLAG(Opcode32Only),
              InstCall);
  NaClDefOp(RegEIP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) |
            NACL_OPFLAG(OperandRelative));

  /* Not supported */
  NaClDefInst(0xe8, NACLi_ILLEGAL,
              NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OpcodeHasImmed) |
              NACL_IFLAG(Opcode64Only),
              InstCall);
  NaClDefOp(RegRIP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) |
            NACL_OPFLAG(OperandRelative));

  NaClDefInst(0xe8, NACLi_JMPZ,
              NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OpcodeHasImmed),
              InstCall);
  NaClDefOp(RegREIP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegRESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) |
            NACL_OPFLAG(OperandRelative));

  NaClDefInst(0xe9, NACLi_JMPZ,
              NACL_IFLAG(Opcode32Only) | NACL_IFLAG(OpcodeHasImmed) |
              NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
              InstJmp);
  NaClDefOp(RegEIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) |
            NACL_OPFLAG(OperandRelative));

  NaClDefInst(0xe9, NACLi_JMPZ,
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OpcodeHasImmed_v),
              InstJmp);
  NaClDefOp(RegREIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) |
            NACL_OPFLAG(OperandRelative));

  NaClDefInst(0xeb, NACLi_JMP8,
              NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b),
              InstJmp);
  NaClDefOp(RegREIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(J_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear) |
            NACL_OPFLAG(OperandRelative));

  NaClDefInst(0xec, NACLi_ILLEGAL, 0, InstIn);
  NaClDefOp(RegAL, NACL_OPFLAG(OpSet));
  NaClDefOp(RegDX, NACL_OPFLAG(OpUse));

  NaClDefInst(0xed, NACLi_ILLEGAL,
              NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
              InstIn);
  NaClDefOp(RegREAX, NACL_OPFLAG(OpSet));
  NaClDefOp(RegDX, NACL_OPFLAG(OpUse));

  NaClDefInst(0xf4, NACLi_386, 0, InstHlt);

  /* Group3 opcode. */
  NaClDefInst(0xF6, NACLi_386,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_b) |
              NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OpcodeRex),
              InstTest);
  NaClDefOp(Opcode0, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xF6, NACLi_386L,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_b) |
              NACL_IFLAG(OpcodeRex) | NACL_IFLAG(OpcodeLockable),
              InstNot);
  NaClDefOp(Opcode2, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(E_Operand, NaClGetIcatFlags(UnaryUpdate, 1));

  NaClDefInst(0xF6, NACLi_386L,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_b) |
              NACL_IFLAG(OpcodeRex) | NACL_IFLAG(OpcodeLockable),
              InstNeg);
  NaClDefOp(Opcode3, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(E_Operand, NaClGetIcatFlags(UnaryUpdate, 1));

  NaClDefInst(0xF6, NACLi_386,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_b) |
              NACL_IFLAG(OpcodeRex),
              InstMul);
  NaClDefOp(Opcode4, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(RegAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xF6, NACLi_386,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_b),
              InstImul);
  NaClDefOp(Opcode5, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(RegAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xF6, NACLi_386,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_b) |
              NACL_IFLAG(OpcodeRex),
              InstDiv);
  NaClDefOp(Opcode6, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(RegAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegAL, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xF6, NACLi_386,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_b) |
              NACL_IFLAG(OpcodeRex),
              InstIdiv);
  NaClDefOp(Opcode7, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(RegAL, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegAH, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegAX, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));

  NaClDefInstMrmChoices_32_64(0xf7, Opcode0, 2, 3);
  NaClDefInst(0xF7, NACLi_386,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_w) |
              NACL_IFLAG(OpcodeHasImmed),
              InstTest);
  NaClDefOp(Opcode0, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xF7, NACLi_386,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_v) |
              NACL_IFLAG(OpcodeHasImmed),
              InstTest);
  NaClDefOp(Opcode0, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xF7, NACLi_386,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_o) |
              NACL_IFLAG(OpcodeHasImmed_v) | NACL_IFLAG(Opcode64Only),
              InstTest);
  NaClDefOp(Opcode0, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInstMrmChoices_32_64(0xf7, Opcode2, 1, 2);
  NaClDefInst(0xF7, NACLi_386L,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_w) |
              NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OpcodeLockable),
              InstNot);
  NaClDefOp(Opcode2, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(E_Operand, NaClGetIcatFlags(UnaryUpdate, 1));

  NaClDefInst(0xF7, NACLi_386L,
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OpcodeInModRm) |
              NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OpcodeUsesRexW) |
              NACL_IFLAG(OpcodeLockable),
              InstNot);
  NaClDefOp(Opcode2, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(E_Operand, NaClGetIcatFlags(UnaryUpdate, 1));

  NaClDefInstMrmChoices_32_64(0xf7, Opcode3, 1, 2);
  NaClDefInst(0xF7, NACLi_386L,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_w) |
              NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OpcodeLockable),
              InstNeg);
  NaClDefOp(Opcode3, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(E_Operand, NaClGetIcatFlags(UnaryUpdate, 1));

  NaClDefInst(0xF7, NACLi_386L,
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OpcodeInModRm) |
              NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OpcodeUsesRexW) |
              NACL_IFLAG(OpcodeLockable),
              InstNeg);
  NaClDefOp(Opcode3, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(E_Operand, NaClGetIcatFlags(UnaryUpdate, 1));

  NaClDefInstMrmChoices_32_64(0xF7, Opcode4, 1, 2);
  NaClDefInst(0xF7, NACLi_386,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_w) |
              NACL_IFLAG(OperandSize_v),
              InstMul);
  NaClDefOp(Opcode4, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(RegREDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegREAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xF7, NACLi_386,
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OpcodeInModRm) |
              NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OpcodeUsesRexW),
              InstMul);
  NaClDefOp(Opcode4, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(RegRDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegRAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));

  NaClDefInstMrmChoices_32_64(0xf7, Opcode5, 1, 2);
  NaClDefInst(0xF7, NACLi_386,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_w) |
              NACL_IFLAG(OperandSize_v),
              InstImul);
  NaClDefOp(Opcode5, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(RegREDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegREAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xF7, NACLi_386,
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OpcodeInModRm) |
              NACL_IFLAG(OperandSize_o),
              InstImul);
  NaClDefOp(Opcode5, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(RegRDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegRAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));

  NaClDefInstMrmChoices_32_64(0xf7, Opcode6, 1, 2);
  NaClDefInst(0xF7, NACLi_386,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_w) |
              NACL_IFLAG(OperandSize_v),
              InstDiv);
  NaClDefOp(Opcode6, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(RegREDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegREAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xF7, NACLi_386,
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OpcodeInModRm) |
              NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OpcodeUsesRexW),
              InstDiv);
  NaClDefOp(Opcode6, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(RegRDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegRAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));

  NaClDefInstMrmChoices_32_64(0xF7, Opcode7, 1, 2);
  NaClDefInst(0xF7, NACLi_386,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_w) |
              NACL_IFLAG(OperandSize_v),
              InstIdiv);
  NaClDefOp(Opcode7, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(RegREDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegREAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xF7, NACLi_386,
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OpcodeInModRm) |
              NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OpcodeUsesRexW),
              InstIdiv);
  NaClDefOp(Opcode7, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(RegRDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegRAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xFC, NACLi_386, 0, InstCld);

  NaClDefInst(0xFE, NACLi_386L,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_b) |
              NACL_IFLAG(OpcodeLockable),
              InstInc);
  NaClDefOp(Opcode0, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(E_Operand, NaClGetIcatFlags(UnaryUpdate, 1));

  NaClDefInst(0xFE, NACLi_386L,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_b) |
              NACL_IFLAG(OpcodeLockable),
              InstDec);
  NaClDefOp(Opcode1, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(E_Operand, NaClGetIcatFlags(UnaryUpdate, 1));

  /* Group5 opcode. */
  NaClDefInstMrmChoices_32_64(0xff, Opcode0, 1, 2);
  NaClDefInst(0xff, NACLi_386L,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_w) |
              NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OpcodeLockable),
              InstInc);
  NaClDefOp(Opcode0, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(E_Operand, NaClGetIcatFlags(UnaryUpdate, 1));

  NaClDefInst(0xff, NACLi_386L,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_o) |
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OpcodeUsesRexW) |
              NACL_IFLAG(OpcodeLockable),
              InstInc);
  NaClDefOp(Opcode0, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(E_Operand, NaClGetIcatFlags(UnaryUpdate, 1));

  NaClDefInstMrmChoices_32_64(0xff, Opcode1, 1, 2);
  NaClDefInst(0xff, NACLi_386L,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_w) |
              NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OpcodeLockable),
              InstDec);
  NaClDefOp(Opcode1, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(E_Operand, NaClGetIcatFlags(UnaryUpdate, 1));

  NaClDefInst(0xff, NACLi_386L,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_o) |
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OpcodeUsesRexW) |
              NACL_IFLAG(OpcodeLockable),
              InstDec);
  NaClDefOp(Opcode1, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(E_Operand, NaClGetIcatFlags(UnaryUpdate, 1));

  NaClDefInstMrmChoices_32_64(0xff, Opcode2, 2, 1);
  NaClDefInst(0xff, NACLi_INDIRECT,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(Opcode32Only) |
              NACL_IFLAG(OperandSize_w),
              InstCall);
  NaClDefOp(Opcode2, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(RegEIP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear));

  NaClDefInst(0xff, NACLi_INDIRECT,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(Opcode32Only) |
              NACL_IFLAG(OperandSize_v),
              InstCall);
  NaClDefOp(Opcode2, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(RegEIP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear));

  NaClDefInst(0xff, NACLi_INDIRECT,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(Opcode64Only) |
              NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeForce64),
              InstCall);
  NaClDefOp(Opcode2, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(RegRIP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear));

  NaClDefInstMrmChoices_32_64(0xff, Opcode4, 2, 1);
  NaClDefInst(0xff, NACLi_INDIRECT,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(Opcode32Only) |
              NACL_IFLAG(OperandSize_w),
              InstJmp);
  NaClDefOp(Opcode4, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(RegEIP, NACL_OPFLAG(OpSet) | OpImplicit);
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear));

  NaClDefInst(0xff, NACLi_INDIRECT,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(Opcode32Only) |
              NACL_IFLAG(OperandSize_v),
              InstJmp);
  NaClDefOp(Opcode4, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(RegEIP, NACL_OPFLAG(OpSet) | OpImplicit);
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear));

  NaClDefInst(0xff, NACLi_INDIRECT,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(Opcode64Only) |
              NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OperandSizeForce64),
              InstJmp);
  NaClDefOp(Opcode4, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(RegRIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OperandNear));

  NaClDefInstMrmChoices_32_64(0xff, Opcode5, 2, 3);
  NaClDefInst(0xff, NACLi_ILLEGAL,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_w),
              InstJmp);
  NaClDefOp(Opcode5, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(RegREIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(Mw_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xff, NACLi_ILLEGAL,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_v),
              InstJmp);
  NaClDefOp(Opcode5, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(RegREIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(Mw_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xff, NACLi_ILLEGAL,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_w) |
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OpcodeUsesRexW),
              InstJmp);
  NaClDefOp(Opcode5, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(RegREIP, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(Mw_Operand, NACL_OPFLAG(OpUse));

  NaClDefInstMrmChoices(0xff, Opcode6, 2);
  NaClDefInst(0xff, NACLi_386,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_w),
              InstPush);
  NaClDefOp(Opcode6, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(RegRESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xff, NACLi_386,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_v) |
              NACL_IFLAG(Opcode32Only),
              InstPush);
  NaClDefOp(Opcode6, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(RegESP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));

  NaClDefInst(0xff, NACLi_386,
              NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(OperandSize_o) |
              NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OperandSizeDefaultIs64),
              InstPush);
  NaClDefOp(Opcode6, NACL_OPFLAG(OperandExtendsOpcode));
  NaClDefOp(RegRSP, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet) |
            NACL_OPFLAG(OpImplicit));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));
}
