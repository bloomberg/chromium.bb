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
 * given the category of instruction.
 */
static NaClOpFlags NaClGetArg1Flags(NaClInstCat icat) {
  DEBUG(NaClLog(LOG_INFO, "NaClGetArg1Flags(%s)\n", NaClInstCatName(icat)));
  switch (icat) {
    case Move:
      return NACL_OPFLAG(OpSet);
    case UnarySet:
    case Exchange:
    case UnaryUpdate:
    case Binary:
      return NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse);
    case Compare:
      return NACL_OPFLAG(OpUse);
    case Push:
    case Pop:
      return NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpSet);
    case Jump:
      return NACL_OPFLAG(OpSet);
    case Uses:
      return NACL_OPFLAG(OpUse);
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
 */
static NaClOpFlags NaClGetArg2PlusFlags(NaClInstCat icat, int visible_index) {
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
      if (visible_index == 2) {
        return NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpDest);
      } else {
        return NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse);
      }
    case Pop:
      return NACL_OPFLAG(OpSet);
    default:
      NaClFatal("NaClGetArg2PlusFlags: unrecognized category");
      /* NOT REACHED */
      return NACL_EMPTY_OPFLAGS;
  }
}

/* Returns the operand flags for the operand with the given
 * operand_index (1-based) argument for the instruciton,  for the given
 * category. Argument visible_index is the (1-based) index of the
 * operand for which flags are being defined. A value of zero implies
 * the argument is not visible. Argument op_index is the actual index
 * of the operand in the modeled instruction.
 */
static NaClOpFlags NaClGetIcatFlags(NaClInstCat icat,
                                    int operand_index,
                                    int visible_count) {
  NaClOpFlags flags = NACL_EMPTY_OPFLAGS;
  DEBUG(NaClLog(LOG_INFO, "NaClGetIcatFlags(%s, %d, %d)\n",
                NaClInstCatName(icat), operand_index, visible_count));
  if (operand_index == 1) {
    flags = NaClGetArg1Flags(icat);
  } else {
    flags = NaClGetArg2PlusFlags(icat, visible_count);
  }
  /* Always flag the first visible argument as a (lockable) destination. */
  if ((visible_count == 1) && (flags & NACL_OPFLAG(OpSet))) {
    flags |= NACL_OPFLAG(OpDest);
  }
  return flags;
}

/* Add set/use/dest flags to all operands of the current instruction,
 * for the given instruction category.
 */
void NaClSetInstCat(NaClInstCat icat) {
  int operand_index = 0;  /* note: this is one-based. */
  int visible_count = 0;
  int i;
  int num_ops;
  NaClInst* inst = NaClGetDefInst();
  num_ops = inst->num_operands;
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
          icat, operand_index, (is_visible ? visible_count : 0)));
    }
  }
  /* Do special fixup for binary case with 3 arguments. In such
   * cases, the first argument is only a set, rather than a set/use.
   */
  if ((Binary == icat) && (4 ==  operand_index)) {
    NaClRemoveOpFlags(0, NACL_OPFLAG(OpUse));
  }
  /* Before returning, add miscellaneous flags defined elsewhere. */
  NaClLockableFlagIfApplicable();
}

void DEF_OPERAND(E__)(NaClInstCat icat, int operand_index) {
  NaClDefOp(E_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Eb_)(NaClInstCat icat, int operand_index) {
  NaClDefOp(Eb_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Edq)(NaClInstCat icat, int operand_index) {
  NaClDefOp(Edq_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(EdQ)(NaClInstCat icat, int operand_index) {
  /* TODO(karl) fix measurement of size to use Rex.W */
  NaClDefOp(E_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Ev_)(NaClInstCat icat, int operand_index) {
  NaClDefOp(E_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Gb_)(NaClInstCat icat, int operand_index) {
  NaClDefOp(Gb_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Gd_)(NaClInstCat icat, int operand_index) {
  NaClDefOp(Gv_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Gdq)(NaClInstCat icat, int operand_index) {
  NaClDefOp(Gdq_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(GdQ)(NaClInstCat icat, int operand_index) {
  /* TODO(karl) fix measurement of size to use Rex.W */
  NaClDefOp(G_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Gv_)(NaClInstCat icat, int operand_index) {
  NaClDefOp(G_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(I__)(NaClInstCat icat, int operand_index) {
  NaClDefOp(I_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Ib_)(NaClInstCat icat, int operand_index) {
  NaClDefOp(Ib_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Mb_)(NaClInstCat icat, int operand_index) {
  NaClDefOp(Mb_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Md_)(NaClInstCat icat, int operand_index) {
  NaClDefOp(Mv_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Mdq)(NaClInstCat icat, int operand_index) {
  NaClDefOp(Mdq_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(MdQ)(NaClInstCat icat, int operand_index) {
  /* TODO(karl) fix measurement of size to use Rex.W */
  NaClDefOp(M_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Mpd)(NaClInstCat icat, int operand_index) {
  NaClDefOp(Mdq_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Mps)(NaClInstCat icat, int operand_index) {
  NaClDefOp(Mdq_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Mq_)(NaClInstCat icat, int operand_index) {
  NaClDefOp(Mo_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Nq_)(NaClInstCat icat, int operand_index) {
  NaClDefOp(Mmx_N_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(ModRmModIs0x3));
}

void DEF_OPERAND(Pd_)(NaClInstCat icat, int operand_index) {
  NaClDefOp(Mmx_Gd_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Pdq)(NaClInstCat icat, int operand_index) {
  /* TODO(karl) add dq size restriction. */
  NaClDefOp(Mmx_G_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(PdQ)(NaClInstCat icat, int operand_index) {
  /* TODO(karl) add d/q size restriction. */
  NaClDefOp(Mmx_G_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Pq_)(NaClInstCat icat, int operand_index) {
  /* TODO(karl) Add q size restriction. */
  NaClDefOp(Mmx_G_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Ppi)(NaClInstCat icat, int operand_index) {
  NaClDefOp(Mmx_G_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Qd_)(NaClInstCat icat, int operand_index) {
  /* TODO(karl) add d size restriction. */
  NaClDefOp(Mmx_E_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Qpi)(NaClInstCat icat, int operand_index) {
  NaClDefOp(Mmx_E_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Qq_)(NaClInstCat icat, int operand_index) {
  /* TODO(karl) add q size restriction. */
  NaClDefOp(Mmx_E_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Udq)(NaClInstCat icat, int operand_index) {
  NaClDefOp(Xmm_E_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(ModRmModIs0x3));
}

void DEF_OPERAND(Upd)(NaClInstCat icat, int operand_index) {
  NaClDefOp(Xmm_E_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(ModRmModIs0x3));
}

void DEF_OPERAND(Ups)(NaClInstCat icat, int operand_index) {
  NaClDefOp(Xmm_E_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(ModRmModIs0x3));
}

void DEF_OPERAND(Uq_)(NaClInstCat icat, int operand_index) {
  NaClDefOp(Xmm_E_Operand, NACL_EMPTY_OPFLAGS);
  NaClAddIFlags(NACL_IFLAG(ModRmModIs0x3));
}

void DEF_OPERAND(Vdq)(NaClInstCat icat, int operand_index) {
  /* TODO(karl) Add dq size restriction. */
  NaClDefOp(Xmm_G_Operand, NACL_EMPTY_OPFLAGS);
}
void DEF_OPERAND(VdQ)(NaClInstCat icat, int operand_index) {
  /* TODO(karl) Add d/q size restriction. */
  NaClDefOp(Xmm_G_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Vpd)(NaClInstCat icat, int operand_index) {
  NaClDefOp(Xmm_G_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Vps)(NaClInstCat icat, int operand_index) {
  NaClDefOp(Xmm_G_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Vq_)(NaClInstCat icat, int operand_index) {
  NaClDefOp(Xmm_Go_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Vsd)(NaClInstCat icat, int operand_index) {
  NaClDefOp(Xmm_G_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Vss)(NaClInstCat icat, int operand_index) {
  NaClDefOp(Xmm_G_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Wdq)(NaClInstCat icat, int operand_index) {
  /* TODO(karl) Add dq size restriction. */
  NaClDefOp(Xmm_E_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Wpd)(NaClInstCat icat, int operand_index) {
  NaClDefOp(Xmm_E_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Wps)(NaClInstCat icat, int operand_index) {
  NaClDefOp(Xmm_E_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Wq_)(NaClInstCat icat, int operand_index) {
  /* TODO(karl) Add q size restriction. */
  NaClDefOp(Xmm_Eo_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Wsd)(NaClInstCat icat, int operand_index) {
  NaClDefOp(Xmm_E_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_OPERAND(Wss)(NaClInstCat icat, int operand_index) {
  NaClDefOp(Xmm_E_Operand, NACL_EMPTY_OPFLAGS);
}

void DEF_NULL_OPRDS_INST(NaClInstType itype, uint8_t opbyte,
                         NaClInstPrefix prefix, NaClMnemonic inst) {
  NaClDefInstPrefix(prefix);
  NaClDefInst(opbyte, itype, NACL_EMPTY_IFLAGS, inst);
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
  DEF_OPERAND(XXX)(icat, 1); \
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
  DEF_OPERAND(XXX)(icat, 1); \
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
  DEF_OPERAND(XXX)(icat, 1); \
  DEF_OPERAND(YYY)(icat, 2); \
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
  NaClDefOp(modrm_opcode, NACL_IFLAG(OperandExtendsOpcode)); \
  DEF_OPERAND(XXX)(icat, 1); \
  DEF_OPERAND(YYY)(icat, 2); \
  NaClSetInstCat(icat); \
  NaClResetToDefaultInstPrefix();             \
}

DEFINE_BINARY_OINST(Ev_, Ib_)

DEFINE_BINARY_OINST(Nq_, I__)

DEFINE_BINARY_OINST(Udq, I__)

DEFINE_BINARY_OINST(Vdq, I__)
