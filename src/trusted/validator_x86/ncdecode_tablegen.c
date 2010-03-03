/* Copyright (c) 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * New table driven generator for a decoder of x86 code.
 *
 * Note: Most of the organization of this document is based on the
 * Opcode Map appendix of one of the following documents:

 * (1) "Intel 64 and IA-32 Architectures
 * Software Developer's Manual (volumes 1, 2a, and 2b; documents
 * 253665.pdf, 253666.pdf, and 253667.pdf)".
 *
 * (2) "Intel 80386 Reference Programmer's Manual" (document
 * http://pdos.csail.mit.edu/6.828/2004/readings/i386/toc.htm).
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <string.h>

/* To turn on debugging of instruction decoding, change value of
 * DEBUGGING to 1.
 */
#define DEBUGGING 0

#include "native_client/src/shared/utils/debugging.h"

#include "native_client/src/trusted/validator_x86/ncdecode_tablegen.h"
#include "native_client/src/include/portability.h"

/* Model an Opcode, sorted by OpcodeInModRm values. */
typedef struct MrmOpcode {
  Opcode opcode;
  struct MrmOpcode* next;
} MrmOpcode;

/* Note: in general all errors in this module will be fatal.
 * To debug: use gdb or your favorite debugger.
 */
static void fatal(const char* s) {
  fprintf(stderr, "%s\n", s);
  fprintf(stderr, "fatal error, cannot recover\n");
  exit(-1);
}

/* Returns the print name of the given run mode. */
static const char* RunModeName(RunMode mode) {
  switch (mode) {
    case X86_32: return "x86-32 bit mode";
    case X86_64: return "x86-64 bit mode";
    default: assert(0);
  }

  /* NOTREACHED */
  return NULL;
}

/* Defines the run mode files that should be generated. */
static RunMode FLAGS_run_mode = X86_32;

/* Holds the current instruction prefix. */
static OpcodePrefix current_opcode_prefix = NoPrefix;

/* Holds the default instruction prefix. */
static OpcodePrefix default_opcode_prefix = NoPrefix;

/* Holds the current opcode instruction being built. */
Opcode* current_opcode = NULL;

/* Holds the current opcode with mrm extention being built. */
MrmOpcode* current_opcode_mrm = NULL;

/* Holds the opcode to model instructions that can't be parsed. */
static Opcode* undefined_opcode = NULL;

/* True if we are to apply sanity checks as we define operantions. */
static Bool apply_sanity_checks = TRUE;

/* Holds all defined opcodes. */
static Opcode* OpcodeTable[NCDTABLESIZE][OpcodePrefixEnumSize];

#define DEFAULT_CHOICE_COUNT (-1)

#define NO_MODRM_OPCODE Unknown_Operand

#define NO_MODRM_OPCODE_INDEX 8

#define MODRM_OPCODE_SIZE (NO_MODRM_OPCODE_INDEX + 1)

/* Holds the expected number of entries in the defined opcodes.
 * Note: the last index corresponds to the modrm opcode, or
 * NO_MODRM_OPCODE if no modrm opcode.
 */
static int OpcodeCount[NCDTABLESIZE][OpcodePrefixEnumSize][MODRM_OPCODE_SIZE];

static MrmOpcode* OpcodeMrmTable[NCDTABLESIZE]
          [OpcodePrefixEnumSize][MODRM_OPCODE_SIZE];

/* Holds encodings of prefix bytes. */
const char* PrefixTable[NCDTABLESIZE];

/* Prints out the opcode prefix being defined, the opcode pattern
 * being defined, and the given error message. Then aborts the
 * execution of the program.
 */
static void FatalOpcode(const char* message) {
  fprintf(stderr, "Prefix: %s\n", OpcodePrefixName(current_opcode_prefix));
  PrintOpcode(stderr, current_opcode);
  fatal(message);
}

/* Prints out what operand is currently being defined, followed by the given
 * error message. Then aborts the execution of the program.
 */
static void FatalOperand(int index, const char* message) {
  if (0 <= index && index <= current_opcode->num_operands) {
    fprintf(stderr, "On operand %d: %s\n", index,
            OperandKindName(current_opcode->operands[index].kind));
  } else {
    fprintf(stderr, "On operand %d:\n", index);
  }
  FatalOpcode(message);
}

/* Define the prefix name for the given opcode, for the given run mode. */
static void EncodeModedPrefixName(const uint8_t byte, const char* name,
                                  const RunMode mode) {
  if (FLAGS_run_mode == mode) {
    PrefixTable[byte] = name;
  }
}

/* Define the prefix name for the given opcode, for all run modes. */
static void EncodePrefixName(const uint8_t byte, const char* name) {
  EncodeModedPrefixName(byte, name, FLAGS_run_mode);
}

/* Change the current opcode prefix to the given value. */
void DefineOpcodePrefix(const OpcodePrefix prefix) {
  current_opcode_prefix = prefix;
}

void DefineDefaultOpcodePrefix(const OpcodePrefix prefix) {
  default_opcode_prefix = prefix;
  DefineOpcodePrefix(prefix);
}

void ResetToDefaultOpcodePrefix() {
  DefineOpcodePrefix(default_opcode_prefix);
}

/* Check that the given operand is an extention of the opcode
 * currently being defined. If not, generate appropriate error
 * message and stop program.
 */
static void CheckIfOperandExtendsOpcode(Operand* operand) {
  if (0 == (operand->flags & OpFlag(OperandExtendsOpcode))) {
    FatalOpcode(
        "First operand should be marked with flag OperandExtendsOpcode");
  }
}

/* Check if an E_Operand operand has been repeated, since it should
 * never appear for more than one argument. If repeated, generate an
 * appropriate error message and terminate.
 */
static void CheckIfEOperandRepeated(int index) {
  int i;
  for (i = 0; i < index; ++i) {
    Operand* operand = &current_opcode->operands[i];
    switch (operand->kind) {
      case Mmx_E_Operand:
      case Xmm_E_Operand:
      case Xmm_Eo_Operand:
      case E_Operand:
      case Eb_Operand:
      case Ew_Operand:
      case Ev_Operand:
      case Eo_Operand:
      case Edq_Operand:
        FatalOperand(index, "Can't use E Operand more than once");
        break;
      default:
        break;
    }
  }
}

/* Called if operand is a G_Operand. Checks that the opcode doesn't specify
 * that the REG field of modrm is an opcode, since G_Operand doesn't make
 * sense in such cases.
 */
static void CheckIfOpcodeInModRm(int index) {
  if ((current_opcode->flags & InstFlag(OpcodeInModRm)) &&
      ((OperandFlags) 0 == (current_opcode->operands[index].flags &
                            InstFlag(AllowGOperandWithOpcodeInModRm)))) {
    FatalOperand(index,
                 "Can't use G Operand, bits being used for opcode in modrm");
  }
}

/* Check if an G_Operand operand has been repeated, since it should
 * never appear for more than one argument. If repeated, generate an
 * appropriate error message and terminate.
 */
static void CheckIfGOperandRepeated(int index) {
  int i;
  for (i = 0; i < index; ++i) {
    Operand* operand = &current_opcode->operands[i];
    switch (operand->kind) {
      case Mmx_G_Operand:
      case Xmm_G_Operand:
      case Xmm_Go_Operand:
      case G_Operand:
      case Gb_Operand:
      case Gw_Operand:
      case Gv_Operand:
      case Go_Operand:
      case Gdq_Operand:
        FatalOperand(index, "Can't use G Operand more than once");
        break;
      default:
        break;
    }
  }
}

/* Check if an I_Operand/J_OPerand operand has been repeated, since it should
 * never appear for more than one argument (both come from the immediate field
 * of the instruction). If repeated, generate an appropriate error message
 * and terminate.
 */
static void CheckIfIOperandRepeated(int index) {
  int i;
  for (i = 0; i < index; ++i) {
    Operand* operand = &current_opcode->operands[i];
    switch (operand->kind) {
      case I_Operand:
      case Ib_Operand:
      case Iw_Operand:
      case Iv_Operand:
      case Io_Operand:
        FatalOperand(index, "Can't use I_Operand more than once");
        break;
      case J_Operand:
      case Jb_Operand:
      case Jv_Operand:
        FatalOperand(index, "Can't use both I_Operand and J_Operand");
        break;
      default:
        break;
    }
  }
}

/* Check that the operand being defined (via the given index), does not
 * specify any inconsistent flags.
 */
static void ApplySanityChecksToOperand(int index) {
  Operand* operand = &current_opcode->operands[index];

  if (!apply_sanity_checks) return;

  /* Check special cases for operand 0. */
  if (index == 0) {
    if (current_opcode->flags & InstFlag(OpcodeInModRm)) {
      if ((operand->kind < Opcode0) ||
          (operand->kind > Opcode7)) {
        FatalOperand(
            index,
            "First operand of OpcodeInModRm  must be in {Opcode0..Opcode7}");
      }
      CheckIfOperandExtendsOpcode(operand);
    }
    if (current_opcode->flags & InstFlag(OpcodePlusR)) {
      if ((operand->kind < OpcodeBaseMinus0) ||
          (operand->kind > OpcodeBaseMinus7)) {
        FatalOperand(
            index,
            "First operand of OpcodePlusR must be in "
            "{OpcodeBaseMinus0..OpcodeBaseMinus7}");
      }
      CheckIfOperandExtendsOpcode(operand);
    }
  }
  if (operand->flags & OpFlag(OperandExtendsOpcode)) {
    if (index > 0) {
      FatalOperand(index, "OperandExtendsOpcode only allowed on first operand");
    }
    if (operand->flags != OpFlag(OperandExtendsOpcode)) {
      FatalOperand(index,
                   "Only OperandExtendsOpcode allowed for flag "
                   "values on this operand");
    }
  }

  /* Check that operand is consistent with other operands defined, or flags
   * defined on the opcode.
   */
  switch (operand->kind) {
    case E_Operand:
      CheckIfEOperandRepeated(index);
      break;
    case Eb_Operand:
      if (current_opcode->flags & InstFlag(OperandSize_b)) {
        FatalOperand(index,
                     "Size implied by OperandSize_b, use E_Operand instead");
      }
      CheckIfEOperandRepeated(index);
      break;
    case Ew_Operand:
      if ((current_opcode->flags & InstFlag(OperandSize_w)) &&
          !(current_opcode->flags & InstFlag(OperandSize_v))) {
        FatalOperand(index,
                     "Size implied by OperandSize_w, use E_Operand instead");
      }
      CheckIfEOperandRepeated(index);
      break;
    case Ev_Operand:
      if (current_opcode->flags & InstFlag(OperandSize_v)) {
        FatalOperand(index,
                     "Size implied by OperandSize_v, use E_Operand instead");
      }
      CheckIfEOperandRepeated(index);
      break;
    case Eo_Operand:
      if (current_opcode->flags & InstFlag(OperandSize_o)) {
        FatalOperand(index,
                     "Size implied by OperandSize_o, use E_Operand instead");
      }
      CheckIfEOperandRepeated(index);
      break;
    case Edq_Operand:
      CheckIfEOperandRepeated(index);
      break;
    case Mmx_E_Operand:
    case Xmm_E_Operand:
      CheckIfEOperandRepeated(index);
      break;
    case Xmm_Eo_Operand:
      if (current_opcode->flags & InstFlag(OperandSize_o)) {
        FatalOperand
            (index,
             "Size implied by OperandSize_o, use Xmm_E_Operand instead");
      }
      CheckIfEOperandRepeated(index);
      break;
    case G_Operand:
      CheckIfGOperandRepeated(index);
      CheckIfOpcodeInModRm(index);
      break;
    case Gb_Operand:
      if (current_opcode->flags & InstFlag(OperandSize_b)) {
        FatalOperand(index,
                     "Size implied by OperandSize_b, use G_Operand instead");
      }
      CheckIfGOperandRepeated(index);
      CheckIfOpcodeInModRm(index);
      break;
    case Gw_Operand:
      if (current_opcode->flags & InstFlag(OperandSize_w)) {
        FatalOperand(index,
                     "Size implied by OperandSize_w, use G_Operand instead");
      }
      CheckIfGOperandRepeated(index);
      CheckIfOpcodeInModRm(index);
      break;
    case Gv_Operand:
      if (current_opcode->flags & InstFlag(OperandSize_v)) {
        FatalOperand(index,
                     "Size implied by OperandSize_v, use G_Operand instead");
      }
      CheckIfGOperandRepeated(index);
      CheckIfOpcodeInModRm(index);
      break;
    case Go_Operand:
      if (current_opcode->flags & InstFlag(OperandSize_o)) {
        FatalOperand(index,
                     "Size implied by OperandSize_o, use G_Operand instead");
      }
      CheckIfGOperandRepeated(index);
      CheckIfOpcodeInModRm(index);
      break;
    case Gdq_Operand:
      CheckIfGOperandRepeated(index);
      CheckIfOpcodeInModRm(index);
      break;
    case Mmx_G_Operand:
    case Xmm_G_Operand:
      CheckIfGOperandRepeated(index);
      CheckIfOpcodeInModRm(index);
      break;
    case Xmm_Go_Operand:
      if (current_opcode->flags & InstFlag(OperandSize_o)) {
        FatalOperand
            (index,
             "Size implied by OperandSize_o, use Xmm_G_Operand instead");
      }
      CheckIfGOperandRepeated(index);
      CheckIfOpcodeInModRm(index);
      break;
    case I_Operand:
      CheckIfIOperandRepeated(index);
      break;
    case Ib_Operand:
      if (current_opcode->flags & InstFlag(OperandSize_b)) {
        FatalOperand(index,
                     "Size implied by OperandSize_b, use I_Operand instead");
      }
      if (current_opcode->flags & InstFlag(OpcodeHasImmed_b)) {
        FatalOperand(index,
                     "Size implied by OpcodeHasImmed_b, use I_Operand instead");
      }
      CheckIfIOperandRepeated(index);
      break;
    case Iw_Operand:
      if (current_opcode->flags & InstFlag(OperandSize_w)) {
        FatalOperand(index,
                     "Size implied by OperandSize_w, use I_Operand instead");
      }
      if (current_opcode->flags & InstFlag(OpcodeHasImmed_w)) {
        FatalOperand(index,
                     "Size implied by OpcodeHasImmed_w, use I_Operand instead");
      }
      CheckIfIOperandRepeated(index);
      break;
    case Iv_Operand:
      if (current_opcode->flags & InstFlag(OperandSize_v)) {
        FatalOperand(index,
                     "Size implied by OperandSize_v, use I_Operand instead");
      }
      if (current_opcode->flags & InstFlag(OpcodeHasImmed_v)) {
        FatalOperand(index,
                     "Size implied by OpcodeHasImmed_v, use I_Operand instead");
      }
      CheckIfIOperandRepeated(index);
      break;
    case Io_Operand:
      if (current_opcode->flags & InstFlag(OperandSize_o)) {
        FatalOperand(index,
                     "Size implied by OperandSize_o, use I_Operand instead");
      }
      if (current_opcode->flags & InstFlag(OpcodeHasImmed_o)) {
        FatalOperand(index,
                     "Size implied by OpcodeHasImmed_o, use I_Operand instead");
      }
      CheckIfIOperandRepeated(index);
      break;
    case OpcodeBaseMinus0:
    case OpcodeBaseMinus1:
    case OpcodeBaseMinus2:
    case OpcodeBaseMinus3:
    case OpcodeBaseMinus4:
    case OpcodeBaseMinus5:
    case OpcodeBaseMinus6:
    case OpcodeBaseMinus7:
      if (0 == (current_opcode->flags & InstFlag(OpcodePlusR))) {
        FatalOperand(index, "Expects opcode to have flag OpcodePlusR");
      }
      break;
    case Opcode0:
    case Opcode1:
    case Opcode2:
    case Opcode3:
    case Opcode4:
    case Opcode5:
    case Opcode6:
    case Opcode7:
      if (0 == (current_opcode->flags & InstFlag(OpcodeInModRm))) {
        FatalOperand(index, "Expects opcode to have flag OpcodeInModRm");
      }
      break;
    default:
      break;
  }
}

/* Define the next operand of the current opcode to have
 * the given kind and flags.
 */
static void DefineOperandInternal(
    OperandKind kind,
    OperandFlags flags) {
  int index;
  assert(NULL != current_opcode);
  index = current_opcode->num_operands++;
  current_opcode->operands[index].kind = kind;
  current_opcode->operands[index].flags = flags;
}

static void InstallCurrentIntoOpcodeMrm(const OpcodePrefix prefix,
                                        const uint8_t opcode,
                                        int mrm_index) {
  if (NULL == OpcodeMrmTable[opcode][prefix][mrm_index]) {
    OpcodeMrmTable[opcode][prefix][mrm_index] = current_opcode_mrm;
  } else {
    MrmOpcode* next = OpcodeMrmTable[opcode][prefix][mrm_index];
    while (NULL != next->next) {
      next = next->next;
    }
    next->next = current_opcode_mrm;
  }
}

static void MoveCurrentToMrmIndex(int mrm_index) {
  /* First remove from default location. */
  uint8_t opcode = current_opcode->opcode[current_opcode->num_opcode_bytes - 1];
  MrmOpcode* prev = NULL;
  MrmOpcode* next =
      OpcodeMrmTable[opcode][current_opcode_prefix][NO_MODRM_OPCODE_INDEX];
  while (current_opcode_mrm != next) {
    if (next == NULL) return;
    prev = next;
    next = next->next;
  }
  if (NULL == prev) {
    OpcodeMrmTable[opcode][current_opcode_prefix][NO_MODRM_OPCODE_INDEX] =
        next->next;
  } else {
    prev->next = next->next;
  }
  current_opcode_mrm = next;
  current_opcode_mrm->next = NULL;
  InstallCurrentIntoOpcodeMrm(current_opcode_prefix, opcode, mrm_index);
}

static void PrintlnOperandFlags(OperandFlags flags) {
  int i;
  for (i = 0; i < OperandFlagEnumSize; ++i) {
    if (flags & OpFlag(i)) {
      printf(" %s", OperandFlagName(i));
    }
  }
  printf("\n");
}

static void ApplySanityChecksToOpcode();

/* Same as previous function, except that sanity checks
 * are applied to see if inconsistent information is
 * being defined.
 */
void DefineOperand(
    OperandKind kind,
    OperandFlags flags) {
  int index = current_opcode->num_operands;
  DEBUG(printf("  %s:", OperandKindName(kind));
        PrintlnOperandFlags(flags));
  if (MAX_NUM_OPERANDS <= index) {
    FatalOperand(index, "Opcode defines too many operands...\n");
  }
  /* If one of the M_Operands, make sure that the ModRm mod field isn't 0x3,
   * so that we don't return registers.
   */
  switch (kind) {
    case M_Operand:
    case Mb_Operand:
    case Mw_Operand:
    case Mv_Operand:
    case Mo_Operand:
    case Mdq_Operand:
    case Mpw_Operand:
    case Mpv_Operand:
    case Mpo_Operand:
      current_opcode->flags |= InstFlag(OpcodeLtC0InModRm);
      ApplySanityChecksToOpcode();
      break;
    case Mmx_N_Operand:
      kind = Mmx_E_Operand;
      current_opcode->flags |= InstFlag(ModRmModIs0x3);
      ApplySanityChecksToOpcode();
      break;
    default:
      break;
  }
  /* Readjust counts if opcode appears in modrm. */
  if (index == 0) {
    switch (kind) {
      case Opcode0:
      case Opcode1:
      case Opcode2:
      case Opcode3:
      case Opcode4:
      case Opcode5:
      case Opcode6:
      case Opcode7:
        MoveCurrentToMrmIndex(kind - Opcode0);
        break;
      default:
        break;
    }
  }
  /* Define and apply sanity checks. */
  DefineOperandInternal(kind, flags);
  ApplySanityChecksToOperand(index);
}

void AddOperandFlags(uint8_t operand_index, OperandFlags more_flags) {
  DEBUG(printf("Adding flags:");
        PrintlnOperandFlags(more_flags));
  if (operand_index <= current_opcode->num_operands) {
    current_opcode->operands[operand_index].flags |= more_flags;
    ApplySanityChecksToOperand(operand_index);
  } else {
    FatalOperand((int) operand_index, "AddOperandFlags: index out of range\n");
  }
}

/* Returns true if the given opcode flags are consistent
 * with the value of FLAGS_run_mode.
 */
static Bool OpcodeFlagsMatchesRunMode(OpcodeFlags flags) {
  if (flags & InstFlag(Opcode32Only)) {
    if (flags & InstFlag(Opcode64Only)) {
      fatal("Can't specify both Opcode32Only and Opcode64Only");
    }
    return FLAGS_run_mode == X86_32;
  } else if (flags & InstFlag(Opcode64Only)) {
    return FLAGS_run_mode == X86_64;
  } else {
    return TRUE;
  }
}

/* Check that the flags defined for an opcode make sense. */
static void ApplySanityChecksToOpcode() {
  if (!apply_sanity_checks) return;
  if ((current_opcode->flags & InstFlag(OpcodeInModRm)) &&
      (current_opcode->flags & InstFlag(OpcodeUsesModRm))) {
    FatalOpcode("OpcodeInModRm automatically implies OpcodeUsesModRm");
  }
  if ((current_opcode->flags & InstFlag(Opcode32Only)) &&
      (current_opcode->flags & InstFlag(Opcode64Only))) {
    FatalOpcode("Can't be both Opcode32Only and Opcode64Only");
  }
  if ((current_opcode->flags & InstFlag(OperandSize_b)) &&
      (current_opcode->flags & (InstFlag(OperandSize_w) |
                                InstFlag(OperandSize_v) |
                                InstFlag(OperandSize_o) |
                                InstFlag(OperandSizeDefaultIs64) |
                                InstFlag(OperandSizeForce64)))) {
    FatalOpcode(
        "Can't specify other operand sizes when specifying OperandSize_b");
  }
  if ((current_opcode->flags & InstFlag(OpcodeInModRm)) &&
      (current_opcode->flags & InstFlag(OpcodePlusR))) {
    FatalOpcode(
        "Can't specify both OpcodeInModRm and OpcodePlusR");
  }
  if ((current_opcode->flags & InstFlag(OpcodeHasImmed_b)) &&
      (current_opcode->flags & InstFlag(OperandSize_b))) {
    FatalOpcode(
        "Size implied by OperandSize_b, use OpcodeHasImmed "
        "rather than OpcodeHasImmed_b");
  }
  if ((current_opcode->flags & InstFlag(OpcodeHasImmed_w)) &&
      (current_opcode->flags & InstFlag(OperandSize_w))) {
    FatalOpcode(
        "Size implied by OperandSize_w, use OpcodeHasImmed "
        "rather than OpcodeHasImmed_w");
  }
  if ((current_opcode->flags & InstFlag(OpcodeHasImmed_v)) &&
      (current_opcode->flags & InstFlag(OperandSize_v))) {
    FatalOpcode(
        "Size implied by OperandSize_v, use OpcodeHasImmed "
        "rather than OpcodeHasImmed_v");
  }
  if ((current_opcode->flags & InstFlag(ModRmModIs0x3)) &&
      (EmptyInstFlags == (current_opcode->flags &
                          (InstFlag(OpcodeUsesModRm) |
                           InstFlag(OpcodeInModRm))))) {
    FatalOpcode(
        "Can't specify ModRmModIs0x3 unless Opcode has modrm byte");
  }
}

static void DefineOpcodeBytes(uint8_t opcode) {
  uint8_t index;
  /* Start by clearing all entries. */
  for (index = 0; index < MAX_OPCODE_BYTES; ++index) {
    current_opcode->opcode[index] = 0x00;
  }

  /* Now fill in non-final bytes. */
  index = 0;
  switch (current_opcode_prefix) {
    case NoPrefix:
      break;
    case Prefix0F:
    case Prefix660F:
    case PrefixF20F:
    case PrefixF30F:
      current_opcode->opcode[0] = 0x0F;
      index = 1;
      break;
    case Prefix0F0F:
      current_opcode->opcode[0] = 0x0F;
      current_opcode->opcode[1] = 0x0F;
      index = 2;
      break;
    case Prefix0F38:
    case Prefix660F38:
    case PrefixF20F38:
      current_opcode->opcode[0] = 0x0F;
      current_opcode->opcode[1] = 0x38;
      index = 2;
      break;
    case Prefix0F3A:
    case Prefix660F3A:
      current_opcode->opcode[0] = 0x0F;
      current_opcode->opcode[1] = 0x3A;
      index = 2;
      break;
    case PrefixD8:
      current_opcode->opcode[0] = 0xD8;
      index = 1;
      break;
    case PrefixD9:
      current_opcode->opcode[0] = 0xD9;
      index = 1;
      break;
    case PrefixDA:
      current_opcode->opcode[0] = 0xDA;
      index = 1;
      break;
    case PrefixDB:
      current_opcode->opcode[0] = 0xDB;
      index = 1;
      break;
    case PrefixDC:
      current_opcode->opcode[0] = 0xDC;
      index = 1;
      break;
    case PrefixDD:
      current_opcode->opcode[0] = 0xDD;
      index = 1;
      break;
    case PrefixDE:
      current_opcode->opcode[0] = 0xDE;
      index = 1;
      break;
    case PrefixDF:
      current_opcode->opcode[0] = 0xDF;
      index = 1;
      break;
    default:
      fatal("Unrecognized opcode prefix in DefineOpcodeBytes");
      break;
  }

  /* Now add final byte. */
  current_opcode->opcode[index] = opcode;
  current_opcode->num_opcode_bytes = index + 1;
}

static void PrintOpcodeDescriptor(FILE* out,
                                  const OpcodePrefix prefix,
                                  const int opcode,
                                  const int modrm_index) {
  if (NO_MODRM_OPCODE_INDEX == modrm_index) {
    fprintf(out, "%s 0x%02x: ",
            OpcodePrefixName(prefix), opcode);
  } else {
    fprintf(out, "%s 0x%02x /%x: ",
            OpcodePrefixName(prefix), opcode, modrm_index);
  }
}

void DefinePrefixOpcodeMrmChoices_32_64(const OpcodePrefix prefix,
                                        const uint8_t opcode,
                                        const OperandKind modrm_opcode,
                                        const int count_32,
                                        const int count_64) {
  int modrm_index = NO_MODRM_OPCODE_INDEX;
  switch (modrm_opcode) {
    case Opcode0:
    case Opcode1:
    case Opcode2:
    case Opcode3:
    case Opcode4:
    case Opcode5:
    case Opcode6:
    case Opcode7:
      modrm_index = modrm_opcode - Opcode0;
      break;
    case Unknown_Operand:
      break;
    default:
      fprintf(stderr, "%s: ", OperandKindName(modrm_opcode));
      fatal(
          "Illegal specification of modrm opcode when defining opcode choices");
  }
  if (OpcodeCount[opcode][prefix][modrm_index] != DEFAULT_CHOICE_COUNT) {
    PrintOpcodeDescriptor(stderr, prefix, opcode, modrm_index);
    fatal("Redefining Opcode choice count");
  }
  if (FLAGS_run_mode == X86_32) {
    OpcodeCount[opcode][prefix][modrm_index] = count_32;
  } else if (FLAGS_run_mode == X86_64) {
    OpcodeCount[opcode][prefix][modrm_index] = count_64;
  }
}

void DefineOpcodeChoices(const uint8_t opcode, const int count) {
  DefinePrefixOpcodeChoices(current_opcode_prefix, opcode, count);
}

void DefineOpcodeMrmChoices(const uint8_t opcode,
                            const OperandKind modrm_opcode,
                            const int count) {
  DefinePrefixOpcodeMrmChoices(current_opcode_prefix, opcode,
                               modrm_opcode, count);
}

void DefinePrefixOpcodeChoices(const OpcodePrefix prefix,
                               const uint8_t opcode,
                               const int count) {
  DefinePrefixOpcodeChoices_32_64(prefix, opcode,
                                  count, count);
}

void DefinePrefixOpcodeMrmChoices(const OpcodePrefix prefix,
                                  const uint8_t opcode,
                                  const OperandKind modrm_opcode,
                                  const int count) {
  DefinePrefixOpcodeMrmChoices_32_64(prefix, opcode, modrm_opcode,
                                     count, count);
}

void DefineOpcodeChoices_32_64(const uint8_t opcode,
                               const int count_32,
                               const int count_64) {
  DefinePrefixOpcodeChoices_32_64(current_opcode_prefix, opcode,
                                  count_32, count_64);
}

void DefineOpcodeMrmChoices_32_64(const uint8_t opcode,
                                  const OperandKind modrm_opcode,
                                  const int count_32,
                                  const int count_64) {
  DefinePrefixOpcodeMrmChoices_32_64(current_opcode_prefix, opcode,
                                     modrm_opcode, count_32, count_64);
}

void DefinePrefixOpcodeChoices_32_64(const OpcodePrefix prefix,
                                     const uint8_t opcode,
                                     const int count_32,
                                     const int count_64) {
  DefinePrefixOpcodeMrmChoices_32_64(prefix, opcode, NO_MODRM_OPCODE,
                                     count_32, count_64);
}

static void PrintlnOpcodeFlags(OpcodeFlags flags) {
  int i;
  for (i = 0; i < OpcodeFlagEnumSize; ++i) {
    if (flags & InstFlag(i)) {
      printf(" %s", OpcodeFlagName(i));
    }
  }
  printf("\n");
}

/* Define the next opcode (instruction), initializing with
 * no operands.
 */
void DefineOpcode(
    const uint8_t opcode,
    const NaClInstType insttype,
    OpcodeFlags flags,
    const InstMnemonic name) {
  /* TODO(karl) If we can deduce a more specific prefix that
   * must be used, due to the flags associated with the opcode,
   * then put on the more specific prefix list. This will make
   * the defining of instructions easier, in that the caller doesn't
   * need to specify the prefix to use.
   */
  int i;

  /* Before starting, expand appropriate implicit flag assumnptions. */
  if (flags & InstFlag(OpcodeLtC0InModRm)) {
    flags |= InstFlag(OpcodeInModRm);
  }

  DEBUG(printf("Define %s %"PRIx8": %s(%d)",
               OpcodePrefixName(current_opcode_prefix),
               opcode, InstMnemonicName(name), name);
        PrintlnOpcodeFlags(flags));

  if (InstMnemonicEnumSize <= name) {
    fatal("Badly defined mnemonic name");
  }

  if (kNaClInstTypeRange <= insttype) {
    fatal("Badly defined inst type");
  }

  /* Create opcode and initialize */
  current_opcode_mrm = (MrmOpcode*) malloc(sizeof(MrmOpcode));
  if (NULL == current_opcode_mrm) {
    fatal("DefineOpcode: malloc failed");
  }
  DEBUG(printf("current = %p\n", (void*) current_opcode_mrm));
  current_opcode_mrm->next = NULL;
  current_opcode = &(current_opcode_mrm->opcode);
  DefineOpcodeBytes(opcode);
  current_opcode->insttype = insttype;
  current_opcode->flags = flags;
  current_opcode->name = name;
  current_opcode->next_rule = NULL;

  /* undefine all operands. */
  current_opcode->num_operands = 0;
  for (i = 0; i < MAX_NUM_OPERANDS; ++i) {
    DefineOperandInternal(Unknown_Operand, 0);
  }
  /* Now reset number of operands to zero. */
  current_opcode->num_operands = 0;

  ApplySanityChecksToOpcode();

  if (name == InstUndefined || !OpcodeFlagsMatchesRunMode(flags)) {
    return;
  }

  /* Install Opcode. */
  if (NULL == OpcodeTable[opcode][current_opcode_prefix]) {
    OpcodeTable[opcode][current_opcode_prefix] = current_opcode;
  } else {
    Opcode* next = OpcodeTable[opcode][current_opcode_prefix];
    while (NULL != next->next_rule) {
      next = next->next_rule;
    }
    next->next_rule = current_opcode;
  }
  /* Install assuming no modrm opcode. Let DefineOperand move if needed. */
  InstallCurrentIntoOpcodeMrm(current_opcode_prefix, opcode,
                              NO_MODRM_OPCODE_INDEX);
}

void AddOpcodeFlags(OpcodeFlags more_flags) {
  DEBUG(printf("Adding opcode flags:");
        PrintlnOpcodeFlags(more_flags));
  current_opcode->flags |= more_flags;
  ApplySanityChecksToOpcode();
}

void DelaySanityChecks() {
  apply_sanity_checks = FALSE;
}

void ApplySanityChecks() {
  apply_sanity_checks = TRUE;
  if (NULL != current_opcode) {
    int i;
    ApplySanityChecksToOpcode();
    for (i = 0; i < current_opcode->num_operands; i++) {
      ApplySanityChecksToOperand(i);
    }
  }
}

static void InitializeOpcodeTables() {
  int i;
  OpcodePrefix prefix;
  int j;
  /* Before starting, verify that we have defined OpcodeFlags and OperandFlags
   * big enough to hold the flags associated with it.
   */
  assert(OpcodeFlagEnumSize <= sizeof(OpcodeFlags) * 8);
  assert(OperandFlagEnumSize <= sizeof(OperandFlags) * 8);

  for (i = 0; i < NCDTABLESIZE; ++i) {
    for (prefix = NoPrefix; prefix < OpcodePrefixEnumSize; ++prefix) {
      OpcodeTable[i][prefix] = NULL;
      for (j = 0; j <= NO_MODRM_OPCODE_INDEX; ++j) {
        OpcodeCount[i][prefix][j] = DEFAULT_CHOICE_COUNT;
      }
    }
    PrefixTable[i] = "0";
  }
  DefineOpcodePrefix(NoPrefix);
  DefineOpcode(0x0, NACLi_ILLEGAL, 0, InstUndefined);
  undefined_opcode = current_opcode;
}

static void DefinePrefixBytes() {
  EncodePrefixName(0x26, "kPrefixSEGES");
  EncodePrefixName(0x2e, "kPrefixSEGCS");
  EncodePrefixName(0x36, "kPrefixSEGSS");
  EncodePrefixName(0x3e, "kPrefixSEGDS");
  EncodePrefixName(0x64, "kPrefixSEGFS");
  EncodePrefixName(0x65, "kPrefixSEGGS");
  EncodePrefixName(0x66, "kPrefixDATA16");
  EncodePrefixName(0x67, "kPrefixADDR16");
  EncodePrefixName(0xf0, "kPrefixLOCK");
  EncodePrefixName(0xf2, "kPrefixREPNE");
  EncodePrefixName(0xf3, "kPrefixREP");

  if (NACL_TARGET_SUBARCH == 64) {
    int i;
    for (i = 0; i < 16; ++i) {
      EncodePrefixName(0x40+i, "kPrefixREX");
    }
  }
}

/* Build the set of x64 opcode (instructions). */
static void BuildOpcodeTables() {
  InitializeOpcodeTables();

  DefinePrefixBytes();

  DefineOneByteOpcodes();

  Define0FOpcodes();

  DefineSseOpcodes();

  DefineX87Opcodes();
}

/* Return the number of opcode rules pointed to by the parameter. */
static int OpcodeListLength(Opcode* next) {
  int count = 0;
  while (NULL != next) {
    ++count;
    next = next->next_rule;
  }
  return count;
}

static int OpcodeMrmListLength(MrmOpcode* next) {
  int count = 0;
  while (NULL != next) {
    ++count;
    next = next->next;
  }
  return count;
}

/* Collect the number of opcode (instructions) in the given table,
 * and return the number found.
 */
static int CountNumberOpcodes() {
  int i;
  OpcodePrefix prefix;
  int count = 0;
  for (i = 0; i < NCDTABLESIZE; ++i) {
    for (prefix = NoPrefix; prefix < OpcodePrefixEnumSize; ++prefix) {
      count += OpcodeListLength(OpcodeTable[i][prefix]);
    }
  }
  return count;
}

static void FatalChoiceCount(const int expected,
                             const int found,
                             const OpcodePrefix prefix,
                             const int opcode,
                             const int modrm_index,
                             MrmOpcode* opcodes) {
  PrintOpcodeDescriptor(stderr, prefix, opcode, modrm_index);
  fprintf(stderr, "Expected %d rules but found %d:\n", expected, found);
  while (NULL != opcodes) {
    PrintOpcode(stderr, &(opcodes->opcode));
    opcodes = opcodes->next;
  }
  fatal("fix before continuing...\n");
}

/* Verify that the number of possible choies for each prefix:opcode matches
 * what was explicitly defined.
 */
static void VerifyOpcodeCounts() {
  int i, j;
  OpcodePrefix prefix;
  for (i = 0; i < NCDTABLESIZE; ++i) {
    for (prefix = NoPrefix; prefix < OpcodePrefixEnumSize; ++prefix) {
      for (j = 0; j < MODRM_OPCODE_SIZE; ++j) {
        MrmOpcode* opcodes = OpcodeMrmTable[i][prefix][j];
        int found = OpcodeMrmListLength(opcodes);
        int expected = OpcodeCount[i][prefix][j];
        if (expected == DEFAULT_CHOICE_COUNT) {
          if (found > 1) {
            FatalChoiceCount(1, found, prefix, i, j, opcodes);
          }
        } else if (expected != found) {
          FatalChoiceCount(expected, found, prefix, i, j, opcodes);
        }
      }
    }
  }
}

/* Generate header information, based on the executable name in argv0,
 * and the file to be generated (defined by fname).
 */
static void PrintHeader(FILE* f, const char* argv0, const char* fname) {
  time_t timeofday;
  if (time(&timeofday) < 0) {
    fprintf(stderr, "time() failed\n");
    exit(-1);
  }
  fprintf(f, "/* %s\n", fname);
  fprintf(f, " * THIS FILE IS AUTO-GENERATED. DO NOT EDIT.\n");
  fprintf(f, " * This file was auto-generated by %s\n", argv0);
  fprintf(f, " * on: %s\n", ctime(&timeofday));
  fprintf(f, " *\n");
  fprintf(f, " * Compiled for %s.\n", RunModeName(FLAGS_run_mode));
  fprintf(f, " *\n");
  fprintf(f, " * You must include ncopcode_desc.h before this file.\n");
  fprintf(f, " */\n\n");
}

/* Print out which bytes correspond to prefix bytes. */
static void PrintPrefixTable(FILE* f) {
  int opc;
  fprintf(f, "static const uint32_t kPrefixTable[NCDTABLESIZE] = {");
  for (opc = 0; opc < NCDTABLESIZE; opc++) {
    if (0 == opc % 16) {
      fprintf(f, "\n  /* 0x%02x-0x%02x */\n  ", opc, opc + 15);
    }
    fprintf(f, "%s, ", PrefixTable[opc]);
  }
  fprintf(f, "\n};\n\n");
}

/* Print out the contents of the defined instructions into the given file. */
static void PrintDecodeTables(FILE* f) {
  int i;
  OpcodePrefix prefix;
  int count = 0;

  /* Build table of all possible instructions. Note that we build
   * build the list of instructions by using a "
  */

  int num_opcodes = CountNumberOpcodes();

  /* lookup_index holds the number of the opcode (instruction) that
   * begins the list of instructions for the corresponding opcode
   * in OpcodeTable.
   */
  int lookup_index[NCDTABLESIZE][OpcodePrefixEnumSize];

  fprintf(f, "static Opcode g_Opcodes[%d] = {\n", num_opcodes);
  for (prefix = NoPrefix; prefix < OpcodePrefixEnumSize; ++prefix) {
    for (i = 0; i < NCDTABLESIZE; ++i) {
      /* Build the list of instructions by knowing that the next
       * element in the list will always follow the current entry,
       * when added to the array.
       */
      Opcode* next = OpcodeTable[i][prefix];
      if (NULL == next) {
        lookup_index[i][prefix] = -1;
      } else {
        int lookahead = count + 1;
        lookup_index[i][prefix] = count;
        while (NULL != next) {
          PrintOpcodeTablegen(f, count, next, lookahead);
          next = next->next_rule;
          ++lookahead;
          ++count;
        }
      }
    }
  }
  fprintf(f, "};\n\n");

  /* Print out the undefined opcode */
  fprintf(f, "static Opcode g_Undefined_Opcode = \n");
  PrintOpcodeTableDriver(f, FALSE, FALSE, 0, undefined_opcode, 0);

  /* Now print lookup table of rules. */
  fprintf(f,
          "static Opcode* "
          "g_OpcodeTable[OpcodePrefixEnumSize][NCDTABLESIZE] = {\n");
  for (prefix = NoPrefix; prefix < OpcodePrefixEnumSize; ++prefix) {
    fprintf(f,"/* %s */\n", OpcodePrefixName(prefix));
    fprintf(f, "{\n");
    for (i = 0; i < NCDTABLESIZE; ++i) {
      /* Take advantage of the fact that the lists were added to
       * the array of opcodes such that the next element in the list
       * will always follow the current entry.
       */
      Opcode* next = OpcodeTable[i][prefix];
      fprintf(f, "  /* %02x */ ", i);
      if (NULL == next) {
        fprintf(f, "NULL");
      } else {
        fprintf(f, "g_Opcodes + %d", lookup_index[i][prefix]);
      }
      fprintf(f, "  ,\n");
    }
    fprintf(f, "},\n");
  }
  fprintf(f, "};\n\n");

  PrintPrefixTable(f);
}

/* Open the given file using the given directives (how). */
FILE* mustopen(const char* fname, const char* how) {
  FILE* f = fopen(fname, how);
  if (f == NULL) {
    fprintf(stderr, "could not fopen(%s, %s)\n", fname, how);
    fprintf(stderr, "exiting now\n");
    exit(-1);
  }
  return f;
}

/* Recognizes flags in argv, processes them, and then removes them.
 * Returns the updated value for argc.
 */
int GrokFlags(int argc, const char* argv[]) {
  int i;
  int new_argc;
  if (argc == 0) return 0;
  new_argc = 1;
  for (i = 1; i < argc; ++i) {
    if (0 == strcmp("-m32", argv[i])) {
      FLAGS_run_mode = X86_32;
    } else if (0 == strcmp("-m64", argv[i])) {
      FLAGS_run_mode = X86_64;
    } else {
      argv[new_argc++] = argv[i];
    }
  }
  return new_argc;
}

int main(const int argc, const char* argv[]) {
  FILE *f;
  int new_argc = GrokFlags(argc, argv);
  if (new_argc != 2) {
    fprintf(stderr,
            "ERROR: usage: ncdecode_tablegen [options] "
            "file\n");
    return -1;
  }

  BuildOpcodeTables();

  VerifyOpcodeCounts();

  f = mustopen(argv[1], "w");
  PrintHeader(f, argv[0], argv[1]);
  PrintDecodeTables(f);
  fclose(f);

  return 0;
}
