/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Set of predefined instruction forms (via procedure calls), providing
 * a more concise way of specifying opcodes.
 */

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

#include "native_client/src/trusted/validator/x86/decoder/generator/ncdecode_forms.h"

#include <assert.h>
#include <ctype.h>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/include/portability_string.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator/x86/decoder/generator/defsize64.h"
#include "native_client/src/trusted/validator/x86/decoder/generator/lock_insts.h"
#include "native_client/src/trusted/validator/x86/decoder/generator/long_mode.h"
#include "native_client/src/trusted/validator/x86/decoder/generator/nacl_illegal.h"
#include "native_client/src/trusted/validator/x86/decoder/generator/nc_def_jumps.h"
#include "native_client/src/trusted/validator/x86/decoder/generator/nc_rep_prefix.h"
#include "native_client/src/trusted/validator/x86/decoder/generator/ncdecode_st.h"
#include "native_client/src/trusted/validator/x86/decoder/generator/ncdecode_tablegen.h"
#include "native_client/src/trusted/validator/x86/decoder/generator/zero_extends.h"

/* To turn on debugging of instruction decoding, change value of
 * DEBUGGING to 1.
 */
#define DEBUGGING 0

#include "native_client/src/shared/utils/debugging.h"

/* Returns the name for the given enumerated value. */
static const char* NaClInstCatName(NaClInstCat cat) {
  int i;
  static struct {
    NaClInstCat cat;
    const char* name;
  } cat_desc[] = {
    { UnarySet , "UnarySet" },
    { UnaryUpdate , "UnaryUpdate" },
    { Move , "Move" },
    { O2Move, "O2Move" },
    { Binary , "Binary" },
    { O2Binary , "O2Binary" },
    { Nary , "Nary" },
    { O1Nary, "O1Nary"},
    { O3Nary, "O3Nary"},
    { Compare , "Compare"},
    { Exchange, "Exchange" },
    { Push, "Push" },
    { Pop, "Pop" },
    { Call, "Call"},
    { SysCall, "SysCall"},
    { SysJump, "SysJump"},
    { Return, "Return"},
    { SysRet, "SysRet"},
    { Jump, "Jump" },
    { Uses, "Uses" },
    { Sets, "Sets" },
    { Lea, "Lea" },
    { Cpuid, "Cpuid" },
    { Other, "Other" },
  };
  for (i = 0; i < NACL_ARRAY_SIZE(cat_desc); ++i) {
    if (cat == cat_desc[i].cat) return cat_desc[i].name;
  }
  /* Shouldn't be reached!! */
  NaClFatal("Unrecognized category");
  /* NOT REACHED */
  return "Unspecified";
}

/* Returns the OpSet and OpUse operand flags for the destination
 * argument of the instruction, given the category of instruction. Argument
 * num_ops is the number of operands the instruction has.
 */
static NaClOpFlags NaClGetArg1Flags(NaClInstCat icat, int num_ops) {
  DEBUG(NaClLog(LOG_INFO, "NaClGetArg1Flags(%s)\n", NaClInstCatName(icat)));
  switch (icat) {
    case Move:
    case O2Move:
    case O1Nary:
    case O3Nary:
    case Lea:
    case UnarySet:
    case Sets:
    case O2Binary:
    case Jump:
    case Return:
    case SysRet:
    case SysJump:
    case Cpuid:
      return NACL_OPFLAG(OpSet);
    case Nary:
    case Exchange:
    case Call:
    case SysCall:
    case UnaryUpdate:
    case Push:
    case Pop:
      return NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse);
    case Binary:
      if (3 == num_ops) {
        return NACL_OPFLAG(OpSet);
      } else {
        return NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse);
      }
    case Compare:
    case Uses:
      return NACL_OPFLAG(OpUse);
    case Other:
      return NACL_EMPTY_OPFLAGS;
      break;
    default:
      NaClFatal("NaClGetArg1Flags: unrecognized category");
      /* NOT REACHED */
      return NACL_EMPTY_OPFLAGS;
  }
}

/* Returns the OpSet, OpUse, and OpDest operand flags for the source argument(s)
 * of an instruction, given the category of instruction.
 *
 * Argument visible_index is the (1-based) index of the operand for which flags
 * are being defined. A value of zero implies the argument is not visible.
 *
 * Argument op_index is the actual index of the operand in the
 * instruction being modeled.
 */
static NaClOpFlags NaClGetArg2PlusFlags(NaClInstCat icat,
                                        int visible_index,
                                        int op_index) {
  DEBUG(NaClLog(LOG_INFO, "NaClGetArgsPlusFlags(%s, %d)\n",
                NaClInstCatName(icat), visible_index));
  switch (icat) {
    case UnarySet:
    case UnaryUpdate:
      NaClLog(LOG_INFO, "icat = %s\n", NaClInstCatName(icat));
      NaClFatal("Illegal to use unary categorization for binary operation");
      /* NOT REACHED */
      return NACL_EMPTY_OPFLAGS;
    case Call:
    case Return:
    case O2Binary:
      if (2 == op_index) {
        return NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse);
      } else {
        return NACL_OPFLAG(OpUse);
      }
    case O3Nary:
      if (op_index <= 3) {
        return NACL_OPFLAG(OpSet);
      } else {
        return NACL_OPFLAG(OpUse);
      }
    case Move:
    case Binary:
    case Push:
    case Jump:
    case Uses:
    case SysRet:
    case Compare:
    case Nary:
    case O1Nary:
      return NACL_OPFLAG(OpUse);
    case O2Move:
      if ((2 == op_index)) {
        return NACL_OPFLAG(OpSet);
      } else {
        return NACL_OPFLAG(OpUse);
      }
    case Exchange:
      if (op_index == 2) {
        /* The second argument is always both a set and a use. */
        if (visible_index == 2) {
          return NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse);
        } else {
          return NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse);
        }
      } else if (op_index == 3) {
        /* If it has a 3rd argument, it is like a cmpxchg. */
        if (visible_index == 1) {
          return NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse);
        } else {
          return NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse);
        }
      } else {
        return NACL_OPFLAG(OpUse);
      }
    case Pop:
    case SysCall:
    case Sets:
      return NACL_OPFLAG(OpSet);
    case SysJump:
      if (op_index <= 4) {
        return NACL_OPFLAG(OpSet);
      } else {
        return NACL_OPFLAG(OpUse);
      }
    case Lea:
    case Other:
      return NACL_EMPTY_OPFLAGS;
    case Cpuid:
      if (op_index <= 2) {
        return NACL_OPFLAG(OpSet);
      } else {
        return NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse);
      }
    default:
      NaClFatal("NaClGetArg2PlusFlags: unrecognized category");
      /* NOT REACHED */
      return NACL_EMPTY_OPFLAGS;
  }
}

/* Returns the OpSet and OpUse operand flags for the operand
 * with the given operand_index (1-based) argument for the instruction,
 *
 * Argument icat is the instruction category of the instruction being
 * modeled.
 *
 * Argument visible_index is the (1-based) index of the operand for which flags
 * are being defined. A value of zero implies the argument is not visible.
 *
 * Argument op_index is the actual index of the operand in the
 * instruction being modeled.
 *
 * Argument num_ops is the number of operands the instruction has (base 1).
 */
static NaClOpFlags NaClGetIcatFlags(NaClInstCat icat,
                                    int operand_index,
                                    int visible_count,
                                    int num_ops) {
  NaClOpFlags flags = NACL_EMPTY_OPFLAGS;
  DEBUG(NaClLog(LOG_INFO, "NaClGetIcatFlags(%s, %d, %d)\n",
                NaClInstCatName(icat), operand_index, visible_count));
  if (operand_index == 1) {
    flags = NaClGetArg1Flags(icat, num_ops);
  } else {
    flags = NaClGetArg2PlusFlags(icat, visible_count, operand_index);
  }
  return flags;
}

/* Add miscellaneous flags defined elsewhere. */
static void NaClAddMiscellaneousFlags(void) {
  DEBUG(NaClLog(LOG_INFO, "-> Adding Miscellaneous Flags\n"));
  NaClAddZeroExtend32FlagIfApplicable();
  NaClLockableFlagIfApplicable();
  NaClAddSizeDefaultIs64();
  NaClAddLongModeIfApplicable();
  NaClAddNaClIllegalIfApplicable();
  NaClAddRepPrefixFlagsIfApplicable();
  NaClAddJumpFlagsIfApplicable();
  DEBUG(NaClLog(LOG_INFO, "<- Adding Miscellaneous Flags\n"));
}

/* Adds OpSet/OpUse flags to operands to the current instruction,
 * based on the given instruction categorization.
 */
static void NaClSetInstCat(NaClInstCat icat) {
  int operand_index = 0;  /* note: this is one-based. */
  int visible_count = 0;
  int i;
  int num_ops;
  NaClModeledInst* inst = NaClGetDefInst();
  num_ops = inst->num_operands;
  for (i = 0; i < num_ops; ++i) {
    Bool is_visible = FALSE;
    ++operand_index;
    if (NACL_EMPTY_OPFLAGS ==
        (inst->operands[i].flags & NACL_OPFLAG(OpImplicit))) {
      is_visible = TRUE;
      ++visible_count;
    }
    NaClAddOpFlags(i, NaClGetIcatFlags(
        icat, operand_index, (is_visible ? visible_count : 0), num_ops));
  }
  /* Do special fixup for binary case with 3 arguments. In such
   * cases, the first argument is only a set, rather than a set/use.
   */
  if ((Binary == icat) && (3 == num_ops)) {
    NaClRemoveOpFlags(0, NACL_OPFLAG(OpUse));
  }
  NaClAddMiscellaneousFlags();
}

/* Returns true if opcode sequence value is a SL (slash)
 * value.
 */
static Bool IsSLValue(int16_t val) {
  return val < 0;
}

/* Returns the opcode denoted by a SL (slash) value. */
static uint8_t SLOpcode(int16_t val) {
  return (uint8_t) ((0 - val) - 1);
}

/* Returns true if any prefix byte values, which need
 * to be considered part of the opcode sequence, is defined
 * by the given instruction. If prefix byte(s) are matched,
 * prefix_count is incremented by the number of bytes matched.
 *
 * Parameters are:
 *   prefix_count - Variable to update on number of matched prefix
 *      bytes.
 *   inst - The instruction we are matching the opcode sequence against.
 *   name_and_opcode_seq - The opcode sequence descriptor we are trying
 *      to match.
 */
static Bool NaClNameOpcodeSeqMatchesPrefixByte(
    int* prefix_count,
    NaClModeledInst* inst,
    const NaClNameOpcodeSeq* name_and_opcode_seq,
    int index) {
  int16_t prefix;
  switch (inst->prefix) {
    case PrefixF20F:
    case PrefixF20F38:
      prefix = PR(0xF2);
      break;
    case PrefixF30F:
      prefix = PR(0xF3);
      break;
    case Prefix660F:
    case Prefix660F38:
    case Prefix660F3A:
      prefix = PR(0x66);
      break;
    default:
      return TRUE;
  }
  if (prefix == name_and_opcode_seq[index].opcode_seq[0]) {
    ++(*prefix_count);
    return TRUE;
  } else {
    return FALSE;
  }
}

Bool NaClInInstructionSet(const NaClMnemonic* names,
                          size_t names_size,
                          const NaClNameOpcodeSeq* name_and_opcode_seq,
                          size_t name_and_opcode_seq_size) {
  size_t i;
  NaClModeledInst* inst = NaClGetDefInst();

  /* First handle cases where all instances of an instruction
   * mnemonic is in the set.
   */
  for (i = 0; i < names_size; ++i) {
    if (inst->name == names[i]) {
      return TRUE;
    }
  }

  /* How handle cases where only a specific opcode sequence of
   * an instruction mnemonic applies.
   */
  for (i = 0; i < name_and_opcode_seq_size; ++i) {
    if (inst->name == name_and_opcode_seq[i].name) {
      int j;
      int prefix_count = 0;
      Bool is_good = TRUE;
      Bool matched_slash = FALSE;
      /* First compare opcode bytes. */
      if (NaClNameOpcodeSeqMatchesPrefixByte(
              &prefix_count, inst, name_and_opcode_seq, (int) i)) {
        for (j = 0; j < inst->num_opcode_bytes; ++j) {
          if (inst->opcode[j] !=
              name_and_opcode_seq[i].opcode_seq[prefix_count + j]) {
            is_good = FALSE;
            break;
          }
        }
      } else {
        is_good = FALSE;
      }
      if (is_good) {
        /* Now compare any values that must by in modrm. */
        for (j = prefix_count + inst->num_opcode_bytes;
             j < NACL_OPCODE_SEQ_SIZE; ++j) {
          int16_t val = name_and_opcode_seq[i].opcode_seq[j];
          if (END_OPCODE_SEQ == val) {
            /* At end of descriptor. See if instruction defines an opcode
             * in the ModRm byte. If so, make sure that we matched the slash.
             */
            if (inst->flags & NACL_IFLAG(OpcodeInModRm)) {
              return matched_slash;
            } else {
              /* At end of descriptor, matched! */
              return TRUE;
            }
          }
          if (IsSLValue(val)) {
            if ((inst->flags & NACL_IFLAG(OpcodeInModRm)) &&
                (SLOpcode(val) == NaClGetOpcodeInModRm(inst->opcode_ext))) {
              /* good, continue search. */
              matched_slash = TRUE;
            } else {
              is_good = FALSE;
              break;
            }
          }
        }
        if (is_good) {
          return TRUE;
        }
      }
    }
  }
  /* If reached, couldn't match, so not in instruction set. */
  return FALSE;
}

static void NaClOperandForm_Ap(void) {
  NaClDefOp(A_Operand, NACL_OPFLAG(OperandFar) | NACL_OPFLAG(OperandRelative));
  NaClAddIFlags(NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OpcodeHasImmed_p));
}

static void NaClOperandForm_CdSlq(void) {
  /* Note: For Cd/q, we don't worry about the size of the
   * control registers for NaCl. Hence, for now, we ignore the
   * size constraint
   */
  NaClDefOp(C_Operand, NACL_EMPTY_OPFLAGS);
}

static void NaClOperandForm_DdSlq(void) {
  /* Note: For Dd/q, we don't worry about the size of the
   * debug register for NaCl. Hence, for now, we ignore the
   * size constraint.
   */
  NaClDefOp(D_Operand, NACL_EMPTY_OPFLAGS);
}

static void NaClOperandForm_Eb(void) {
  /* For Eb, we must worry about earlier arguments, which in some cases,
   * have already specified valid effective operand sizes. If so, we must make
   * the size of the this operand be byte specific. If size hasn't been
   * specified yet, we go ahead and assume the effective operand size of the
   * instructruction as being a byte.
   */
  NaClModeledInst* inst = NaClGetDefInst();
  if (inst->flags & (NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
                     NACL_IFLAG(OperandSize_o))) {
    NaClDefOp(Eb_Operand, NACL_EMPTY_OPFLAGS);
  } else {
    NaClDefOp(E_Operand, NACL_EMPTY_OPFLAGS);
    NaClAddIFlags(NACL_IFLAG(OperandSize_b));
  }
}

static void NaClOperandForm_Ed(void) {
  NaClDefOp(Ev_Operand, NACL_EMPTY_OPFLAGS);
}

static void NaClOperandForm_EdSlq(void) {
  /* Note: For Ed/q we assume that only sizes d (32) and q (64) are possible.
   * Hence, we don't allow a data 66 prefix effect the size.
   */
  NaClDefOp(E_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) |
                NACL_IFLAG(SizeIgnoresData16));
}

static void NaClOperandForm_EdSlqSld(void) {
  /* Ed/q/d is used for Ed/q when operand size is 32 bits (vs 64 bits).
   * Note: For Ed/q we assume that only sizes d (32) and q (64) are possible.
   * Hence, we don't allow a data 66 prefix effect the size.
   */
  NaClDefOp(E_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_v) | NACL_IFLAG(SizeIgnoresData16));
}

static void NaClOperandForm_EdSlqSlq(void) {
  /* Ed/q/q is used for Ed/q when operand size is 64 bits (vs 32 bits).
   * Note: For Ed/q we assume that only sizes d (32) and q (64) are possible.
   * Hence, we don't allow a data 66 prefix effect the size.
   */
  NaClDefOp(E_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_o) | NACL_IFLAG(SizeIgnoresData16));
}

static void NaClOperandForm_Ev(void) {
  NaClDefOp(E_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
                NACL_IFLAG(OperandSize_o));
}

static void NaClOperandForm_Ew(void) {
  NaClDefOp(Ew_Operand, NACL_EMPTY_OPFLAGS);
}

static void NaClOperandForm_Fvw(void) {
  NaClDefOp(RegRFLAGS, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w));
}

static void NaClOperandForm_Fvd(void) {
  NaClDefOp(RegRFLAGS, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_v));
}

static void NaClOperandForm_Fvq(void) {
  NaClDefOp(RegRFLAGS, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_o));
}

static void NaClOperandForm_Gb(void) {
  NaClDefOp(G_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_b));
}

static void NaClOperandForm_Gd(void) {
  NaClDefOp(Gv_Operand, NACL_EMPTY_OPFLAGS);
}

static void NaClOperandForm_GdQ(void) {
  /* Note: For Gd/q we assume that only sizes d (32) and q (64) are possible.
   * Hence, we don't allow a data 66 prefix effect the size.
   */
  NaClDefOp(G_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) |
                NACL_IFLAG(SizeIgnoresData16));
}

static void NaClOperandForm_Gq(void) {
  NaClDefOp(Go_Operand, NACL_EMPTY_OPFLAGS);
}

static void NaClOperandForm_Gv(void) {
  NaClDefOp(G_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
                NACL_IFLAG(OperandSize_o));
}

static void NaClOperandForm_Gw(void) {
  NaClDefOp(Gw_Operand, NACL_EMPTY_OPFLAGS);
}

static void NaClOperandForm_Ib(void) {
  int i;
  NaClModeledInst* inst = NaClGetDefInst();
  /* First look to see if 1st or 2nd instance of an immediate value,
   * since different opcode flags that must be used are different.
   */
  for (i = 0; i < inst->num_operands; ++i) {
    if (I_Operand == inst->operands[i].kind) {
      /* Second instance!. */
      NaClDefOp(I2_Operand, NACL_EMPTY_OPFLAGS);
      NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed2_b));
      return;
    }
  }
  /* First instance of Ib. */
  NaClDefOp(I_Operand, NACL_EMPTY_OPFLAGS);
  if (inst->flags & NACL_IFLAG(OperandSize_b)) {
    NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed));
  } else {
    NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  }
}

static void NaClOperandForm_I2b(void) {
  NaClDefOp(I2_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed2_b));
}

static void NaClOperandForm_Iv(void) {
  NaClDefOp(I_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_w) |
                NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o));
}

static void NaClOperandForm_Iw(void) {
  NaClDefOp(I_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_w));
}

static void NaClOperandForm_Iz(void) {
  NaClDefOp(I_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_z));
}

static void NaClOperandForm_Jb(void) {
  NaClDefOp(J_Operand,
            NACL_OPFLAG(OperandRelative) | NACL_OPFLAG(OperandNear));
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed) | NACL_IFLAG(OperandSize_b));
}

/* Installs Jz, except for size constraints. Assumes immediate value
 * follows z (size) conventions, unless the argument implies to define
 * immediate sizes based on v.
 */
static void DefOperandJzBaseUseImmedV(Bool use_immed_v) {
  NaClDefOp(J_Operand,
            NACL_OPFLAG(OperandRelative) | NACL_OPFLAG(OperandNear));
  if (use_immed_v) {
    NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_v));
  } else {
    NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_z));
  }
}

/* Installs Jz where immediate value is based on z size. */
static void DefOperandJzBase(void) {
  /* Note: when in 32-bit mode, the relative offset can be a 16 or 32 bit
   * immediate offset, depending on the operand size. When in 64-bit mode,
   * the relative offset is ALWAYS a 32-bit immediate value (see page
   * 76 for CALL of AMD manual (document 24594-Rev.3.14-September 2007,
   * "AMD64 Architecture Programmer's manual Volume 3: General-Purpose
   * and System Instructions").
   */
  DefOperandJzBaseUseImmedV(X86_64 == NACL_FLAGS_run_mode);
}

static void NaClOperandForm_Jz(void) {
  DefOperandJzBase();
  NaClAddIFlags(NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
                NACL_IFLAG(OperandSize_o));
}

static void NaClOperandForm_Jzd(void) {
  DefOperandJzBaseUseImmedV(TRUE);
  NaClAddIFlags(NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o));
}

static void NaClOperandForm_Jzw(void) {
  DefOperandJzBase();
  NaClAddIFlags(NACL_IFLAG(OperandSize_w));
  /* Note: Special case 64-bit with 66 prefix, which is not supported on
   * some Intel Chips (See Call/Jcc instructions in Intel document
   * 253666-030US - March 2009, "Intel 654 and IA-32 Architectures
   * Software Developer's Manual, Volume2A").
   */
  if (X86_64 == NACL_FLAGS_run_mode) {
    NaClAddIFlags(NACL_IFLAG(NaClIllegal));
  }
}

static void NaClOperandForm_M(void) {
  NaClDefOp(M_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OpcodeAllowsData16));
}

static void NaClOperandForm_Ma(void) {
  NaClDefOp(M_Operand, NACL_EMPTY_OPFLAGS);
  NaClDefOp(M_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v));
}

static void NaClOperandForm_Mb(void) {
  NaClDefOp(Mb_Operand, NACL_EMPTY_OPFLAGS);
}

static void NaClOperandForm_Md(void) {
  NaClDefOp(Mv_Operand, NACL_EMPTY_OPFLAGS);
}

static void NaClOperandForm_Mdq(void) {
  NaClDefOp(Mdq_Operand, NACL_EMPTY_OPFLAGS);
}

static void NaClOperandForm_Mf(void) {
  NaClDefOp(M_Operand, NACL_EMPTY_OPFLAGS);
}

static void NaClOperandForm_MdSlq(void) {
  /* Note: For Ed/q we assume that only sizes d (32) and q (64) are possible.
   * Hence, we don't allow a data 66 prefix effect the size.
   */
  NaClDefOp(M_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) |
                NACL_IFLAG(SizeIgnoresData16));
}

static void NaClOperandForm_Mp(void) {
  /* TODO(karl) fix measurement size. */
  NaClDefOp(M_Operand, NACL_OPFLAG(OperandFar));
}

static void NaClOperandForm_Ms(void) {
  /* TODO(karl): fix size of data accessed in memory. */
  NaClDefOp(M_Operand, NACL_EMPTY_OPFLAGS);
}

static void NaClOperandForm_Mq(void) {
  NaClDefOp(Mo_Operand, NACL_EMPTY_OPFLAGS);
}

static void NaClOperandForm_Mv(void) {
  NaClDefOp(M_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
                NACL_IFLAG(OperandSize_o));
}

static void NaClOperandForm_Mw(void) {
  NaClDefOp(Mw_Operand, NACL_EMPTY_OPFLAGS);
}

static void NaClOperandForm_MwSlRv(void) {
  /* TODO(karl) Verify that Mw/Rv as same as Ev? */
  NaClOperandForm_Ev();
}

static void NaClOperandForm_Ob(void) {
  NaClDefOp(O_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_b) | NACL_IFLAG(OpcodeHasImmed_Addr));
}

static void NaClOperandForm_Ov(void) {
  NaClDefOp(O_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_Addr) | NACL_IFLAG(OperandSize_w) |
                NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o));
}


static void NaClOperandForm_One(void) {
  NaClDefOp(Const_1, NACL_EMPTY_OPFLAGS);
}

static void NaClOperandForm_PdSlq(void) {
  /* Note: For Pd/q we assume that only sizes d (32) and q (64) are possible.
   * Hence, we don't allow a data 66 prefix effect the size.
   */
  NaClDefOp(Mmx_G_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) |
                NACL_IFLAG(SizeIgnoresData16));
}

static void NaClOperandForm_PdSlqSld(void) {
  /* Pd/q/d is used for Pd/q when operand size is 32 bits (vs 64 bits).
   * Note: For Pd/q we assume that only sizes d (32) and q (64) are possible.
   * Hence, we don't allow a data 66 prefix effect the size.
   */
  NaClDefOp(Mmx_G_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_v) | NACL_IFLAG(SizeIgnoresData16));
}

static void NaClOperandForm_PdSlqSlq(void) {
  /* Pd/q/q is used for Pd/q when operand size is 64 bits (vs 32 bits).
   * Note: For Pd/q we assume that only sizes d (32) and q (64) are possible.
   * Hence, we don't allow a data 66 prefix effect the size.
   */
  NaClDefOp(Mmx_G_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_o) | NACL_IFLAG(SizeIgnoresData16));
}

static void NaClOperandForm_Pq(void) {
  /* TODO(karl) Add q size restriction. */
  NaClDefOp(Mmx_G_Operand, NACL_EMPTY_OPFLAGS);
}

static void NaClOperandForm_PRq(void) {
  /* Note: We ignore sizes for Mmx operands, since they
   * aren't used to compute memory addresses in NaCl.
   */
  NaClDefOp(Mmx_N_Operand, NACL_EMPTY_OPFLAGS);
}

static void NaClOperandForm_Qd(void) {
  /* TODO(karl) add d size restriction. */
  NaClDefOp(Mmx_E_Operand, NACL_EMPTY_OPFLAGS);
}

static void NaClOperandForm_Qq(void) {
  /* TODO(karl) add q size restriction. */
  NaClDefOp(Mmx_E_Operand, NACL_EMPTY_OPFLAGS);
}

static void NaClOperandForm_Rd(void) {
  NaClDefOp(Ev_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(ModRmModIs0x3));
}

static void NaClOperandForm_Rq(void) {
  NaClDefOp(Eo_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(ModRmModIs0x3));
}

static void NaClOperandForm_RdSlMb(void) {
  /* Note: For Rd/Mb, we ignore the size on memory,
   * since we don't need to specify sizes on memory.
   */
  NaClDefOp(Ev_Operand, NACL_EMPTY_OPFLAGS);
}

static void NaClOperandForm_RdSlMw(void) {
  /* Note: For Rd/Mw, we ignore the size on memory,
   * since we don't need to specify sizes on memory for NaCl.
   */
  NaClDefOp(Ev_Operand, NACL_EMPTY_OPFLAGS);
}

static void NaClOperandForm_RdSlq(void) {
  /* Note: It appears that Rd/q opcodes are connected to
   * the architecture size, and only have one choice based
   * on that size.
   */
  if (X86_32 == NACL_FLAGS_run_mode) {
    NaClOperandForm_Rd();
  } else {
    NaClOperandForm_Rq();
  }
}

static void NaClOperandForm_RdSlqSlMb(void) {
  /* Note: For Rd/q/Mb, we ignore the size on memory,
   * since we don't need to specify sizes on memory for NaCl.
   */
  NaClOperandForm_EdSlq();
}

static void NaClOperandForm_RdSlqSlMw(void) {
  /* Note: For Rd/q/Mw, we ignore the size on memory,
   * since we don't need to specify sizes on memory for NaCl.
   */
  NaClOperandForm_EdSlq();
}

static void NaClOperandForm_rAXv(void) {
  NaClDefOp(RegREAX, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
                NACL_IFLAG(OperandSize_o));
}

static void NaClOperandForm_rAXva(void) {
  NaClDefOp(RegREAXa, NACL_EMPTY_OPFLAGS);
}

static void NaClOperandForm_rAXvd(void) {
  NaClDefOp(RegEAX, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_v));
}

static void NaClOperandForm_rAXvq(void) {
  NaClDefOp(RegRAX, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_o));
}

static void NaClOperandForm_rAXvw(void) {
  NaClDefOp(RegAX, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w));
}

static void NaClOperandForm_rBXv(void) {
  NaClDefOp(RegREBX, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
                NACL_IFLAG(OperandSize_o));
}

static void NaClOperandForm_rCXv(void) {
  NaClDefOp(RegRECX, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
                NACL_IFLAG(OperandSize_o));
}

static void NaClOperandForm_rDXv(void) {
  NaClDefOp(RegREDX, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
                NACL_IFLAG(OperandSize_o));
}

static void NaClOperandForm_rSPv(void) {
  NaClDefOp(RegRESP, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
                NACL_IFLAG(OperandSize_o));
}

static void NaClOperandForm_rBPv(void) {
  NaClDefOp(RegREBP, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
                NACL_IFLAG(OperandSize_o));
}

static void NaClOperandForm_rSIv(void) {
  NaClDefOp(RegRESI, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
                NACL_IFLAG(OperandSize_o));
}

static void NaClOperandForm_rDIv(void) {
  NaClDefOp(RegREDI, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
                NACL_IFLAG(OperandSize_o));
}

/* Note: The interpretation for r8b is documented in ncdecode_forms.h. That is,
 *   r8 - The 8 registers AL, CL, DL, BL, AH, CH, DH, and BH if no REX prefix.
 *        with A REX prefix, use the registers AL, CL, DL, BL, SPL, BPL, SIL,
 *        DIL, and the optional registers r8-r15 if REX.b is set. Register
 *        choice is based on the register value embedded in the opcode.
 */
static void NaClOperandForm_r8b(void) {
  NaClDefOp(G_OpcodeBase, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_b));
}

/* Note: The interpretation for r8v is documented in ncdecode_forms.h. That is,
 *   r8 - The 8 registers rAX, rCX, rDX, rBX, rSP, rBP, rSI, rDI, and the
 *        optional registers r8-r15 if REX.b is set, based on the register value
 *        embedded in the opcode.
 */
static void NaClOperandForm_r8v(void) {
  NaClDefOp(G_OpcodeBase, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
                NACL_IFLAG(OperandSize_o));
}

/* Note: The interpretation for r8vd is documented in ncdecode_forms.h. That is,
 *   r8 - The 8 registers rAX, rCX, rDX, rBX, rSP, rBP, rSI, rDI, and the
 *        optional registers r8-r15 if REX.b is set, based on the register value
 *        embedded in the opcode
 *   vd - A doubleword only when the effective operand size matches.
 */
static void NaClOperandForm_r8vd(void) {
  NaClDefOp(G_OpcodeBase, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_v));
}

/* Note: The interpretation for r8vq is documented in ncdecode_forms.h. That is,
 *   r8 - The 8 registers rAX, rCX, rDX, rBX, rSP, rBP, rSI, rDI, and the
 *        optional registers r8-r15 if REX.b is set, based on the register value
 *        embedded in the opcode.
 *   vq - A quadword only when the effective operand size matches.
 */
static void NaClOperandForm_r8vq(void) {
  NaClDefOp(G_OpcodeBase, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_o));
}

static void NaClOperandForm_SGz(void) {
  NaClDefOp(Seg_G_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
                NACL_IFLAG(OperandSize_o));
}

static void NaClOperandForm_Sw(void) {
  NaClDefOp(S_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(ModRmRegSOperand));
}

static void NaClOperandForm_Udq(void) {
  NaClDefOp(Xmm_E_Operand, NACL_EMPTY_OPFLAGS);
}

static void NaClOperandForm_UdqMd(void) {
  /* TODO: how to define size, based on register (Udq) or
   * memory (Md).
   */
  NaClDefOp(Xmm_E_Operand, NACL_EMPTY_OPFLAGS);
}

static void NaClOperandForm_UdqMq(void) {
  /* TODO: how to define size, based on register (Udq) or
   * memory (Mq).
   */
  NaClDefOp(Xmm_E_Operand, NACL_EMPTY_OPFLAGS);
}

static void NaClOperandForm_UdqMw(void) {
  /* TODO: how to define size, based on register (Udq) or
   * memory (Mw).
   */
  NaClDefOp(Xmm_E_Operand, NACL_EMPTY_OPFLAGS);
}

static void NaClOperandForm_Vdq(void) {
  /* TODO(karl) Add dq size restriction. */
  NaClDefOp(Xmm_G_Operand, NACL_EMPTY_OPFLAGS);
}

static void NaClOperandForm_VdSlq(void) {
  /* Note: For Vd/q we assume that only sizes d (32) and q (64) are possible.
   * Hence, we don't allow a data 66 prefix effect the size.
   */
  NaClDefOp(Xmm_G_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o) |
                NACL_IFLAG(SizeIgnoresData16));
}

static void NaClOperandForm_VdSlqSld(void) {
  /* Vd/q/d is used for Vd/q when operand size is 32 bits (vs 64 bits).
   * Note: For Vd/q we assume that only sizes d (32) and q (64) are possible.
   * Hence, we don't allow a data 66 prefix effect the size.
   */
  NaClDefOp(Xmm_G_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_v) | NACL_IFLAG(SizeIgnoresData16));
}

static void NaClOperandForm_VdSlqSlq(void) {
  /* Vd/q/q is used for Vd/q when operand size is 64 bits (vs 32 bits).
   * Note: For Vd/q we assume that only sizes d (32) and q (64) are possible.
   * Hence, we don't allow a data 66 prefix effect the size.
   */
  NaClDefOp(Xmm_G_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_o) | NACL_IFLAG(SizeIgnoresData16));
}

static void NaClOperandForm_Vpd(void) {
  NaClDefOp(Xmm_G_Operand, NACL_EMPTY_OPFLAGS);
}

static void NaClOperandForm_Vps(void) {
  NaClDefOp(Xmm_G_Operand, NACL_EMPTY_OPFLAGS);
}

static void NaClOperandForm_Vq(void) {
  NaClDefOp(Xmm_Go_Operand, NACL_EMPTY_OPFLAGS);
}

static void NaClOperandForm_Vsd(void) {
  NaClDefOp(Xmm_G_Operand, NACL_EMPTY_OPFLAGS);
}

static void NaClOperandForm_Vss(void) {
  NaClDefOp(Xmm_G_Operand, NACL_EMPTY_OPFLAGS);
}

static void NaClOperandForm_VRdq(void) {
  /* Note: We ignore sizes for Mmx operands, since they
   * aren't used to compute memory addresses in NaCl.
   */
  NaClDefOp(Xmm_E_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(ModRmModIs0x3));
}

static void NaClOperandForm_VRps(void) {
  /* Note: We ignore sizes for Xmm operands, since they
   * aren't used to compute memory addresses in NaCl.
   */
  NaClDefOp(Xmm_E_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(ModRmModIs0x3));
}

static void NaClOperandForm_VRq(void) {
  /* Note: We ignore sizes for Xmm operands, since they
   * aren't used to compute memory addresses in NaCl.
   */
  NaClDefOp(Xmm_E_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(ModRmModIs0x3));
}

static void NaClOperandForm_VRpd(void) {
  /* Note: We ignore sizes for Xmm operands, since they
   * aren't used to compute memory addresses in NaCl.
   */
  NaClDefOp(Xmm_E_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(ModRmModIs0x3));
}

static void NaClOperandForm_Wdq(void) {
  /* TODO(karl) Add dq size restriction. */
  NaClDefOp(Xmm_E_Operand, NACL_EMPTY_OPFLAGS);
}

static void NaClOperandForm_Wpd(void) {
  NaClDefOp(Xmm_E_Operand, NACL_EMPTY_OPFLAGS);
}

static void NaClOperandForm_Wps(void) {
  NaClDefOp(Xmm_E_Operand, NACL_EMPTY_OPFLAGS);
}

static void NaClOperandForm_Wq(void) {
  /* TODO(karl) Add q size restriction. */
  NaClDefOp(Xmm_Eo_Operand, NACL_EMPTY_OPFLAGS);
}

static void NaClOperandForm_Wsd(void) {
  NaClDefOp(Xmm_E_Operand, NACL_EMPTY_OPFLAGS);
}

static void NaClOperandForm_Wss(void) {
  NaClDefOp(Xmm_E_Operand, NACL_EMPTY_OPFLAGS);
}

static void NaClOperandForm_Xb(void) {
  NaClDefOp(RegDS_ESI, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_b));
}

static void NaClOperandForm_Xvw(void) {
  NaClDefOp(RegDS_ESI, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w));
}

static void NaClOperandForm_Xvd(void) {
  NaClDefOp(RegDS_ESI, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_v));
}

static void NaClOperandForm_Xvq(void) {
  NaClDefOp(RegDS_ESI, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_o));
}

static void NaClOperandForm_Xzd(void) {
  NaClDefOp(RegDS_ESI, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o));
}

static void NaClOperandForm_Xzw(void) {
  NaClDefOp(RegDS_ESI, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w));
}

static void NaClOperandForm_Yb(void) {
  NaClDefOp(RegES_EDI, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_b));
}

static void NaClOperandForm_Yvd(void) {
  NaClDefOp(RegES_EDI, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_v));
}

static void NaClOperandForm_Yvq(void) {
  NaClDefOp(RegES_EDI, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_o));
}

static void NaClOperandForm_Yvw(void) {
  NaClDefOp(RegES_EDI, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w));
}

static void NaClOperandForm_Yzw(void) {
  NaClDefOp(RegES_EDI, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w));
}

static void NaClOperandForm_Yzd(void) {
  NaClDefOp(RegES_EDI, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o));
}

static void NaClOperandForm_Zvd(void) {
  NaClDefOp(RegDS_EDI, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_v));
}

/**************************************************************************
 * The following code is code that takes a opcode description string, and *
 * generates the corresponding instruction                                *
 *                                                                        *
 * TODO(karl) Remove macro implementations once we have moved to this new *
 * implementation.                                                        *
 *************************************************************************/

/* Define the (maximum) size that working buffers, for the translation code. */
#define BUFSIZE 256

/* Define the maximum number of operands that can appear in an opcode
 * description string.
 */
#define MAX_OPERANDS 10

#define MAX_DEFOPS 1000

#define BUF_SIZE 1024

/* The maximum number of symbol substitutions that can be applied to an
 * opcode description string. Used mainly to make sure that if a
 * substitution occurs, the code will not looop infinitely.
 */
#define MAX_ST_SUBSTITUTIONS 100

/* The current opcode string description being translated. Mainly used
 * to generate useful error messages.
 */
static char* kCachedDesc = "???";

/* The set of possible characters that can follow a symbol when doing
 * symbol substitution.
 */
static const char* kSymbolTerminators = " :+/{},@";

/* Generates a fatal error message for the given opcode description string. */
static void NaClDefDescFatal(const char* desc, const char* message) {
  NaClLog(LOG_FATAL, "NaClDefine '%s': %s\n", desc, message);
}

/* Generates a fatal error message for the cached opcode description string. */
static void NaClDefFatal(const char* message) {
  NaClDefDescFatal(kCachedDesc, message);
}

/* replace all occurrences of "@NAME" with the corresponding
 * value in the given symbol table.
 *
 * NOTE: only MAX_ST_SUBSTITUTIONS are allowed, to make sure
 * that we don't infinite loop.
 */
static void NaClStExpand(char* desc, struct NaClSymbolTable* st) {
  const char* marker;
  int attempts_left = MAX_ST_SUBSTITUTIONS;
  if (NULL == st) return;
  for (marker = strchr(desc, '@');
       (NULL != marker) && attempts_left;
       marker = strchr(desc, '@')) {
    char name[BUFSIZE];
    size_t name_len = 0;
    /* Extract name */
    const char *ch_ptr = marker+1;
    while (*ch_ptr && (NULL == strchr(kSymbolTerminators, *ch_ptr))) {
      name[name_len++] = *(ch_ptr++);
    }
    name[name_len] = '\0';
    if (name_len) {
      /* Get corresponding symbol table value. */
      NaClStValue* val = NaClSymbolTableGet(name, st);
      if (NULL != val) {
        /* Substitute and update desc. */
        char buffer[BUFSIZE];
        char* buffer_ptr = buffer;
        const char* desc_ptr = desc;
        char v_buffer[BUFSIZE];
        const char* v_buffer_ptr = v_buffer;
        /* Copy text before @name. */
        while (desc_ptr != marker) {
          *(buffer_ptr++) = *(desc_ptr++);
        }
        /* Do substitution of symbol value. */
        switch (val->kind) {
          case nacl_byte:
            SNPRINTF(v_buffer, BUFSIZE, "%02"NACL_PRIx8, val->value.byte_value);
            break;
          case nacl_text:
            v_buffer_ptr = val->value.text_value;
            break;
          case nacl_int:
            SNPRINTF(v_buffer, BUFSIZE, "%d", val->value.int_value);
            break;
          default:
            NaClDefDescFatal(desc, "Unable to expand @ variable");
            break;
        }
        while (*v_buffer_ptr) {
          *(buffer_ptr++) = *(v_buffer_ptr++);
        }
        /* copy text after @name. */
        desc_ptr = ch_ptr;
        while (*desc_ptr) {
          *(buffer_ptr++) = *(desc_ptr++);
        }
        *buffer_ptr = '\0';
        strcpy(desc, buffer);
        --attempts_left;
        continue;
      }
    }
    /* If reached, unable to do substitution, stop. */
    break;
  }
}

/* Verify argument is a string describing a sequence of byte,
 * i.e. pairs of hex values (with no space inbetween), describing
 * the opcode base of an instruction.
 */
static void NaClVerifyByteBaseAssumptions(const char* byte) {
  size_t len = strlen(byte);
  if ((len < 2) || (len % 2)) {
    NaClDefFatal("opcode (hex) base length must be divisible by 2");
  }
  if (len != strspn(byte, "0123456789aAbBcCdDeEfF")) {
    NaClDefFatal("opcode base not hex value");
  }
}

/* Given a pointer to a string describing a byte using hexidecimal values,
 * extract the corresponding byte value.
 *
 * Note: Assumes that the length of base is 2.
 */
static uint8_t NaClExtractByte(const char* base) {
  return (uint8_t) STRTOULL(base + strlen(base) - 2, NULL, 16);
}

/* Given a string of bytes describing a prefix, return the
 * corresponding enumerated prefix value.
 */
static NaClInstPrefix NaClExtractPrefixValue(const char* prefix_name,
                                             size_t prefix_size) {
  if (prefix_size == 0) {
    return NoPrefix;
  } else {
    size_t i;
    NaClInstPrefix prefix;
    char prefix_text[BUFSIZE];
    char* text_ptr;
    strcpy(prefix_text, "Prefix");
    text_ptr = prefix_text + strlen("Prefix");
    for (i = 0; i < prefix_size; ++i) {
      text_ptr[i] = toupper(prefix_name[i]);
    }
    text_ptr[prefix_size] = '\0';
    for (prefix = 0; prefix < NaClInstPrefixEnumSize; ++prefix) {
      if (0 == strcmp(NaClInstPrefixName(prefix), prefix_text)) {
        return prefix;
      }
    }
  }
  NaClDefFatal("Opcode prefix not understood");
  /* NOT REACHED */
  return NaClInstPrefixEnumSize;
}

/* Given a string of byte values, describing the opcode base
 * of an instruction, extract out the corresponding opcode
 * prefix (i.e. the prefix defined by all but the last byte in
 * the opcode base.
 *
 * Note: Assumes that NaClVerifyByteBaseAssumptions was called
 * on the given parameter.
 */
static NaClInstPrefix NaClExtractPrefix(const char* base) {
  size_t len = strlen(base);
  if (len >= 2) {
    return NaClExtractPrefixValue(base, len - 2);
  }
  NaClDefFatal("Opcode prefix not understood");
  /* NOT REACHED */
  return NaClInstPrefixEnumSize;
}

static void NaClVerifyOctalDigit(const char* str, const char* message) {
  if ((1 != strlen(str)) || (1 != strspn(str, "01234567"))) {
    NaClDefFatal(message);
  }
}

/*
 * Given a string describing an opcode, the type of the
 * instruction, and the mnemonic associated with the instruction,
 * parse the opcode string and define the corresponding instruction.
 *
 * Note: See ??? for descriptions of legal opcode strings.
 */
static void NaClParseOpcode(const char* opcode,
                            NaClInstType insttype,
                            NaClMnemonic name,
                            struct NaClSymbolTable* st) {
  char buf[BUFSIZE];
  char* marker;
  char* buffer = buf;
  char* base;
  char* reg_offset = NULL;
  char* opcode_ext = NULL;
  char* opcode_3d_ext = NULL;
  char* opcode_rm_ext = NULL;
  NaClInstPrefix prefix;
  uint8_t opcode_value;
  Bool is_inst_defined = FALSE;
  strcpy(buf, opcode);

  /* Remove leading whitespace. */
  while(' ' == *buffer) ++buffer;
  base = buffer;

  /* Start by finding any valid suffix (i.e. +X or /X), and remove.
   * Put the corresponding suffix into reg_offset(+), or opcode_ext(/).
   */
  /* Try to recognize an opcode extension. */
  marker = strchr(buffer, '/');
  if (NULL != marker) {
    *marker = '\0';
    opcode_ext = buffer + strlen(base) + 1;
    marker = strchr(opcode_ext, '/');
    if (NULL != marker) {
      *marker = '\0';
      opcode_rm_ext = opcode_ext + strlen(opcode_ext) + 1;
      if (strlen(opcode_rm_ext) != 1) {
        NaClDefFatal("modrm r/m opcode extension can only be "
                     "a single digit");
      }
    }
    if (strlen(opcode_ext) != 1) {
      NaClDefFatal("modrm opcode extension can only be a single digit");
    }
  } else {
    /* Try to recognize a 3dnow extension. */
    marker = strchr(buffer, '.');
    if ((NULL != marker) && (0 == strncmp(marker, "..", 2))) {
      *marker = '\0';
      opcode_3d_ext = marker + 2;
    }
  }

  marker = strchr(buffer, '+');
  if (NULL != marker) {
    *marker = '\0';
    reg_offset = buffer + strlen(base) + 1;
  }

  /* Now verify the opcode string, less the suffix, is valid. If so,
   * Pull out the instruction prefix and opcode byte.
   */
  NaClVerifyByteBaseAssumptions(base);
  if (NULL == opcode_3d_ext) {
    prefix = NaClExtractPrefix(base);
  } else {
    prefix = NaClExtractPrefixValue(base, strlen(base));
  }
  NaClDefInstPrefix(prefix);
  opcode_value = NaClExtractByte(base + strlen(base) - 2);

  if (reg_offset != NULL) {
    int reg = (int) STRTOULL(reg_offset, NULL, 10);
    if (reg < 0) {
      NaClDefFatal("can't add negative values to opcode");
    } else if ((reg + opcode_value) >= NCDTABLESIZE) {
      NaClDefFatal("opcode addition too large");
    }
    if (NULL == NaClSymbolTableGet("add_reg?", st)) {
      NaClVerifyOctalDigit(reg_offset, "invalid opcode register offset");
      NaClDefInst(opcode_value + reg, insttype, NACL_IFLAG(OpcodePlusR), name);
      NaClDefOpcodeRegisterValue(reg);
    }
    else {
      NaClDefInst(opcode_value + reg, insttype, NACL_EMPTY_IFLAGS, name);
    }
    is_inst_defined = TRUE;
  }
  if (opcode_ext != NULL) {
    if (0 == strcmp("r", opcode_ext)) {
      if (! is_inst_defined) {
        NaClDefInst(opcode_value, insttype, NACL_EMPTY_IFLAGS, name);
      }
      NaClAddIFlags(NACL_IFLAG(OpcodeUsesModRm));
    } else {
      int ext;
      NaClVerifyOctalDigit(opcode_ext, "invalid modrm opcode extension");
      ext = (int) STRTOULL(opcode_ext, NULL, 10);
      if (! is_inst_defined) {
        NaClDefInst(opcode_value, insttype, NACL_EMPTY_IFLAGS, name);
      }
      NaClAddIFlags(NACL_IFLAG(OpcodeInModRm));
      NaClDefOpcodeExtension(ext);
    }
    is_inst_defined = TRUE;
    if (opcode_rm_ext != NULL) {
      NaClVerifyOctalDigit(opcode_rm_ext, "invalid modrm r/m opcode extension");
      NaClDefineOpcodeModRmRmExtension((int) STRTOULL(opcode_rm_ext, NULL, 10));
    }
  }
  if (!is_inst_defined) {
    NaClIFlags flags = NACL_EMPTY_IFLAGS;
    if (opcode_3d_ext != NULL) {
      opcode_value = NaClExtractByte(opcode_3d_ext);
    }
    NaClDefInst(opcode_value, insttype, flags, name);
  }
}

/* Given the text of a mnemonic, return the corresponding mnemonic. */
static NaClMnemonic NaClAssemName(const char* name) {
  NaClMnemonic i;
  for (i = (NaClMnemonic) 0; i < NaClMnemonicEnumSize; ++i) {
    if (0 == strcmp(NaClMnemonicName(i), name)) {
      return i;
    }
  }
  NaClDefFatal("Can't find name for mnemonic");
 /* NOT REACHED */
  return NaClMnemonicEnumSize;
}

/* Given the name of a register, define the corresponding operand on
 * the instruction being defined.
 */
static void NaClExtractRegister(const char* reg_name) {
  char buffer[BUFSIZE];
  char* buf_ptr = buffer;
  char* reg_ptr = (char*) reg_name;
  NaClOpKind kind;
  strcpy(buf_ptr, "Reg");
  buf_ptr += strlen("Reg");
  while (*reg_ptr) {
    char ch = *(reg_ptr++);
    *(buf_ptr++) = toupper(ch);
  }
  *buf_ptr = '\0';
  for (kind = 0; kind < NaClOpKindEnumSize; ++kind) {
    if (0 == strcmp(NaClOpKindName(kind), buffer)) {
      NaClDefOp(kind, NACL_EMPTY_OPFLAGS);
      return;
    }
  }
  NaClDefFatal("Unable to find register");
}

/* Helper function to add a define operand function into
 * a symbol table.
 */
static void NaClSymbolTablePutDefOp(const char* name,
                                    NaClDefOperand defop,
                                    struct NaClSymbolTable* st) {
  NaClStValue value;
  value.kind = nacl_defop;
  value.value.defop_value = defop;
  NaClSymbolTablePut(name, &value, st);
}

/* Given the name describing legal values for an operand,
 * call the corresponding function to define the corresponding
 * operand.
 */
static void NaClExtractOperandForm(const char* form) {
  static struct NaClSymbolTable* defop_st = NULL;
  NaClStValue* value;
  if (NULL == defop_st) {
    defop_st = NaClSymbolTableCreate(MAX_DEFOPS, NULL);
    NaClSymbolTablePutDefOp("Ap",      NaClOperandForm_Ap,        defop_st);
    NaClSymbolTablePutDefOp("Cd/q",    NaClOperandForm_CdSlq,     defop_st);
    NaClSymbolTablePutDefOp("Dd/q",    NaClOperandForm_DdSlq,     defop_st);
    NaClSymbolTablePutDefOp("Eb",      NaClOperandForm_Eb,        defop_st);
    NaClSymbolTablePutDefOp("Ed",      NaClOperandForm_Ed,        defop_st);
    NaClSymbolTablePutDefOp("Ed/q",    NaClOperandForm_EdSlq,     defop_st);
    NaClSymbolTablePutDefOp("Ed/q/d",  NaClOperandForm_EdSlqSld,  defop_st);
    NaClSymbolTablePutDefOp("Ed/q/q",  NaClOperandForm_EdSlqSlq,  defop_st);
    NaClSymbolTablePutDefOp("Ev",      NaClOperandForm_Ev,        defop_st);
    NaClSymbolTablePutDefOp("Ew",      NaClOperandForm_Ew,        defop_st);
    NaClSymbolTablePutDefOp("Fvd",     NaClOperandForm_Fvd,       defop_st);
    NaClSymbolTablePutDefOp("Fvq",     NaClOperandForm_Fvq,       defop_st);
    NaClSymbolTablePutDefOp("Fvw",     NaClOperandForm_Fvw,       defop_st);
    NaClSymbolTablePutDefOp("Gb",      NaClOperandForm_Gb,        defop_st);
    NaClSymbolTablePutDefOp("Gd",      NaClOperandForm_Gd,        defop_st);
    NaClSymbolTablePutDefOp("Gd/q",    NaClOperandForm_GdQ,       defop_st);
    NaClSymbolTablePutDefOp("Gq",      NaClOperandForm_Gq,        defop_st);
    NaClSymbolTablePutDefOp("Gv",      NaClOperandForm_Gv,        defop_st);
    NaClSymbolTablePutDefOp("Gw",      NaClOperandForm_Gw,        defop_st);
    NaClSymbolTablePutDefOp("Gw",      NaClOperandForm_Gw,        defop_st);
    NaClSymbolTablePutDefOp("Ib",      NaClOperandForm_Ib,        defop_st);
    NaClSymbolTablePutDefOp("Iv",      NaClOperandForm_Iv,        defop_st);
    NaClSymbolTablePutDefOp("Iw",      NaClOperandForm_Iw,        defop_st);
    NaClSymbolTablePutDefOp("Iz",      NaClOperandForm_Iz,        defop_st);
    NaClSymbolTablePutDefOp("I2b",     NaClOperandForm_I2b,       defop_st);
    NaClSymbolTablePutDefOp("Jb",      NaClOperandForm_Jb,        defop_st);
    NaClSymbolTablePutDefOp("Jz",      NaClOperandForm_Jz,        defop_st);
    NaClSymbolTablePutDefOp("Jzd",     NaClOperandForm_Jzd,       defop_st);
    NaClSymbolTablePutDefOp("Jzw",     NaClOperandForm_Jzw,       defop_st);
    NaClSymbolTablePutDefOp("M",       NaClOperandForm_M,         defop_st);
    NaClSymbolTablePutDefOp("Ma",      NaClOperandForm_Ma,        defop_st);
    NaClSymbolTablePutDefOp("Mb",      NaClOperandForm_Mb,        defop_st);
    NaClSymbolTablePutDefOp("Md",      NaClOperandForm_Md,        defop_st);
    NaClSymbolTablePutDefOp("Md/q",    NaClOperandForm_MdSlq,     defop_st);
    NaClSymbolTablePutDefOp("Mdq",     NaClOperandForm_Mdq,       defop_st);
    NaClSymbolTablePutDefOp("Mf",      NaClOperandForm_Mf,        defop_st);
    NaClSymbolTablePutDefOp("Mp",      NaClOperandForm_Mp,        defop_st);
    NaClSymbolTablePutDefOp("Mq",      NaClOperandForm_Mq,        defop_st);
    NaClSymbolTablePutDefOp("Ms",      NaClOperandForm_Ms,        defop_st);
    NaClSymbolTablePutDefOp("Mv",      NaClOperandForm_Mv,        defop_st);
    NaClSymbolTablePutDefOp("Mw/Rv",   NaClOperandForm_MwSlRv,    defop_st);
    NaClSymbolTablePutDefOp("Mw",      NaClOperandForm_Mw,        defop_st);
    NaClSymbolTablePutDefOp("Ob",      NaClOperandForm_Ob,        defop_st);
    NaClSymbolTablePutDefOp("Ov",      NaClOperandForm_Ov,        defop_st);
    NaClSymbolTablePutDefOp("Pd/q",    NaClOperandForm_PdSlq,     defop_st);
    NaClSymbolTablePutDefOp("Pd/q/d",  NaClOperandForm_PdSlqSld,  defop_st);
    NaClSymbolTablePutDefOp("Pd/q/q",  NaClOperandForm_PdSlqSlq,  defop_st);
    NaClSymbolTablePutDefOp("Pq",      NaClOperandForm_Pq,        defop_st);
    NaClSymbolTablePutDefOp("PRq",     NaClOperandForm_PRq,       defop_st);
    NaClSymbolTablePutDefOp("Qd",      NaClOperandForm_Qd,        defop_st);
    NaClSymbolTablePutDefOp("Qq",      NaClOperandForm_Qq,        defop_st);
    NaClSymbolTablePutDefOp("Rd/q",    NaClOperandForm_RdSlq,     defop_st);
    NaClSymbolTablePutDefOp("Rd/q/Mb", NaClOperandForm_RdSlqSlMb, defop_st);
    NaClSymbolTablePutDefOp("Rd/q/Mw", NaClOperandForm_RdSlqSlMw, defop_st);
    NaClSymbolTablePutDefOp("Rd/Mb",   NaClOperandForm_RdSlMb,    defop_st);
    NaClSymbolTablePutDefOp("Rd/Mw",   NaClOperandForm_RdSlMw,    defop_st);
    NaClSymbolTablePutDefOp("rAXv",    NaClOperandForm_rAXv,      defop_st);
    NaClSymbolTablePutDefOp("rAXva",   NaClOperandForm_rAXva,     defop_st);
    NaClSymbolTablePutDefOp("rAXvd",   NaClOperandForm_rAXvd,     defop_st);
    NaClSymbolTablePutDefOp("rAXvq",   NaClOperandForm_rAXvq,     defop_st);
    NaClSymbolTablePutDefOp("rAXvw",   NaClOperandForm_rAXvw,     defop_st);
    NaClSymbolTablePutDefOp("rBXv",    NaClOperandForm_rBXv,      defop_st);
    NaClSymbolTablePutDefOp("rCXv",    NaClOperandForm_rCXv,      defop_st);
    NaClSymbolTablePutDefOp("rDXv",    NaClOperandForm_rDXv,      defop_st);
    NaClSymbolTablePutDefOp("rSPv",    NaClOperandForm_rSPv,      defop_st);
    NaClSymbolTablePutDefOp("rBPv",    NaClOperandForm_rBPv,      defop_st);
    NaClSymbolTablePutDefOp("rSIv",    NaClOperandForm_rSIv,      defop_st);
    NaClSymbolTablePutDefOp("rDIv",    NaClOperandForm_rDIv,      defop_st);
    NaClSymbolTablePutDefOp("r8b",     NaClOperandForm_r8b,       defop_st);
    NaClSymbolTablePutDefOp("r8v",     NaClOperandForm_r8v,       defop_st);
    NaClSymbolTablePutDefOp("r8vd",    NaClOperandForm_r8vd,      defop_st);
    NaClSymbolTablePutDefOp("r8vq",    NaClOperandForm_r8vq,      defop_st);
    NaClSymbolTablePutDefOp("SGz",     NaClOperandForm_SGz,       defop_st);
    NaClSymbolTablePutDefOp("Sw",      NaClOperandForm_Sw,        defop_st);
    NaClSymbolTablePutDefOp("Udq",     NaClOperandForm_Udq,       defop_st);
    NaClSymbolTablePutDefOp("Udq/Md",  NaClOperandForm_UdqMd,     defop_st);
    NaClSymbolTablePutDefOp("Udq/Mq",  NaClOperandForm_UdqMq,     defop_st);
    NaClSymbolTablePutDefOp("Udq/Mw",  NaClOperandForm_UdqMw,     defop_st);
    NaClSymbolTablePutDefOp("Vdq",     NaClOperandForm_Vdq,       defop_st);
    NaClSymbolTablePutDefOp("Vd/q",    NaClOperandForm_VdSlq,     defop_st);
    NaClSymbolTablePutDefOp("Vd/q/d",  NaClOperandForm_VdSlqSld,  defop_st);
    NaClSymbolTablePutDefOp("Vd/q/q",  NaClOperandForm_VdSlqSlq,  defop_st);
    NaClSymbolTablePutDefOp("Vpd",     NaClOperandForm_Vpd,       defop_st);
    NaClSymbolTablePutDefOp("Vps",     NaClOperandForm_Vps,       defop_st);
    NaClSymbolTablePutDefOp("Vq",      NaClOperandForm_Vq,        defop_st);
    NaClSymbolTablePutDefOp("Vsd",     NaClOperandForm_Vsd,       defop_st);
    NaClSymbolTablePutDefOp("Vss",     NaClOperandForm_Vss,       defop_st);
    NaClSymbolTablePutDefOp("VRdq",    NaClOperandForm_VRdq,      defop_st);
    NaClSymbolTablePutDefOp("VRpd",    NaClOperandForm_VRpd,      defop_st);
    NaClSymbolTablePutDefOp("VRps",    NaClOperandForm_VRps,      defop_st);
    NaClSymbolTablePutDefOp("VRq",     NaClOperandForm_VRq,       defop_st);
    NaClSymbolTablePutDefOp("Wdq",     NaClOperandForm_Wdq,       defop_st);
    NaClSymbolTablePutDefOp("Wpd",     NaClOperandForm_Wpd,       defop_st);
    NaClSymbolTablePutDefOp("Wps",     NaClOperandForm_Wps,       defop_st);
    NaClSymbolTablePutDefOp("Wq",      NaClOperandForm_Wq,        defop_st);
    NaClSymbolTablePutDefOp("Wsd",     NaClOperandForm_Wsd,       defop_st);
    NaClSymbolTablePutDefOp("Wss",     NaClOperandForm_Wss,       defop_st);
    NaClSymbolTablePutDefOp("Xb",      NaClOperandForm_Xb,        defop_st);
    NaClSymbolTablePutDefOp("Xvd",     NaClOperandForm_Xvd,       defop_st);
    NaClSymbolTablePutDefOp("Xvq",     NaClOperandForm_Xvq,       defop_st);
    NaClSymbolTablePutDefOp("Xvw",     NaClOperandForm_Xvw,       defop_st);
    NaClSymbolTablePutDefOp("Xzw",     NaClOperandForm_Xzw,       defop_st);
    NaClSymbolTablePutDefOp("Xzd",     NaClOperandForm_Xzd,       defop_st);
    NaClSymbolTablePutDefOp("Yb",      NaClOperandForm_Yb,        defop_st);
    NaClSymbolTablePutDefOp("Yvd",     NaClOperandForm_Yvd,       defop_st);
    NaClSymbolTablePutDefOp("Yvq",     NaClOperandForm_Yvq,       defop_st);
    NaClSymbolTablePutDefOp("Yvw",     NaClOperandForm_Yvw,       defop_st);
    NaClSymbolTablePutDefOp("Yzd",     NaClOperandForm_Yzd,       defop_st);
    NaClSymbolTablePutDefOp("Yzw",     NaClOperandForm_Yzw,       defop_st);
    NaClSymbolTablePutDefOp("Zvd",     NaClOperandForm_Zvd,       defop_st);
  }
  value = NaClSymbolTableGet(form, defop_st);
  if (NULL == value || (value->kind != nacl_defop)) {
    NaClDefFatal("Malformed $defop form");
  }
  value->value.defop_value();
}

/* Given a string describing the operand, define the corresponding
 * operand. Returns the set of operand flags that must be
 * defined for the operand, based on the contents of the operand string.
 *
 * Note: See ??? for descriptions of legal operand strings.
 */
static NaClOpFlags NaClExtractOperand(const char* operand) {
  NaClOpFlags op_flags = NACL_EMPTY_OPFLAGS;
  switch (*operand) {
    case '{':
      if ('}' == operand[strlen(operand) - 1]) {
        char buffer[BUFSIZE];
        strcpy(buffer, operand+1);
        buffer[strlen(buffer)-1] = '\0';
        op_flags =
            NaClExtractOperand(buffer) |
            NACL_OPFLAG(OpImplicit);
      } else {
        NaClDefFatal("Malformed implicit braces");
      }
      break;
    case '1':
      if (1 == strlen(operand)) {
        NaClOperandForm_One();
      } else {
        NaClDefFatal("Malformed operand argument");
      }
      break;
    case '%':
      NaClExtractRegister(operand+1);
      break;
    case '$':
      NaClExtractOperandForm(operand+1);
      break;
    default:
      NaClDefFatal("Malformed operand argument");
  }
  return op_flags;
}

/* Given a string describing the operand, and an index corresponding
 * to the position of the operand in the corresponding opcode description
 * string, define the corresponding operand in the model being generated.
 */
static void NaClParseOperand(const char* operand, int arg_index) {
  NaClAddOpFlags(arg_index, NaClExtractOperand(operand));
  NaClAddOpFormat(arg_index, operand);
}

void NaClBegDef(const char* desc, NaClInstType insttype,
                struct NaClSymbolTable* st) {
  char* name;
  char* arg;
  int num_args = 0;
  char buffer[BUFSIZE];
  char expanded_desc[BUFSIZE];
  char* opcode;
  char* assem_desc;
  NaClMnemonic mnemonic;
  char* old_kdesc = kCachedDesc;
  kCachedDesc = (char*) desc;

  /* Do symbol table substitutions. */
  strcpy(buffer, desc);
  NaClStExpand(buffer, st);
  strcpy(expanded_desc, buffer);
  kCachedDesc = expanded_desc;

  /* Separate the description into opcode sequence and
   * assembly description.
   */
  opcode = strtok(buffer, ":");
  if (NULL == opcode) {
    NaClDefFatal("opcode not separated from assembly description using ':'");
  }
  assem_desc = strtok(NULL, ":");
  if (NULL == assem_desc) {
    NaClDefFatal("Can't find assembly description");
  }
  if (NULL != strtok(NULL, ":")) {
    NaClDefFatal("Malformed assembly description");
  }

  /* extract nmemonic name of instruction. */
  name = strtok(assem_desc, " ");
  if (NULL == name) {
    NaClDefFatal("Can't find mnemonic name");
  }
  mnemonic = NaClAssemName(name);
  NaClDelaySanityChecks();
  NaClParseOpcode(opcode, insttype, mnemonic, st);

  /* Now extract operands. */
  while ((arg = strtok(NULL, ", "))) {
    NaClParseOperand(arg, num_args);
    ++num_args;
    if (num_args == MAX_OPERANDS) {
      NaClDefFatal("Too many operands");
    }
  }
  kCachedDesc = old_kdesc;
}

void NaClBegD32(const char* desc, NaClInstType insttype,
                struct NaClSymbolTable* st) {
  NaClBegDef(desc, insttype, st);
  NaClAddIFlags(NACL_IFLAG(Opcode32Only));
}

void NaClBegD64(const char* desc, NaClInstType insttype,
                struct NaClSymbolTable* st) {
  NaClBegDef(desc, insttype, st);
  NaClAddIFlags(NACL_IFLAG(Opcode64Only));
}

void NaClBegDefPlatform(NaClTargetPlatform platform,
                        const char* desc, NaClInstType insttype,
                        struct NaClSymbolTable* st) {
  switch (platform) {
    case T32:
      NaClBegD32(desc, insttype, st);
      break;
    case T64:
      NaClBegD64(desc, insttype, st);
      break;
    case Tall:
      NaClBegDef(desc, insttype, st);
      break;
    default:
      /* This shouldn't happen. */
      NaClDefFatal("Don't understand platform");
  }
}

void NaClEndDef(NaClInstCat icat) {
  NaClSetInstCat(icat);
  NaClApplySanityChecks();
  NaClResetToDefaultInstPrefix();
}

void NaClDefine(const char* desc, NaClInstType insttype,
                struct NaClSymbolTable* st, NaClInstCat cat) {
  NaClBegDef(desc, insttype, st);
  NaClEndDef(cat);
}

void NaClDef_32(const char* desc, NaClInstType insttype,
                struct NaClSymbolTable* st, NaClInstCat cat) {
  NaClBegD32(desc, insttype, st);
  NaClEndDef(cat);
}

void NaClDef_64(const char* desc, NaClInstType insttype,
                struct NaClSymbolTable* st, NaClInstCat cat) {
  NaClBegD64(desc, insttype, st);
  NaClEndDef(cat);
}

void NaClDefinePlatform(NaClTargetPlatform platform,
                        const char* desc, NaClInstType insttype,
                        struct NaClSymbolTable* st, NaClInstCat cat) {
  switch (platform) {
    case T32:
      NaClDef_32(desc, insttype, st, cat);
      break;
    case T64:
      NaClDef_64(desc, insttype, st, cat);
      break;
    case Tall:
      NaClDefine(desc, insttype, st, cat);
      break;
    default:
      /* This shouldn't happen. */
      NaClDefFatal("Don't understand platform");
  }
}

/* Generate an error message for the given instruction description,
 * if min <0 or max > 7.
 */
static void NaClDefCheckRange(const char* desc, int min, int max, int cutoff) {
  if (min < 0) {
    NaClDefDescFatal(desc, "Minimum value must be >= 0");
  } else if (max > cutoff) {
    char buffer[BUF_SIZE];
    SNPRINTF(buffer, BUF_SIZE, "Maximum value must be <= %d", cutoff);
    NaClDefDescFatal(desc, buffer);
  }
}

/* Define a symbol table size that can hold a small number of
 * symbols (i.e. limit to at most 5 definitions).
 */
#define NACL_SMALL_ST (5)

void NaClDefReg(const char* desc, int min, int max,
                NaClInstType insttype, struct NaClSymbolTable* context_st,
                NaClInstCat cat) {
  int i;
  struct NaClSymbolTable* st = NaClSymbolTableCreate(NACL_SMALL_ST, context_st);
  NaClDefCheckRange(desc, min, max, kMaxRegisterIndexInOpcode);
  for (i = min; i <= max; ++i) {
    NaClSymbolTablePutInt("reg", i, st);
    NaClDefine(desc, insttype, st, cat);
  }
  NaClSymbolTableDestroy(st);
}

void NaClDefIter(const char* desc, int min, int max,
                 NaClInstType insttype, struct NaClSymbolTable* context_st,
                 NaClInstCat cat) {
  int i;
  struct NaClSymbolTable* st = NaClSymbolTableCreate(NACL_SMALL_ST, context_st);
  NaClDefCheckRange(desc, min, max, NCDTABLESIZE);
  NaClSymbolTablePutInt("add_reg?", 1, st);
  for (i = min; i <= max; ++i) {
    NaClSymbolTablePutInt("i", i, st);
    NaClDefine(desc, insttype, st, cat);
  }
  NaClSymbolTableDestroy(st);
}
