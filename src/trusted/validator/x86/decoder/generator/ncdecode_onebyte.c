/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Defines the one byte x86 opcodes. Based on opcodes given in table A-1 of
 * appendix "A2 - Opcode Encodings", in AMD document 24594-Rev.3.14-September
 * 2007, "AMD64 Architecture Programmer's manual Volume 3: General-Purpose
 * and System Instructions".
 *
 * TODO(karl): This file is being updated to match Appendix A2.
 * Not all instructions have yet been converted to the form used in
 * in table A-1 cited above.
 */

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/trusted/validator/x86/decoder/generator/ncdecode_forms.h"
#include "native_client/src/trusted/validator/x86/decoder/generator/ncdecode_st.h"
#include "native_client/src/trusted/validator/x86/decoder/generator/ncdecode_tablegen.h"

/* Defines the buffer size to use to generate strings */
#define BUFFER_SIZE 256

/* Define a symbol table size that can hold a small number of
 * symbols (i.e. limit to at most 5 definitions).
 */
#define NACL_SMALL_ST (5)

/* Define the number of register values that can appear in an opcode. */

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

/* Returns true if the given mnemonic name corresponds
 * to Compare.
 */
static Bool NaClIsGroup1CompName(NaClMnemonic name) {
  int i;
  for (i = 0; i < NACL_ARRAY_SIZE(NaClGroup1CompName); ++i) {
    if (name == NaClGroup1CompName[i]) return TRUE;
  }
  return FALSE;
}

/* Returns the set/use categorization for a group 1 mnemonic name. */
static NaClInstCat NaClGroup1Cat(NaClMnemonic name) {
  return NaClIsGroup1CompName(name) ? Compare : Binary;
}

/* Define binary operation XX+00 to XX+05, for the binary operators
 * add, or, adc, sbb, and, sub, xor, and cmp. Base is the value XX.
 * Name is the name of the operation. Extra flags are any additional
 * flags that are true to a specific binary operator, rather than
 * all binary operators.
 */
static void NaClBuildBinaryOps_00_05(const uint8_t base,
                                     const NaClInstType itype,
                                     const NaClMnemonic name) {
  NaClInstCat cat = NaClGroup1Cat(name);
  struct NaClSymbolTable* st = NaClSymbolTableCreate(NACL_SMALL_ST, NULL);
  NaClSymbolTablePutText("name", NaClMnemonicName(name), st);
  NaClSymbolTablePutByte("base", base, st);

  NaClDefine("@base: @name $Eb, $Gb", itype, st, cat);

  NaClSymbolTablePutByte("base", base+1, st);
  NaClDefine("@base: @name $Ev, $Gv", itype, st, cat);

  NaClSymbolTablePutByte("base", base+2, st);
  NaClDefine("@base: @name $Gb, $Eb", itype, st, cat);

  NaClSymbolTablePutByte("base", base+3, st);
  NaClDefine("@base: @name $Gv, $Ev", itype, st, cat);

  NaClSymbolTablePutByte("base", base+4, st);
  NaClDefine("@base: @name %al, $Ib", itype, st, cat);

  NaClSymbolTablePutByte("base", base+5, st);
  NaClDefine("@base: @name $rAXv, $Iz", itype, st, cat);

  NaClSymbolTableDestroy(st);
}

/* Define the Xchg operator where the register is embedded in the opcode */
static void NaClDefXchgRegister(void) {
  /* Note: xchg is commutative, so order of operands is unimportant. */
  int i;
  struct NaClSymbolTable* st = NaClSymbolTableCreate(NACL_SMALL_ST, NULL);
  for (i = 0; i <= kMaxRegisterIndexInOpcode; ++i) {
    NaClSymbolTablePutInt("i", i, st);
    NaClDefine("90+@i: Xchg $r8v, $rAXv", NACLi_386, st , Exchange);
  }
}

/* Define the increment and descrement operators XX+00 to XX+07. Base is
 * the value XX. Name is the name of the increment/decrement operator.
 */
static void NaClDefIncOrDec_00_07(const uint8_t base,
                                  const NaClMnemonic name,
                                  struct NaClSymbolTable* context_st) {
  static char* reg[kMaxRegisterIndexInOpcode + 1] = {
    "rAXv",
    "rCXv",
    "rDXv",
    "rBXv",
    "rSPv",
    "rBPv",
    "rSIv",
    "rDIv"
  };

  struct NaClSymbolTable* st = NaClSymbolTableCreate(NACL_SMALL_ST, context_st);
  int i;
  NaClSymbolTablePutText("name", NaClMnemonicName(name), st);
  for (i = 0; i <= kMaxRegisterIndexInOpcode; ++i) {
    NaClSymbolTablePutByte("opcode", base + i, st);
    NaClSymbolTablePutText("reg", reg[i], st);
    /* Note: Since the following instructions are only defined in
     * 32-bit mode, operand size 8 will not apply, and hence rXX
     * is equivalent to eXX (as used in the ADM manual).
     */
    NaClDef_32("@opcode: @name $@reg", NACLi_386, st, UnaryUpdate);
  }
  NaClSymbolTableDestroy(st);
}

/* Define the push and pop operators XX+00 to XX+17. Base is
 * the value of XX. Name is the name of the push/pop operator.
 */
static void NaClDefPushOrPop_00_07(const uint8_t base,
                                   const NaClMnemonic name,
                                   struct NaClSymbolTable* context_st) {
  int i;
  NaClInstCat cat = (name == InstPush ? Push : Pop);
  struct NaClSymbolTable* st = NaClSymbolTableCreate(NACL_SMALL_ST, context_st);
  NaClSymbolTablePutByte("base", base, st);
  NaClSymbolTablePutText("name", NaClMnemonicName(name), st);
  for (i = 0; i <= kMaxRegisterIndexInOpcode; ++i) {
    NaClSymbolTablePutInt("i", i, st);
    NaClDefine("@base+@i: @name {%@sp}, $r8v", NACLi_386, st, cat);
  }
  NaClSymbolTableDestroy(st);
}

static void NaClDefGroup1OpcodesInModRm(struct NaClSymbolTable* context_st) {
  int i;
  struct NaClSymbolTable* st = NaClSymbolTableCreate(NACL_SMALL_ST, context_st);
  for (i = 0; i < NACL_ARRAY_SIZE(NaClGroup1OpcodeName); ++i) {
    NaClMnemonic name = NaClGroup1OpcodeName[i];
    NaClInstCat cat = NaClGroup1Cat(name);
    NaClSymbolTablePutInt("i", i, st);
    NaClSymbolTablePutText("name", NaClMnemonicName(name), st);
    NaClDefine("80/@i: @name $Eb, $Ib", NACLi_386, st, cat);
    NaClDefine("81/@i: @name $Ev, $Iz", NACLi_386, st, cat);
    /* The AMD manual shows 0x82 as a synonym for 0x80 in 32-bit mode only */
    NaClDef_32("82/@i: @name $Eb, $Ib", NACLi_386, st, cat);
    NaClDef_64("82/@i: Invalid", NACLi_INVALID, st, Other);
    NaClDefine("83/@i: @name $Ev, $Ib", NACLi_386, st, cat);
  }
  NaClSymbolTableDestroy(st);
}

static void NaClDefJump8Opcode(uint8_t opcode, NaClMnemonic name,
                               struct NaClSymbolTable* context_st) {
  struct NaClSymbolTable* st = NaClSymbolTableCreate(NACL_SMALL_ST, context_st);
  NaClSymbolTablePutByte("opcode", opcode, st);
  NaClSymbolTablePutText("name", NaClMnemonicName(name), st);
  NaClDefine("@opcode: @name {%@ip}, $Jb", NACLi_386, st, Jump);
}

/* Defines a conditional jump instruction that is dependent on address size. */
typedef struct {
  /* The platform(s) the instruction is defined on. */
  const NaClTargetPlatform platform;
  /* The register XX the instruction conditionalizes the jump on. */
  const char* reg;
  /* The address size held in register XX. */
  const NaClIFlag addr_size;
  /* Flags to add to register XX. */
  const NaClOpFlags xx_flags;
  /* The mnemonic name of the instruction. */
  const char* name;
} CondJumpInst;

/* Generates conditiona jump instructions, based on the set of specified
 * instructions.
 *
 * Parameters are:
 *   context_st - Symbol table defining other values if needed.
 *   opcode - The opcode value that defines the address jump instruction.
 *   address_insts - The address jump instructions to define.
 */
static void NaClDefCondJump(struct NaClSymbolTable* context_st,
                            uint8_t opcode,
                            CondJumpInst address_insts[],
                            size_t address_insts_size) {
  size_t i;
  int num_32_insts = 0;
  int num_64_insts = 0;
  char buffer[BUFFER_SIZE];
  struct NaClSymbolTable* st = NaClSymbolTableCreate(NACL_SMALL_ST, context_st);
  SNPRINTF(buffer, BUFFER_SIZE, "%02x", opcode);

  /* Count the number of forms for each type of instruction, and record. */
  for (i = 0; i < address_insts_size; i++) {
    switch (address_insts[i].platform) {
      case T32:
        num_32_insts++;
        break;
      case T64:
        num_64_insts++;
        break;
      case Tall:
        num_32_insts++;
        num_64_insts++;
        break;
    }
  }
  NaClDefPrefixInstChoices_32_64(NoPrefix, opcode, num_32_insts, num_64_insts);

  /* Generate each form of the instruction. */
  for (i = 0; i < address_insts_size; ++i) {
    NaClSymbolTablePutText("opcode", buffer, st);
    NaClSymbolTablePutText("name", address_insts[i].name, st);
    NaClSymbolTablePutText("reg",  address_insts[i].reg, st);
    NaClBegDefPlatform(address_insts[i].platform,
                       "@opcode: @name {%@ip}, {%@reg}, $Jb",
                       NACLi_386, st);
    NaClAddIFlags(NACL_IFLAG(address_insts[i].addr_size));
    NaClAddOpFlags(1, address_insts[i].xx_flags);
    NaClEndDef(Jump);
  }
}

/* Generates a jXXz instruction, based on the use of register XX
 * (XX in cx, ecx, rcx).
 */
static void NaClDefJumpRegZero(struct NaClSymbolTable* context_st) {
  CondJumpInst inst[3] = {
    { T32,  "cx",  AddressSize_w, NACL_EMPTY_OPFLAGS, "Jcxz" },
    { Tall, "ecx", AddressSize_v, NACL_EMPTY_OPFLAGS, "Jecxz" },
    { T64,  "rcx", AddressSize_o, NACL_EMPTY_OPFLAGS, "Jrcxz" }
  };
  NaClDefCondJump(context_st, 0xe3, inst, NACL_ARRAY_SIZE(inst));
}

/* Generates the loop, loopne, and loope instructions. */
static void NaClDefLoop(struct NaClSymbolTable* context_st) {
  /* The forms each loop instruction can have. */
  CondJumpInst inst_forms[3] = {
    { T32,  "cx",  AddressSize_w, NACL_OPFLAG(OpSet), "???" },
    { Tall, "ecx", AddressSize_v, NACL_OPFLAG(OpSet), "???" },
    { T64,  "rcx", AddressSize_o, NACL_OPFLAG(OpSet), "???" }
  };
  /* The opcode/name of each loop instruction. */
  struct {
    uint8_t opcode;
    const char* name;
  } loop_inst[3] = {
    {0xe0, "Loopne"},
    {0xe1, "Loope"},
    {0xe2, "Loop"},
  };

  /* Generate all combinations of the loop instruction. */
  size_t i;
  for (i = 0; i < NACL_ARRAY_SIZE(loop_inst); i++) {
    size_t j;
    for (j = 0; j < NACL_ARRAY_SIZE(inst_forms); j++) {
      inst_forms[j].name = loop_inst[i].name;
    }
    NaClDefCondJump(context_st, loop_inst[i].opcode,
                    inst_forms, NACL_ARRAY_SIZE(inst_forms));
  }
}

static const NaClMnemonic NaClGroup2OpcodeName[8] = {
  InstRol,
  InstRor,
  InstRcl,
  InstRcr,
  InstShl,  /* same as Sal */
  InstShr,
  InstShl,  /* same as Sal; disallowed as an alias encoding */
  InstSar
};

static void NaClDefGroup2OpcodesInModRm(struct NaClSymbolTable* context_st) {
  int i;
  struct NaClSymbolTable* st = NaClSymbolTableCreate(NACL_SMALL_ST, context_st);
  for (i = 0; i < NACL_ARRAY_SIZE(NaClGroup2OpcodeName); i++) {
    /* The "/6" encodings of "Shl" are aliases of the "/4" encodings.
     * "/4" is used by assemblers.  We disallow "/6" on the grounds of
     * minimalism.
     */
    NaClInstType insttype = i == 6 ? NACLi_ILLEGAL : NACLi_386;
    NaClMnemonic name;
    NaClSymbolTablePutInt("i", i, st);
    name = NaClGroup2OpcodeName[i];
    NaClSymbolTablePutText("name", NaClMnemonicName(name), st);
    NaClDefine("c0/@i: @name $Eb, $Ib", insttype, st, Binary);
    NaClDefine("c1/@i: @name $Ev, $Ib", insttype, st, Binary);
    NaClDefine("d0/@i: @name $Eb, 1", insttype, st, Binary);
    NaClDefine("d1/@i: @name $Ev, 1", insttype, st, Binary);
    NaClDefine("d2/@i: @name $Eb, %cl", insttype, st, Binary);
    NaClDefine("d3/@i: @name $Ev, %cl", insttype, st, Binary);
  }
  NaClSymbolTableDestroy(st);
}

void NaClDefOneByteInsts(struct NaClSymbolTable* st) {

  NaClDefInstPrefix(NoPrefix);

  NaClBuildBinaryOps_00_05(0x00, NACLi_386, InstAdd);
  NaClDef_32("06: Push {%@sp}, %es", NACLi_386, st, Push);
  NaClDef_64("06: Invalid", NACLi_INVALID, st, Other);
  NaClDef_32("07: Pop  {%@sp}, %es", NACLi_386, st, Pop);
  NaClDef_64("07: Invalid", NACLi_INVALID, st, Other);
  NaClBuildBinaryOps_00_05(0x08, NACLi_386, InstOr);
  NaClDef_32("0e: Push {%@sp}, %cs", NACLi_386, st, Push);
  NaClDef_64("0e: Invalid", NACLi_INVALID, st, Other);
  /* 0x0F is a two-byte opcode prefix. */
  NaClDefine("0f: Invalid", NACLi_INVALID, st, Other);
  NaClBuildBinaryOps_00_05(0x10, NACLi_386, InstAdc);
  NaClDef_32("16: Push {%@sp}, %ss", NACLi_386, st, Push);
  NaClDef_64("16: Invalid", NACLi_INVALID, st, Other);
  NaClDef_32("17: Pop  {%@sp}, %ss", NACLi_386, st, Pop);
  NaClDef_64("17: Invalid", NACLi_INVALID, st, Other);
  NaClBuildBinaryOps_00_05(0x18, NACLi_386, InstSbb);
  NaClDef_32("1e: Push {%@sp}, %ds", NACLi_386, st, Push);
  NaClDef_64("1e: Invalid", NACLi_INVALID, st, Other);
  NaClDef_32("1f: Pop  {%@sp}, %ds", NACLi_386, st, Pop);
  NaClDef_64("1f: Invalid", NACLi_INVALID, st, Other);
  NaClBuildBinaryOps_00_05(0x20, NACLi_386, InstAnd);
  /* 26: segment ES prefix */
  NaClDefine("26: Invalid", NACLi_INVALID, st, Other);
  NaClDef_32("27: Daa", NACLi_386, st, Other);
  NaClDef_64("27: Invalid", NACLi_INVALID, st, Other);
  NaClBuildBinaryOps_00_05(0x28, NACLi_386, InstSub);
  /* 2e: segment CS prefix*/
  NaClDefine("2e: Invalid", NACLi_INVALID, st, Other);
  NaClDef_32("2f: Das", NACLi_386, st, Other);
  NaClDef_64("2f: Invalid", NACLi_INVALID, st, Other);
  NaClBuildBinaryOps_00_05(0x30, NACLi_386, InstXor);
  /* 36: segment SS prefix */
  NaClDefine("36: Invalid", NACLi_INVALID, st, Other);
  NaClDef_32("37: Aaa {%al}", NACLi_386, st, UnaryUpdate);
  NaClDef_64("37: Invalid", NACLi_INVALID, st, Other);
  NaClBuildBinaryOps_00_05(0x38, NACLi_386, InstCmp);
  /* Ox3e segment DS prefix */
  NaClDefine("3e: Invalid", NACLi_INVALID, st, Other);
  NaClDef_32("3f: Aas", NACLi_386, st, Other);
  NaClDef_64("3f: Invalid", NACLi_INVALID, st, Other);
  NaClDefIncOrDec_00_07(0x40, InstInc, st);
  NaClDefIncOrDec_00_07(0x48, InstDec, st);
  NaClDefPushOrPop_00_07(0x50, InstPush, st);
  NaClDefPushOrPop_00_07(0x58, InstPop, st);
  NaClDefPrefixInstChoices_32_64(NoPrefix, 0x60, 2, 1);
  NaClBegD32("60: Pusha {%@sp}, {%gp7}", NACLi_386, st);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w));
  NaClEndDef(Push);
  NaClBegD32("60: Pushad {%@sp}, {%gp7}", NACLi_386, st);
  NaClAddIFlags(NACL_IFLAG(OperandSize_v));
  NaClEndDef(Push);
  NaClDefPrefixInstChoices_32_64(NoPrefix, 0x61, 2, 1);
  NaClDef_64("60: Invalid", NACLi_INVALID, st, Other);
  NaClBegD32("61: Popa {%@sp}, {%gp7}", NACLi_386, st);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w));
  NaClEndDef(Pop);
  NaClBegD32("61: Popad {%@sp}, {%gp7}", NACLi_386, st);
  NaClAddIFlags(NACL_IFLAG(OperandSize_v));
  NaClEndDef(Pop);
  NaClDef_64("61: Invalid", NACLi_INVALID, st, Other);
  NaClDef_32("62: Bound $Gv, $Ma", NACLi_386, st, Uses);
  NaClDef_64("62: Invalid", NACLi_INVALID, st, Other);
  NaClDef_32("63: Arpl $Ew, $Gw", NACLi_SYSTEM, st, Binary);
  NaClDef_64("63: Movsxd $Gv, $Ed", NACLi_386, st, Move);
  /* 64: segment FS prefix */
  NaClDefine("64: Invalid", NACLi_INVALID, st, Other);
  /* 65: segment GS prefix */
  NaClDefine("65: Invalid", NACLi_INVALID, st, Other);
  /* 66: data size prefix */
  NaClDefine("66: Invalid", NACLi_INVALID, st, Other);
  /* 67: address size prefix */
  NaClDefine("67: Invalid", NACLi_INVALID, st, Other);
  NaClDefine("68: Push {%@sp} $Iz", NACLi_386, st, Push);
  NaClDefine("69: Imul $Gv, $Ev, $Iz", NACLi_386, st, Binary);
  NaClDefine("6a: Push {%@sp} $Ib", NACLi_386, st, Push);
  NaClDefine("6b: Imul $Gv, $Ev, $Ib", NACLi_386, st, Binary);
  NaClDefine("6c: Insb {$Yb}, {%dx}", NACLi_386, st, Move);
  NaClDefPrefixInstChoices(NoPrefix, 0x6D, 2);
  NaClDefine("6d: Insw {$Yzw}, {%dx}", NACLi_386, st, Move);
  NaClDefine("6d: Insd {$Yzd}, {%dx}", NACLi_386, st, Move);
  NaClDefine("6e: Outsb {%dx}, {$Xb}", NACLi_386, st, Uses);
  NaClDefPrefixInstChoices(NoPrefix, 0x6F, 2);
  NaClDefine("6f: Outsw {%dx}, {$Xzw}", NACLi_386, st, Uses);
  NaClDefine("6f: Outsd {%dx}, {$Xzd}", NACLi_386, st, Uses);
  NaClDefJump8Opcode(0x70, InstJo, st);
  NaClDefJump8Opcode(0x71, InstJno, st);
  NaClDefJump8Opcode(0x72, InstJb, st);
  NaClDefJump8Opcode(0x73, InstJnb, st);
  NaClDefJump8Opcode(0x74, InstJz, st);
  NaClDefJump8Opcode(0x75, InstJnz, st);
  NaClDefJump8Opcode(0x76, InstJbe, st);
  NaClDefJump8Opcode(0x77, InstJnbe, st);
  NaClDefJump8Opcode(0x78, InstJs, st);
  NaClDefJump8Opcode(0x79, InstJns, st);
  NaClDefJump8Opcode(0x7a, InstJp, st);
  NaClDefJump8Opcode(0x7b, InstJnp, st);
  NaClDefJump8Opcode(0x7c, InstJl, st);
  NaClDefJump8Opcode(0x7d, InstJnl, st);
  NaClDefJump8Opcode(0x7e, InstJle, st);
  NaClDefJump8Opcode(0x7f, InstJnle, st);
  /* Defines 80-83, using opcode in mod/rm */
  NaClDefGroup1OpcodesInModRm(st);
  NaClDefine("84: Test $Eb, $Gb", NACLi_386, st, Compare);
  NaClDefine("85: Test $Ev, $Gv", NACLi_386, st, Compare);
  NaClDefine("86: Xchg $Eb, $Gb", NACLi_386, st, Exchange);
  NaClDefine("87: Xchg $Ev, $Gv", NACLi_386, st, Exchange);
  NaClDefine("88: Mov $Eb, $Gb", NACLi_386, st, Move);
  NaClDefine("89: Mov $Ev, $Gv", NACLi_386, st, Move);
  NaClDefine("8a: Mov $Gb, $Eb", NACLi_386, st, Move);
  NaClDefine("8b: Mov $Gv, $Ev", NACLi_386, st, Move);
  NaClDefine("8c: Mov $Mw/Rv, $Sw", NACLi_386, st, Move);
  NaClBegDef("8d: Lea $Gv, $M", NACLi_386, st);
  NaClAddOpFlags(1, NACL_OPFLAG(OpAddress));
  NaClEndDef(Lea);
  NaClDefine("8e: Mov $Sw, $Ew", NACLi_386, st, Move);
  NaClDefine("8f/0: Pop {%@sp}, $Ev", NACLi_386, st, Pop);
  NaClDefine("8f/r: Invalid", NACLi_INVALID, st, Other);
  /* 90-97: exchange register. */
  NaClDefXchgRegister();
  NaClDefPrefixInstChoices_32_64(NoPrefix, 0x98, 2, 3);
  NaClBegDef("98: Cbw {%ax}, {%al}", NACLi_386, st);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w));
  NaClEndDef(Move);
  NaClBegDef("98: Cwde {%eax}, {%ax}", NACLi_386, st);
  NaClAddIFlags(NACL_IFLAG(OperandSize_v));
  NaClAddOpFlags(0, NACL_OPFLAG(OperandSignExtends_v));
  NaClEndDef(Move);
  NaClBegD64("98: Cdqe {%rax}, {%eax}", NACLi_386, st);
  NaClAddIFlags(NACL_IFLAG(OperandSize_o));
  NaClEndDef(Move);
  NaClDefPrefixInstChoices_32_64(NoPrefix, 0x99, 2, 3);
  NaClBegDef("99: Cwd {%dx}, {%ax}", NACLi_386, st);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w));
  NaClEndDef(Move);
  NaClBegDef("99: Cdq {%edx}, {%eax}", NACLi_386, st);
  NaClAddIFlags(NACL_IFLAG(OperandSize_v));
  NaClEndDef(Move);
  NaClBegD64("99: Cqo {%rdx}, {%rax}", NACLi_386, st);
  NaClAddIFlags(NACL_IFLAG(OperandSize_o));
  NaClEndDef(Move);
  NaClDef_32("9a: Call {%@ip}, {%@sp}, $Ap", NACLi_386, st, Call);
  NaClDef_64("9a: Invalid", NACLi_INVALID, st, Other);
  NaClDefine("9b: Fwait", NACLi_X87, st, Other);
  NaClDefPrefixInstChoices(NoPrefix, 0x9c, 2);
  NaClDefine("9c: Pushf {%@sp}, {$Fvw}", NACLi_386, st, Push);
  NaClDef_32("9c: Pushfd {%@sp}, {$Fvd}", NACLi_386, st, Push);
  NaClDef_64("9c: Pushfq {%@sp}, {$Fvq}", NACLi_386, st, Push);
  NaClDefPrefixInstChoices(NoPrefix, 0x9d, 2);
  NaClDefine("9d: Popf {%@sp}, {$Fvw}", NACLi_386, st, Pop);
  NaClDef_32("9d: Popfd {%@sp}, {$Fvd}", NACLi_386, st, Pop);
  NaClDef_64("9d: Popfq {%@sp}, {$Fvq}", NACLi_386, st, Pop);
  NaClDefine("9e: Sahf {%ah}", NACLi_LAHF, st, Uses);
  NaClDefine("9f: Lahf {%ah}", NACLi_LAHF, st, UnarySet);
  NaClDefine("a0: Mov %al, $Ob", NACLi_386, st, Move);
  NaClDefine("a1: Mov $rAXv, $Ov", NACLi_386, st, Move);
  NaClDefine("a2: Mov $Ob, %al", NACLi_386, st, Move);
  NaClDefine("a3: Mov $Ov, $rAXv", NACLi_386, st, Move);
  NaClDefine("a4: Movsb $Yb, $Xb", NACLi_386, st, Move);
  NaClDefPrefixInstChoices_32_64(NoPrefix, 0xa5, 2, 3);
  NaClDefine("a5: Movsw $Yvw, $Xvw", NACLi_386, st, Move);
  NaClDefine("a5: Movsd $Yvd, $Xvd", NACLi_386, st, Move);
  NaClDef_64("a5: Movsq $Yvq, $Xvq", NACLi_386, st, Move);
  NaClDefine("a6: Cmpsb $Yb, $Xb", NACLi_386, st, Compare);
  NaClDefPrefixInstChoices_32_64(NoPrefix, 0xa7, 2, 3);
  NaClDefine("a7: Cmpsw $Yvw, $Xvw", NACLi_386, st, Compare);
  NaClDefine("a7: Cmpsd $Yvd, $Xvd", NACLi_386, st, Compare);
  NaClDef_64("a7: Cmpsq $Yvq, $Xvq", NACLi_386, st, Compare);
  NaClDefine("a8: Test %al, $Ib", NACLi_386, st, Compare);
  NaClDefine("a9: Test $rAXv, $Iz", NACLi_386, st, Compare);
  NaClDefine("aa: Stosb $Yb, {%al}", NACLi_386, st, Move);
  NaClDefPrefixInstChoices_32_64(NoPrefix, 0xab, 2, 3);
  NaClDefine("ab: Stosw $Yvw, {$rAXvw}", NACLi_386, st, Move);
  NaClDefine("ab: Stosd $Yvd, {$rAXvd}", NACLi_386, st, Move);
  NaClDef_64("ab: Stosq $Yvq, {$rAXvq}", NACLi_386, st, Move);
  NaClDefine("ac: Lodsb {%al}, $Xb", NACLi_386, st, Move);
  NaClDefPrefixInstChoices_32_64(NoPrefix, 0xad, 2, 3);
  NaClDefine("ad: Lodsw {$rAXvw}, $Xvw", NACLi_386, st, Move);
  NaClDefine("ad: Lodsd {$rAXvd}, $Xvd", NACLi_386, st, Move);
  NaClDef_64("ad: Lodsq {$rAXvq}, $Xvq", NACLi_386, st, Move);
  NaClDefine("ae: Scasb {%al}, $Yb", NACLi_386, st, Compare);
  NaClDefPrefixInstChoices_32_64(NoPrefix, 0xaf, 2, 3);
  NaClDefine("af: Scasw {$rAXvw}, $Yvw", NACLi_386, st, Compare);
  NaClDefine("af: Scasd {$rAXvd}, $Yvd", NACLi_386, st, Compare);
  NaClDef_64("af: Scasq {$rAXvq}, $Yvq", NACLi_386, st, Compare);
  NaClDefReg("b0+@reg: Mov $r8b, $Ib", 0, 7, NACLi_386, st, Move);
  NaClDefReg("b8+@reg: Mov $r8v, $Iv", 0, 7, NACLi_386, st, Move);
  /* 0xC0 and 0xC1 defined by NaClDefGroup2OpcodesInModRm, called below. */
  NaClDefine("c2: Ret {%@ip}, {%@sp}, $Iw", NACLi_386, st, Return);
  NaClDefine("c3: Ret {%@ip}, {%@sp}", NACLi_386, st, Return);
  NaClDef_32("c4: Les $SGz, $Mp", NACLi_386, st, Lea);
  NaClDef_64("c4: Invalid", NACLi_INVALID, st, Other);
  NaClDef_32("c5: Lds $SGz, $Mp", NACLi_386, st, Lea);
  NaClDef_64("c5: Invalid", NACLi_INVALID, st, Other);
  NaClDefine("c6/0: Mov $Eb, $Ib", NACLi_386, st, Move);
  NaClDefine("c6/r: Invalid", NACLi_INVALID, st, Other);
  NaClDefine("c7/0: Mov $Ev, $Iz", NACLi_386, st, Move);
  NaClDefine("c7/r: Invalid", NACLi_INVALID, st, Other);
  NaClDefine("c8: Enter {%@sp}, {%@bp}, $Iw, $I2b", NACLi_386, st, Call);
  NaClDefine("c9: Leave {%@sp}, {%@bp}", NACLi_386, st, Return);
  NaClDefine("ca: Ret {%@ip} {%@sp}, $Iw", NACLi_RETURN, st, Return);
  NaClDefine("cb: Ret {%@ip} {%@sp}", NACLi_RETURN, st, Return);
  NaClDefine("cc: Int3", NACLi_SYSTEM, st, Other);
  NaClDefine("cd: Int $Ib", NACLi_386, st, Uses);
  NaClDefine("ce: Into", NACLi_386, st, Other);
  NaClDefPrefixInstChoices_32_64(NoPrefix, 0xcf, 2, 3);
  NaClBegDef("cf: Iretd {%@ip} {%@sp}", NACLi_SYSTEM, st);
  NaClAddIFlags(NACL_IFLAG(OperandSize_v));
  NaClEndDef(Return);
  NaClBegD64("cf: Iretq {%@ip} {%@sp}", NACLi_SYSTEM, st);
  NaClAddIFlags(NACL_IFLAG(OperandSize_o));
  NaClEndDef(Return);
  NaClBegDef("cf: Iret {%@ip} {%@sp}", NACLi_SYSTEM, st);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w));
  NaClEndDef(Return);
  /* Group 2 - 0xD0, 0XD1, 0xD2, 0xD3 */
  NaClDefGroup2OpcodesInModRm(st);
  NaClDef_32("d4: Aam {%ax} $Ib", NACLi_386, st, Binary);
  NaClDef_64("d4: Invalid", NACLi_INVALID, st, Other);
  NaClDef_32("d5: Aad {%ax}, $Ib", NACLi_386, st, Binary);
  NaClDef_64("d5: Invalid", NACLi_INVALID, st, Other);
  /* TODO(Karl): Intel manual (see comments above) has a blank entry
   * for opcode 0xd6, which states that blank entries in the tables
   * correspond to reserved (undefined) values Should we treat this
   * accordingly? (currently illegal).
   */
  NaClDef_32("d6: Salc {%al}", NACLi_386, st, Other);
  NaClDef_64("d6: Invalid", NACLi_INVALID, st, Other);
  NaClDefine("d7: Xlat {%al}, {%DS_EBX}", NACLi_386, st, Binary);
  /* Note: 0xd8 through 0xdf is defined in ncdecodeX87.c */
  NaClDefLoop(st); /* opcodes e0, e1, and e2 */
  /* 0xe3 - jXXz */
  NaClDefJumpRegZero(st);
  NaClDefine("e4: In %al, $Ib", NACLi_386, st, Move);
  NaClDefine("e5: In $rAXv, $Ib", NACLi_386, st, Move);
  NaClDefine("e6: Out $Ib, %al", NACLi_386, st, Move);
  NaClDefine("e7: Out $Ib, $rAXv", NACLi_386, st, Move);
  /* Note: We special case the 66 prefix on direct jumps and calls, by
   * explicitly disallowing 16-bit direct branches. This is done partly because
   * some versions (within x86-64) are not supported in such cases. However,
   * NaCl also doesn't want to allow 16-bit direct branches.
   */
  NaClDefine("e8: Call {%@ip}, {%@sp}, $Jzd", NACLi_386, st, Call);
  NaClDefine("e9: Jmp {%@ip}, $Jzd", NACLi_386, st, Jump);
  NaClDef_32("ea: Jmp {%@ip}, $Ap", NACLi_386, st, Jump);
  NaClDef_64("ea: Invalid", NACLi_INVALID, st, Other);
  NaClDefine("eb: Jmp {%@ip}, $Jb", NACLi_386, st, Jump);
  NaClDefine("ec: In %al, %dx", NACLi_386, st, Move);
  NaClDefine("ed: In $rAXv, %dx", NACLi_386, st, Move);
  NaClDefine("ee: Out %dx, %al", NACLi_386, st, Move);
  NaClDefine("ef: Out %dx, $rAXv", NACLi_386, st, Move);
  /* f0 : lock prefix. */
  NaClDefine("f0: Invalid", NACLi_INVALID, st, Other);
  NaClDefine("f1: Int1", NACLi_386, st, Other);
  /* f2: Repne prefix. */
  NaClDefine("f2: Invalid", NACLi_INVALID, st, Other);
  /*  f3: Rep prefix. */
  NaClDefine("f3: Invalid", NACLi_INVALID, st, Other);
  NaClDefine("f4: Hlt", NACLi_386, st, Other);
  NaClDefine("f5: Cmc", NACLi_386, st, Other);
  /* The "/1" encodings of "Test" are aliases of the "/0" encodings.
   * "/0" is used by assemblers.  We disallow "/1" on the grounds of
   * minimalism.
   */
  NaClDefine("f6/0: Test $Eb, $Ib", NACLi_386, st, Compare);
  NaClDefine("f6/1: Test $Eb, $Ib", NACLi_ILLEGAL, st, Compare);
  NaClDefine("f6/2: Not $Eb", NACLi_386, st, UnaryUpdate);
  NaClDefine("f6/3: Neg $Eb", NACLi_386, st, UnaryUpdate);
  NaClDefine("f6/4: Mul {%ax}, {%al}, $Eb", NACLi_386, st, Binary);
  NaClDefine("f6/5: Imul {%ax}, {%al}, $Eb", NACLi_386, st, Binary);
  NaClDefine("f6/6: Div {%ax}, {%al}, $Eb", NACLi_386, st, Binary);
  NaClDefine("f6/7: Idiv {%ax}, {%al},$Eb", NACLi_386, st, Binary);
  NaClDefine("f7/0: Test $Ev, $Iz", NACLi_386, st, Compare);
  NaClDefine("f7/1: Test $Ev, $Iz", NACLi_ILLEGAL, st, Compare);
  NaClDefine("f7/2: Not $Ev", NACLi_386, st, UnaryUpdate);
  NaClDefine("f7/3: Neg $Ev", NACLi_386, st, UnaryUpdate);
  NaClDefine("f7/4: Mul {%redx}, {%reax}, $Ev", NACLi_386, st, O2Binary);
  NaClDefine("f7/5: Imul {%redx}, {%reax}, $Ev", NACLi_386, st, O2Binary);
  NaClDefine("f7/6: Div {%redx},  {%reax}, $Ev", NACLi_386, st, O2Binary);
  NaClDefine("f7/7: Idiv {%redx}, {%reax}, $Ev", NACLi_386, st, O2Binary);
  NaClDefine("f8: Clc", NACLi_386, st, Other);
  NaClDefine("f9: Stc", NACLi_386, st, Other);
  NaClDefine("fa: Cli", NACLi_SYSTEM, st, Other);
  NaClDefine("fb: Sti", NACLi_SYSTEM, st, Other);
  NaClDefine("fc: Cld", NACLi_386, st, Other);
  NaClDefine("fd: Std", NACLi_386, st, Other);
  NaClDefine("fe/0: Inc $Eb", NACLi_386, st, UnaryUpdate);
  NaClDefine("fe/1: Dec $Eb", NACLi_386, st, UnaryUpdate);
  NaClDefine("fe/2: Invalid", NACLi_INVALID, st, Other);
  NaClDefine("fe/3: Invalid", NACLi_INVALID, st, Other);
  NaClDefine("fe/4: Invalid", NACLi_INVALID, st, Other);
  NaClDefine("fe/5: Invalid", NACLi_INVALID, st, Other);
  NaClDefine("fe/6: Invalid", NACLi_INVALID, st, Other);
  NaClDefine("fe/7: Invalid", NACLi_INVALID, st, Other);
  NaClDefine("ff/0: Inc $Ev", NACLi_386, st, UnaryUpdate);
  NaClDefine("ff/1: Dec $Ev", NACLi_386, st, UnaryUpdate);
  NaClBegDef("ff/2: Call {%@ip}, {%@sp}, $Ev", NACLi_386, st);
  NaClAddOpFlags(2, NACL_OPFLAG(OperandNear));
  NaClEndDef(Call);
  NaClDefine("ff/3: Call {%@ip}, {%@sp}, $Mp", NACLi_386, st, Call);
  NaClBegDef("ff/4: Jmp {%@ip}, $Ev", NACLi_386, st);
  NaClAddOpFlags(1, NACL_OPFLAG(OperandNear));
  NaClEndDef(Jump);
  NaClDefine("ff/5: Jmp {%@ip}, $Mp", NACLi_386, st, Jump);
  NaClDefine("ff/6: Push {%@sp}, $Ev", NACLi_386, st, Push);
  NaClDefine("ff/7: Invalid", NACLi_INVALID, st, Other);
}
