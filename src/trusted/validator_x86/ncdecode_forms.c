/* Copyright (c) 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Set of predefined instruction forms (via procedure calls), providing
 * a more concise way of specifying opcodes.
 */

#include <assert.h>
#include <ctype.h>

#include "native_client/src/trusted/validator_x86/ncdecode_forms.h"

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/include/portability_string.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator_x86/defsize64.h"
#include "native_client/src/trusted/validator_x86/lock_insts.h"
#include "native_client/src/trusted/validator_x86/nacl_illegal.h"
#include "native_client/src/trusted/validator_x86/ncdecode_st.h"
#include "native_client/src/trusted/validator_x86/ncdecode_tablegen.h"
#include "native_client/src/trusted/validator_x86/zero_extends.h"

/* To turn on debugging of instruction decoding, change value of
 * DEBUGGING to 1.
 */
#define DEBUGGING 0

#include "native_client/src/shared/utils/debugging.h"

const char* NaClInstCatName(NaClInstCat cat) {
  int i;
  static struct {
    NaClInstCat cat;
    const char* name;
  } cat_desc[] = {
    { UnarySet , "UnarySet" },
    { UnaryUpdate , "UnaryUpdate" },
    { Move , "Move" },
    { Binary , "Binary" },
    { Compare , "Compare"},
    { Exchange, "Exchange" },
    { Push, "Push" },
    { Pop, "Pop" },
    { Call, "Call"},
    { Return, "Return"},
    { Jump, "Jump" },
    { Uses, "Uses" },
    { Lea, "Lea" },
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

/* Returns the OpSet, OpUse, and OpDest operand flags for the destination
 * argument of the instruction, given the category of instruction. Argument
 * num_ops is the number of operands the instruction has.
 */
static NaClOpFlags NaClGetArg1Flags(NaClInstCat icat, int num_ops) {
  DEBUG(NaClLog(LOG_INFO, "NaClGetArg1Flags(%s)\n", NaClInstCatName(icat)));
  switch (icat) {
    case Move:
    case Lea:
      return NACL_OPFLAG(OpSet);
    case UnarySet:
    case Exchange:
    case Call:
    case UnaryUpdate:
      return NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse);
    case Binary:
      if (3 == num_ops) {
        return NACL_OPFLAG(OpSet);
      } else {
        return NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse);
      }
    case Compare:
      return NACL_OPFLAG(OpUse);
    case Push:
    case Pop:
      return NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet);
    case Jump:
    case Return:
      return NACL_OPFLAG(OpSet);
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
    case Move:
    case Binary:
    case Push:
    case Jump:
    case Uses:
      return NACL_OPFLAG(OpUse);
    case Compare:
      return NACL_OPFLAG(OpUse);
    case Exchange:
      if (op_index == 2) {
        if (visible_index == 2) {
          return NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpDest);
        } else {
          return NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse);
        }
      } else {
        return NACL_OPFLAG(OpUse);
      }
    case Pop:
      return NACL_OPFLAG(OpSet);
    case Call:
    case Return:
      /* First operand is always the instruction pointer. The second, is
       * always the stack pointer. The third  is optional. If it is provided,
       * it defines how many bytes to pop from the stack.
       */
      if (op_index == 2) {
        /* Stack pointer, which is updated. */
        return NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse);
      } else {
        /* Optional value containing the number of bytes to pop. */
        return NACL_OPFLAG(OpUse);
      }
    case Lea:
    case Other:
      return NACL_EMPTY_OPFLAGS;
    default:
      NaClFatal("NaClGetArg2PlusFlags: unrecognized category");
      /* NOT REACHED */
      return NACL_EMPTY_OPFLAGS;
  }
}

/* Returns the OpSet, OpUse, and OpDest operand flags for the operand
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
  /* Always flag the first visible argument as a (lockable) destination. */
  if ((visible_count == 1) && (flags & NACL_OPFLAG(OpSet))) {
    flags |= NACL_OPFLAG(OpDest);
  }
  return flags;
}

/* Add miscellaneous flags defined elsewhere. */
static void NaClAddMiscellaneousFlags() {
  NaClAddZeroExtend32FlagIfApplicable();
  NaClLockableFlagIfApplicable();
  NaClAddSizeDefaultIs64();
  NaClAddNaClIllegalIfApplicable();
}

/* Add set/use/dest flags to all operands of the current instruction,
 * for the given instruction category.
 */
void NaClSetInstCat(NaClInstCat icat) {
  int operand_index = 0;  /* note: this is one-based. */
  int visible_count = 0;
  int i;
  int num_ops;
  int real_num_ops;
  NaClInst* inst = NaClGetDefInst();
  num_ops = inst->num_operands;
  real_num_ops = num_ops;  /* Until proven otherwise. */
  for (i = 0; i < num_ops; ++i) {
    if (NACL_EMPTY_OPFLAGS ==
        (inst->operands[i].flags & NACL_OPFLAG(OperandExtendsOpcode))) {
      Bool is_visible = FALSE;
      ++operand_index;
      if (NACL_EMPTY_OPFLAGS ==
          (inst->operands[i].flags & NACL_OPFLAG(OpImplicit))) {
        is_visible = TRUE;
        ++visible_count;
      }
      NaClAddOpFlags(i, NaClGetIcatFlags(
          icat, operand_index, (is_visible ? visible_count : 0), real_num_ops));
    } else {
      /* Note: This code assumes that OperandExtendsOpcode operands must precede
       * real operands.
       */
      --real_num_ops;
    }
  }
  /* Do special fixup for binary case with 3 arguments. In such
   * cases, the first argument is only a set, rather than a set/use.
   */
  if ((Binary == icat) && (4 ==  operand_index)) {
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
static NaClOpKind SLOpcode(int16_t val) {
  return Opcode0 - val;
}

Bool NaClInInstructionSet(const NaClMnemonic* names,
                          size_t names_size,
                          const NaClNameOpcodeSeq* name_and_opcode_seq,
                          size_t name_and_opcode_seq_size) {
  size_t i;
  NaClInst* inst = NaClGetDefInst();

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
      Bool is_good = TRUE;
      /* First compare opcode bytes. */
      for (j = 0; j < inst->num_opcode_bytes; ++j) {
        if (inst->opcode[j] != name_and_opcode_seq[i].opcode_seq[j]) {
          is_good = FALSE;
          break;
        }
      }
      if (is_good) {
        /* Now compare any values that must by in modrm. */
        for (j = inst->num_opcode_bytes; j < NACL_OPCODE_SEQ_SIZE; ++j) {
          int16_t val = name_and_opcode_seq[i].opcode_seq[j];
          if (END_OPCODE_SEQ == val) {
            /* At end of descriptor, matched! */
            return TRUE;
          }
          if (IsSLValue(val)) {
            /* Compare to opcode in operand[0]. */
            if ((inst->num_operands > 0) &&
                (inst->operands[0].flags &&
                 NACL_OPFLAG(OperandExtendsOpcode)) &&
                (SLOpcode(val) ==  inst->operands[0].kind)) {
              /* good, continue search. */
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

/* adds instruction flag denoting that a modrm byte is needed, if the
 * instruction doesn't already define it.
 */
static void NaClAddUsesModRmOperand(NaClOpKind op_kind) {
  NaClInst* inst = NaClGetDefInst();
  if (NACL_EMPTY_IFLAGS == (inst->flags & NACL_IFLAG(OpcodeInModRm))) {
    NaClAddIFlags(NACL_IFLAG(OpcodeUsesModRm));
  }
  NaClDefOp(op_kind, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(E__)() {
  NaClAddUsesModRmOperand(E_Operand);
}

void DEF_OPERAND(Eb_)() {
  NaClAddUsesModRmOperand(E_Operand);
  NaClAddIFlags(NACL_IFLAG(OperandSize_b));
}

void DEF_OPERAND(Edq)() {
  NaClAddUsesModRmOperand(Edq_Operand);
}

void DEF_OPERAND(EdQ)() {
  /* TODO(karl) fix measurement of size to use Rex.W */
  NaClAddUsesModRmOperand(E_Operand);
}

void DEF_OPERAND(Ev_)() {
  NaClAddUsesModRmOperand(E_Operand);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
                NACL_IFLAG(OperandSize_o));
}

void DEF_OPERAND(Fvw)() {
  NaClDefOp(RegRFLAGS, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w));
}

void DEF_OPERAND(Fvd)() {
  NaClDefOp(RegRFLAGS, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_v));
}

void DEF_OPERAND(Fvq)() {
  NaClDefOp(RegRFLAGS, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_o));
}

void DEF_OPERAND(Gb_)() {
  NaClAddUsesModRmOperand(G_Operand);
  NaClAddIFlags(NACL_IFLAG(OperandSize_b));
}

void DEF_OPERAND(Gd_)() {
  NaClAddUsesModRmOperand(Gv_Operand);
}

void DEF_OPERAND(Gdq)() {
  NaClAddUsesModRmOperand(Gdq_Operand);
}

void DEF_OPERAND(GdQ)() {
  NaClAddUsesModRmOperand(G_Operand);
  NaClAddIFlags(NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o));
}

void DEF_OPERAND(Gv_)() {
  NaClAddUsesModRmOperand(G_Operand);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
                NACL_IFLAG(OperandSize_o));
}

void DEF_OPERAND(I__)() {
  NaClDefOp(I_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Ib_)() {
  NaClInst* inst = NaClGetDefInst();
  NaClDefOp(I_Operand, NACL_EMPTY_OPFLAGS);
  if (inst->flags & NACL_IFLAG(OperandSize_b)) {
    NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed));
  } else {
    NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  }
}

void DEF_OPERAND(Iw_)() {
  NaClDefOp(I_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_w));
}

void DEF_OPERAND(Iz_)() {
  NaClDefOp(I_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_z));
}

/* Installs Jz, except for size constraints. */
static void DefOperandJzBase() {
  NaClDefOp(J_Operand,
            NACL_OPFLAG(OperandRelative) | NACL_OPFLAG(OperandNear));
  /* Note: when in 32-bit mode, the relative offset can be a 16 or 32 bit
   * immediate offset, depending on the operand size. When in 64-bit mode,
   * the relative offset is ALWAYS a 32-bit immediate value (see page
   * 76 for CALL of AMD manual (document 24594-Rev.3.14-September 2007,
   * "AMD64 Architecture Programmer's manual Volume 3: General-Purpose
   * and System Instructions").
   */
  if (X86_32 == NACL_FLAGS_run_mode) {
    NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_z));
  } else {
    NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_v));
  }
}

void DEF_OPERAND(Jz_)() {
  DefOperandJzBase();
  NaClAddIFlags(NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
                NACL_IFLAG(OperandSize_o));
}

void DEF_OPERAND(Jzd)() {
  DefOperandJzBase();
  NaClAddIFlags(NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o));
}

void DEF_OPERAND(Jzw)() {
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

void DEF_OPERAND(Mb_)() {
  NaClDefOp(Mb_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Md_)() {
  NaClDefOp(Mv_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Mdq)() {
  NaClDefOp(Mdq_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(MdQ)() {
  /* TODO(karl) fix measurement of size to use Rex.W */
  NaClDefOp(M_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Mpd)() {
  NaClDefOp(Mdq_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Mps)() {
  NaClDefOp(Mdq_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Mq_)() {
  NaClDefOp(Mo_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Nq_)() {
  NaClDefOp(Mmx_N_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(ModRmModIs0x3));
}

void DEF_OPERAND(One)() {
  NaClDefOp(Const_1, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Pd_)() {
  NaClDefOp(Mmx_Gd_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Pdq)() {
  /* TODO(karl) add dq size restriction. */
  NaClDefOp(Mmx_G_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(PdQ)() {
  /* TODO(karl) add d/q size restriction. */
  NaClDefOp(Mmx_G_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Pq_)() {
  /* TODO(karl) Add q size restriction. */
  NaClDefOp(Mmx_G_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Ppi)() {
  NaClDefOp(Mmx_G_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Qd_)() {
  /* TODO(karl) add d size restriction. */
  NaClDefOp(Mmx_E_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Qpi)() {
  NaClDefOp(Mmx_E_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Qq_)() {
  /* TODO(karl) add q size restriction. */
  NaClDefOp(Mmx_E_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(rAXv)() {
  NaClDefOp(RegREAX, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
                NACL_IFLAG(OperandSize_o));
}

void DEF_OPERAND(rBXv)() {
  NaClDefOp(RegREBX, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
                NACL_IFLAG(OperandSize_o));
}

void DEF_OPERAND(rCXv)() {
  NaClDefOp(RegRECX, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
                NACL_IFLAG(OperandSize_o));
}

void DEF_OPERAND(rDXv)() {
  NaClDefOp(RegREDX, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
                NACL_IFLAG(OperandSize_o));
}

void DEF_OPERAND(rSPv)() {
  NaClDefOp(RegRESP, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
                NACL_IFLAG(OperandSize_o));
}

void DEF_OPERAND(rBPv)() {
  NaClDefOp(RegREBP, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
                NACL_IFLAG(OperandSize_o));
}

void DEF_OPERAND(rSIv)() {
  NaClDefOp(RegRESI, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
                NACL_IFLAG(OperandSize_o));
}

void DEF_OPERAND(rDIv)() {
  NaClDefOp(RegREDI, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
                NACL_IFLAG(OperandSize_o));
}

/* Note: The interpretation for r8v is documented in ncdecode_forms.h. That is,
 *   r8 - The 8 registers rAX, rCX, rDX, rBX, rSP, rBP, rSI, rDI, and the
 *        optional registers r8-r15 if REX.b is set, based on the register value
 *        embedded in the opcode.
 */
void DEF_OPERAND(r8v)() {
  NaClDefOp(G_OpcodeBase, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
                NACL_IFLAG(OperandSize_o));
}

void DEF_OPERAND(Udq)() {
  NaClDefOp(Xmm_E_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(ModRmModIs0x3));
}

void DEF_OPERAND(Upd)() {
  NaClDefOp(Xmm_E_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(ModRmModIs0x3));
}

void DEF_OPERAND(Ups)() {
  NaClDefOp(Xmm_E_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(ModRmModIs0x3));
}

void DEF_OPERAND(Uq_)() {
  NaClDefOp(Xmm_E_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(ModRmModIs0x3));
}

void DEF_OPERAND(Vdq)() {
  /* TODO(karl) Add dq size restriction. */
  NaClDefOp(Xmm_G_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(VdQ)() {
  /* TODO(karl) Add d/q size restriction. */
  NaClDefOp(Xmm_G_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Vpd)() {
  NaClDefOp(Xmm_G_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Vps)() {
  NaClDefOp(Xmm_G_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Vq_)() {
  NaClDefOp(Xmm_Go_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Vsd)() {
  NaClDefOp(Xmm_G_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Vss)() {
  NaClDefOp(Xmm_G_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Wdq)() {
  /* TODO(karl) Add dq size restriction. */
  NaClDefOp(Xmm_E_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Wpd)() {
  NaClDefOp(Xmm_E_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Wps)() {
  NaClDefOp(Xmm_E_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Wq_)() {
  /* TODO(karl) Add q size restriction. */
  NaClDefOp(Xmm_Eo_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Wsd)() {
  NaClDefOp(Xmm_E_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Wss)() {
  NaClDefOp(Xmm_E_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_NULL_OPRDS_INST(NaClInstType itype, uint8_t opbyte,
                         NaClInstPrefix prefix, NaClMnemonic inst) {
  NaClDefInstPrefix(prefix);
  NaClDefInst(opbyte, itype, NACL_EMPTY_IFLAGS, inst);
  NaClAddMiscellaneousFlags();
  NaClResetToDefaultInstPrefix();
}

void NaClDefInvModRmInst(NaClInstPrefix prefix, uint8_t opcode,
                         NaClOpKind modrm_opcode) {
  NaClDefInstPrefix(prefix);
  NaClDefInvalid(opcode);
  NaClAddIFlags(NACL_IFLAG(OpcodeInModRm));
  NaClDefOp(modrm_opcode, NACL_IFLAG(OperandExtendsOpcode));
  NaClResetToDefaultInstPrefix();
}

/* Generic macro for implementing a unary instruction function whose
 * argument is described by a (3) character type name. Defines
 * implementation for corresponding function declared by macro
 * DECLARE_UNARY_INST.
 */
#define DEFINE_UNARY_INST(XXX) \
void DEF_UNARY_INST(XXX)(NaClInstType itype, uint8_t opbyte, \
                         NaClInstPrefix prefix, NaClMnemonic inst,      \
                         NaClInstCat icat) {                            \
  NaClDefInstPrefix(prefix);                                            \
  NaClDefInst(opbyte, itype, NACL_IFLAG(OpcodeUsesModRm), inst);        \
  DEF_OPERAND(XXX)(); \
  NaClSetInstCat(icat); \
  NaClResetToDefaultInstPrefix();               \
}

/* Generic macro for implementing a unary instruction whose
 * argument is described by a (3) character type name. Defines
 * implementation for corresponding function declared by macro
 * DELARE_UNARY_OINST.
 */
#define DEFINE_UNARY_OINST(XXX) \
void DEF_USUBO_INST(XXX)(NaClInstType itype, uint8_t opbyte, \
                         NaClInstPrefix prefix, \
                         NaClOpKind modrm_opcode, \
                         NaClMnemonic inst, \
                         NaClInstCat icat) { \
  NaClDefInstPrefix(prefix); \
  NaClDefInst(opbyte, itype, NACL_IFLAG(OpcodeInModRm), inst); \
  NaClDefOp(modrm_opcode, NACL_IFLAG(OperandExtendsOpcode)); \
  DEF_OPERAND(XXX)(); \
  NaClSetInstCat(icat); \
  NaClResetToDefaultInstPrefix();             \
}

DEFINE_UNARY_OINST(Mb_)

/* Generic macro for implementing a binary instruction function whose
 * arguments are described by (3) character type names. Defines
 * implementation for corresponding function declared by macro
 * DECLARE_BINARY_INST.
 */
#define DEFINE_BINARY_INST(XXX, YYY) \
void DEF_BINST(XXX, YYY)(NaClInstType itype, uint8_t opbyte, \
                         NaClInstPrefix prefix, NaClMnemonic inst, \
                         NaClInstCat icat) { \
  NaClDefInstPrefix(prefix); \
  NaClDefInst(opbyte, itype, NACL_IFLAG(OpcodeUsesModRm), inst); \
  DEF_OPERAND(XXX)(); \
  DEF_OPERAND(YYY)(); \
  NaClSetInstCat(icat); \
  NaClResetToDefaultInstPrefix();             \
}

DEFINE_BINARY_INST(Eb_, Gb_)

DEFINE_BINARY_INST(Edq, Pd_)

DEFINE_BINARY_INST(EdQ, PdQ)

DEFINE_BINARY_INST(Edq, Pdq)

DEFINE_BINARY_INST(EdQ, VdQ)

DEFINE_BINARY_INST(Edq, Vdq)

DEFINE_BINARY_INST(Ev_, Gv_)

DEFINE_BINARY_INST(Gd_, Nq_)

DEFINE_BINARY_INST(Gd_, Udq)

DEFINE_BINARY_INST(Gd_, Upd)

DEFINE_BINARY_INST(Gd_, Ups)

DEFINE_BINARY_INST(Gdq, Wsd)

DEFINE_BINARY_INST(GdQ, Wsd)

DEFINE_BINARY_INST(Gdq, Wss)

DEFINE_BINARY_INST(GdQ, Wss)

DEFINE_BINARY_INST(Md_, Vss)

DEFINE_BINARY_INST(MdQ, GdQ)

DEFINE_BINARY_INST(Mdq, Vdq)

DEFINE_BINARY_INST(Mdq, Vpd)

DEFINE_BINARY_INST(Mdq, Vps)

DEFINE_BINARY_INST(Mpd, Vpd)

DEFINE_BINARY_INST(Mps, Vps)

DEFINE_BINARY_INST(Mq_, Pq_)

DEFINE_BINARY_INST(Mq_, Vps)

DEFINE_BINARY_INST(Mq_, Vq_)

DEFINE_BINARY_INST(Mq_, Vsd)

DEFINE_BINARY_INST(Ppi, Wpd)

DEFINE_BINARY_INST(Ppi, Wps)

DEFINE_BINARY_INST(Pq_, E__)

DEFINE_BINARY_INST(Pq_, EdQ)

DEFINE_BINARY_INST(Pq_, Nq_)

DEFINE_BINARY_INST(Pq_, Qd_)

DEFINE_BINARY_INST(Pq_, Qq_)

DEFINE_BINARY_INST(Pq_, Uq_)

DEFINE_BINARY_INST(Pq_, Wpd)

DEFINE_BINARY_INST(Pq_, Wps)

DEFINE_BINARY_INST(Qq_, Pq_)

DEFINE_BINARY_INST(Vdq, E__)

DEFINE_BINARY_INST(Vdq, Edq)

DEFINE_BINARY_INST(Vdq, EdQ)

DEFINE_BINARY_INST(Vdq, Mdq)

DEFINE_BINARY_INST(Vdq, Udq)

DEFINE_BINARY_INST(Vdq, Uq_)

DEFINE_BINARY_INST(Vdq, Wdq)

DEFINE_BINARY_INST(Vdq, Wps)

DEFINE_BINARY_INST(Vdq, Wq_)

DEFINE_BINARY_INST(Vpd, Qpi)

DEFINE_BINARY_INST(Vpd, Qq_)

DEFINE_BINARY_INST(Vpd, Wdq)

DEFINE_BINARY_INST(Vpd, Wpd)

DEFINE_BINARY_INST(Vpd, Wq_)

DEFINE_BINARY_INST(Vpd, Wsd)

DEFINE_BINARY_INST(Vps, Mq_)

DEFINE_BINARY_INST(Vps, Qpi)

DEFINE_BINARY_INST(Vps, Qq_)

DEFINE_BINARY_INST(Vps, Uq_)

DEFINE_BINARY_INST(Vps, Wpd)

DEFINE_BINARY_INST(Vps, Wps)

DEFINE_BINARY_INST(Vps, Wq_)

DEFINE_BINARY_INST(Vq_, Mq_)

DEFINE_BINARY_INST(Vq_, Wdq)

DEFINE_BINARY_INST(Vq_, Wpd)

DEFINE_BINARY_INST(Vq_, Wq_)

DEFINE_BINARY_INST(Vsd, Edq)

DEFINE_BINARY_INST(Vsd, EdQ)

DEFINE_BINARY_INST(Vsd, Mq_)

DEFINE_BINARY_INST(Vsd, Wsd)

DEFINE_BINARY_INST(Vsd, Wss)

DEFINE_BINARY_INST(Vss, Edq)

DEFINE_BINARY_INST(Vss, EdQ)

DEFINE_BINARY_INST(Vss, Wsd)

DEFINE_BINARY_INST(Vss, Wss)

DEFINE_BINARY_INST(Wdq, Vdq)

DEFINE_BINARY_INST(Wpd, Vpd)

DEFINE_BINARY_INST(Wps, Vps)

DEFINE_BINARY_INST(Wq_, Vq_)

DEFINE_BINARY_INST(Wsd, Vsd)

DEFINE_BINARY_INST(Wss, Vss)

/* Generic macro for implementing a binary instruction whose
 * arguments are described by (3) character type names. Defines
 * implementation for corresponding function declared by macro
 * DELARE_BINARY_OINST.
 */
#define DEFINE_BINARY_OINST(XXX, YYY) \
void DEF_OINST(XXX, YYY)(NaClInstType itype, uint8_t opbyte, \
                         NaClInstPrefix prefix, \
                         NaClOpKind modrm_opcode, \
                         NaClMnemonic inst, \
                         NaClInstCat icat) { \
  NaClDefInstPrefix(prefix); \
  NaClDefInst(opbyte, itype, NACL_IFLAG(OpcodeInModRm), inst); \
  NaClDefOp(modrm_opcode, NACL_OPFLAG(OperandExtendsOpcode)); \
  DEF_OPERAND(XXX)(); \
  DEF_OPERAND(YYY)(); \
  NaClSetInstCat(icat); \
  NaClResetToDefaultInstPrefix();             \
}

DEFINE_BINARY_OINST(Ev_, Ib_)

DEFINE_BINARY_OINST(Nq_, I__)

DEFINE_BINARY_OINST(Udq, I__)

DEFINE_BINARY_OINST(Vdq, I__)

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
static void NaClDefDescFatal(char* desc, const char* message) {
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

/* Given a string of byte values, describing the opcode base
 * of an instruction, extract out the corresponding opcode
 * prefix (i.e. the prefix defined by all but the last byte in
 * the opcode base.
 *
 * Note: Assumes that NaClVerifyByteBaseAssumptions was called
 * on the given parameter.
 */
NaClInstPrefix NaClExtractPrefix(const char* base) {
  size_t len = strlen(base);
  if (len == 2) {
    return NoPrefix;
  } else {
    size_t i;
    NaClInstPrefix prefix;
    char prefix_text[BUFSIZE];
    char* text_ptr;
    size_t prefix_size = len - 2;
    strcpy(prefix_text, "Prefix");
    text_ptr = prefix_text + strlen("Prefix");
    for (i = 0; i < prefix_size; ++i) {
      text_ptr[i] = toupper(base[i]);
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

/*
 * Given a string describing an opcode, the type of the
 * instruction, and the mnemonic associated with the instruction,
 * parse the opcode string and define the corresponding instruction.
 *
 * Note: See ??? for descriptions of legal opcode strings.
 */
static void NaClParseOpcode(const char* opcode,
                            NaClInstType insttype,
                            NaClMnemonic name) {
  char buffer[BUFSIZE];
  char* marker;
  const char* base = buffer;
  char* reg_offset = NULL;
  char* opcode_ext = NULL;
  NaClInstPrefix prefix;
  uint8_t opcode_value;
  strcpy(buffer, opcode);

  /* Start by finding any valid suffix (i.e. +X or /X), and remove.
   * Put the corresponding suffix into reg_offset(+), or opcode_ext(/).
   */
  marker = strchr(buffer, '+');
  if (NULL != marker) {
    *marker = '\0';
    reg_offset = buffer + strlen(base) + 1;
    if (strlen(reg_offset) != 1) {
      NaClDefFatal("register offset in opcode can only be a single digit");
    }
  } else {
    marker = strchr(buffer, '/');
    if (NULL != marker) {
      *marker = '\0';
      opcode_ext = buffer + strlen(base) + 1;
      if (strlen(opcode_ext) != 1) {
        NaClDefFatal("modrm opcode extension can only be a single digit");
      }
    }
  }

  /* Now verify the opcode string, less the suffix, is valid. If so,
   * Pull out the instruction prefix and opcode byte.
   */
  NaClVerifyByteBaseAssumptions(base);
  prefix = NaClExtractPrefix(base);
  NaClDefInstPrefix(prefix);
  opcode_value = NaClExtractByte(base + strlen(base) - 2);

  if (reg_offset != NULL) {
    int reg;
    if ((1 != strlen(reg_offset)) || (1 != strspn(reg_offset, "01234567"))) {
      NaClDefFatal("invalid opcode register offset");
    }
    reg = (int) STRTOULL(reg_offset, NULL, 10);
    NaClDefInst(opcode_value + reg, insttype, NACL_IFLAG(OpcodePlusR), name);
    NaClDefOp(OpcodeBaseMinus0 + reg, NACL_IFLAG(OperandExtendsOpcode));
  } else if (opcode_ext != NULL) {
    if (0 == strcmp("r", opcode_ext)) {
      NaClDefInst(opcode_value, insttype, NACL_IFLAG(OpcodeUsesModRm), name);
    } else {
      int ext;
      if ((1 != strlen(opcode_ext)) || (1 != strspn(opcode_ext, "01234567"))) {
        NaClDefFatal("invalid modrm opcode extension");
      }
      ext = (int) STRTOULL(opcode_ext, NULL, 10);
      NaClDefInst(opcode_value, insttype, NACL_IFLAG(OpcodeInModRm), name);
      NaClDefOp(Opcode0 + ext, NACL_OPFLAG(OperandExtendsOpcode));
    }
  } else {
    NaClDefInst(opcode_value, insttype, NACL_EMPTY_IFLAGS, name);
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
    /* TODO(karl): Complete this out as more forms are needed. */
    defop_st = NaClSymbolTableCreate(MAX_DEFOPS, NULL);
    NaClSymbolTablePutDefOp("Eb",    DEF_OPERAND(Eb_),  defop_st);
    NaClSymbolTablePutDefOp("Ev",    DEF_OPERAND(Ev_),  defop_st);
    NaClSymbolTablePutDefOp("Fvd",   DEF_OPERAND(Fvd),  defop_st);
    NaClSymbolTablePutDefOp("Fvq",   DEF_OPERAND(Fvq),  defop_st);
    NaClSymbolTablePutDefOp("Fvw",   DEF_OPERAND(Fvw),  defop_st);
    NaClSymbolTablePutDefOp("Gb",    DEF_OPERAND(Gb_),  defop_st);
    NaClSymbolTablePutDefOp("Gv",    DEF_OPERAND(Gv_),  defop_st);
    NaClSymbolTablePutDefOp("Ib",    DEF_OPERAND(Ib_),  defop_st);
    NaClSymbolTablePutDefOp("Iw",    DEF_OPERAND(Iw_),  defop_st);
    NaClSymbolTablePutDefOp("Iz",    DEF_OPERAND(Iz_),  defop_st);
    NaClSymbolTablePutDefOp("Jz",    DEF_OPERAND(Jz_),  defop_st);
    NaClSymbolTablePutDefOp("Jzd",   DEF_OPERAND(Jzd),  defop_st);
    NaClSymbolTablePutDefOp("Jzw",   DEF_OPERAND(Jzw),  defop_st);
    NaClSymbolTablePutDefOp("rAXv",  DEF_OPERAND(rAXv), defop_st);
    NaClSymbolTablePutDefOp("rBXv",  DEF_OPERAND(rBXv), defop_st);
    NaClSymbolTablePutDefOp("rCXv",  DEF_OPERAND(rCXv), defop_st);
    NaClSymbolTablePutDefOp("rDXv",  DEF_OPERAND(rDXv), defop_st);
    NaClSymbolTablePutDefOp("rSPv",  DEF_OPERAND(rSPv), defop_st);
    NaClSymbolTablePutDefOp("rBPv",  DEF_OPERAND(rBPv), defop_st);
    NaClSymbolTablePutDefOp("rSIv",  DEF_OPERAND(rSIv), defop_st);
    NaClSymbolTablePutDefOp("rDIv",  DEF_OPERAND(rDIv), defop_st);
    NaClSymbolTablePutDefOp("r8v",   DEF_OPERAND(r8v), defop_st);
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
        DEF_OPERAND(One)();
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
  NaClAddOperandFlags(arg_index, NaClExtractOperand(operand));
}

void NaClBegDef(const char* desc, NaClInstType insttype,
                struct NaClSymbolTable* st) {
  char* name;
  char* arg;
  int num_args = 0;
  char buffer[BUFSIZE];
  char expanded_desc[BUFSIZE];
  char operands_desc[BUFSIZE];
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
  operands_desc[0] = '\0';

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
  NaClParseOpcode(opcode, insttype, mnemonic);

  /* Now extract operands. */
  while ((arg = strtok(NULL, ", "))) {
    NaClParseOperand(arg, num_args);
    ++num_args;
    if (num_args == MAX_OPERANDS) {
      NaClDefFatal("Too many operands");
    }
    if (1 == num_args) {
      SNPRINTF(operands_desc, BUFSIZE, "%s", arg);
    } else {
      char* cpy_operands_desc = strdup(operands_desc);
      SNPRINTF(operands_desc, BUFSIZE, "%s, %s",
               cpy_operands_desc, arg);
    }
  }
  NaClAddOperandsDesc(operands_desc);
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
