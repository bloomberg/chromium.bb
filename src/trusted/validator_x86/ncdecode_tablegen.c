/* Copyright (c) 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * New table driven generator for a decoder of x86 code.
 *
 * Note: Most of the organization of this document is based on the
 * NaClOpcode Map appendix of one of the following documents:

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

#include "native_client/src/trusted/validator_x86/ncdecode_tablegen.h"
#include "native_client/src/trusted/validator_x86/ncdecode_forms.h"
#include "native_client/src/include/portability.h"

/* To turn on debugging of instruction decoding, change value of
 * DEBUGGING to 1.
 */
#define DEBUGGING 0

#include "native_client/src/shared/utils/debugging.h"

/* Define the number of possible bytes values. */
#define NACL_NUM_BYTE_VALUES 256

/* Model an NaClInst, sorted by OpcodeInModRm values. */
typedef struct NaClMrmInst {
  NaClInst inst;
  struct NaClMrmInst* next;
} NaClMrmInst;

/* Note: in general all errors in this module will be fatal.
 * To debug: use gdb or your favorite debugger.
 */
static void NaClFatal(const char* s) {
  fprintf(stderr, "%s\n", s);
  fprintf(stderr, "fatal error, cannot recover\n");
  exit(-1);
}

/* Returns the print name of the given run mode. */
static const char* NaClRunModeName(NaClRunMode mode) {
  switch (mode) {
    case X86_32: return "x86-32 bit mode";
    case X86_64: return "x86-64 bit mode";
    default: assert(0);
  }

  /* NOTREACHED */
  return NULL;
}

/* Defines the run mode files that should be generated. */
static NaClRunMode FLAGS_run_mode = NaClRunModeSize;

/* Holds the current instruction prefix. */
static NaClInstPrefix current_opcode_prefix = NoPrefix;

/* Holds the default instruction prefix. */
static NaClInstPrefix default_opcode_prefix = NoPrefix;

/* Holds the current instruction being built. */
static NaClInst* current_inst = NULL;

/* Holds the root of the trie of known instruction opcode sequences. */
static NaClInstNode* inst_node_root = NULL;

/* Holds the current opcode sequence to be associated with the next
 * defined opcode.
 */
static NaClInstNode* current_inst_node = NULL;

/* Holds the candidate for the current_inst_node, when
 * NaClDefInst is called.
 */
static NaClInstNode* current_cand_inst_node = NULL;

/* Holds the current instruction with mrm extention being built. */
static NaClMrmInst* current_inst_mrm = NULL;

/* Holds the instruction to model instructions that can't be parsed. */
static NaClInst* undefined_inst = NULL;

/* True if we are to apply sanity checks as we define operantions. */
static Bool apply_sanity_checks = TRUE;

/* Holds all defined opcodes. */
static NaClInst* NaClInstTable[NCDTABLESIZE][NaClInstPrefixEnumSize];

#define NACL_DEFAULT_CHOICE_COUNT (-1)

#define NACL_NO_MODRM_OPCODE Unknown_Operand

#define NACL_NO_MODRM_OPCODE_INDEX 8

#define NACL_MODRM_OPCODE_SIZE (NACL_NO_MODRM_OPCODE_INDEX + 1)

/* Holds the expected number of entries in the defined instructions.
 * Note: the last index corresponds to the modrm opcode, or
 * NACL_NO_MODRM_OPCODE if no modrm opcode.
 */
static int NaClInstCount[NCDTABLESIZE]
          [NaClInstPrefixEnumSize][NACL_MODRM_OPCODE_SIZE];

static NaClMrmInst* NaClInstMrmTable[NCDTABLESIZE]
          [NaClInstPrefixEnumSize][NACL_MODRM_OPCODE_SIZE];

/* Holds encodings of prefix bytes. */
const char* NaClPrefixTable[NCDTABLESIZE];

/* Prints out the opcode prefix being defined, the opcode pattern
 * being defined, and the given error message. Then aborts the
 * execution of the program.
 */
static void NaClFatalInst(const char* message) {
  fprintf(stderr, "Prefix: %s\n", NaClInstPrefixName(current_opcode_prefix));
  NaClInstPrint(stderr, current_inst);
  NaClFatal(message);
}

/* Prints out what operand is currently being defined, followed by the given
 * error message. Then aborts the execution of the program.
 */
static void NaClFatalOp(int index, const char* message) {
  if (0 <= index && index <= current_inst->num_operands) {
    fprintf(stderr, "On operand %d: %s\n", index,
            NaClOpKindName(current_inst->operands[index].kind));
  } else {
    fprintf(stderr, "On operand %d:\n", index);
  }
  NaClFatalInst(message);
}

/* Define the prefix name for the given opcode, for the given run mode. */
static void NaClEncodeModedPrefixName(const uint8_t byte, const char* name,
                                      const NaClRunMode mode) {
  if (FLAGS_run_mode == mode) {
    NaClPrefixTable[byte] = name;
  }
}

/* Define the prefix name for the given opcode, for all run modes. */
static void NaClEncodePrefixName(const uint8_t byte, const char* name) {
  NaClEncodeModedPrefixName(byte, name, FLAGS_run_mode);
}

/* Change the current opcode prefix to the given value. */
void NaClDefInstPrefix(const NaClInstPrefix prefix) {
  current_opcode_prefix = prefix;
}

void NaClDefDefaultInstPrefix(const NaClInstPrefix prefix) {
  default_opcode_prefix = prefix;
  NaClDefInstPrefix(prefix);
}

void NaClResetToDefaultInstPrefix() {
  NaClDefInstPrefix(default_opcode_prefix);
}

/* Check that the given operand is an extention of the opcode
 * currently being defined. If not, generate appropriate error
 * message and stop program.
 */
static void NaClCheckIfOpExtendsInst(NaClOp* operand) {
  if (NACL_EMPTY_OPFLAGS == (operand->flags &
                             NACL_OPFLAG(OperandExtendsOpcode))) {
    NaClFatalInst(
        "First operand should be marked with flag OperandExtendsOpcode");
  }
}

/* Check if an E_Operand operand has been repeated, since it should
 * never appear for more than one argument. If repeated, generate an
 * appropriate error message and terminate.
 */
static void NaClCheckIfERepeated(int index) {
  int i;
  for (i = 0; i < index; ++i) {
    NaClOp* operand = &current_inst->operands[i];
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
        NaClFatalOp(index, "Can't use E Operand more than once");
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
static void NaClCheckIfOpcodeInModRm(int index) {
  if ((current_inst->flags & NACL_IFLAG(OpcodeInModRm)) &&
      (NACL_EMPTY_OPFLAGS == (current_inst->operands[index].flags &
                              NACL_IFLAG(AllowGOperandWithOpcodeInModRm)))) {
    NaClFatalOp(index,
                 "Can't use G Operand, bits being used for opcode in modrm");
  }
}

/* Check if an G_Operand operand has been repeated, since it should
 * never appear for more than one argument. If repeated, generate an
 * appropriate error message and terminate.
 */
static void NaClCheckIfGRepeated(int index) {
  int i;
  for (i = 0; i < index; ++i) {
    NaClOp* operand = &current_inst->operands[i];
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
        NaClFatalOp(index, "Can't use G Operand more than once");
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
static void NaClCheckIfIRepeated(int index) {
  int i;
  for (i = 0; i < index; ++i) {
    NaClOp* operand = &current_inst->operands[i];
    switch (operand->kind) {
      case I_Operand:
      case Ib_Operand:
      case Iw_Operand:
      case Iv_Operand:
      case Io_Operand:
        NaClFatalOp(index, "Can't use I_Operand more than once");
        break;
      case J_Operand:
      case Jb_Operand:
      case Jv_Operand:
        NaClFatalOp(index, "Can't use both I_Operand and J_Operand");
        break;
      default:
        break;
    }
  }
}

/* Check that the operand being defined (via the given index), does not
 * specify any inconsistent flags.
 */
static void NaClApplySanityChecksToOp(int index) {
  NaClOp* operand = &current_inst->operands[index];

  if (!apply_sanity_checks) return;

  /* Check special cases for operand 0. */
  if (index == 0) {
    if (current_inst->flags & NACL_IFLAG(OpcodeInModRm)) {
      if ((operand->kind < Opcode0) ||
          (operand->kind > Opcode7)) {
        NaClFatalOp(
            index,
            "First operand of OpcodeInModRm  must be in {Opcode0..Opcode7}");
      }
      NaClCheckIfOpExtendsInst(operand);
    }
    if (current_inst->flags & NACL_IFLAG(OpcodePlusR)) {
      if ((operand->kind < OpcodeBaseMinus0) ||
          (operand->kind > OpcodeBaseMinus7)) {
        NaClFatalOp(
            index,
            "First operand of OpcodePlusR must be in "
            "{OpcodeBaseMinus0..OpcodeBaseMinus7}");
      }
      NaClCheckIfOpExtendsInst(operand);
    }
  }
  if (operand->flags & NACL_OPFLAG(OperandExtendsOpcode)) {
    if (index > 0) {
      NaClFatalOp(index, "OperandExtendsOpcode only allowed on first operand");
    }
    if (operand->flags != NACL_OPFLAG(OperandExtendsOpcode)) {
      NaClFatalOp(index,
                   "Only OperandExtendsOpcode allowed for flag "
                   "values on this operand");
    }
  }

  /* Check that operand is consistent with other operands defined, or flags
   * defined on the opcode.
   */
  switch (operand->kind) {
    case E_Operand:
      NaClCheckIfERepeated(index);
      break;
    case Eb_Operand:
      if (current_inst->flags & NACL_IFLAG(OperandSize_b)) {
        NaClFatalOp(index,
                     "Size implied by OperandSize_b, use E_Operand instead");
      }
      NaClCheckIfERepeated(index);
      break;
    case Ew_Operand:
      if ((current_inst->flags & NACL_IFLAG(OperandSize_w)) &&
          !(current_inst->flags & NACL_IFLAG(OperandSize_v))) {
        NaClFatalOp(index,
                     "Size implied by OperandSize_w, use E_Operand instead");
      }
      NaClCheckIfERepeated(index);
      break;
    case Ev_Operand:
      if (current_inst->flags & NACL_IFLAG(OperandSize_v)) {
        NaClFatalOp(index,
                     "Size implied by OperandSize_v, use E_Operand instead");
      }
      NaClCheckIfERepeated(index);
      break;
    case Eo_Operand:
      if (current_inst->flags & NACL_IFLAG(OperandSize_o)) {
        NaClFatalOp(index,
                     "Size implied by OperandSize_o, use E_Operand instead");
      }
      NaClCheckIfERepeated(index);
      break;
    case Edq_Operand:
      NaClCheckIfERepeated(index);
      break;
    case Mmx_E_Operand:
    case Xmm_E_Operand:
      NaClCheckIfERepeated(index);
      break;
    case Xmm_Eo_Operand:
      if (current_inst->flags & NACL_IFLAG(OperandSize_o)) {
        NaClFatalOp
            (index,
             "Size implied by OperandSize_o, use Xmm_E_Operand instead");
      }
      NaClCheckIfERepeated(index);
      break;
    case G_Operand:
      NaClCheckIfGRepeated(index);
      NaClCheckIfOpcodeInModRm(index);
      break;
    case Gb_Operand:
      if (current_inst->flags & NACL_IFLAG(OperandSize_b)) {
        NaClFatalOp(index,
                     "Size implied by OperandSize_b, use G_Operand instead");
      }
      NaClCheckIfGRepeated(index);
      NaClCheckIfOpcodeInModRm(index);
      break;
    case Gw_Operand:
      if (current_inst->flags & NACL_IFLAG(OperandSize_w)) {
        NaClFatalOp(index,
                     "Size implied by OperandSize_w, use G_Operand instead");
      }
      NaClCheckIfGRepeated(index);
      NaClCheckIfOpcodeInModRm(index);
      break;
    case Gv_Operand:
      if (current_inst->flags & NACL_IFLAG(OperandSize_v)) {
        NaClFatalOp(index,
                     "Size implied by OperandSize_v, use G_Operand instead");
      }
      NaClCheckIfGRepeated(index);
      NaClCheckIfOpcodeInModRm(index);
      break;
    case Go_Operand:
      if (current_inst->flags & NACL_IFLAG(OperandSize_o)) {
        NaClFatalOp(index,
                     "Size implied by OperandSize_o, use G_Operand instead");
      }
      NaClCheckIfGRepeated(index);
      NaClCheckIfOpcodeInModRm(index);
      break;
    case Gdq_Operand:
      NaClCheckIfGRepeated(index);
      NaClCheckIfOpcodeInModRm(index);
      break;
    case Mmx_G_Operand:
    case Xmm_G_Operand:
      NaClCheckIfGRepeated(index);
      NaClCheckIfOpcodeInModRm(index);
      break;
    case Xmm_Go_Operand:
      if (current_inst->flags & NACL_IFLAG(OperandSize_o)) {
        NaClFatalOp
            (index,
             "Size implied by OperandSize_o, use Xmm_G_Operand instead");
      }
      NaClCheckIfGRepeated(index);
      NaClCheckIfOpcodeInModRm(index);
      break;
    case I_Operand:
      NaClCheckIfIRepeated(index);
      break;
    case Ib_Operand:
      if (current_inst->flags & NACL_IFLAG(OperandSize_b)) {
        NaClFatalOp(index,
                     "Size implied by OperandSize_b, use I_Operand instead");
      }
      if (current_inst->flags & NACL_IFLAG(OpcodeHasImmed_b)) {
        NaClFatalOp(index,
                     "Size implied by OpcodeHasImmed_b, use I_Operand instead");
      }
      NaClCheckIfIRepeated(index);
      break;
    case Iw_Operand:
      if (current_inst->flags & NACL_IFLAG(OperandSize_w)) {
        NaClFatalOp(index,
                     "Size implied by OperandSize_w, use I_Operand instead");
      }
      if (current_inst->flags & NACL_IFLAG(OpcodeHasImmed_w)) {
        NaClFatalOp(index,
                     "Size implied by OpcodeHasImmed_w, use I_Operand instead");
      }
      NaClCheckIfIRepeated(index);
      break;
    case Iv_Operand:
      if (current_inst->flags & NACL_IFLAG(OperandSize_v)) {
        NaClFatalOp(index,
                     "Size implied by OperandSize_v, use I_Operand instead");
      }
      if (current_inst->flags & NACL_IFLAG(OpcodeHasImmed_v)) {
        NaClFatalOp(index,
                     "Size implied by OpcodeHasImmed_v, use I_Operand instead");
      }
      NaClCheckIfIRepeated(index);
      break;
    case Io_Operand:
      if (current_inst->flags & NACL_IFLAG(OperandSize_o)) {
        NaClFatalOp(index,
                     "Size implied by OperandSize_o, use I_Operand instead");
      }
      if (current_inst->flags & NACL_IFLAG(OpcodeHasImmed_o)) {
        NaClFatalOp(index,
                     "Size implied by OpcodeHasImmed_o, use I_Operand instead");
      }
      NaClCheckIfIRepeated(index);
      break;
    case OpcodeBaseMinus0:
    case OpcodeBaseMinus1:
    case OpcodeBaseMinus2:
    case OpcodeBaseMinus3:
    case OpcodeBaseMinus4:
    case OpcodeBaseMinus5:
    case OpcodeBaseMinus6:
    case OpcodeBaseMinus7:
      if (NACL_EMPTY_IFLAGS == (current_inst->flags &
                                NACL_IFLAG(OpcodePlusR))) {
        NaClFatalOp(index, "Expects opcode to have flag OpcodePlusR");
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
      if (NACL_EMPTY_IFLAGS == (current_inst->flags &
                                NACL_IFLAG(OpcodeInModRm))) {
        NaClFatalOp(index, "Expects opcode to have flag OpcodeInModRm");
      }
      break;
    default:
      break;
  }
}

/* Define the next operand of the current opcode to have
 * the given kind and flags.
 */
static void NaClDefOpInternal(NaClOpKind kind, NaClOpFlags flags) {
  int index;
  assert(NULL != current_inst);
  index = current_inst->num_operands++;
  current_inst->operands[index].kind = kind;
  current_inst->operands[index].flags = flags;
}

static void NaClInstallCurrentIntoOpcodeMrm(const NaClInstPrefix prefix,
                                            const uint8_t opcode,
                                            int mrm_index) {
  if (NULL == NaClInstMrmTable[opcode][prefix][mrm_index]) {
    NaClInstMrmTable[opcode][prefix][mrm_index] = current_inst_mrm;
  } else {
    NaClMrmInst* next = NaClInstMrmTable[opcode][prefix][mrm_index];
    while (NULL != next->next) {
      next = next->next;
    }
    next->next = current_inst_mrm;
  }
}

/* Removes the current_inst_mrm from the corresponding
 * NaClInstTable.
 *
 * Used when Opcode32Only or Opcode64Only flag is added, and
 * the flag doesn't match the subarchitecture being modeled.
 */
static void NaClRemoveCurrentInstMrmFromInstTable() {
  uint8_t opcode = current_inst->opcode[current_inst->num_opcode_bytes - 1];
  NaClInst* prev = NULL;
  NaClInst* next = NaClInstTable[opcode][current_inst->prefix];
  while (NULL != next) {
    if (current_inst == next) {
      /* Found - remove! */
      if (NULL == prev) {
        NaClInstTable[opcode][current_inst->prefix] = next->next_rule;
      } else {
        prev->next_rule = next->next_rule;
      }
      return;
    } else  {
      prev = next;
      next = next->next_rule;
    }
  }
}

/* Removes the current_inst_mrm from the corresponding
 * NaClInstMrmTable.
 *
 * Used when Opcode32Only or Opcode64Only flag is added, and
 * the flag doesn't match the subarchitecture being modeled.
 */
static void NaClRemoveCurrentInstMrmFromInstMrmTable() {
  /* Be sure to try opcode in first operand (if applicable),
   * and the default list NACL_NO_MODRM_OPCODE_INDEX, in case
   * the operand hasn't been processed yet.
   */
  int mrm_opcode = NACL_NO_MODRM_OPCODE_INDEX;
  if (current_inst->flags & NACL_IFLAG(OpcodeInModRm) &&
      current_inst->num_operands > 0) {
    switch(current_inst->operands[0].kind) {
      case Opcode0:
      case Opcode1:
      case Opcode2:
      case Opcode3:
      case Opcode4:
      case Opcode5:
      case Opcode6:
      case Opcode7:
        mrm_opcode = current_inst->operands[0].kind - Opcode0;
        break;
      default:
        break;
    }
  }
  while (1) {
    uint8_t opcode = current_inst->opcode[current_inst->num_opcode_bytes - 1];
    NaClMrmInst* prev = NULL;
    NaClMrmInst* next =
        NaClInstMrmTable[opcode][current_inst->prefix][mrm_opcode];
    while (NULL != next) {
      if (current_inst_mrm == next) {
        /* Found - remove! */
        if (NULL == prev) {
          NaClInstMrmTable[opcode][current_inst->prefix][mrm_opcode] =
              next->next;
        } else {
          prev->next = next->next;
        }
        return;
      } else {
        prev = next;
        next = next->next;
      }
    }
    if (mrm_opcode == NACL_NO_MODRM_OPCODE_INDEX) return;
    mrm_opcode = NACL_NO_MODRM_OPCODE_INDEX;
  }
}

static void NaClMoveCurrentToMrmIndex(int mrm_index) {
  /* First remove from default location. */
  uint8_t opcode = current_inst->opcode[current_inst->num_opcode_bytes - 1];
  NaClMrmInst* prev = NULL;
  NaClMrmInst* next =
      NaClInstMrmTable[opcode][current_opcode_prefix]
                      [NACL_NO_MODRM_OPCODE_INDEX];
  while (current_inst_mrm != next) {
    if (next == NULL) return;
    prev = next;
    next = next->next;
  }
  if (NULL == prev) {
    NaClInstMrmTable[opcode][current_opcode_prefix]
                    [NACL_NO_MODRM_OPCODE_INDEX] =
        next->next;
  } else {
    prev->next = next->next;
  }
  current_inst_mrm = next;
  current_inst_mrm->next = NULL;
  NaClInstallCurrentIntoOpcodeMrm(current_opcode_prefix, opcode, mrm_index);
}

static void NaClPrintlnOpFlags(NaClOpFlags flags) {
  int i;
  for (i = 0; i < NaClOpFlagEnumSize; ++i) {
    if (flags & NACL_OPFLAG(i)) {
      printf(" %s", NaClOpFlagName(i));
    }
  }
  printf("\n");
}

static void NaClApplySanityChecksToInst();

/* Same as previous function, except that sanity checks
 * are applied to see if inconsistent information is
 * being defined.
 */
void NaClDefOp(
    NaClOpKind kind,
    NaClOpFlags flags) {
  int index = current_inst->num_operands;
  DEBUG(printf("  %s:", NaClOpKindName(kind));
        NaClPrintlnOpFlags(flags));
  if (NACL_MAX_NUM_OPERANDS <= index) {
    NaClFatalOp(index, "NaClOpcode defines too many operands...\n");
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
      current_inst->flags |= NACL_IFLAG(OpcodeLtC0InModRm);
      NaClApplySanityChecksToInst();
      break;
    case Mmx_N_Operand:
      kind = Mmx_E_Operand;
      current_inst->flags |= NACL_IFLAG(ModRmModIs0x3);
      NaClApplySanityChecksToInst();
      break;
    default:
      break;
  }

  /* Readjust counts if opcode appears in modrm, and not a specific opcode
   * sequence.
   */
  if ((index == 0) && (NULL == current_inst_node)) {
    switch (kind) {
      case Opcode0:
      case Opcode1:
      case Opcode2:
      case Opcode3:
      case Opcode4:
      case Opcode5:
      case Opcode6:
      case Opcode7:
        NaClMoveCurrentToMrmIndex(kind - Opcode0);
        break;
      default:
        break;
    }
  }
  /* Define and apply sanity checks. */
  NaClDefOpInternal(kind, flags);
  NaClApplySanityChecksToOp(index);
}

void NaClAddOpFlags(uint8_t operand_index, NaClOpFlags more_flags) {
  DEBUG(printf("Adding flags:");
        NaClPrintlnOpFlags(more_flags));
  if (operand_index <= current_inst->num_operands) {
    current_inst->operands[operand_index].flags |= more_flags;
    NaClApplySanityChecksToOp(operand_index);
  } else {
    NaClFatalOp((int) operand_index, "NaClAddOpFlags: index out of range\n");
  }
}

/* Returns true if the given opcode flags are consistent
 * with the value of FLAGS_run_mode.
 */
static Bool NaClIFlagsMatchesRunMode(NaClIFlags flags) {
  if (flags & NACL_IFLAG(Opcode32Only)) {
    if (flags & NACL_IFLAG(Opcode64Only)) {
      NaClFatal("Can't specify both Opcode32Only and Opcode64Only");
    }
    return FLAGS_run_mode == X86_32;
  } else if (flags & NACL_IFLAG(Opcode64Only)) {
    return FLAGS_run_mode == X86_64;
  } else if (flags & NACL_IFLAG(Opcode32Only)) {
    return FLAGS_run_mode == X86_32;
  } else {
    return TRUE;
  }
}

/* Check that the flags defined for an opcode make sense. */
static void NaClApplySanityChecksToInst() {
  if (!apply_sanity_checks) return;
  if ((current_inst->flags & NACL_IFLAG(OpcodeInModRm)) &&
      (current_inst->flags & NACL_IFLAG(OpcodeUsesModRm))) {
    NaClFatalInst("OpcodeInModRm automatically implies OpcodeUsesModRm");
  }
  if ((current_inst->flags & NACL_IFLAG(Opcode32Only)) &&
      (current_inst->flags & NACL_IFLAG(Opcode64Only))) {
    NaClFatalInst("Can't be both Opcode32Only and Opcode64Only");
  }
  if ((current_inst->flags & NACL_IFLAG(OperandSize_b)) &&
      (current_inst->flags & (NACL_IFLAG(OperandSize_w) |
                              NACL_IFLAG(OperandSize_v) |
                              NACL_IFLAG(OperandSize_o) |
                              NACL_IFLAG(OperandSizeDefaultIs64) |
                              NACL_IFLAG(OperandSizeForce64)))) {
    NaClFatalInst(
        "Can't specify other operand sizes when specifying OperandSize_b");
  }
  if ((current_inst->flags & NACL_IFLAG(OpcodeInModRm)) &&
      (current_inst->flags & NACL_IFLAG(OpcodePlusR))) {
    NaClFatalInst(
        "Can't specify both OpcodeInModRm and OpcodePlusR");
  }
  if ((current_inst->flags & NACL_IFLAG(OpcodeHasImmed_b)) &&
      (current_inst->flags & NACL_IFLAG(OperandSize_b))) {
    NaClFatalInst(
        "Size implied by OperandSize_b, use OpcodeHasImmed "
        "rather than OpcodeHasImmed_b");
  }
  if ((current_inst->flags & NACL_IFLAG(OpcodeHasImmed_w)) &&
      (current_inst->flags & NACL_IFLAG(OperandSize_w))) {
    NaClFatalInst(
        "Size implied by OperandSize_w, use OpcodeHasImmed "
        "rather than OpcodeHasImmed_w");
  }
  if ((current_inst->flags & NACL_IFLAG(OpcodeHasImmed_v)) &&
      (current_inst->flags & NACL_IFLAG(OperandSize_v))) {
    NaClFatalInst(
        "Size implied by OperandSize_v, use OpcodeHasImmed "
        "rather than OpcodeHasImmed_v");
  }
  if ((current_inst->flags & NACL_IFLAG(ModRmModIs0x3)) &&
      (NACL_EMPTY_IFLAGS == (current_inst->flags &
                             (NACL_IFLAG(OpcodeUsesModRm) |
                              NACL_IFLAG(OpcodeInModRm))))) {
    NaClFatalInst(
        "Can't specify ModRmModIs0x3 unless Opcode has modrm byte");
  }
}

static void NaClDefBytes(uint8_t opcode) {
  uint8_t index;
  current_inst->prefix = current_opcode_prefix;

  /* Start by clearing all entries. */
  for (index = 0; index < NACL_MAX_OPCODE_BYTES; ++index) {
    current_inst->opcode[index] = 0x00;
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
      current_inst->opcode[0] = 0x0F;
      index = 1;
      break;
    case Prefix0F0F:
      current_inst->opcode[0] = 0x0F;
      current_inst->opcode[1] = 0x0F;
      index = 2;
      break;
    case Prefix0F38:
    case Prefix660F38:
    case PrefixF20F38:
      current_inst->opcode[0] = 0x0F;
      current_inst->opcode[1] = 0x38;
      index = 2;
      break;
    case Prefix0F3A:
    case Prefix660F3A:
      current_inst->opcode[0] = 0x0F;
      current_inst->opcode[1] = 0x3A;
      index = 2;
      break;
    case PrefixD8:
      current_inst->opcode[0] = 0xD8;
      index = 1;
      break;
    case PrefixD9:
      current_inst->opcode[0] = 0xD9;
      index = 1;
      break;
    case PrefixDA:
      current_inst->opcode[0] = 0xDA;
      index = 1;
      break;
    case PrefixDB:
      current_inst->opcode[0] = 0xDB;
      index = 1;
      break;
    case PrefixDC:
      current_inst->opcode[0] = 0xDC;
      index = 1;
      break;
    case PrefixDD:
      current_inst->opcode[0] = 0xDD;
      index = 1;
      break;
    case PrefixDE:
      current_inst->opcode[0] = 0xDE;
      index = 1;
      break;
    case PrefixDF:
      current_inst->opcode[0] = 0xDF;
      index = 1;
      break;
    default:
      NaClFatal("Unrecognized opcode prefix in NaClDefBytes");
      break;
  }

  /* Now add final byte. */
  current_inst->opcode[index] = opcode;
  current_inst->num_opcode_bytes = index + 1;
}

static void NaClPrintInstDescriptor(FILE* out,
                                    const NaClInstPrefix prefix,
                                    const int opcode,
                                    const int modrm_index) {
  if (NACL_NO_MODRM_OPCODE_INDEX == modrm_index) {
    fprintf(out, "%s 0x%02x: ",
            NaClInstPrefixName(prefix), opcode);
  } else {
    fprintf(out, "%s 0x%02x /%x: ",
            NaClInstPrefixName(prefix), opcode, modrm_index);
  }
}

void NaClDefPrefixInstMrmChoices_32_64(const NaClInstPrefix prefix,
                                       const uint8_t opcode,
                                       const NaClOpKind modrm_opcode,
                                       const int count_32,
                                       const int count_64) {
  int modrm_index = NACL_NO_MODRM_OPCODE_INDEX;
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
      fprintf(stderr, "%s:", NaClOpKindName(modrm_opcode));
      NaClFatal(
          "Illegal specification of modrm opcode when defining opcode choices");
  }
  if (NaClInstCount[opcode][prefix][modrm_index] != NACL_DEFAULT_CHOICE_COUNT) {
    NaClPrintInstDescriptor(stderr, prefix, opcode, modrm_index);
    NaClFatal("Redefining NaClOpcode choice count");
  }
  if (FLAGS_run_mode == X86_32) {
    NaClInstCount[opcode][prefix][modrm_index] = count_32;
  } else if (FLAGS_run_mode == X86_64) {
    NaClInstCount[opcode][prefix][modrm_index] = count_64;
  }
}

void NaClDefInstChoices(const uint8_t opcode, const int count) {
  NaClDefPrefixInstChoices(current_opcode_prefix, opcode, count);
}

void NaClDefInstMrmChoices(const uint8_t opcode,
                           const NaClOpKind modrm_opcode,
                           const int count) {
  NaClDefPrefixInstMrmChoices(current_opcode_prefix, opcode,
                              modrm_opcode, count);
}

void NaClDefPrefixInstChoices(const NaClInstPrefix prefix,
                              const uint8_t opcode,
                              const int count) {
  NaClDefPrefixInstChoices_32_64(prefix, opcode, count, count);
}

void NaClDefPrefixInstMrmChoices(const NaClInstPrefix prefix,
                                 const uint8_t opcode,
                                 const NaClOpKind modrm_opcode,
                                 const int count) {
  NaClDefPrefixInstMrmChoices_32_64(prefix, opcode, modrm_opcode,
                                         count, count);
}

void NaClDefInstChoices_32_64(const uint8_t opcode,
                              const int count_32,
                              const int count_64) {
  NaClDefPrefixInstChoices_32_64(current_opcode_prefix, opcode,
                                 count_32, count_64);
}

void NaClDefInstMrmChoices_32_64(const uint8_t opcode,
                                 const NaClOpKind modrm_opcode,
                                 const int count_32,
                                 const int count_64) {
  NaClDefPrefixInstMrmChoices_32_64(current_opcode_prefix, opcode,
                                    modrm_opcode, count_32, count_64);
}

void NaClDefPrefixInstChoices_32_64(const NaClInstPrefix prefix,
                                    const uint8_t opcode,
                                    const int count_32,
                                    const int count_64) {
  NaClDefPrefixInstMrmChoices_32_64(prefix, opcode, NACL_NO_MODRM_OPCODE,
                                    count_32, count_64);
}

/* Define the next opcode (instruction), initializing with
 * no operands.
 */
void NaClDefInst(
    const uint8_t opcode,
    const NaClInstType insttype,
    NaClIFlags flags,
    const NaClMnemonic name) {
  /* TODO(karl) If we can deduce a more specific prefix that
   * must be used, due to the flags associated with the opcode,
   * then put on the more specific prefix list. This will make
   * the defining of instructions easier, in that the caller doesn't
   * need to specify the prefix to use.
   */
  int i;

  /* Before starting, install an opcode sequence if applicable. */
  if (NULL != current_cand_inst_node) {
    current_inst_node = current_cand_inst_node;
  }
  current_cand_inst_node = NULL;

  /* Before starting, expand appropriate implicit flag assumnptions. */
  if (flags & NACL_IFLAG(OpcodeLtC0InModRm)) {
    flags |= NACL_IFLAG(OpcodeInModRm);
  }

  DEBUG(printf("Define %s %"NACL_PRIx8": %s(%d)",
               NaClInstPrefixName(current_opcode_prefix),
               opcode, NaClMnemonicName(name), name);
        NaClIFlagsPrint(stdout, flags));

  if (NaClMnemonicEnumSize <= name) {
    NaClFatal("Badly defined mnemonic name");
  }

  if (kNaClInstTypeRange <= insttype) {
    NaClFatal("Badly defined inst type");
  }

  /* Create opcode and initialize */
  current_inst_mrm = (NaClMrmInst*) malloc(sizeof(NaClMrmInst));
  if (NULL == current_inst_mrm) {
    NaClFatal("NaClDefInst: malloc failed");
  }
  DEBUG(printf("current = %p\n", (void*) current_inst_mrm));
  current_inst_mrm->next = NULL;
  current_inst = &(current_inst_mrm->inst);
  NaClDefBytes(opcode);
  current_inst->insttype = insttype;
  current_inst->flags = flags;
  current_inst->name = name;
  current_inst->next_rule = NULL;

  /* undefine all operands. */
  current_inst->num_operands = 0;
  for (i = 0; i < NACL_MAX_NUM_OPERANDS; ++i) {
    NaClDefOpInternal(Unknown_Operand, 0);
  }
  /* Now reset number of operands to zero. */
  current_inst->num_operands = 0;

  NaClApplySanityChecksToInst();

  if (name == InstUndefined || !NaClIFlagsMatchesRunMode(flags)) {
    return;
  }

  if (NULL == current_inst_node) {
    /* Install NaClOpcode. */
    DEBUG(printf("  standard install\n"));
    if (NULL == NaClInstTable[opcode][current_opcode_prefix]) {
      NaClInstTable[opcode][current_opcode_prefix] = current_inst;
    } else {
      NaClInst* next = NaClInstTable[opcode][current_opcode_prefix];
      while (NULL != next->next_rule) {
        next = next->next_rule;
      }
      next->next_rule = current_inst;
    }
    /* Install assuming no modrm opcode. Let NaClDefOp move if needed. */
    NaClInstallCurrentIntoOpcodeMrm(current_opcode_prefix, opcode,
                                NACL_NO_MODRM_OPCODE_INDEX);
  } else if (NULL == current_inst_node->matching_inst) {
    DEBUG(printf("  instruction sequence install\n"));
    current_inst_node->matching_inst = current_inst;
  } else {
    NaClFatalInst(
        "Can't define more than one opcode for the same opcode sequence");
  }
}

/* Simple (fast hack) routine to extract a byte value from a character string.
 */
static int NaClExtractByte(const char* chars, const char* opcode_seq) {
  char buffer[3];
  int i;
  for (i = 0; i < 2; ++i) {
    char ch = *(chars++);
    if ('\0' == ch) {
      fprintf(stderr,
              "Odd number of characters in opcode sequence: '%s'\n",
              opcode_seq);
      NaClFatal("Fix before continuing!");
    }
    buffer[i] = ch;
  }
  buffer[2] = '\0';
  return strtoul(buffer, NULL, 16);
}

static NaClInstNode* NaClNewInstNode() {
  int i;
  NaClInstNode* root = (NaClInstNode*) malloc(sizeof(NaClInstNode));
  root->matching_inst = NULL;
  for (i = 0; i < NACL_NUM_BYTE_VALUES; ++i) {
    root->succs[i] = NULL;
  }
  return root;
}

/* Simple (fast hack) recursive routine to automatically install
 * an opcode sequence into the rooted trie.
 */
static NaClInstNode* NaClInstallInstSeq(int index,
                                        const char* opcode_seq,
                                        const char* chars_left,
                                        NaClInstNode** root_ptr) {
  NaClInstNode* root = *root_ptr;
  if (index > NACL_MAX_BYTES_PER_X86_INSTRUCTION) {
    fprintf(stderr,
            "Too many bytes specified for opcode sequence: '%s'\n",
            opcode_seq);
    NaClFatal("Fix before continuing!\n");
  }
  if (NULL == root) {
    root = NaClNewInstNode();
    *root_ptr = root;
  }
  if (*chars_left) {
    int byte = NaClExtractByte(chars_left, opcode_seq);
    return NaClInstallInstSeq(
        index+1, opcode_seq, chars_left + 2, &(root->succs[byte]));
  } else {
    return root;
  }
}

void NaClDefInstSeq(const char* opcode_seq) {
  /* First check that opcode sequence not defined twice without a corresponding
   * call to NaClDefInst.
   */
  if (NULL != current_cand_inst_node) {
    fprintf(stderr,
            "Multiple definitions for opcode sequence: '%s'\n", opcode_seq);
    NaClFatal("Fix before continuing!");
  }
  /* Now install into lookup trie. */
  current_cand_inst_node =
      NaClInstallInstSeq(0, opcode_seq, opcode_seq, &inst_node_root);
}

void NaClAddIFlags(NaClIFlags more_flags) {
  DEBUG(printf("Adding instruction flags:");
        NaClIFlagsPrint(stdout, more_flags);
        printf("\n"));
  current_inst->flags |= more_flags;
  if (!NaClIFlagsMatchesRunMode(more_flags)) {
    NaClRemoveCurrentInstMrmFromInstTable();
    NaClRemoveCurrentInstMrmFromInstMrmTable();
  }
  NaClApplySanityChecksToInst();
}

void NaClDelaySanityChecks() {
  apply_sanity_checks = FALSE;
}

void NaClApplySanityChecks() {
  apply_sanity_checks = TRUE;
  if (NULL != current_inst) {
    int i;
    NaClApplySanityChecksToInst();
    for (i = 0; i < current_inst->num_operands; i++) {
      NaClApplySanityChecksToOp(i);
    }
  }
}

static void NaClInitInstTables() {
  int i;
  NaClInstPrefix prefix;
  int j;
  /* Before starting, verify that we have defined NaClOpcodeFlags
   * and NaClOpFlags big enough to hold the flags associated with it.
   */
  assert(NaClIFlagEnumSize <= sizeof(NaClIFlags) * 8);
  assert(NaClOpFlagEnumSize <= sizeof(NaClOpFlags) * 8);
  assert(NaClDisallowsFlagEnumSize <= sizeof(NaClDisallowsFlags) * 8);

  for (i = 0; i < NCDTABLESIZE; ++i) {
    for (prefix = NoPrefix; prefix < NaClInstPrefixEnumSize; ++prefix) {
      NaClInstTable[i][prefix] = NULL;
      for (j = 0; j <= NACL_NO_MODRM_OPCODE_INDEX; ++j) {
        NaClInstCount[i][prefix][j] = NACL_DEFAULT_CHOICE_COUNT;
      }
    }
    NaClPrefixTable[i] = "0";
  }
  NaClDefInstPrefix(NoPrefix);
  NaClDefInst(0x0, NACLi_ILLEGAL, 0, InstUndefined);
  undefined_inst = current_inst;
}

static void NaClDefPrefixBytes() {
  NaClEncodePrefixName(0x26, "kPrefixSEGES");
  NaClEncodePrefixName(0x2e, "kPrefixSEGCS");
  NaClEncodePrefixName(0x36, "kPrefixSEGSS");
  NaClEncodePrefixName(0x3e, "kPrefixSEGDS");
  NaClEncodePrefixName(0x64, "kPrefixSEGFS");
  NaClEncodePrefixName(0x65, "kPrefixSEGGS");
  NaClEncodePrefixName(0x66, "kPrefixDATA16");
  NaClEncodePrefixName(0x67, "kPrefixADDR16");
  NaClEncodePrefixName(0xf0, "kPrefixLOCK");
  NaClEncodePrefixName(0xf2, "kPrefixREPNE");
  NaClEncodePrefixName(0xf3, "kPrefixREP");

  if (FLAGS_run_mode == X86_64) {
    int i;
    for (i = 0; i < 16; ++i) {
      NaClEncodePrefixName(0x40+i, "kPrefixREX");
    }
  }
}

/* Define the given character sequence, associated with the given byte
 * opcode, as a nop.
 */
static void NaClDefNopSeq(const char* sequence, uint8_t opcode) {
  NaClDefInstSeq(sequence);
  NaClDefInst(opcode, NACLi_386, NACL_EMPTY_IFLAGS, InstNop);
}

static void NaClDefNops() {
  /* nop */
  NaClDefNopSeq("90", 0x90);
  NaClDefNopSeq("6690", 0x90);
  /* nop [%[re]ax] */
  NaClDefNopSeq("0f1f00", 0x1f);
  /* nop [%[re]ax+0] */
  NaClDefNopSeq("0f1f4000", 0x1f);
  /* nop [%[re]ax*1+0] */
  NaClDefNopSeq("0f1f440000", 0x1f);
  /* nop [%[re]ax+%[re]ax*1+0] */
  NaClDefNopSeq("660f1f440000", 0x1f);
  /* nop [%[re]ax+0] */
  NaClDefNopSeq("0f1f8000000000", 0x1f);
  /* nop [%[re]ax+%[re]ax*1+0] */
  NaClDefNopSeq("0f1f840000000000", 0x1f);
  /* nop [%[re]ax+%[re]ax+1+0] */
  NaClDefNopSeq("660f1f840000000000", 0x1f);
  /* nop %cs:[%re]ax+%[re]ax*1+0] */
  NaClDefNopSeq("662e0f1f840000000000", 0x1f);
  /* nop %cs:[%re]ax+%[re]ax*1+0] */
  NaClDefNopSeq("66662e0f1f840000000000", 0x1f);
  /* nop %cs:[%re]ax+%[re]ax*1+0] */
  NaClDefNopSeq("6666662e0f1f840000000000", 0x1f);
  /* nop %cs:[%re]ax+%[re]ax*1+0] */
  NaClDefNopSeq("666666662e0f1f840000000000", 0x1f);
  /* nop %cs:[%re]ax+%[re]ax*1+0] */
  NaClDefNopSeq("66666666662e0f1f840000000000", 0x1f);
  /* nop %cs:[%re]ax+%[re]ax*1+0] */
  NaClDefNopSeq("6666666666662e0f1f840000000000", 0x1f);
  /* UD2 */
  NaClDefNopSeq("0f0b", 0x0b);
}

/* Build the set of x64 opcode (instructions). */
static void NaClBuildInstTables() {
  NaClInitInstTables();

  NaClDefPrefixBytes();

  NaClDefOneByteInsts();

  NaClDef0FInsts();

  NaClDefSseInsts();

  NaClDefX87Insts();

  NaClDefNops();
}

/* Return the number of instruction rules pointed to by the parameter. */
static int NaClInstListLength(NaClInst* next) {
  int count = 0;
  while (NULL != next) {
    ++count;
    next = next->next_rule;
  }
  return count;
}

static int NaClInstMrmListLength(NaClMrmInst* next) {
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
static int NaClCountNumberInsts() {
  int i;
  NaClInstPrefix prefix;
  int count = 0;
  for (i = 0; i < NCDTABLESIZE; ++i) {
    for (prefix = NoPrefix; prefix < NaClInstPrefixEnumSize; ++prefix) {
      count += NaClInstListLength(NaClInstTable[i][prefix]);
    }
  }
  return count;
}

static void NaClFatalChoiceCount(const int expected,
                                 const int found,
                                 const NaClInstPrefix prefix,
                                 const int opcode,
                                 const int modrm_index,
                                 NaClMrmInst* insts) {
  NaClPrintInstDescriptor(stderr, prefix, opcode, modrm_index);
  fprintf(stderr, "Expected %d rules but found %d:\n", expected, found);
  while (NULL != insts) {
    NaClInstPrint(stderr, &(insts->inst));
    insts = insts->next;
  }
  NaClFatal("fix before continuing...\n");
}

/* Verify that the number of possible choies for each prefix:opcode matches
 * what was explicitly defined.
 */
static void NaClVerifyInstCounts() {
  int i, j;
  NaClInstPrefix prefix;
  for (i = 0; i < NCDTABLESIZE; ++i) {
    for (prefix = NoPrefix; prefix < NaClInstPrefixEnumSize; ++prefix) {
      for (j = 0; j < NACL_MODRM_OPCODE_SIZE; ++j) {
        NaClMrmInst* insts = NaClInstMrmTable[i][prefix][j];
        int found = NaClInstMrmListLength(insts);
        int expected = NaClInstCount[i][prefix][j];
        if (expected == NACL_DEFAULT_CHOICE_COUNT) {
          if (found > 1) {
            NaClFatalChoiceCount(1, found, prefix, i, j, insts);
          }
        } else if (expected != found) {
          NaClFatalChoiceCount(expected, found, prefix, i, j, insts);
        }
      }
    }
  }
}

/* Generate header information, based on the executable name in argv0,
 * and the file to be generated (defined by fname).
 */
static void NaClPrintHeader(FILE* f, const char* argv0, const char* fname) {
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
  fprintf(f, " * Compiled for %s.\n", NaClRunModeName(FLAGS_run_mode));
  fprintf(f, " *\n");
  fprintf(f, " * You must include ncopcode_desc.h before this file.\n");
  fprintf(f, " */\n\n");
}

/* Print out which bytes correspond to prefix bytes. */
static void NaClPrintPrefixTable(FILE* f) {
  int opc;
  fprintf(f, "static const uint32_t kNaClPrefixTable[NCDTABLESIZE] = {");
  for (opc = 0; opc < NCDTABLESIZE; opc++) {
    if (0 == opc % 16) {
      fprintf(f, "\n  /* 0x%02x-0x%02x */\n  ", opc, opc + 15);
    }
    fprintf(f, "%s, ", NaClPrefixTable[opc]);
  }
  fprintf(f, "\n};\n\n");
}

static int NaClCountInstSeqs(NaClInstNode* root) {
  if (root == NULL) {
    return 0;
  } else {
    int count = 0;
    int index;
    if (NULL != root->matching_inst) count++;
    for (index = 0; index < NACL_NUM_BYTE_VALUES; ++index) {
      count += NaClCountInstSeqs(root->succs[index]);
    }
    return count;
  }
}

static int NaClCountInstNodes(NaClInstNode* root) {
  if (NULL == root) {
    return 0;
  } else {
    int count = 1;
    int index;
    for (index = 0; index < NACL_NUM_BYTE_VALUES; ++index) {
      count += NaClCountInstNodes(root->succs[index]);
    }
    return count;
  }
}

static void NaClPrintInstTrieNode(NaClInstNode* root, int g_opcode_index,
                                  int root_index, FILE* f) {
  if (NULL == root) {
    return;
  } else {
    int i = 0;
    int next_index = root_index + 1;
    fprintf(f, "  /* %d */\n", root_index);
    fprintf(f, "  { ");
    if (NULL == root->matching_inst) {
      fprintf(f, "NULL");
    } else {
      fprintf(f, "g_Opcodes + %d", g_opcode_index);
    }
    fprintf(f, ", {\n");
    for (i = 0; i < NACL_NUM_BYTE_VALUES; ++i) {
      fprintf(f, "    /* %02x */ ", i);
      if (NULL == root->succs[i]) {
        fprintf(f, "NULL");
      } else {
        fprintf(f, "g_OpcodeSeq + %d", next_index);
        next_index += NaClCountInstNodes(root->succs[i]);
      }
      fprintf(f, ",\n");
    }
    fprintf(f, "    } },\n");
    next_index = root_index + 1;
    for (i = 0; i < NACL_NUM_BYTE_VALUES; ++i) {
      NaClPrintInstTrieNode(root->succs[i], g_opcode_index, next_index, f);
      g_opcode_index += NaClCountInstSeqs(root->succs[i]);
      next_index += NaClCountInstNodes(root->succs[i]);
    }
  }
}

/* Prints out the contents of the opcode sequence overrides into the
 * given file.
 */
static void NaClPrintInstSeqTrie(int g_opcode_index,
                                 NaClInstNode* root,
                                 FILE* f) {
  /* Make sure trie isn't empty, since empty arrays create warning messages. */
  int num_trie_nodes;
  if (root == NULL) root = NaClNewInstNode();
  num_trie_nodes = NaClCountInstNodes(root);
  fprintf(f, "static NaClInstNode g_OpcodeSeq[%d] = {\n", num_trie_nodes);
  NaClPrintInstTrieNode(root, g_opcode_index, 0, f);
  fprintf(f, "};\n");
}

static void NaClPrintInstsInInstTrie(int current_index,
                                     NaClInstNode* root,
                                     FILE* f) {
  int index;
  if (NULL == root) return;
  if (NULL != root->matching_inst) {
    NaClInstPrintTablegen(f, current_index, root->matching_inst, 0);
    current_index++;
  }
  for (index = 0; index < NACL_NUM_BYTE_VALUES; ++index) {
    NaClPrintInstsInInstTrie(current_index, root->succs[index], f);
    current_index += NaClCountInstSeqs(root->succs[index]);
  }
}

/* Print out the contents of the defined instructions into the given file. */
static void NaClPrintDecodeTables(FILE* f) {
  int i;
  NaClInstPrefix prefix;
  int count = 0;

  /* lookup_index holds the number of the opcode (instruction) that
   * begins the list of instructions for the corresponding opcode
   * in NaClOpcodeTable.
   */
  int lookup_index[NCDTABLESIZE][NaClInstPrefixEnumSize];

  /* Build table of all possible instructions. Note that we build
   * build the list of instructions by using a "g_NaClOpcodes" address.
   */
  int num_opcodes = NaClCountNumberInsts();
  int num_seq_opcodes = NaClCountInstSeqs(inst_node_root);

  fprintf(f,
          "static NaClInst g_Opcodes[%d] = {\n",
          num_opcodes + num_seq_opcodes);
  for (prefix = NoPrefix; prefix < NaClInstPrefixEnumSize; ++prefix) {
    for (i = 0; i < NCDTABLESIZE; ++i) {
      /* Build the list of instructions by knowing that the next
       * element in the list will always follow the current entry,
       * when added to the array.
       */
      NaClInst* next = NaClInstTable[i][prefix];
      if (NULL == next) {
        lookup_index[i][prefix] = -1;
      } else {
        int lookahead = count + 1;
        lookup_index[i][prefix] = count;
        while (NULL != next) {
          NaClInstPrintTablegen(f, count, next, lookahead);
          next = next->next_rule;
          ++lookahead;
          ++count;
        }
      }
    }
  }
  NaClPrintInstsInInstTrie(num_opcodes, inst_node_root, f);
  fprintf(f, "};\n\n");

  /* Print out the undefined opcode */
  fprintf(f, "static NaClInst g_Undefined_Opcode = \n");
  NaClInstPrintTableDriver(f, FALSE, FALSE, 0, undefined_inst, 0);

  /* Now print lookup table of rules. */
  fprintf(f,
          "static NaClInst* "
          "g_OpcodeTable[NaClInstPrefixEnumSize][NCDTABLESIZE] = {\n");
  for (prefix = NoPrefix; prefix < NaClInstPrefixEnumSize; ++prefix) {
    fprintf(f,"/* %s */\n", NaClInstPrefixName(prefix));
    fprintf(f, "{\n");
    for (i = 0; i < NCDTABLESIZE; ++i) {
      /* Take advantage of the fact that the lists were added to
       * the array of opcodes such that the next element in the list
       * will always follow the current entry.
       */
      NaClInst* next = NaClInstTable[i][prefix];
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

  NaClPrintPrefixTable(f);

  NaClPrintInstSeqTrie(num_opcodes, inst_node_root, f);
}

/* Open the given file using the given directives (how). */
static FILE* NaClMustOpen(const char* fname, const char* how) {
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
static int NaClGrokFlags(int argc, const char* argv[]) {
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
  int new_argc = NaClGrokFlags(argc, argv);
  if ((new_argc != 2) || (FLAGS_run_mode == NaClRunModeSize)) {
    fprintf(stderr,
            "ERROR: usage: ncdecode_tablegen <architecture_flag> "
            "file\n");
    return -1;
  }

  NaClBuildInstTables();

  NaClVerifyInstCounts();

  f = NaClMustOpen(argv[1], "w");
  NaClPrintHeader(f, argv[0], argv[1]);
  NaClPrintDecodeTables(f);
  fclose(f);

  return 0;
}
