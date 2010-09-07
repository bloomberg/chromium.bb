/* Copyright (c) 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Set of predefined instruction forms (via procedure calls), providing
 * a more concise way of specifying opcodes.
 */

#include <assert.h>

#include "native_client/src/trusted/validator_x86/ncdecode_forms.h"

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator_x86/lock_insts.h"
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

/* Returns the operand flags for the destination argument of the instruction,
 * given the category of instruction. Argument num_ops is the number of
 * operands the instruction has (base 1).
 */
static NaClOpFlags NaClGetArg1Flags(NaClInstCat icat, int num_ops) {
  DEBUG(NaClLog(LOG_INFO, "NaClGetArg1Flags(%s)\n", NaClInstCatName(icat)));
  switch (icat) {
    case Move:
    case Lea:
      return NACL_OPFLAG(OpSet);
    case UnarySet:
    case Exchange:
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

/* Returns the operand flags for the source argument(s) of the instruction,
 * given the category of instruction. Argument visible_index is the
 * (1-based) index of the operand for which flags are being defined. A
 * value of zero implies the argument is not visible.
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
    case Lea:
    case Other:
      return NACL_EMPTY_OPFLAGS;
    default:
      NaClFatal("NaClGetArg2PlusFlags: unrecognized category");
      /* NOT REACHED */
      return NACL_EMPTY_OPFLAGS;
  }
}

/* Returns the operand flags for the operand with the given
 * operand_index (1-based) argument for the instruciton,  for the given
 * category.
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

void DEF_OPERAND(E__)() {
  NaClDefOp(E_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Eb_)() {
  NaClDefOp(E_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_b));
}

void DEF_OPERAND(Edq)() {
  NaClDefOp(Edq_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(EdQ)() {
  /* TODO(karl) fix measurement of size to use Rex.W */
  NaClDefOp(E_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Ev_)() {
  NaClDefOp(E_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
                NACL_IFLAG(OperandSize_o));
}

void DEF_OPERAND(Gb_)() {
  NaClDefOp(G_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_b));
}

void DEF_OPERAND(Gd_)() {
  NaClDefOp(Gv_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Gdq)() {
  NaClDefOp(Gdq_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(GdQ)() {
  NaClDefOp(G_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(OperandSize_v) | NACL_IFLAG(OperandSize_o));
}

void DEF_OPERAND(Gv_)() {
  NaClDefOp(G_Operand, NACL_EMPTY_OPFLAGS);
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
