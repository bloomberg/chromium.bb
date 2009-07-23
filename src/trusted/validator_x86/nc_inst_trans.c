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
 * Translates the recognized opcode (instruction) in the instruction state
 * into an opcode expression.
 */

#include <stdio.h>
#include <assert.h>

#include "native_client/src/trusted/validator_x86/nc_inst_trans.h"
#include "native_client/src/trusted/validator_x86/nc_inst_state.h"
#include "native_client/src/trusted/validator_x86/nc_inst_state_internal.h"
#include "native_client/src/trusted/validator_x86/ncop_exps.h"

/* To turn on debugging of instruction decoding, change value of
 * DEBUGGING to 1.
 */
#define DEBUGGING 0

#if DEBUGGING
/* Defines to execute statement(s) s if in debug mode. */
#define DEBUG(s) s
#else
/* Defines to not include statement(s) s if not in debugging mode. */
#define DEBUG(s) do { if (0) { s; } } while(0)
#endif

/* Append the given expression node onto the given vector of expression
 * nodes. Returns the appended expression node.
 */
static ExprNode* AppendExprNode(ExprNodeKind kind,
                                int32_t value,
                                ExprNodeFlags flags,
                                ExprNodeVector* vector) {
  ExprNode* node;
  assert(vector->number_expr_nodes < MAX_EXPR_NODES);
  node = &vector->node[vector->number_expr_nodes++];
  node->kind = kind;
  node->value = value;
  node->flags = flags;
  return node;
}

/* Report the given message and quit because we don't know
 * how to recover.
 */
static void FatallyLost(const char* message) {
  fprintf(stderr, "FATAL: %s\n", message);
  exit(1);
}

/* An untranslateable error has been found. report and quit.
 * Use the state to give useful information on where the
 * translator was when the error occurred.
 * Note: returns NULL of type ExprNode* so that callers can
 * make control flow happy, for those cases where the compiler
 * doesn't recognize that this function never returns.
 */
static ExprNode* FatalError(const char* message,
                           NcInstState* state) {
  fprintf(stderr,
          "FATAL: At %"PRIxPcAddress", unable to translate: %s\n",
          NcInstStateVpc(state), message);
  exit(1);
  /* NOT REACHED */
  return NULL;
}

/* Append the given constant onto the given vector of expression
 * nodes. Returns the appended expression node.
 */
static ExprNode* AppendConstant(uint64_t value, ExprNodeFlags flags,
                                ExprNodeVector* vector) {
  uint32_t val1;
  uint32_t val2;
  SplitExprConstant(value, &val1, &val2);
  if (val2 == 0) {
    return AppendExprNode(ExprConstant, val1, flags, vector);
  } else {
    ExprNode* root = AppendExprNode(ExprConstant64, 0, flags, vector);
    AppendExprNode(ExprConstant, val1, ExprFlag(ExprUnsignedHex), vector);
    AppendExprNode(ExprConstant, val2, ExprFlag(ExprUnsignedHex), vector);
    return root;
  }
}

/* Define the number of general purpose registers defined for the given
 * subarchitecture.
 */
#if NACL_TARGET_SUBARCH == 64
#define REGISTER_TABLE_SIZE 16
#else
#define REGISTER_TABLE_SIZE 8
#endif

/* Define the available 8-bit registers, for the given subarchitecture.
 * Note: The order is important, and is based on the indexing values used
 * in the ModRm and SIB bytes (and the REX prefix if appropriate).
 */
static const OperandKind RegisterTable8[REGISTER_TABLE_SIZE] = {
  RegAL,
  RegCL,
  RegDL,
  RegBL,
#if NACL_TARGET_SUBARCH == 64
  RegSPL,
  RegBPL,
  RegSIL,
  RegDIL,
  RegR8L,
  RegR9L,
  RegR10L,
  RegR11L,
  RegR12L,
  RegR13L,
  RegR14L,
  RegR15L
#else
  RegAH,
  RegCH,
  RegDH,
  RegBH
#endif
};

/* Define the available 16-bit registers, for the given subarchitecture.
 * Note: The order is important, and is based on the indexing values used
 * in the ModRm and SIB bytes (and the REX prefix if appropriate).
 */
static const OperandKind RegisterTable16[REGISTER_TABLE_SIZE] = {
  RegAX,
  RegCX,
  RegDX,
  RegBX,
  RegSP,
  RegBP,
  RegSI,
  RegDI,
#if NACL_TARGET_SUBARCH == 64
  RegR8W,
  RegR9W,
  RegR10W,
  RegR11W,
  RegR12W,
  RegR13W,
  RegR14W,
  RegR15W,
#endif
};

/* Define the available 32-bit registers, for the given subarchitecture.
 * Note: The order is important, and is based on the indexing values used
 * in the ModRm and SIB bytes (and the REX prefix if appropriate).
 */
static const OperandKind RegisterTable32[REGISTER_TABLE_SIZE] = {
  RegEAX,
  RegECX,
  RegEDX,
  RegEBX,
  RegESP,
  RegEBP,
  RegESI,
  RegEDI,
#if NACL_TARGET_SUBARCH == 64
  RegR8D,
  RegR9D,
  RegR10D,
  RegR11D,
  RegR12D,
  RegR13D,
  RegR14D,
  RegR15D
#endif
};

/* Define the available 64-bit registers, for the given subarchitecture.
 * Note: The order is important, and is based on the indexing values used
 * in the ModRm and SIB bytes (and the REX prefix if appropriate).
 */
#if NACL_TARGET_SUBARCH == 64
static const OperandKind RegisterTable64[REGISTER_TABLE_SIZE] = {
  RegRAX,
  RegRCX,
  RegRDX,
  RegRBX,
  RegRSP,
  RegRBP,
  RegRSI,
  RegRDI,
  RegR8,
  RegR9,
  RegR10,
  RegR11,
  RegR12,
  RegR13,
  RegR14,
  RegR15
};
#endif

/* Define a type corresponding to the arrays RegisterTable8,
 * RegisterTable16, RegisterTable32, and RegisterTable64.
 */
typedef const OperandKind RegisterTableGroup[REGISTER_TABLE_SIZE];

/* Define the set of available registers, categorized by size.
 * Note: The order is important, and is based on the indexing values used
 * in the ModRm and SIB bytes (and the REX prefix if appropriate).
 */
static RegisterTableGroup* const RegisterTable[] = {
  &RegisterTable8,
  &RegisterTable16,
  &RegisterTable32,
#if NACL_TARGET_SUBARCH == 64
  &RegisterTable64
#endif
  /* TODO(karl) Add MMX registers etc. */
};


/* Define possible register categories. */
typedef enum {
  RegSize8,
  RegSize16,
  RegSize32,
  RegSize64
} RegKind;

static const char* const g_RegKindName[4] = {
  "RegSize8",
  "RegSize16",
  "RegSize32",
  "RegSize64"
};

const char* RegKindName(RegKind kind) {
  return g_RegKindName[kind];
}

static OperandKind LookupRegister(RegKind kind, int reg_index) {
  if (32 == NACL_TARGET_SUBARCH && kind == RegSize64) {
    FatallyLost("Architecture doesn't define 64 bit registers");
  }
  return (*(RegisterTable[kind]))[reg_index];
}

/* Returns the (ExprNodeFlag) size of the given register. */
static ExprNodeFlags GetRegisterSize(OperandKind register_name) {
  switch (register_name) {
  case RegAL:
  case RegBL:
  case RegCL:
  case RegDL:
  case RegAH:
  case RegBH:
  case RegCH:
  case RegDH:
  case RegDIL:
  case RegSIL:
  case RegBPL:
  case RegSPL:
  case RegR8L:
  case RegR9L:
  case RegR10L:
  case RegR11L:
  case RegR12L:
  case RegR13L:
  case RegR14L:
  case RegR15L:
    return ExprFlag(ExprSize8);
  case RegAX:
  case RegBX:
  case RegCX:
  case RegDX:
  case RegSI:
  case RegDI:
  case RegBP:
  case RegSP:
  case RegR8W:
  case RegR9W:
  case RegR10W:
  case RegR11W:
  case RegR12W:
  case RegR13W:
  case RegR14W:
  case RegR15W:
    return ExprFlag(ExprSize16);
  case RegEAX:
  case RegEBX:
  case RegECX:
  case RegEDX:
  case RegESI:
  case RegEDI:
  case RegEBP:
  case RegESP:
  case RegR8D:
  case RegR9D:
  case RegR10D:
  case RegR11D:
  case RegR12D:
  case RegR13D:
  case RegR14D:
  case RegR15D:
    return ExprFlag(ExprSize32);
  case RegCS:
  case RegDS:
  case RegSS:
  case RegES:
  case RegFS:
  case RegGS:
    return ExprFlag(ExprSize16);
  case RegEIP:
    return ExprFlag(ExprSize32);
  case RegRIP:
    return ExprFlag(ExprSize64);
  case RegRAX:
  case RegRBX:
  case RegRCX:
  case RegRDX:
  case RegRSI:
  case RegRDI:
  case RegRBP:
  case RegRSP:
  case RegR8:
  case RegR9:
  case RegR10:
  case RegR11:
  case RegR12:
  case RegR13:
  case RegR14:
  case RegR15:
    return ExprFlag(ExprSize64);
  default:
    return 0;
  }
}

/* Appends the given kind of register onto the vector of expression nodes.
 * Returns the appended register.
 */
static ExprNode* AppendRegister(OperandKind r, ExprNodeVector* vector) {
  ExprNode* node;
  DEBUG(printf("append register %s\n", OperandKindName(r)));
  node = AppendExprNode(ExprRegister, r, GetRegisterSize(r), vector);
  DEBUG(PrintExprNodeVector(stdout, vector));
  return node;
}

/* Given the given register kind, and the corresponding index, append
 * the appropriate register onto the vector of expression nodes.
 * Returns the appended register
 */
static ExprNode* AppendRegisterKind(NcInstState* state,
                                    RegKind kind, int reg_index) {
  DEBUG(printf("AppendRegisterKind(%d, %d) = %s\n",
               (int) kind, reg_index, RegKindName(kind)));
  return AppendRegister(LookupRegister(kind, reg_index), &state->nodes);
}

/* Given an operand of the corresponding opcode instruction of the
 * given state, return what kind of register should be used, based
 * on the operand size.
 */
static RegKind ExtractOperandRegKind(NcInstState* state,
                                     Operand* operand) {
  if (operand->kind >= Gb_Operand && operand->kind <= Go_Operand) {
    return (RegKind) operand->kind - Gb_Operand;
  } else if (state->opcode->flags & InstFlag(OperandSize_b)) {
    return RegSize8;
  } else if (state->operand_size == 1) {
    return RegSize8;
  } else if (state->operand_size == 4) {
    return RegSize32;
  } else if (state->operand_size == 2) {
    return RegSize16;
  } else if (state->operand_size == 8) {
    return RegSize64;
  } else {
    return RegSize32;
  }
}

/* Given an address of the corresponding opcode instruction of the
 * given state, return what kind of register should be used, based
 * on the operand size.
 */
static RegKind ExtractAddressRegKind(NcInstState* state,
                                     Operand* operand) {
  if (state->address_size == 16) {
    return RegSize16;
  } else if (state->address_size == 64) {
    return RegSize64;
  } else {
    return RegSize32;
  }
}

/* Given we want to translate an operand (of the form G_Operand),
 * for the given register index, generate the corresponding register
 * expression, and append it to the vector of expression nodes.
 * Returns the appended register.
 */
static ExprNode* AppendOperandRegister(
    NcInstState* state, Operand* operand, int reg_index) {
  DEBUG(printf("Translate register %d\n", reg_index));
  return AppendRegisterKind(state,
                            ExtractOperandRegKind(state, operand),
                            reg_index);
}

/* For the given instruction state, and the corresponding 3-bit specification
 * of a register, update it to a 4-bit specification, based on the REX.R bit.
 */
static int GetRexRRegister(NcInstState* state, int reg) {
  DEBUG(printf("Get GenRexRRegister\n"));
  if (NACL_TARGET_SUBARCH == 64 && (state->rexprefix & 0x4)) {
    reg += 8;
  }
  return reg;
}

/* For the given instruction state, and the corresponding 3-bit specification
 * of a register, update it to a 4-bit specification, based on the REX.X bit.
 */
static int GetRexXRegister(NcInstState* state, int reg) {
  DEBUG(printf("Get GenRexXRegister\n"));
  if (NACL_TARGET_SUBARCH == 64 && (state->rexprefix & 0x2)) {
    reg += 8;
  }
  return reg;
}

/* For the given instruvtion state, and the corresponding 3-bit specification
 * of a register, update it to a 4-bit specification, based on the REX.B bit.
 */
static int GetRexBRegister(NcInstState* state, int reg) {
  DEBUG(printf("Get GenRexBRegister\n"));
  if (NACL_TARGET_SUBARCH == 64 && (state->rexprefix & 0x1)) {
    DEBUG(printf("rexprefix == %02x\n", state->rexprefix));
    reg += 8;
  }
  return reg;
}

/* Return the general purpose register associated with the modrm.reg
 * field.
 */
static int GetGenRegRegister(NcInstState* state) {
  DEBUG(printf("Get GenRegRegister\n"));
  return GetRexRRegister(state, modrm_reg(state->modrm));
}

/* Return the general purpose register associated with the modrm.rm
 * field.
 */
static int GetGenRmRegister(NcInstState* state) {
  DEBUG(printf("Get GenRmRegister\n"));
  return GetRexBRegister(state, modrm_rm(state->modrm));
}

/* Get the register index from the difference of the opcode, and
 * its opcode base.
 */
static ExprNode* AppendOpcodeBaseRegister(
    NcInstState* state, Operand* operand) {
  /* Note: Difference held as first operand (by convention). */
  int reg_index;
  assert(state->opcode->num_operands > 1);
  reg_index = state->opcode->operands[0].kind - OpcodeBaseMinus0;
  assert(reg_index >= 0 && reg_index < 8);
  DEBUG(printf("Translate opcode base register %d\n", reg_index));
  return AppendRegisterKind(state, ExtractOperandRegKind(state, operand),
                            GetRexBRegister(state, reg_index));
}

/* Model the extraction of a displacement value and the associated flags. */
typedef struct {
  uint64_t value;
  ExprNodeFlags flags;
} Displacement;

static void InitializeDisplacement(uint64_t value, ExprNodeFlags flags,
                                   Displacement* displacement) {
  displacement->value = value;
  displacement->flags = flags;
}

/* Extract the binary value from the specified bytes of the instruction. */
uint64_t ExtractUnsignedBinaryValue(NcInstState* state,
                                    int start_byte, int num_bytes) {
  int i;
  uint64_t value = 0;
  for (i = 0; i < num_bytes; ++i) {
    uint8_t byte = state->mpc[start_byte + i];
    value += (((uint64_t) byte) << (i * 8));
  }
  return value;
}

int64_t ExtractSignedBinaryValue(NcInstState* state,
                                 int start_byte, int num_bytes) {
  int i;
  int64_t value = 0;
  for (i = 0; i < num_bytes; ++i) {
    uint8_t byte = state->mpc[start_byte + i];
    value |= (((uint64_t) byte) << (i * 8));
  }
  return value;
}

/* Given the number of bytes for a literal constant, return the corresponding
 * expr node flags that represent the value of the parsed bytes.
 */
static ExprNodeFlags GetExprSizeFlagForBytes(uint8_t num_bytes) {
  switch (num_bytes) {
    case 1:
      return ExprFlag(ExprSize8);
      break;
    case 2:
      return ExprFlag(ExprSize16);
      break;
    case 4:
      return ExprFlag(ExprSize32);
    default:
      return 0;
  }
}

/* Given the corresponding instruction state, return the
 * corresponding displacement value, and any expression node
 * flags that should be associated with the displacement value.
 */
static void ExtractDisplacement(NcInstState* state,
                                Displacement* displacement) {
  /* First compute the displacement value. */
  displacement->value = ExtractUnsignedBinaryValue(state,
                                                   state->first_disp_byte,
                                                   state->num_disp_bytes);

  /* Now compute any appropriate flags to be associated with the value. */
  displacement->flags =
      ExprFlag(ExprSignedHex) | GetExprSizeFlagForBytes(state->num_disp_bytes);
}

/* Append the displacement value of the given instruction state
 * onto the vector of expression nodes. Returns the appended displacement
 * value.
 */
static ExprNode* AppendDisplacement(NcInstState* state) {
  Displacement displacement;
  DEBUG(printf("append displacement\n"));
  ExtractDisplacement(state, &displacement);
  return AppendConstant(displacement.value, displacement.flags, &state->nodes);
}

/* Get the binary value denoted by the immediate bytes of the state. */
static uint64_t ExtractUnsignedImmediate(NcInstState* state) {
  return ExtractUnsignedBinaryValue(state,
                                    state->first_imm_byte,
                                    state->num_imm_bytes);
}

/* Get the binary value denoted by the immediate bytes of the state. */
static int64_t ExtractSignedImmediate(NcInstState* state) {
  return ExtractSignedBinaryValue(state,
                                  state->first_imm_byte,
                                  state->num_imm_bytes);
}

/* Append the immediate value of the given instruction state onto
 * The vector of expression nodes. Returns the appended immediate value.
 */
static ExprNode* AppendImmediate(NcInstState* state) {
  ExprNodeFlags flags;

  /* First compute the immediate value. */
  uint64_t value;
  DEBUG(printf("append immediate\n"));
  value = ExtractUnsignedImmediate(state);

  /* Now compute any appropriate flags to be associated with the immediate
   * value.
   */
  flags =
      ExprFlag(ExprUnsignedHex) | GetExprSizeFlagForBytes(state->num_imm_bytes);

  /* Append the generated immediate value onto the vector. */
  return AppendConstant(value, flags,  &state->nodes);
}

/* Append the immediate value of the given instruction as the displacement
 * of a memory offset.
 */
static ExprNode* AppendMemoryOffsetImmediate(NcInstState* state) {
  ExprNodeFlags flags;
  uint64_t value;
  ExprNode* root;
  DEBUG(printf("append memory offset immediate\n"));
  root = AppendExprNode(ExprMemOffset, 0, 0, &state->nodes);

  AppendRegister(RegUnknown, &state->nodes);
  AppendRegister(RegUnknown, &state->nodes);
  AppendConstant(1, ExprFlag(ExprSize8), &state->nodes);
  value = ExtractUnsignedImmediate(state);
  DEBUG(printf("value = 0x%016"PRIx64"\n", value));
  flags =
      ExprFlag(ExprUnsignedHex) | GetExprSizeFlagForBytes(state->num_imm_bytes);
  AppendConstant(value, flags, &state->nodes);
  return root;
}

/* Compute the (relative) immediate value defined by the difference
 * between the PC and the immedaite value of the instruction.
 */
static ExprNode* AppendRelativeImmediate(NcInstState* state) {
  PcNumber next_pc = (PcNumber) state->vpc + state->length;
  PcNumber val = (PcNumber) ExtractSignedImmediate(state);

  DEBUG(printf("append relative immediate\n"));

  /* Sign extend value */
  switch (state->num_imm_bytes) {
    case 1:
      val = next_pc + (int8_t) val;
      break;
    case 2:
      val = next_pc + (int16_t) val;
      break;
    case 4:
      val = next_pc + (int32_t) val;
      break;
    default:
      assert(0);
      break;
  }

  return AppendConstant(val, ExprFlag(ExprUnsignedHex), &state->nodes);
}

/* Append a memory offset for the given memory offset defined by
 * the formula "base + index*scale + displacement". If no index
 * is used, its value should be RegUnknown. Argument displacement_flags
 * are flags that should be associated with the generated displacement
 * value
 */
static ExprNode* AppendMemoryOffset(NcInstState* state,
                                    OperandKind base,
                                    OperandKind index,
                                    uint8_t scale,
                                    Displacement* displacement) {
  ExprNode* root;
  ExprNode* n;

  DEBUG(printf("memory offset(%s + %s * %d +  %"PRId64")\n",
               OperandKindName(base),
               OperandKindName(index),
               scale,
               displacement->value));
  if (NACL_TARGET_SUBARCH == 64) {
    root = AppendExprNode(ExprMemOffset, 0, 0, &state->nodes);
  } else {
    /* Need to add segmentation base. */
    root = AppendExprNode(ExprSegmentAddress, 0, 0, &state->nodes);
    n = AppendRegister(((base == RegBP || base == RegEBP) ? RegSS : RegDS),
                       &state->nodes);
    n->flags |= ExprFlag(ExprUsed);
    AppendExprNode(ExprMemOffset, 0, 0, &state->nodes);
  }
  n = AppendRegister(base, &state->nodes);
  if (base != RegUnknown) {
    n->flags |= ExprFlag(ExprUsed);
  }
  n = AppendRegister(index, &state->nodes);
  if (index == RegUnknown) {
    /* Scale not applicable, check that value is 1. */
    assert(scale == 1);
  } else {
    n->flags |= ExprFlag(ExprUsed);
  }
  AppendConstant(scale, ExprFlag(ExprSize8), &state->nodes);
  AppendConstant(displacement->value, displacement->flags, &state->nodes);
  DEBUG(printf("finished appending memory offset:\n"));
  DEBUG(PrintExprNodeVector(stdout, &state->nodes));
  return root;
}

/* Define the possible scaling factors that can be defined in the
 * SIB byte of the parsed instruction.
 */
static uint8_t sib_scale[4] = { 1, 2, 4, 8 };

/* Extract out the expression defined by the SIB byte of the instruction
 * in the given instruction state, and append it to the vector of
 * expression nodes. Return the corresponding expression node that
 * is the root of the appended expression.
 */
static ExprNode* AppendSib(NcInstState* state) {
  /* TODO(karl) Add effect of REX prefix to this (see table 2-5
   * of Intel Manual).
   */
  int index = sib_index(state->sib);
  int base = sib_base(state->sib);
  int scale = 1;
  RegKind kind = NACL_TARGET_SUBARCH == 64 ? RegSize64 : RegSize32;
  OperandKind base_reg = RegUnknown;
  OperandKind index_reg = RegUnknown;
  Displacement displacement;
  DEBUG(printf("append sib:\n"));
  InitializeDisplacement(0, 0, &displacement);
  if (0x5 == base) {
    switch (modrm_mod(state->modrm)) {
      case 0:
        break;
      case 1:
      case 2:
        base_reg = RegEBP;
        break;
      default:
        return FatalError("SIB value", state);
    }
  } else {
    base_reg = LookupRegister(kind,
                              GetRexBRegister(state, sib_base(state->sib)));
  }
  if (0x4 != index) {
    index_reg = LookupRegister(kind, GetRexXRegister(state, index));
    scale = sib_scale[sib_ss(state->sib)];
  }
  if (state->num_disp_bytes > 0) {
    ExtractDisplacement(state, &displacement);
  } else {
    displacement.flags = ExprFlag(ExprSize8);
  }
  return AppendMemoryOffset(state, base_reg, index_reg, scale, &displacement);
}

/* Get the Effective address in the mod/rm byte, if the modrm.mod field
 * is 00, and append it to the vector of expression nodes. Operand is
 * the corresponding operand of the opcode associated with the instruction
 * of the given state that corresponds to the effective address. Returns
 * the root of the appended effective address.
 */
static ExprNode* AppendMod00EffectiveAddress(
    NcInstState* state, Operand* operand) {
  DEBUG(printf("Translate modrm(%02x).mod == 00\n", state->modrm));
  switch (modrm_rm(state->modrm)) {
    case 4:
      return AppendSib(state);
    case 5:
      if (NACL_TARGET_SUBARCH == 64) {
        Displacement displacement;
        ExtractDisplacement(state, &displacement);
        return AppendMemoryOffset(state,
                                  RegRIP,
                                  RegUnknown,
                                  1,
                                  &displacement);
      } else {
        return AppendDisplacement(state);
      }
    default: {
      Displacement displacement;
      InitializeDisplacement(0, ExprFlag(ExprSize8), &displacement);
      return AppendMemoryOffset(state,
                                LookupRegister(
                                    ExtractAddressRegKind(state, operand),
                                    GetGenRmRegister(state)),
                                RegUnknown,
                                1,
                                &displacement);
    }
  }
  /* NOT REACHED */
  return NULL;
}

/* Get the Effective address in the mod/rm byte, if the modrm.mod field
 * is 01, and append it to the vector of expression nodes. Operand is
 * the corresponding operand of the opcode associated with the instruction
 * of the given state that corresponds to the effective address. Returns
 * the root of the appended effective address.
 */
static ExprNode* AppendMod01EffectiveAddress(
    NcInstState* state, Operand* operand) {
  DEBUG(printf("Translate modrm(%02x).mod == 01\n", state->modrm));
  if (4 == modrm_rm(state->modrm)) {
    return AppendSib(state);
  } else {
    Displacement displacement;
    ExtractDisplacement(state, &displacement);
    return AppendMemoryOffset(state,
                              LookupRegister(
                                  ExtractAddressRegKind(state, operand),
                                  GetGenRmRegister(state)),
                              RegUnknown,
                              1,
                              &displacement);
  }
}

/* Get the Effective address in the mod/rm byte, if the modrm.mod field
 * is 10, and append it to the vector of expression nodes. Operand is
 * the corresponding operand of the opcode associated with the instruction
 * of the given state that corresponds to the effective address. Returns
 * the root of the appended effective address.
 */
static ExprNode* AppendMod10EffectiveAddress(
    NcInstState* state, Operand* operand) {
  DEBUG(printf("Translate modrm(%02x).mod == 10\n", state->modrm));
  if (4 == modrm_rm(state->modrm)) {
    return AppendSib(state);
  } else {
    Displacement displacement;
    OperandKind base =
        LookupRegister(ExtractAddressRegKind(state, operand),
                       GetGenRmRegister(state));
    ExtractDisplacement(state, &displacement);
    return AppendMemoryOffset(state, base, RegUnknown, 1, &displacement);
  }
}

/* Get the Effective address in the mod/rm byte, if the modrm.mod field
 * is 11, and append it to the vector of expression nodes. Operand is
 * the corresponding operand of the opcode associated with the instruction
 * of the given state that corresponds to the effective address. Returns
 * the root of the appended effective address.
 */
static ExprNode* AppendMod11EffectiveAddress(
    NcInstState* state, Operand* operand) {
  DEBUG(printf("Translate modrm(%02x).mod == 11\n", state->modrm));
  return AppendOperandRegister(state,
                               operand,
                               GetGenRmRegister(state));
}

/* Given the corresponding operand of the opcode associated with the
 * instruction of the given state, append the corresponding expression
 * nodes that it corresponds to. Returns the root of the corresponding
 * appended expression tree.
 */
static ExprNode* AppendOperand(NcInstState* state, Operand* operand) {
  DEBUG(printf("append operand %s\n", OperandKindName(operand->kind)));
  switch (operand->kind) {
    case E_Operand:
    case Eb_Operand:
    case Ew_Operand:
    case Ev_Operand:
    case Eo_Operand:
      /* TODO(karl) Should we add limitations that simple registers
       * not allowed in M_Operand cases?
       */
    case M_Operand:
    case Mb_Operand:
    case Mw_Operand:
    case Mv_Operand:
    case Mo_Operand:
      switch(modrm_mod(state->modrm)) {
        case 0:
          return AppendMod00EffectiveAddress(state, operand);
        case 1:
          return AppendMod01EffectiveAddress(state, operand);
        case 2:
          return AppendMod10EffectiveAddress(state, operand);
        case 3:
          return AppendMod11EffectiveAddress(state, operand);
        default:
          break;
      }
      break;
    case G_Operand:
    case Gb_Operand:
    case Gw_Operand:
    case Gv_Operand:
    case Go_Operand:
      return AppendOperandRegister(state, operand, GetGenRegRegister(state));
    case G_OpcodeBase:
      return AppendOpcodeBaseRegister(state, operand);
    case I_Operand:
    case Ib_Operand:
    case Iw_Operand:
    case Iv_Operand:
    case Io_Operand:
      return AppendImmediate(state);
    case J_Operand:
    case Jb_Operand:
    case Jw_Operand:
    case Jv_Operand:
      /* TODO(karl) use operand flags OperandNear and OperandRelative to decide
       * how to process the J operand (see Intel manual for call statement).
       */
      return AppendRelativeImmediate(state);
    case O_Operand:
    case Ob_Operand:
    case Ow_Operand:
    case Ov_Operand:
    case Oo_Operand:
      return AppendMemoryOffsetImmediate(state);
    case RegUnknown:
    case RegAL:
    case RegBL:
    case RegCL:
    case RegDL:
    case RegAH:
    case RegBH:
    case RegCH:
    case RegDH:
    case RegDIL:
    case RegSIL:
    case RegBPL:
    case RegSPL:
    case RegR8L:
    case RegR9L:
    case RegR10L:
    case RegR11L:
    case RegR12L:
    case RegR13L:
    case RegR14L:
    case RegR15L:
    case RegAX:
    case RegBX:
    case RegCX:
    case RegDX:
    case RegSI:
    case RegDI:
    case RegBP:
    case RegSP:
    case RegR8W:
    case RegR9W:
    case RegR10W:
    case RegR11W:
    case RegR12W:
    case RegR13W:
    case RegR14W:
    case RegR15W:
    case RegEAX:
    case RegEBX:
    case RegECX:
    case RegEDX:
    case RegESI:
    case RegEDI:
    case RegEBP:
    case RegESP:
    case RegR8D:
    case RegR9D:
    case RegR10D:
    case RegR11D:
    case RegR12D:
    case RegR13D:
    case RegR14D:
    case RegR15D:
    case RegCS:
    case RegDS:
    case RegSS:
    case RegES:
    case RegFS:
    case RegGS:
    case RegEFLAGS:
    case RegRFLAGS:
    case RegEIP:
    case RegRIP:
    case RegRAX:
    case RegRBX:
    case RegRCX:
    case RegRDX:
    case RegRSI:
    case RegRDI:
    case RegRBP:
    case RegRSP:
    case RegR8:
    case RegR9:
    case RegR10:
    case RegR11:
    case RegR12:
    case RegR13:
    case RegR14:
    case RegR15:
      return AppendRegister(operand->kind, &state->nodes);
    case RegRESP:
      return AppendRegister(state->address_size == 64 ? RegRSP : RegESP,
                            &state->nodes);
    case RegREAX:
      switch (state->operand_size) {
        case 1:
          return AppendRegister(RegAL, &state->nodes);
        case 2:
          return AppendRegister(RegAX, &state->nodes);
        case 4:
        default:
          return AppendRegister(RegEAX, &state->nodes);
        case 8:
          return AppendRegister(RegRAX, &state->nodes);
      }
      break;
    case RegREIP:
      return AppendRegister(state->address_size == 64 ? RegRIP : RegEIP,
                            &state->nodes);
    case RegREBP:
      return AppendRegister(state->address_size == 64 ? RegRBP : RegEBP,
                            &state->nodes);

    case Const_1:
      return AppendConstant(1,
                            ExprFlag(ExprSize8) | ExprFlag(ExprUnsignedInt),
                            &state->nodes);
      /* TODO(karl) fill in the rest of the possibilities of type
       * OperandKind, or remove them if not needed.
       */
    default:
      /* Give up, use the default of undefined. */
      break;
  }
  return FatalError("Operand", state);
}

/* Given that the given expression node is the root of the expression
 * tree generated by translating the given operand, transfer over
 * any appropriate flags (such as set/use information).
 */
static ExprNode* AddOperandSetUse(ExprNode* node, Operand* operand) {
  if (operand->flags & OpFlag(OpSet)) {
    node->flags |= ExprFlag(ExprSet);
  }
  if (operand->flags & OpFlag(OpUse)) {
    node->flags |= ExprFlag(ExprUsed);
  }
  if (operand->flags & OpFlag(OpAddress)) {
    node->flags |= ExprFlag(ExprAddress);
  }
  return node;
}

void BuildExprNodeVector(struct NcInstState* state) {
  DEBUG(printf("building expression vector for pc = %"PRIxPcAddress":\n",
               NcInstStateVpc(state)));
  if (InstUndefined == state->opcode->insttype) {
    FatalError("instruction", state);
  } else {
    int i;
    for (i = 0; i < state->opcode->num_operands; i++) {
      Operand* op = &state->opcode->operands[i];
      if (0 == (op->flags & OpFlag(OperandExtendsOpcode))) {
        ExprNode* n;
        DEBUG(printf("translating operand %d:\n", i));
        n = AppendExprNode(OperandReference, i, 0, &state->nodes);
        if (op->flags & OpFlag(OpImplicit)) {
          n->flags |= ExprFlag(ExprImplicit);
        }
        AddOperandSetUse(AppendOperand(state, op), op);
        DEBUG(PrintExprNodeVector(stdout, &state->nodes));
      }
    }
  }
}
