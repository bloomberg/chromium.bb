/* Copyright (c) 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Set of predefined instruction forms (via procedure calls), providing
 * a more concise way of specifying opcodes.
 */

#include "native_client/src/trusted/validator_x86/ncdecode_forms.h"
#include "native_client/src/trusted/validator_x86/ncdecode_tablegen.h"

NaClOpFlags NaClGetDestFlags(NcInstCat icat) {
  switch (icat) {
    case Move:
    case Exchange:
      return NACL_OPFLAG(OpSet);
    case Binary:
      return NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpUse);
    case Compare:
      return NACL_OPFLAG(OpUse);
  }
  /* NOT REACHED */
  return NACL_EMPTY_OPFLAGS;
}

NaClOpFlags NaClGetSourceFlags(NcInstCat icat) {
  switch (icat) {
    case Move:
    case Binary:
    case Compare:
      return NACL_OPFLAG(OpUse);
    case Exchange:
      return NACL_OPFLAG(OpSet);
  }
  /* NOT REACHED */
  return NACL_EMPTY_OPFLAGS;
}

NaClOpFlags NaClGetIcatFlags(NcInstCat icat, int operand_index) {
  return operand_index == 1 ? NaClGetDestFlags(icat) : NaClGetSourceFlags(icat);
}

void DEF_OPERAND(Edq)(NcInstCat icat, int operand_index) {
  NaClDefOp(Edq_Operand, NaClGetIcatFlags(icat, operand_index));
}

void DEF_OPERAND(EdQ)(NcInstCat icat, int operand_index) {
  /* TODO(karl) fix measurement of size to use Rex.W */
  NaClDefOp(E_Operand, NaClGetIcatFlags(icat, operand_index));
}

void DEF_OPERAND(Gd_)(NcInstCat icat, int operand_index) {
  NaClDefOp(Gv_Operand, NaClGetIcatFlags(icat, operand_index));
}

void DEF_OPERAND(Gdq)(NcInstCat icat, int operand_index) {
  NaClDefOp(Gdq_Operand, NaClGetIcatFlags(icat, operand_index));
}

void DEF_OPERAND(I__)(NcInstCat icat, int operand_index) {
  NaClDefOp(I_Operand, NaClGetIcatFlags(icat, operand_index));
}

void DEF_OPERAND(Md_)(NcInstCat icat, int operand_index) {
  NaClDefOp(Mv_Operand, NaClGetIcatFlags(icat, operand_index));
}

void DEF_OPERAND(Mdq)(NcInstCat icat, int operand_index) {
  NaClDefOp(Mdq_Operand, NaClGetIcatFlags(icat, operand_index));
}

void DEF_OPERAND(Mpd)(NcInstCat icat, int operand_index) {
  NaClDefOp(Mdq_Operand, NaClGetIcatFlags(icat, operand_index));
}

void DEF_OPERAND(Mps)(NcInstCat icat, int operand_index) {
  NaClDefOp(Mdq_Operand, NaClGetIcatFlags(icat, operand_index));
}

void DEF_OPERAND(Mq_)(NcInstCat icat, int operand_index) {
  NaClDefOp(Mo_Operand, NaClGetIcatFlags(icat, operand_index));
}

void DEF_OPERAND(Nq_)(NcInstCat icat, int operand_index) {
  NaClDefOp(Mmx_N_Operand, NaClGetIcatFlags(icat, operand_index));
}

void DEF_OPERAND(Pd_)(NcInstCat icat, int operand_index) {
  NaClDefOp(Mmx_Gd_Operand, NaClGetIcatFlags(icat, operand_index));
}

void DEF_OPERAND(Pdq)(NcInstCat icat, int operand_index) {
  /* TODO(karl) add dq size restriction. */
  NaClDefOp(Mmx_G_Operand, NaClGetIcatFlags(icat, operand_index));
}

void DEF_OPERAND(PdQ)(NcInstCat icat, int operand_index) {
  /* TODO(karl) add d/q size restriction. */
  NaClDefOp(Mmx_G_Operand, NaClGetIcatFlags(icat, operand_index));
}

void DEF_OPERAND(Pq_)(NcInstCat icat, int operand_index) {
  /* TODO(karl) Add q size restriction. */
  NaClDefOp(Mmx_G_Operand, NaClGetIcatFlags(icat, operand_index));
}

void DEF_OPERAND(Ppi)(NcInstCat icat, int operand_index) {
  NaClDefOp(Mmx_G_Operand, NaClGetIcatFlags(icat, operand_index));
}

void DEF_OPERAND(Qd_)(NcInstCat icat, int operand_index) {
  /* TODO(karl) add d size restriction. */
  NaClDefOp(Mmx_E_Operand, NaClGetIcatFlags(icat, operand_index));
}

void DEF_OPERAND(Qpi)(NcInstCat icat, int operand_index) {
  NaClDefOp(Mmx_E_Operand, NaClGetIcatFlags(icat, operand_index));
}

void DEF_OPERAND(Qq_)(NcInstCat icat, int operand_index) {
  /* TODO(karl) add q size restriction. */
  NaClDefOp(Mmx_E_Operand, NaClGetIcatFlags(icat, operand_index));
}

void DEF_OPERAND(Udq)(NcInstCat icat, int operand_index) {
  /* TODO(karl) Add code to restrict valid values to using
   * the ModRm r/m field, when the ModRm mod field must be 0x3.
   */
  NaClDefOp(Xmm_E_Operand, NaClGetIcatFlags(icat, operand_index));
}

void DEF_OPERAND(Upd)(NcInstCat icat, int operand_index) {
  /* TODO(karl) Add code to restrict valid values to using
   * the ModRm r/m field, when the ModRm mod field must be 0x3.
   */
  NaClDefOp(Xmm_E_Operand, NaClGetIcatFlags(icat, operand_index));
}

void DEF_OPERAND(Ups)(NcInstCat icat, int operand_index) {
  /* TODO(karl) Add code to restrict valid values to using
   * the ModRm r/m field, when the ModRm mod field must be 0x3.
   */
  NaClDefOp(Xmm_E_Operand, NaClGetIcatFlags(icat, operand_index));
}

void DEF_OPERAND(Uq_)(NcInstCat icat, int operand_index) {
  /* TODO(karl) Add code to restrict valid values to using
   * the ModRm r/m field, when the ModRm mod field must be 0x3.
   */
  NaClDefOp(Xmm_E_Operand, NaClGetIcatFlags(icat, operand_index));
}

void DEF_OPERAND(Vdq)(NcInstCat icat, int operand_index) {
  /* TODO(karl) Add dq size restriction. */
  NaClDefOp(Xmm_G_Operand, NaClGetIcatFlags(icat, operand_index));
}
void DEF_OPERAND(VdQ)(NcInstCat icat, int operand_index) {
  /* TODO(karl) Add d/q size restriction. */
  NaClDefOp(Xmm_G_Operand, NaClGetIcatFlags(icat, operand_index));
}

void DEF_OPERAND(Vpd)(NcInstCat icat, int operand_index) {
  NaClDefOp(Xmm_G_Operand, NaClGetIcatFlags(icat, operand_index));
}

void DEF_OPERAND(Vps)(NcInstCat icat, int operand_index) {
  NaClDefOp(Xmm_G_Operand, NaClGetIcatFlags(icat, operand_index));
}

void DEF_OPERAND(Vq_)(NcInstCat icat, int operand_index) {
  NaClDefOp(Xmm_Go_Operand, NaClGetIcatFlags(icat, operand_index));
}

void DEF_OPERAND(Vsd)(NcInstCat icat, int operand_index) {
  NaClDefOp(Xmm_G_Operand, NaClGetIcatFlags(icat, operand_index));
}

void DEF_OPERAND(Vss)(NcInstCat icat, int operand_index) {
  NaClDefOp(Xmm_G_Operand, NaClGetIcatFlags(icat, operand_index));
}

void DEF_OPERAND(Wdq)(NcInstCat icat, int operand_index) {
  /* TODO(karl) Add dq size restriction. */
  NaClDefOp(Xmm_E_Operand, NaClGetIcatFlags(icat, operand_index));
}

void DEF_OPERAND(Wpd)(NcInstCat icat, int operand_index) {
  NaClDefOp(Xmm_E_Operand, NaClGetIcatFlags(icat, operand_index));
}

void DEF_OPERAND(Wps)(NcInstCat icat, int operand_index) {
  NaClDefOp(Xmm_E_Operand, NaClGetIcatFlags(icat, operand_index));
}

void DEF_OPERAND(Wq_)(NcInstCat icat, int operand_index) {
  /* TODO(karl) Add q size restriction. */
  NaClDefOp(Xmm_Eo_Operand, NaClGetIcatFlags(icat, operand_index));
}

void DEF_OPERAND(Wsd)(NcInstCat icat, int operand_index) {
  NaClDefOp(Xmm_E_Operand, NaClGetIcatFlags(icat, operand_index));
}

void DEF_OPERAND(Wss)(NcInstCat icat, int operand_index) {
  NaClDefOp(Xmm_E_Operand, NaClGetIcatFlags(icat, operand_index));
}

void DEF_NULL_OPRDS_INST(NaClInstType itype, uint8_t opbyte,
                         NaClInstPrefix prefix, NaClMnemonic inst) {
  NaClDefInstPrefix(prefix);
  NaClDefInst(opbyte, itype, NACL_EMPTY_IFLAGS, inst);
  NaClResetToDefaultInstPrefix();
}

/* Generic macro for implementing a binary instruction function whose
 * arguments are described by (3) character type names. Defines
 * implementation for corresponding function declared by macro
 * DECLARE_BINARY_INST.
 */
#define DEFINE_BINARY_INST(XXX, YYY) \
void DEF_BINST(XXX, YYY)(NaClInstType itype, uint8_t opbyte, \
                         NaClInstPrefix prefix, NaClMnemonic inst, \
                         NcInstCat icat) { \
  NaClDefInstPrefix(prefix); \
  NaClDefInst(opbyte, itype, NACL_IFLAG(OpcodeUsesModRm), inst); \
  DEF_OPERAND(XXX)(icat, 1); \
  DEF_OPERAND(YYY)(icat, 2); \
  NaClResetToDefaultInstPrefix();             \
}

DEFINE_BINARY_INST(Edq, Pd_)

DEFINE_BINARY_INST(EdQ, PdQ)

DEFINE_BINARY_INST(Edq, Pdq)

DEFINE_BINARY_INST(EdQ, VdQ)

DEFINE_BINARY_INST(Edq, Vdq)

DEFINE_BINARY_INST(Gd_, Nq_)

DEFINE_BINARY_INST(Gd_, Udq)

DEFINE_BINARY_INST(Gd_, Upd)

DEFINE_BINARY_INST(Gd_, Ups)

DEFINE_BINARY_INST(Gdq, Wsd)

DEFINE_BINARY_INST(Gdq, Wss)

DEFINE_BINARY_INST(Md_, Vss)

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

DEFINE_BINARY_INST(Pq_, EdQ)

DEFINE_BINARY_INST(Pq_, Nq_)

DEFINE_BINARY_INST(Pq_, Qd_)

DEFINE_BINARY_INST(Pq_, Qq_)

DEFINE_BINARY_INST(Pq_, Uq_)

DEFINE_BINARY_INST(Pq_, Wpd)

DEFINE_BINARY_INST(Pq_, Wps)

DEFINE_BINARY_INST(Qq_, Pq_)

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

DEFINE_BINARY_INST(Vps, Wpd)

DEFINE_BINARY_INST(Vps, Wps)

DEFINE_BINARY_INST(Vps, Wq_)

DEFINE_BINARY_INST(Vq_, Mq_)

DEFINE_BINARY_INST(Vq_, Wdq)

DEFINE_BINARY_INST(Vq_, Wq_)

DEFINE_BINARY_INST(Vsd, Edq)

DEFINE_BINARY_INST(Vsd, Mq_)

DEFINE_BINARY_INST(Vsd, Wsd)

DEFINE_BINARY_INST(Vsd, Wss)

DEFINE_BINARY_INST(Vss, Edq)

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
                         NcInstCat icat) { \
  NaClDefInstPrefix(prefix); \
  NaClDefInst(opbyte, itype, NACL_IFLAG(OpcodeInModRm), inst); \
  NaClDefOp(modrm_opcode, NACL_IFLAG(OperandExtendsOpcode)); \
  DEF_OPERAND(XXX)(icat, 1); \
  DEF_OPERAND(YYY)(icat, 2); \
  NaClResetToDefaultInstPrefix();             \
}

DEFINE_BINARY_OINST(Nq_, I__)

DEFINE_BINARY_OINST(Udq, I__)

DEFINE_BINARY_OINST(Vdq, I__)
