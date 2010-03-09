/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
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

#include "native_client/src/shared/utils/debugging.h"

/* Return the segment register to use if DS is the default. */
static OperandKind GetDsSegmentRegister(NcInstState* state) {
  if (NACL_TARGET_SUBARCH == 32) {
    if (state->prefix_mask & kPrefixSEGCS) {
      return RegCS;
    } else if (state->prefix_mask & kPrefixSEGSS) {
      return RegSS;
    } else if (state->prefix_mask & kPrefixSEGFS) {
      return RegFS;
    } else if (state->prefix_mask & kPrefixSEGGS) {
      return RegGS;
    } else if (state->prefix_mask & kPrefixSEGES) {
      return RegES;
    } else if (state->prefix_mask & kPrefixSEGDS) {
      return RegDS;
    } else {
      return RegDS;
    }
  } else {
    return RegDS;
  }
}

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
          "FATAL: At %"NACL_PRIxPcAddress", unable to translate: %s\n",
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
  DEBUG(printf("Append constant %"NACL_PRIx64" : ", value);
        PrintExprNodeFlags(stdout, flags);
        printf("\n"));
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

/* Define the available 8-bit registers, for the given subarchitecture,
 * assuming the REX prefix is not present.
 * Note: The order is important, and is based on the indexing values used
 * in the ModRm and SIB bytes (and the REX prefix if appropriate).
 */
static const OperandKind RegisterTable8NoRex[REGISTER_TABLE_SIZE] = {
  RegAL,
  RegCL,
  RegDL,
  RegBL,
  RegAH,
  RegCH,
  RegDH,
  RegBH,
#if NACL_TARGET_SUBARCH == 64
  RegUnknown,
  RegUnknown,
  RegUnknown,
  RegUnknown,
  RegUnknown,
  RegUnknown,
  RegUnknown,
  RegUnknown
#endif
};

/* Define a mapping from RegisterTable8Rex indicies, to the corresponding
 * register index in RegisterTable64, for which each register in
 * RegisterTable8Rex is a subregister in RegisterTable64, assuming the
 * REX prefix isn't defined for the instruction.
 * Note: this index will only be used if the corresponding index in
 * RegisterTable8NoRex is not RegUnknown.
 */
static const int RegisterTable8NoRexTo64[REGISTER_TABLE_SIZE] = {
  0,
  1,
  2,
  3,
  0,
  1,
  2,
  3,
#if NACL_TARGET_SUBARCH == 64
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0
#endif
};

/* Define the available 8-bit registers, for the given subarchitecture,
 * assuming the rex prefix is present.
 * Note: The order is important, and is based on the indexing values used
 * in the ModRm and SIB bytes (and the REX prefix if appropriate).
 */
static const OperandKind RegisterTable8Rex[REGISTER_TABLE_SIZE] = {
  RegAL,
  RegCL,
  RegDL,
  RegBL,
#if NACL_TARGET_SUBARCH == 64
  RegSPL,
  RegBPL,
  RegSIL,
  RegDIL,
  RegR8B,
  RegR9B,
  RegR10B,
  RegR11B,
  RegR12B,
  RegR13B,
  RegR14B,
  RegR15B
#else
  RegAH,
  RegCH,
  RegDH,
  RegBH
#endif
};

/* Define a mapping from RegisterTable8Rex indicies, to the corresponding
 * register index in RegisterTable64, for which each register in
 * RegisterTable8Rex is a subregister in RegisterTable64, assuming the
 * REX prefix is defined for the instruction.
 * Note: this index will only be used if the corresponding index in
 * RegisterTable8Rex is not RegUnknown.
 */
static const int RegisterTable8RexTo64[REGISTER_TABLE_SIZE] = {
  0,
  1,
  2,
  3,
#if NACL_TARGET_SUBARCH == 64
  4,
  5,
  6,
  7,
  8,
  9,
  10,
  11,
  12,
  13,
  14,
  15
#else
  0,
  1,
  2,
  3
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
static const OperandKind RegisterTable64[REGISTER_TABLE_SIZE] = {
#if NACL_TARGET_SUBARCH == 64
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
#else
  RegUnknown,
  RegUnknown,
  RegUnknown,
  RegUnknown,
  RegUnknown,
  RegUnknown,
  RegUnknown,
  RegUnknown
#endif
};

/* Define the available Mmx registers, for the given subarchitecture.
 * Note: The order is important, and is based on the indexing values
 * used in the ModRm and SIB bytes (and the REX prefix if appropriate).
 */
static const OperandKind RegisterTableMmx[REGISTER_TABLE_SIZE] = {
  RegMMX0,
  RegMMX1,
  RegMMX2,
  RegMMX3,
  RegMMX4,
  RegMMX5,
  RegMMX6,
  RegMMX7,
#if NACL_TARGET_SUBARCH == 64
  /* Intentionally repeat values, since Rex.B/R has no effect. */
  RegMMX0,
  RegMMX1,
  RegMMX2,
  RegMMX3,
  RegMMX4,
  RegMMX5,
  RegMMX6,
  RegMMX7
#endif
};

/* Define the available Xmm registers, for the given subarchitecture.
 * Note: The order is important, and is based on the indexing values
 * used in the ModRm and SIB bytes (and the REX prefix if appropriate).
 */
static const OperandKind RegisterTableXmm[REGISTER_TABLE_SIZE] = {
  RegXMM0,
  RegXMM1,
  RegXMM2,
  RegXMM3,
  RegXMM4,
  RegXMM5,
  RegXMM6,
  RegXMM7,
#if NACL_TARGET_SUBARCH == 64
  RegXMM8,
  RegXMM9,
  RegXMM10,
  RegXMM11,
  RegXMM12,
  RegXMM13,
  RegXMM14,
  RegXMM15
#endif
};

/* Define a type corresponding to the arrays RegisterTable8,
 * RegisterTable16, RegisterTable32, and RegisterTable64.
 */
typedef const OperandKind RegisterTableGroup[REGISTER_TABLE_SIZE];

OperandKind NcGet64For32BitRegister(OperandKind reg32) {
#if NACL_TARGET_SUBARCH == 64
  int i;
  for (i = 0; i < REGISTER_TABLE_SIZE; ++i) {
    if (reg32 == RegisterTable32[i]) {
      return RegisterTable64[i];
    }
  }
#endif
  return RegUnknown;
}

OperandKind NcGet32For64BitRegister(OperandKind reg64) {
#if NACL_TARGET_SUBARCH == 64
  int i;
  for (i = 0; i < REGISTER_TABLE_SIZE; ++i) {
    if (reg64 == RegisterTable64[i]) {
      return RegisterTable32[i];
    }
  }
#endif
  return RegUnknown;
}

/* Once initialized, contains mapping from registers of size <= 64 to the
 * corresponding 64 bit register (or RegUnknown if no such register), when
 * there is a rex prefix in the instruction.
 *
 * Note: Initialized by function BuildSubreg64Reg.
 */
static OperandKind* Subreg64RegRex = NULL;

/* Once initialized, contains mapping from registers of size <= 64 to the
 * corresponding 64 bit register (or RegUnknown if no such register), when
 * there isn't a rex prefix in the instruction.
 *
 * Note: Initialized by function BuildSubreg64Reg.
 */
static OperandKind* Subreg64RegNoRex = NULL;

/* Add the mapping from subregisters (in subtable) to the corresponding
 * registers (in table).
 *
 * Parameters:
 *    subreg_table - The subregister table to update.
 *    subtable - The subregister table
 *    table - The register table the subregister is being checked
 *            against.
 *    index_fold - Any index remapping (i.e. fold) to be applied.
 */
static void AddSubRegistersFold(
    OperandKind subreg_table[OperandKindEnumSize],
    const OperandKind subtable[REGISTER_TABLE_SIZE],
    const OperandKind table[REGISTER_TABLE_SIZE],
    const int index_fold[REGISTER_TABLE_SIZE]) {
  int i;
  for (i = 0; i < REGISTER_TABLE_SIZE; ++i) {
    OperandKind subreg = subtable[i];
    if (subreg != RegUnknown) {
      subreg_table[subreg] = table[index_fold[i]];
    }
  }
}

/* Add the mapping from subregisters (in subtable) to the corresponding
 * registers (in table).
 *
 * Parameters:
 *    subreg_table - The subregister table to update.
 *    subtable - The subregister table
 *    table - The register table the subregister is being checked
 *            against.
 */
static void AddSubRegisters(OperandKind subreg_table[OperandKindEnumSize],
                            const OperandKind subtable[REGISTER_TABLE_SIZE],
                            const OperandKind table[REGISTER_TABLE_SIZE]) {
  static const int kIdentityFold[REGISTER_TABLE_SIZE] = {
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
#if NACL_TARGET_SUBARCH == 64
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15
#endif
  };
  AddSubRegistersFold(subreg_table, subtable, table, kIdentityFold);
}

/* Build Subreg64RegRex and Subreg64RegNoRex using encoded register tables. */
static void BuildSubreg64Regs() {
  if (Subreg64RegRex == NULL) {
    int i;
    /* Create lookup vectors, and initialize to unknown. */
    Subreg64RegRex = (OperandKind*)
        calloc(sizeof(OperandKind), OperandKindEnumSize);
    Subreg64RegNoRex = (OperandKind*)
        calloc(sizeof(OperandKind), OperandKindEnumSize);
    for (i = 0; i < OperandKindEnumSize; ++i) {
      Subreg64RegRex[i] =  RegUnknown;
      Subreg64RegNoRex[i] = RegUnknown;
    }

    /* Add register tables. */
    AddSubRegistersFold(Subreg64RegNoRex,
                        RegisterTable8NoRex,
                        RegisterTable64,
                        RegisterTable8NoRexTo64);
    AddSubRegistersFold(Subreg64RegRex,
                        RegisterTable8Rex,
                        RegisterTable64,
                        RegisterTable8RexTo64);

    AddSubRegisters(Subreg64RegNoRex, RegisterTable16, RegisterTable64);
    AddSubRegisters(Subreg64RegRex, RegisterTable16, RegisterTable64);

    AddSubRegisters(Subreg64RegNoRex, RegisterTable32, RegisterTable64);
    AddSubRegisters(Subreg64RegRex, RegisterTable32, RegisterTable64);

    AddSubRegisters(Subreg64RegNoRex, RegisterTable64, RegisterTable64);
    AddSubRegisters(Subreg64RegRex, RegisterTable64, RegisterTable64);
  }
}

Bool NcIs64Subregister(NcInstState* state,
                       OperandKind subreg, OperandKind reg64) {
  BuildSubreg64Regs();
  return (state->rexprefix
          ? Subreg64RegRex[subreg]
          : Subreg64RegNoRex[subreg]) == RegisterTable64[reg64];
}

Bool Is32To64RegisterPair(OperandKind reg32, OperandKind reg64) {
  return reg64 == NcGet64For32BitRegister(reg32);
}

/* Define the set of available registers, categorized by size.
 * Note: The order is important, and is based on the indexing values used
 * in the ModRm and SIB bytes (and the REX prefix if appropriate).
 */
static RegisterTableGroup* const RegisterTable[] = {
  &RegisterTable8NoRex,
  &RegisterTable16,
  &RegisterTable32,
  &RegisterTable64,
  &RegisterTableMmx,
  &RegisterTableXmm
};

/* Define possible register categories. */
typedef enum {
  /* Note: the following all have register tables
   * for the corresponding general purpose registers.
   */
  RegSize8,
  RegSize16,
  RegSize32,
  RegSize64,
  RegMMX,
  RegXMM,
  /* Note: sizes below this point don't define general
   * purpose registers, and hence, don't have a lookup
   * value in the register tables.
   */
  RegSize128,
  RegUndefined,   /* Always returns RegUnknown. */
} RegKind;

static const char* const g_RegKindName[] = {
  "RegSize8",
  "RegSize16",
  "RegSize32",
  "RegSize64",
  "RegMMX",
  "RegXMM",
  "RegSize128",
  "RegUndefined"
};

const char* RegKindName(RegKind kind) {
  return g_RegKindName[kind];
}

/* Define ModRm register categories. */
typedef enum {
  ModRmGeneral,
  ModRmMmx,
  ModRmXmm,
  /* Don't allow top level registers in Effective address. */
  ModRmNoTopLevelRegisters
} ModRmRegisterKind;

static const char* const g_ModRmRegisterKindName[] = {
  "ModRmGeneral",
  "ModRmMmx",
  "ModRmXmm",
  "ModRmNoTopLevelRegisters"
};

/* Given an operand kind, return the size specification associated with
 * the operand kind.
 */
static RegKind GetOperandKindRegKind(OperandKind kind) {
  switch (kind) {
    case Eb_Operand:
    case Gb_Operand:
    case Ib_Operand:
    case Jb_Operand:
    case Mb_Operand:
    case Ob_Operand:
      return RegSize8;
    case Aw_Operand:
    case Ew_Operand:
    case Gw_Operand:
    case Iw_Operand:
    case Jw_Operand:
    case Mw_Operand:
    case Mpw_Operand:
    case Ow_Operand:
      return RegSize16;
    case Av_Operand:
    case Ev_Operand:
    case Gv_Operand:
    case Iv_Operand:
    case Jv_Operand:
    case Mv_Operand:
    case Mpv_Operand:
    case Ov_Operand:
    case Mmx_Gd_Operand:
      return RegSize32;
    case Ao_Operand:
    case Eo_Operand:
    case Go_Operand:
    case Io_Operand:
    case Mo_Operand:
    case Mpo_Operand:
    case Oo_Operand:
    case Xmm_Eo_Operand:
    case Xmm_Go_Operand:
    case Mmx_E_Operand:
    case Mmx_G_Operand:
      return RegSize64;
    case Edq_Operand:
    case Gdq_Operand:
    case Mdq_Operand:
    case Xmm_E_Operand:
    case Xmm_G_Operand:
      return RegSize128;
    default:
      return RegUndefined;
  }
}

static OperandKind LookupRegister(NcInstState* state,
                                  RegKind kind, int reg_index) {
  DEBUG(printf("Lookup register (rex=%"NACL_PRIx8") %s:%d\n",
               state->rexprefix, RegKindName(kind), reg_index));
  if (32 == NACL_TARGET_SUBARCH && kind == RegSize64) {
    FatallyLost("Architecture doesn't define 64 bit registers");
  } else if (RegSize128 <= kind) {
    return RegUnknown;
  }
  if (64 == NACL_TARGET_SUBARCH && kind == RegSize8 && state->rexprefix) {
    return RegisterTable8Rex[reg_index];
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
  case RegR8B:
  case RegR9B:
  case RegR10B:
  case RegR11B:
  case RegR12B:
  case RegR13B:
  case RegR14B:
  case RegR15B:
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
  return AppendRegister(LookupRegister(state, kind, reg_index), &state->nodes);
}

/* Given an operand of the corresponding opcode instruction of the
 * given state, return what kind of register should be used, based
 * on the operand size.
 */
static RegKind ExtractOperandRegKind(NcInstState* state,
                                     Operand* operand) {
  RegKind reg_kind = GetOperandKindRegKind(operand->kind);
  switch (reg_kind) {
    case RegSize8:
    case RegSize16:
    case RegSize32:
    case RegSize64:
      return reg_kind;
    default:
      /* Size not explicitly defined, pick up from operand size. */
      if (state->opcode->flags & InstFlag(OperandSize_b)) {
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
}

/* Given an address of the corresponding opcode instruction of the
 * given state, return what kind of register should be used.
 */
static RegKind ExtractAddressRegKind(NcInstState* state) {
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
    NcInstState* state,
    Operand* operand,
    int reg_index,
    ModRmRegisterKind modrm_reg_kind) {
  RegKind reg_kind = RegSize32;
  DEBUG(printf("modrm_reg_kind = %s\n",
               g_ModRmRegisterKindName[modrm_reg_kind]));
  switch (modrm_reg_kind) {
    default:
    case ModRmGeneral:
      reg_kind = ExtractOperandRegKind(state, operand);
      break;
    case ModRmMmx:
      reg_kind = RegMMX;
      break;
    case ModRmXmm:
      reg_kind = RegXMM;
      break;
    case ModRmNoTopLevelRegisters:
      reg_kind = RegUndefined;
  }
  DEBUG(printf("Translate register %d, %s\n",
               reg_index, g_RegKindName[reg_kind]));
  return AppendRegisterKind(state, reg_kind, reg_index);
}

/* Same as AppendOperandRegister, except register is combined with ES
 * To define a segment address.
 */
static ExprNode* AppendEsOperandRegister(
    NcInstState* state,
    Operand* operand,
    int reg_index,
    ModRmRegisterKind modrm_reg_kind) {
  ExprNode* results = AppendExprNode(ExprSegmentAddress, 0, 0, &state->nodes);
  AppendRegister(RegES, &state->nodes);
  AppendOperandRegister(state, operand, reg_index, modrm_reg_kind);
  return results;
}

/* For the given instruction state, and the corresponding 3-bit specification
 * of a register, update it to a 4-bit specification, based on the REX.R bit.
 */
static int GetRexRRegister(NcInstState* state, int reg) {
  DEBUG(printf("Get GenRexRRegister %d\n", reg));
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

/* For the given instruction state, and the corresponding 3-bit specification
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

/* Get the ST register defined from the difference of the opcode, and
 * its opcode base.
 */
static ExprNode* AppendStOpcodeBaseRegister(NcInstState* state) {
  /* Note: Difference held as first operand (by convention). */
  int reg_index;
  assert(state->opcode->num_operands > 1);
  reg_index = state->opcode->operands[0].kind - OpcodeBaseMinus0;
  assert(reg_index >= 0 && reg_index < 8);
  DEBUG(printf("Translate opcode base register %d\n", reg_index));
  return AppendRegister(RegST0 + reg_index, &state->nodes);
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
                                Displacement* displacement,
                                ExprNodeFlags flags) {
  DEBUG(printf("-> Extract displacement, flags = ");
        PrintExprNodeFlags(stdout, flags);
        printf("\n"));
  /* First compute the displacement value. */
  displacement->value = ExtractUnsignedBinaryValue(state,
                                                   state->first_disp_byte,
                                                   state->num_disp_bytes);

  /* Now compute any appropriate flags to be associated with the value. */
  displacement->flags = flags | GetExprSizeFlagForBytes(state->num_disp_bytes);
  DEBUG(printf("<- value = %"NACL_PRIx64", flags = ", displacement->value);
        PrintExprNodeFlags(stdout, displacement->flags);
        printf("\n"));
}

/* Append the displacement value of the given instruction state
 * onto the vector of expression nodes. Returns the appended displacement
 * value.
 */
static ExprNode* AppendDisplacement(NcInstState* state) {
  Displacement displacement;
  DEBUG(printf("append displacement\n"));
  ExtractDisplacement(state, &displacement, ExprFlag(ExprSignedHex));
  return AppendConstant(displacement.value, displacement.flags, &state->nodes);
}

/* Get the binary value denoted by the immediate bytes of the state. */
static uint64_t ExtractUnsignedImmediate(NcInstState* state) {
  return ExtractUnsignedBinaryValue(state,
                                    state->first_imm_byte,
                                    state->num_imm_bytes);
}

/* Get the binary value denoted by the 2nd immediate bytes of the state. */
static uint64_t ExtractUnsignedImmediate2(NcInstState* state) {
  return ExtractUnsignedBinaryValue(
      state,
      state->first_imm_byte + state->num_imm_bytes,
      state->num_imm2_bytes);
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

/* Append the second immediate value of the given instruction state onto
 * the vector of expression nodes. Returns the appended immediate value.
 */
static ExprNode* AppendImmediate2(NcInstState* state) {
  ExprNodeFlags flags;

  /* First compute the immedaite value. */
  uint64_t value;
  DEBUG(printf("append 2nd immedaite\n"));

  value = ExtractUnsignedImmediate2(state);

  /* Now compute any appropriate flags to be associated with the immediate
   * value.
   */
  flags =
      ExprFlag(ExprUnsignedHex) |
      GetExprSizeFlagForBytes(state->num_imm2_bytes);

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
  DEBUG(printf("value = 0x%016"NACL_PRIx64"\n", value));
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

  return AppendConstant(val,
                        ExprFlag(ExprUnsignedHex) | ExprFlag(ExprJumpTarget),
                        &state->nodes);
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
  ExprNode* root = NULL;
  ExprNode* n;

  DEBUG(printf(
      "memory offset(%s + %s * %d +  %"NACL_PRId64" : %"NACL_PRIx32")\n",
      OperandKindName(base),
      OperandKindName(index),
      scale,
      displacement->value,
      displacement->flags));
  if (NACL_TARGET_SUBARCH == 64) {
    if (state->prefix_mask & kPrefixSEGFS) {
      root = AppendExprNode(ExprSegmentAddress, 0, 0, &state->nodes);
      n = AppendRegister(RegFS, &state->nodes);
      n->flags |= ExprFlag(ExprUsed);
    } else if (state->prefix_mask & kPrefixSEGGS) {
      root = AppendExprNode(ExprSegmentAddress, 0, 0, &state->nodes);
      n = AppendRegister(RegGS, &state->nodes);
      n->flags |= ExprFlag(ExprUsed);
    }
    if (root == NULL) {
      root = AppendExprNode(ExprMemOffset, 0, 0, &state->nodes);
    } else {
      AppendExprNode(ExprMemOffset, 0, 0, &state->nodes);
    }
  } else {
    /* Need to add segmentation base. */
    root = AppendExprNode(ExprSegmentAddress, 0, 0, &state->nodes);
    n = AppendRegister(((base == RegBP || base == RegEBP)
                        ? RegSS
                        : GetDsSegmentRegister(state)),
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

/* Extract the base register from the SIB byte. */
static OperandKind GetSibBase(NcInstState* state) {
  int base = sib_base(state->sib);
  OperandKind base_reg = RegUnknown;
  if (0x5 == base) {
    switch (modrm_mod(state->modrm)) {
      case 0:
        break;
      case 1:
      case 2:
        if (NACL_TARGET_SUBARCH == 64) {
          if (state->rexprefix & 0x1) {
            base_reg = RegR13;
          } else {
            base_reg = RegRBP;
          }
        } else {
          base_reg = RegEBP;
        }
        break;
      default:
        FatalError("SIB value", state);
    }
  } else {
    RegKind kind = ExtractAddressRegKind(state);
    base_reg = LookupRegister(state, kind, GetRexBRegister(state, base));
  }
  return base_reg;
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
  int index = sib_index(state->sib);
  int scale = 1;
  RegKind kind = ExtractAddressRegKind(state);
  OperandKind base_reg;
  OperandKind index_reg = RegUnknown;
  Displacement displacement;
  DEBUG(printf("append sib: %02x\n", state->sib));
  InitializeDisplacement(0, 0, &displacement);
  base_reg = GetSibBase(state);
  if (0x4 != index || NACL_TARGET_SUBARCH != 64 || (state->rexprefix & 0x2)) {
    index_reg = LookupRegister(state, kind, GetRexXRegister(state, index));
    scale = sib_scale[sib_ss(state->sib)];
  }
  if (state->num_disp_bytes > 0) {
    ExtractDisplacement(state, &displacement,
                        ExprFlag(ExprSignedHex));
  } else {
    displacement.flags = ExprFlag(ExprSize8);
  }
  return AppendMemoryOffset(state, base_reg, index_reg, scale, &displacement);
}

static void AppendEDI(NcInstState* state) {
  switch (state->address_size) {
    case 16:
      AppendRegister(RegDI, &state->nodes);
      break;
    case 32:
      AppendRegister(RegEDI, &state->nodes);
      break;
    case 64:
      AppendRegister(RegRDI, &state->nodes);
      break;
    default:
      FatalError("Address size for ES:EDI not correctly defined", state);
      break;
  }
}

static ExprNode* AppendDS_EDI(NcInstState* state) {
  ExprNode* results = AppendExprNode(ExprSegmentAddress, 0, 0, &state->nodes);
  AppendRegister(GetDsSegmentRegister(state),  &state->nodes);
  AppendEDI(state);
  return results;
}

static ExprNode* AppendES_EDI(NcInstState* state) {
  ExprNode* results = AppendExprNode(ExprSegmentAddress, 0, 0, &state->nodes);
  AppendRegister(RegES, &state->nodes);
  AppendEDI(state);
  return results;
}

static ExprNode* AppendSegmentDX_AX(NcInstState* state) {
  ExprNode* results = AppendExprNode(ExprSegmentAddress, 0, 0, &state->nodes);
  switch (state->operand_size) {
    case 2:
      AppendRegister(RegDX, &state->nodes);
      AppendRegister(RegAX, &state->nodes);
      break;
    case 4:
      AppendRegister(RegEDX, &state->nodes);
      AppendRegister(RegEAX, &state->nodes);
      break;
    case 8:
      AppendRegister(RegRDX, &state->nodes);
      AppendRegister(RegRAX, &state->nodes);
      break;
    default:
      FatalError("Address size for segment DX:AX not correctly defined", state);
      break;
  }
  return results;
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
        ExtractDisplacement(state, &displacement, ExprFlag(ExprSignedHex));
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
                                    state,
                                    ExtractAddressRegKind(state),
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
    ExtractDisplacement(state, &displacement, ExprFlag(ExprSignedHex));
    return AppendMemoryOffset(state,
                              LookupRegister(
                                  state,
                                  ExtractAddressRegKind(state),
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
        LookupRegister(state,
                       ExtractAddressRegKind(state),
                       GetGenRmRegister(state));
    ExtractDisplacement(state, &displacement, ExprFlag(ExprSignedHex));
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
    NcInstState* state, Operand* operand, ModRmRegisterKind modrm_reg_kind) {
  DEBUG(printf("Translate modrm(%02x).mod == 11, %s\n",
               state->modrm, g_ModRmRegisterKindName[modrm_reg_kind]));
  return AppendOperandRegister(state,
                               operand,
                               GetGenRmRegister(state), modrm_reg_kind);
}

/* Compute the effect address using the Mod/Rm and SIB bytes. */
static ExprNode* AppendEffectiveAddress(
    NcInstState* state, Operand* operand, ModRmRegisterKind modrm_reg_kind) {
  switch(modrm_mod(state->modrm)) {
    case 0:
      return AppendMod00EffectiveAddress(state, operand);
    case 1:
      return AppendMod01EffectiveAddress(state, operand);
    case 2:
      return AppendMod10EffectiveAddress(state, operand);
    case 3:
      return AppendMod11EffectiveAddress(state, operand, modrm_reg_kind);
    default:
      break;
  }
  return FatalError("Operand", state);
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
    case Edq_Operand:
      /* TODO(karl) Should we add limitations that simple registers
       * not allowed in M_Operand cases?
       */
    case M_Operand:
    case Mb_Operand:
    case Mw_Operand:
    case Mv_Operand:
    case Mo_Operand:
    case Mdq_Operand: {
        ExprNode* address =
            AppendEffectiveAddress(state, operand, ModRmGeneral);
        /* Near operands are jump addresses. Mark them as such. */
        if (operand->flags & OpFlag(OperandNear)) {
          address->flags |= ExprFlag(ExprJumpTarget);
        }
        return address;
      }
      break;
    case G_Operand:
    case Gb_Operand:
    case Gw_Operand:
    case Gv_Operand:
    case Go_Operand:
    case Gdq_Operand:
      return AppendOperandRegister(state, operand, GetGenRegRegister(state),
                                   ModRmGeneral);

    case ES_G_Operand:
      return AppendEsOperandRegister(state, operand, GetGenRegRegister(state),
                                     ModRmGeneral);
    case G_OpcodeBase:
      return AppendOpcodeBaseRegister(state, operand);
    case I_Operand:
    case Ib_Operand:
    case Iw_Operand:
    case Iv_Operand:
    case Io_Operand:
      return AppendImmediate(state);
    case I2_Operand:
      return AppendImmediate2(state);
    case J_Operand:
    case Jb_Operand:
    case Jw_Operand:
    case Jv_Operand:
      /* TODO(karl) use operand flags OperandNear and OperandRelative to decide
       * how to process the J operand (see Intel manual for call statement).
       */
      return AppendRelativeImmediate(state);
    case Mmx_Gd_Operand:
    case Mmx_G_Operand:
      return AppendOperandRegister(state, operand, GetGenRegRegister(state),
                                   ModRmMmx);
    case Mmx_E_Operand:
      return AppendEffectiveAddress(state, operand, ModRmMmx);
    case Xmm_G_Operand:
    case Xmm_Go_Operand:
      return AppendOperandRegister(state, operand, GetGenRegRegister(state),
                                   ModRmXmm);
    case Xmm_E_Operand:
    case Xmm_Eo_Operand:
      return AppendEffectiveAddress(state, operand, ModRmXmm);
    case O_Operand:
    case Ob_Operand:
    case Ow_Operand:
    case Ov_Operand:
    case Oo_Operand:
      return AppendMemoryOffsetImmediate(state);
    case St_Operand:
      return AppendStOpcodeBaseRegister(state);
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
    case RegR8B:
    case RegR9B:
    case RegR10B:
    case RegR11B:
    case RegR12B:
    case RegR13B:
    case RegR14B:
    case RegR15B:
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
    case RegST0:
    case RegST1:
    case RegST2:
    case RegST3:
    case RegST4:
    case RegST5:
    case RegST6:
    case RegST7:
    case RegMMX1:
    case RegMMX2:
    case RegMMX3:
    case RegMMX4:
    case RegMMX5:
    case RegMMX6:
    case RegMMX7:
    case RegXMM0:
    case RegXMM1:
    case RegXMM2:
    case RegXMM3:
    case RegXMM4:
    case RegXMM5:
    case RegXMM6:
    case RegXMM7:
    case RegXMM8:
    case RegXMM9:
    case RegXMM10:
    case RegXMM11:
    case RegXMM12:
    case RegXMM13:
    case RegXMM14:
    case RegXMM15:
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
    case RegREDX:
      switch (state->operand_size) {
        case 1:
          return AppendRegister(RegDL, &state->nodes);
        case 2:
          return AppendRegister(RegDX, &state->nodes);
        case 4:
          return AppendRegister(RegEDX, &state->nodes);
        case 8:
          return AppendRegister(RegRDX, &state->nodes);
      }
      break;
    case RegREIP:
      return AppendRegister(state->address_size == 64 ? RegRIP : RegEIP,
                            &state->nodes);
    case RegREBP:
      return AppendRegister(state->address_size == 64 ? RegRBP : RegEBP,
                            &state->nodes);

    case RegDS_EDI:
      return AppendDS_EDI(state);

    case RegES_EDI:
      return AppendES_EDI(state);

    case SegmentDX_AX:
      return AppendSegmentDX_AX(state);

    case Const_1:
      return AppendConstant(1,
                            ExprFlag(ExprSize8) | ExprFlag(ExprUnsignedHex),
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
  DEBUG(printf("building expression vector for pc = %"NACL_PRIxPcAddress":\n",
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
