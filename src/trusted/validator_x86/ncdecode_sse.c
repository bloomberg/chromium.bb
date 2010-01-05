/*
 * Copyright (c) 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that be
 * found in the LICENSE file.
 */

/*
 * Defines SSE instructions.
 */

#include "native_client/src/trusted/validator_x86/ncdecode_tablegen.h"

/* Note: Most (although not all) of the SSE instructions have MMX and
 * XMM variants. The MMX variant typically doesn't have a 66 prefix while
 * the XXM version has a 66 prefix. As a result, we specify data using
 * 2-element arrays, where the first element is what to do if the 66 prefix
 * is omitted, while the second element is if the 66 prefix is included.
 *
 * In addition, to allow instructions that only have one of the two variants,
 * we use the value OpcodePRefixEnumSize, for the prefix specification value
 * to denote that that version isn't defined.
 */

/* Define a generic way for computing the array size of a declared array. */
#if !defined(ARRAYSIZE)
#define ARRAYSIZE(a) sizeof(a)/sizeof(a[0])
#endif

/* Defines the general category of instruction, and is used to define
 * set/use values for instructions. That is, most X86 instructions
 * have the form:
 *
 *     OP Dest, Source
 *
 * where OP is some mnemonic name, the first argument is the DEST (because
 * side effecting operations change this value), and Source is a second (use)
 * argument.
 *
 * Reading the text associated with each instruction, one should be able to
 * categorize (most) instructions, into one of the following:
 */
typedef enum {
  Move,       /* Dest := f(Source) for some f. */
  Binary,     /* Dest := f(Dest, Source) for some f. */
  Compare,    /* Sets flag using f(Dest, Source). The value of Dest is not
                 modified.
              */
} NcInstCat;

/* Returns the operand flags for the destination argument of the instruction,
 * given the category of instruction.
 */
static OperandFlags GetDestFlags(NcInstCat icat) {
  switch (icat) {
    case Move:
      return OpFlag(OpSet);
    case Binary:
      return OpFlag(OpSet) | OpFlag(OpUse);
    case Compare:
      return OpFlag(OpUse);
  }
  /* NOT REACHED */
  return (OperandFlags) 0;
}

/* Returns the operand flags for the source argument of the instruction,
 * given the category of instruction.
 */
static OperandFlags GetSourceFlags(NcInstCat icat) {
  return OpFlag(OpUse);
}


/* Here's a slightly different strategy for how to encode some of */
/* the SSE* instructions.                                         */
static void DefineVpsWpsInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_G_Operand, GetDestFlags(icat));
  DefineOperand(Xmm_E_Operand, GetSourceFlags(icat));
}

static void DefineWpsVpsInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_E_Operand, GetDestFlags(icat));
  DefineOperand(Xmm_G_Operand, GetSourceFlags(icat));
}

static void DefineVq_Mq_Inst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_G_Operand, GetDestFlags(icat));
  DefineOperand(M_Operand, GetSourceFlags(icat));
}

static void DefineMq_Vq_Inst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(M_Operand, GetDestFlags(icat));
  DefineOperand(Xmm_G_Operand, GetSourceFlags(icat));
}

static void DefineVpsWq_Inst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_G_Operand, GetDestFlags(icat));
  DefineOperand(Xmm_E_Operand, GetSourceFlags(icat));
}

static void DefineWssVssInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_E_Operand, GetDestFlags(icat));
  DefineOperand(Xmm_G_Operand, GetSourceFlags(icat));
}

static void DefineVssWssInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_G_Operand, GetDestFlags(icat));
  DefineOperand(Xmm_E_Operand, GetSourceFlags(icat));
}

static void DefineVpdWpdInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_G_Operand, GetDestFlags(icat));
  DefineOperand(Xmm_E_Operand, GetSourceFlags(icat));
}

static void DefineWpdVpdInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_E_Operand, GetDestFlags(icat));
  DefineOperand(Xmm_G_Operand, GetSourceFlags(icat));
}

static void DefineVpdWq_Inst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_G_Operand, GetDestFlags(icat));
  DefineOperand(Xmm_E_Operand, GetSourceFlags(icat));
}

static void DefineVsdWsdInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_G_Operand, GetDestFlags(icat));
  DefineOperand(Xmm_E_Operand, GetSourceFlags(icat));
}

static void DefineWsdVsdInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_E_Operand, GetDestFlags(icat));
  DefineOperand(Xmm_G_Operand, GetSourceFlags(icat));
}

static void DefineVq_Wq_Inst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_G_Operand, GetDestFlags(icat));
  DefineOperand(Xmm_E_Operand, GetSourceFlags(icat));
}

static void DefineVpsQpiInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_G_Operand, GetDestFlags(icat));
  DefineOperand(E_Operand, GetSourceFlags(icat));
}

static void DefineMpsVpsInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(M_Operand, GetDestFlags(icat));
  DefineOperand(Xmm_G_Operand, GetSourceFlags(icat));
}

static void DefinePpiWpsInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(G_Operand, GetDestFlags(icat));
  DefineOperand(Xmm_E_Operand, GetSourceFlags(icat));
}

static void DefineVpdQpiInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_G_Operand, GetDestFlags(icat));
  DefineOperand(Mmx_E_Operand, GetSourceFlags(icat));
}

static void DefineMpdVpdInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(M_Operand, GetDestFlags(icat));
  DefineOperand(Xmm_G_Operand, GetSourceFlags(icat));
}

static void DefinePpiWpdInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Mmx_G_Operand, GetDestFlags(icat));
  DefineOperand(Xmm_E_Operand, GetSourceFlags(icat));
}

static void DefineQpiWpdInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Mmx_E_Operand, GetDestFlags(icat));
  DefineOperand(Xmm_E_Operand, GetSourceFlags(icat));
}

static void DefineVsdEdqInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_G_Operand, GetDestFlags(icat));
  DefineOperand(E_Operand, GetSourceFlags(icat));
}

static void DefineGdqWsdInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(G_Operand, GetDestFlags(icat));
  DefineOperand(Xmm_E_Operand, GetSourceFlags(icat));
}

static void DefineGdqUpsInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(G_Operand, GetDestFlags(icat));
  DefineOperand(Xmm_E_Operand, GetSourceFlags(icat));
}

static void DefineGdqUpdInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(G_Operand, GetDestFlags(icat));
  DefineOperand(Xmm_E_Operand, GetSourceFlags(icat));
}

static void DefineVdqWdqInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_G_Operand, GetDestFlags(icat));
  DefineOperand(Xmm_E_Operand, GetSourceFlags(icat));
}

static void DefineGd_UdqInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(G_Operand, GetDestFlags(icat));
  DefineOperand(Xmm_E_Operand, GetSourceFlags(icat));
}

static void DefineMdqVdqInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(M_Operand, GetDestFlags(icat));
  DefineOperand(Xmm_G_Operand, GetSourceFlags(icat));
}

static void DefineVpdWdqInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_G_Operand, GetDestFlags(icat));
  DefineOperand(Xmm_E_Operand, GetSourceFlags(icat));
}

static void DefineVdqUdqInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_G_Operand, GetDestFlags(icat));
  DefineOperand(Xmm_E_Operand, GetSourceFlags(icat));
}

static void DefineVdqMdqInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_G_Operand, GetDestFlags(icat));
  DefineOperand(M_Operand, GetSourceFlags(icat));
}


static void DefineEdqVdqInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(E_Operand, GetDestFlags(icat));
  DefineOperand(Xmm_G_Operand, GetSourceFlags(icat));
}

static void DefinePq_Qd_Inst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Mmx_G_Operand, GetDestFlags(icat));
  DefineOperand(Mmx_E_Operand, GetSourceFlags(icat));
}

static void DefinePq_Qq_Inst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Mmx_G_Operand, GetDestFlags(icat));
  DefineOperand(Mmx_E_Operand, GetSourceFlags(icat));
}

static void DefineEdqPd_Inst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(E_Operand, GetDestFlags(icat));
  DefineOperand(Mmx_G_Operand, GetSourceFlags(icat));
}

static void DefineQq_Pq_Inst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Mmx_E_Operand, GetDestFlags(icat));
  DefineOperand(Mmx_G_Operand, GetSourceFlags(icat));
}

static void DefineMq_Pq_Inst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(M_Operand, GetDestFlags(icat));
  DefineOperand(Mmx_G_Operand, GetSourceFlags(icat));
}

static void DefineGd_Nq_Inst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(G_Operand, GetDestFlags(icat));
  DefineOperand(Mmx_E_Operand, GetSourceFlags(icat));
}

static void DefinePq_Nq_Inst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Mmx_E_Operand, GetDestFlags(icat));
  DefineOperand(Mmx_G_Operand, GetSourceFlags(icat));
}

static void DefinePd_EdqInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Mmx_G_Operand, GetDestFlags(icat));
  DefineOperand(E_Operand, GetSourceFlags(icat));
}

static void DefineWdqVdqInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst,
                             NcInstCat icat) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_E_Operand, GetDestFlags(icat));
  DefineOperand(Xmm_G_Operand, GetSourceFlags(icat));
}

static void DefineBinarySseOpcodes() {
  DefineVdqWdqInst(NACLi_SSSE3, 0x08, Prefix660F38, InstPsignb, Binary);
  DefineVdqWdqInst(NACLi_SSSE3, 0x09, Prefix660F38, InstPsignw, Binary);
  DefineVdqWdqInst(NACLi_SSSE3, 0x0A, Prefix660F38, InstPsignd, Binary);

  DefineVpsWpsInst(NACLi_SSE, 0x10, Prefix0F, InstMovups, Move);
  DefineWpsVpsInst(NACLi_SSE, 0x11, Prefix0F, InstMovups, Move);
  DefineVq_Mq_Inst(NACLi_SSE, 0x12, Prefix0F, InstMovlps, Move);
  DefineMq_Vq_Inst(NACLi_SSE, 0x13, Prefix0F, InstMovlps, Move);
  DefineVpsWq_Inst(NACLi_SSE, 0x14, Prefix0F, InstUnpcklps, Binary);
  DefineVpsWq_Inst(NACLi_SSE, 0x15, Prefix0F, InstUnpckhps, Binary);
  DefineVq_Mq_Inst(NACLi_SSE, 0x16, Prefix0F, InstMovhps, Move);
  DefineMq_Vq_Inst(NACLi_SSE, 0x17, Prefix0F, InstMovhps, Move);
  DefineVssWssInst(NACLi_SSE, 0x10, PrefixF30F, InstMovss, Move);
  DefineWssVssInst(NACLi_SSE, 0x11, PrefixF30F, InstMovss, Move);

  DefineVpdWpdInst(NACLi_SSE2, 0x10, Prefix660F, InstMovupd, Move);
  DefineWpdVpdInst(NACLi_SSE2, 0x11, Prefix660F, InstMovupd, Move);
  DefineVq_Mq_Inst(NACLi_SSE2, 0x12, Prefix660F, InstMovlpd, Move);
  DefineMq_Vq_Inst(NACLi_SSE2, 0x13, Prefix660F, InstMovlpd, Move);
  DefineVpdWq_Inst(NACLi_SSE2, 0x14, Prefix660F, InstUnpcklpd, Binary);
  DefineVpdWq_Inst(NACLi_SSE2, 0x15, Prefix660F, InstUnpckhpd, Binary);
  DefineVq_Mq_Inst(NACLi_SSE2, 0x16, Prefix660F, InstMovhpd, Move);
  DefineMq_Vq_Inst(NACLi_SSE2, 0x17, Prefix660F, InstMovhpd, Move);

  DefineVsdWsdInst(NACLi_SSE2, 0x10, PrefixF20F, InstMovsd, Move);
  DefineWsdVsdInst(NACLi_SSE2, 0x11, PrefixF20F, InstMovsd, Move);

  DefineVq_Wq_Inst(NACLi_SSE3, 0x12, PrefixF20F, InstMovddup, Move);
  DefineVq_Wq_Inst(NACLi_SSE3, 0x12, PrefixF30F, InstMovsldup, Move);
  DefineVq_Wq_Inst(NACLi_SSE3, 0x16, PrefixF30F, InstMovshdup, Move);

  DefineVpsWpsInst(NACLi_SSE, 0x28, Prefix0F, InstMovaps, Move);
  DefineWpsVpsInst(NACLi_SSE, 0x29, Prefix0F, InstMovaps, Move);
  DefineVpsQpiInst(NACLi_SSE, 0x2a, Prefix0F, InstCvtpi2ps, Move);
  DefineMpsVpsInst(NACLi_SSE, 0x2b, Prefix0F, InstMovntps, Move);
  DefinePpiWpsInst(NACLi_SSE, 0x2c, Prefix0F, InstCvttps2pi, Move);
  DefinePpiWpsInst(NACLi_SSE, 0x2d, Prefix0F, InstCvtps2pi, Move);
  DefineVssWssInst(NACLi_SSE, 0x2e, Prefix0F, InstUcomiss, Compare);
  DefineVssWssInst(NACLi_SSE, 0x2f, Prefix0F, InstComiss, Compare);

  DefineVpsQpiInst(NACLi_SSE, 0x2a, PrefixF30F, InstCvtsi2ss, Move);
  DefineMpsVpsInst(NACLi_SSE4A, 0x2b, PrefixF30F, InstMovntss, Move);
  DefinePpiWpsInst(NACLi_SSE, 0x2c, PrefixF30F, InstCvttss2si, Move);
  DefinePpiWpsInst(NACLi_SSE, 0x2d, PrefixF30F, InstCvtss2si, Move);

  DefineVpdWpdInst(NACLi_SSE2, 0x28, Prefix660F, InstMovapd, Move);
  DefineWpdVpdInst(NACLi_SSE2, 0x29, Prefix660F, InstMovapd, Move);
  DefineVpdQpiInst(NACLi_SSE2, 0x2a, Prefix660F, InstCvtpi2pd, Move);
  DefineMpdVpdInst(NACLi_SSE2, 0x2b, Prefix660F, InstMovntpd, Move);
  DefinePpiWpdInst(NACLi_SSE2, 0x2c, Prefix660F, InstCvttpd2pi, Move);
  DefineQpiWpdInst(NACLi_SSE2, 0x2d, Prefix660F, InstCvtpd2pi, Move);
  DefineVsdWsdInst(NACLi_SSE2, 0x2e, Prefix660F, InstUcomisd, Compare);
  DefineVsdWsdInst(NACLi_SSE2, 0x2f, Prefix660F, InstComisd, Compare);

  DefineVsdEdqInst(NACLi_SSE2, 0x2a, PrefixF20F, InstCvtsi2sd, Move);
  DefineGdqWsdInst(NACLi_SSE2, 0x2c, PrefixF20F, InstCvttsd2si, Move);
  DefineGdqWsdInst(NACLi_SSE2, 0x2d, PrefixF20F, InstCvtsd2si, Move);

  DefineGdqUpsInst(NACLi_SSE, 0x50, Prefix0F, InstMovmskps, Move);
  DefineVpsWpsInst(NACLi_SSE, 0x51, Prefix0F, InstSqrtps, Move);
  DefineVpsWpsInst(NACLi_SSE, 0x52, Prefix0F, InstRsqrtps, Move);
  DefineVpsWpsInst(NACLi_SSE, 0x53, Prefix0F, InstRcpps, Move);
  DefineVpsWpsInst(NACLi_SSE, 0x54, Prefix0F, InstAndps, Binary);
  DefineVpsWpsInst(NACLi_SSE, 0x55, Prefix0F, InstAndnps, Binary);
  DefineVpsWpsInst(NACLi_SSE, 0x56, Prefix0F, InstOrps, Binary);
  DefineVpsWpsInst(NACLi_SSE, 0x57, Prefix0F, InstXorps, Binary);

  DefineGdqUpdInst(NACLi_SSE2, 0x50, Prefix660F, InstMovmskpd, Move);
  DefineVpdWpdInst(NACLi_SSE2, 0x51, Prefix660F, InstSqrtpd, Move);
  DefineVpdWpdInst(NACLi_SSE2, 0x54, Prefix660F, InstAndpd, Binary);
  DefineVpdWpdInst(NACLi_SSE2, 0x55, Prefix660F, InstAndnpd, Binary);
  DefineVpdWpdInst(NACLi_SSE2, 0x56, Prefix660F, InstOrpd, Binary);
  DefineVpdWpdInst(NACLi_SSE2, 0x57, Prefix660F, InstXorpd, Binary);

  DefineVsdWsdInst(NACLi_SSE2, 0x51, PrefixF20F, InstSqrtsd, Move);

  DefineVssWssInst(NACLi_SSE, 0x51, PrefixF30F, InstSqrtss, Move);
  DefineVssWssInst(NACLi_SSE, 0x52, PrefixF30F, InstRsqrtps, Move);
  DefineVssWssInst(NACLi_SSE, 0x53, PrefixF30F, InstRcpps, Move);

  DefineVpsWpsInst(NACLi_SSE, 0x58, Prefix0F, InstAddps, Binary);
  DefineVpsWpsInst(NACLi_SSE, 0x59, Prefix0F, InstMulps, Binary);
  DefineVpsWpsInst(NACLi_SSE2, 0x5a, Prefix0F, InstCvtps2pd, Move);
  DefineVpsWpsInst(NACLi_SSE2, 0x5b, Prefix0F, InstCvtdq2ps, Move);
  DefineVpsWpsInst(NACLi_SSE, 0x5c, Prefix0F, InstSubps, Binary);
  DefineVpsWpsInst(NACLi_SSE, 0x5d, Prefix0F, InstMinps, Binary);
  DefineVpsWpsInst(NACLi_SSE, 0x5e, Prefix0F, InstDivps, Binary);
  DefineVpsWpsInst(NACLi_SSE, 0x5f, Prefix0F, InstMaxps, Binary);

  DefineVssWssInst(NACLi_SSE, 0x58, PrefixF30F, InstAddss, Binary);
  DefineVssWssInst(NACLi_SSE, 0x59, PrefixF30F, InstMulss, Binary);
  DefineVssWssInst(NACLi_SSE2, 0x5a, PrefixF30F, InstCvtss2sd, Move);
  DefineVssWssInst(NACLi_SSE2, 0x5b, PrefixF30F, InstCvttps2dq, Move);
  DefineVssWssInst(NACLi_SSE, 0x5c, PrefixF30F, InstSubss, Binary);
  DefineVssWssInst(NACLi_SSE, 0x5d, PrefixF30F, InstMinss, Binary);
  DefineVssWssInst(NACLi_SSE, 0x5e, PrefixF30F, InstDivss, Binary);
  DefineVssWssInst(NACLi_SSE, 0x5f, PrefixF30F, InstMaxss, Binary);

  DefineVpdWpdInst(NACLi_SSE2, 0x58, Prefix660F, InstAddpd, Binary);
  DefineVpdWpdInst(NACLi_SSE2, 0x59, Prefix660F, InstMulpd, Binary);
  DefineVpdWpdInst(NACLi_SSE2, 0x5a, Prefix660F, InstCvtss2sd, Move);
  DefineVpdWpdInst(NACLi_SSE2, 0x5b, Prefix660F, InstCvtps2dq, Move);
  DefineVpdWpdInst(NACLi_SSE2, 0x5c, Prefix660F, InstSubpd, Binary);
  DefineVpdWpdInst(NACLi_SSE2, 0x5d, Prefix660F, InstMinpd, Binary);
  DefineVpdWpdInst(NACLi_SSE2, 0x5e, Prefix660F, InstDivpd, Binary);
  DefineVpdWpdInst(NACLi_SSE2, 0x5f, Prefix660F, InstMaxpd, Binary);

  DefineVsdWsdInst(NACLi_SSE2, 0x58, PrefixF20F, InstAddsd, Binary);
  DefineVsdWsdInst(NACLi_SSE2, 0x59, PrefixF20F, InstMulsd, Binary);
  DefineVssWssInst(NACLi_SSE2, 0x5a, PrefixF20F, InstCvtsd2ss, Move);
  /* hole at 5D */
  DefineVsdWsdInst(NACLi_SSE2, 0x5c, PrefixF20F, InstSubsd, Binary);
  DefineVsdWsdInst(NACLi_SSE2, 0x5d, PrefixF20F, InstMinsd, Binary);
  DefineVsdWsdInst(NACLi_SSE2, 0x5e, PrefixF20F, InstDivsd, Binary);
  DefineVsdWsdInst(NACLi_SSE2, 0x5f, PrefixF20F, InstMaxsd, Binary);

  /* pshufw is done below */

  DefineVdqWdqInst(NACLi_SSE2, 0x60, Prefix660F, InstPunpcklbw, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0x61, Prefix660F, InstPunpcklwd, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0x62, Prefix660F, InstPunpckldq, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0x63, Prefix660F, InstPacksswb, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0x64, Prefix660F, InstPcmpgtb, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0x65, Prefix660F, InstPcmpgtw, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0x66, Prefix660F, InstPcmpgtd, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0x67, Prefix660F, InstPackuswb, Binary);

  /* a bunch of MMX instructions at 0x68-0x6f with no prefix */
  DefineVdqWdqInst(NACLi_SSE2, 0x6f, PrefixF30F, InstMovdqu, Move);
  DefineVdqWdqInst(NACLi_SSE2, 0x68, Prefix660F, InstPunpckhbw, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0x69, Prefix660F, InstPunpckhwd, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0x6a, Prefix660F, InstPunpckhdq, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0x6b, Prefix660F, InstPackssdw, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0x6c, Prefix660F, InstPunpcklqdq, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0x6d, Prefix660F, InstPunpckhqdq, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0x6e, Prefix660F, InstMovd, Move);
  DefineVdqWdqInst(NACLi_SSE2, 0x6f, Prefix660F, InstMovdqa, Move);

  DefineVdqWdqInst(NACLi_SSE2, 0x74, Prefix660F, InstPcmpeqb, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0x75, Prefix660F, InstPcmpeqw, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0x76, Prefix660F, InstPcmpeqd, Binary);

  DefineVpdWpdInst(NACLi_SSE2, 0x7c, Prefix660F, InstHaddpd, Binary);
  DefineVpdWpdInst(NACLi_SSE2, 0x7d, Prefix660F, InstHsubpd, Binary);
  DefineVpsWpsInst(NACLi_SSE3, 0x7c, PrefixF20F, InstHaddps, Binary);
  DefineVpsWpsInst(NACLi_SSE3, 0x7d, PrefixF20F, InstHsubps, Binary);
  /* 0x7e and 0x7f are for some move opcodes that are done elsewhere */
  DefineEdqVdqInst(NACLi_SSE2, 0x7e, Prefix660F, InstMovd, Move);
  DefineVq_Wq_Inst(NACLi_SSE2, 0x7e, PrefixF30F, InstMovq, Move);
  DefineWdqVdqInst(NACLi_SSE2, 0x7f, Prefix660F, InstMovdqa, Move);
  DefineWdqVdqInst(NACLi_SSE2, 0x7f, PrefixF30F, InstMovdqu, Move);

  DefineVpsWpsInst(NACLi_SSE3, 0xd0, PrefixF20F, InstAddsubps, Binary);
  DefineVpdWpdInst(NACLi_SSE3, 0xd0, Prefix660F, InstAddsubpd, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0xd1, Prefix660F, InstPsrlw, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0xd2, Prefix660F, InstPsrld, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0xd3, Prefix660F, InstPsrlq, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0xd4, Prefix660F, InstPaddq, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0xd5, Prefix660F, InstPmullw, Binary);
  /* 0xd6 - movq */
  DefineGd_UdqInst(NACLi_SSE2, 0xd7, Prefix660F, InstPmovmskb, Move);

  DefineVdqWdqInst(NACLi_SSE2, 0xd8, Prefix660F, InstPsubusb, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0xd9, Prefix660F, InstPsubusw, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0xda, Prefix660F, InstPminub, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0xdb, Prefix660F, InstPand, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0xdc, Prefix660F, InstPaddusb, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0xdd, Prefix660F, InstPaddusw, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0xde, Prefix660F, InstPmaxub, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0xdf, Prefix660F, InstPandn, Binary);

  DefineVdqWdqInst(NACLi_SSE2, 0xe0, Prefix660F, InstPavgb, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0xe1, Prefix660F, InstPsraw, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0xe2, Prefix660F, InstPsrad, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0xe3, Prefix660F, InstPavgw, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0xe4, Prefix660F, InstPmulhuw, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0xe5, Prefix660F, InstPmulhw, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0xe6, Prefix660F, InstCvttpd2dq, Move);
  DefineMdqVdqInst(NACLi_SSE2, 0xe7, Prefix660F, InstMovntdq, Move);
  DefineVpdWdqInst(NACLi_SSE2, 0xe6, PrefixF30F, InstCvttdq2pd, Move);

  DefineVdqWdqInst(NACLi_SSE2, 0xe8, Prefix660F, InstPsubsb, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0xe9, Prefix660F, InstPsubsw, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0xea, Prefix660F, InstPminsw, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0xeb, Prefix660F, InstPor, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0xec, Prefix660F, InstPaddsb, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0xed, Prefix660F, InstPaddsw, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0xee, Prefix660F, InstPmaxsw, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0xef, Prefix660F, InstPxor, Binary);

  DefineVdqWdqInst(NACLi_SSE2, 0xf1, Prefix660F, InstPsllw, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0xf2, Prefix660F, InstPslld, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0xf3, Prefix660F, InstPsllq, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0xf4, Prefix660F, InstPmuludq, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0xf5, Prefix660F, InstPmaddwd, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0xf6, Prefix660F, InstPsadbw, Binary);
  /* TODO(karl) fix; Not modeled correctly, target is DS:EDI for maskmovdqu */
  DefineVdqUdqInst(NACLi_SSE2, 0xf7, Prefix660F, InstMaskmovdqu, Compare);

  DefineVdqMdqInst(NACLi_SSE3, 0xf0, PrefixF20F, InstLddqu, Move);

  DefineVdqWdqInst(NACLi_SSE2, 0xf8, Prefix660F, InstPsubb, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0xf9, Prefix660F, InstPsubw, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0xfa, Prefix660F, InstPsubd, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0xfb, Prefix660F, InstPsubq, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0xfc, Prefix660F, InstPaddb, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0xfd, Prefix660F, InstPaddw, Binary);
  DefineVdqWdqInst(NACLi_SSE2, 0xfe, Prefix660F, InstPaddd, Binary);
}

static void DefineMmxOpcodes() {

  DefinePq_Qq_Inst(NACLi_SSSE3, 0x08, Prefix0F38, InstPsignb, Binary);
  DefinePq_Qq_Inst(NACLi_SSSE3, 0x09, Prefix0F38, InstPsignw, Binary);
  DefinePq_Qq_Inst(NACLi_SSSE3, 0x0A, Prefix0F38, InstPsignd, Binary);

  /*  0x77 NACLi_MMX Prefix0f InstEmms */

  DefinePq_Qd_Inst(NACLi_MMX, 0x60, Prefix0F, InstPunpcklbw, Binary);
  DefinePq_Qd_Inst(NACLi_MMX, 0x61, Prefix0F, InstPunpcklwd, Binary);
  DefinePq_Qd_Inst(NACLi_MMX, 0x62, Prefix0F, InstPunpckldq, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0x63, Prefix0F, InstPacksswb, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0x64, Prefix0F, InstPcmpgtb, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0x65, Prefix0F, InstPcmpgtw, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0x66, Prefix0F, InstPcmpgtd, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0x67, Prefix0F, InstPackuswb, Binary);

  DefinePq_Qd_Inst(NACLi_MMX, 0x68, Prefix0F, InstPunpckhbw, Binary);
  DefinePq_Qd_Inst(NACLi_MMX, 0x69, Prefix0F, InstPunpckhwd, Binary);
  DefinePq_Qd_Inst(NACLi_MMX, 0x6a, Prefix0F, InstPunpckhdq, Binary);
  DefinePq_Qd_Inst(NACLi_MMX, 0x6b, Prefix0F, InstPackssdw, Binary);
  DefinePd_EdqInst(NACLi_MMX, 0x6e, Prefix0F, InstMovd, Move);
  DefinePq_Qq_Inst(NACLi_MMX, 0x6f, Prefix0F, InstMovq, Move);

  DefinePq_Qq_Inst(NACLi_MMX, 0x74, Prefix0F, InstPcmpeqb, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0x75, Prefix0F, InstPcmpeqw, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0x76, Prefix0F, InstPcmpeqd, Binary);

  DefineEdqPd_Inst(NACLi_MMX, 0x7e, Prefix0F, InstMovd, Move);
  DefineQq_Pq_Inst(NACLi_MMX, 0x7f, Prefix0F, InstMovq, Move);

  DefinePq_Qq_Inst(NACLi_MMX, 0xd1, Prefix0F, InstPsrlw, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0xd2, Prefix0F, InstPsrld, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0xd3, Prefix0F, InstPsrlq, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0xd4, Prefix0F, InstPaddq, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0xd5, Prefix0F, InstPmullw, Binary);
  DefineGd_Nq_Inst(NACLi_MMX, 0xd7, Prefix0F, InstPmovmskb, Move);

  DefinePq_Qq_Inst(NACLi_MMX, 0xd8, Prefix0F, InstPsubusb, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0xd9, Prefix0F, InstPsubusw, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0xda, Prefix0F, InstPminub, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0xdb, Prefix0F, InstPand, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0xdc, Prefix0F, InstPaddusb, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0xdd, Prefix0F, InstPaddusw, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0xde, Prefix0F, InstPmaxub, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0xdf, Prefix0F, InstPandn, Binary);

  DefinePq_Qq_Inst(NACLi_MMX, 0xe0, Prefix0F, InstPavgb, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0xe1, Prefix0F, InstPsraw, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0xe2, Prefix0F, InstPsrad, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0xe3, Prefix0F, InstPavgw, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0xe4, Prefix0F, InstPmulhuw, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0xe5, Prefix0F, InstPmulhw, Binary);
  DefineMq_Pq_Inst(NACLi_MMX, 0xe7, Prefix0F, InstMovntq, Move);

  DefinePq_Qq_Inst(NACLi_MMX, 0xe8, Prefix0F, InstPsubsb, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0xe9, Prefix0F, InstPsubsw, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0xea, Prefix0F, InstPminsw, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0xeb, Prefix0F, InstPor, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0xec, Prefix0F, InstPaddwb, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0xed, Prefix0F, InstPaddsw, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0xee, Prefix0F, InstPmaxsw, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0xef, Prefix0F, InstPxor, Binary);

  DefinePq_Qq_Inst(NACLi_MMX, 0xf1, Prefix0F, InstPsllw, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0xf2, Prefix0F, InstPslld, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0xf3, Prefix0F, InstPsllq, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0xf4, Prefix0F, InstPsmuludq, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0xf5, Prefix0F, InstPmaddwd, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0xf6, Prefix0F, InstPsadbw, Binary);
  /* TODO(karl) fix; Not modeled correctly, target is DS:EDI for maskmovdqu */
  DefinePq_Nq_Inst(NACLi_MMX, 0xf7, Prefix0F, InstMaskmovq, Compare);

  DefinePq_Qq_Inst(NACLi_MMX, 0xf8, Prefix0F, InstPsubb, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0xf9, Prefix0F, InstPsubw, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0xfa, Prefix0F, InstPsubd, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0xfb, Prefix0F, InstPsubq, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0xfc, Prefix0F, InstPaddb, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0xfd, Prefix0F, InstPaddw, Binary);
  DefinePq_Qq_Inst(NACLi_MMX, 0xfe, Prefix0F, InstPaddd, Binary);
}

static void DefineNarySseOpcodes() {

  /* TODO(karl) - Check what other instructions should be using g_OperandSize_v
   * and g_OpreandSize_o to check sizes (using new flag OperandSizeIgnore66 to
   * help differentiate sizes).
   */

  /* Define other forms of MMX and XMM operations. */

  DefineOpcodePrefix(Prefix660F3A);
  DefineOpcode(0x61,
               NACLi_SSE42,
               InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeHasImmed_b) |
               InstFlag(OperandSize_w) | InstFlag(OperandSize_v),
               InstPcmpestri);
  DefineOperand(RegECX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegEAX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegEDX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(Xmm_G_Operand, OpFlag(OpUse));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix660F3A);
  DefineOpcode(0x61,
               NACLi_SSE42,
               InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeHasImmed_b) |
               InstFlag(OperandSize_o) | InstFlag(OpcodeUsesRexW),
               InstPcmpestri);
  DefineOperand(RegRCX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegRAX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegRDX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(Xmm_G_Operand, OpFlag(OpUse));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix660F3A);
  DefineOpcode(0x60,
               NACLi_SSE42,
               InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeHasImmed_b) |
               InstFlag(OperandSize_w) | InstFlag(OperandSize_v),
               InstPcmpestrm);
  DefineOperand(RegXMM0, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegEAX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegEDX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(Xmm_G_Operand, OpFlag(OpUse));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix660F3A);
  DefineOpcode(0x60,
               NACLi_SSE42,
               InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeHasImmed_b) |
               InstFlag(OperandSize_o) | InstFlag(OpcodeUsesRexW),
               InstPcmpestrm);
  DefineOperand(RegXMM0, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegRAX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(RegRDX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(Xmm_G_Operand, OpFlag(OpUse));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix660F3A);
  DefineOpcode(0x63,
               NACLi_SSE42,
               InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeHasImmed_b)|
               InstFlag(OperandSize_w) | InstFlag(OperandSize_v),
               InstPcmpistri);
  DefineOperand(RegECX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(Xmm_G_Operand, OpFlag(OpUse));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix660F3A);
  DefineOpcode(0x63,
               NACLi_SSE42,
               InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeHasImmed_b)|
               InstFlag(OperandSize_o) | InstFlag(OpcodeUsesRexW),
               InstPcmpistri);
  DefineOperand(RegRCX, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(Xmm_G_Operand, OpFlag(OpUse));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix660F3A);
  DefineOpcode(0x62,
               NACLi_SSE42,
               InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeHasImmed_b),
               InstPcmpistrm);
  DefineOperand(RegXMM0, OpFlag(OpSet) | OpFlag(OpImplicit));
  DefineOperand(Xmm_G_Operand, OpFlag(OpUse));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix660F3A);
  DefineOpcode(0x15,
               NACLi_SSE41,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_b) |
               InstFlag(OpcodeHasImmed),
               InstPextrb);
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OperandZeroExtends_v));
  DefineOperand(Xmm_G_Operand, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix660F3A);
  DefineOpcode(0x16,
               NACLi_SSE41,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_v) |
               InstFlag(OpcodeHasImmed_b),
               InstPextrd);
  DefineOperand(E_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_G_Operand, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix660F3A);
  DefineOpcode(0x16,
               NACLi_SSE41,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_o) |
               InstFlag(OpcodeHasImmed_b) | InstFlag(OpcodeUsesRexW),
               InstPextrq);
  DefineOperand(E_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_G_Operand, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix0F);
  DefineOpcode(0xC5,
               NACLi_SSE41,
               InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeHasImmed_b),
               InstPextrw);
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OperandZeroExtends_v));
  DefineOperand(Mmx_G_Operand, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix660F);
  DefineOpcode(0xC5,
               NACLi_SSE41,
               InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeHasImmed_b),
               InstPextrw);
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OperandZeroExtends_v));
  DefineOperand(Xmm_G_Operand, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix660F3A);
  DefineOpcode(0x15,
               NACLi_SSE41,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_w) |
               InstFlag(OpcodeHasImmed_b),
               InstPextrw);
  DefineOperand(E_Operand, OpFlag(OpSet) | OpFlag(OperandZeroExtends_v));
  DefineOperand(Xmm_G_Operand, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix660F3A);
  DefineOpcode(0x20,
               NACLi_SSE41,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_b) |
               InstFlag(OpcodeHasImmed),
               InstPinsrb);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(E_Operand, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix660F3A);
  DefineOpcode(0x22,
               NACLi_SSE41,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_v) |
               InstFlag(OpcodeHasImmed_b),
               InstPinsrd);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(E_Operand, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix660F3A);
  DefineOpcode(0x22,
               NACLi_SSE41,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_o) |
               InstFlag(OpcodeHasImmed_b) | InstFlag(OpcodeUsesRexW),
               InstPinsrq);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(E_Operand, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix0F);
  DefineOpcode(0xC4,
               NACLi_SSE,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_w) |
               InstFlag(OpcodeHasImmed_b),
               InstPinsrw);
  DefineOperand(Mmx_G_Operand, OpFlag(OpSet));
  DefineOperand(E_Operand, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix660F);
  DefineOpcode(0xC4,
               NACLi_SSE,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_w) |
               InstFlag(OpcodeHasImmed_b),
               InstPinsrw);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(E_Operand, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix660F);
  DefineOpcode(0x70,
               NACLi_SSE2,
               InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeHasImmed_b),
               InstPshufd);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(PrefixF30F);
  DefineOpcode(0x70,
               NACLi_SSE2,
               InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeHasImmed_b),
               InstPshufhw);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(PrefixF20F);
  DefineOpcode(0x70,
               NACLi_SSE2,
               InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeHasImmed_b),
               InstPshuflw);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix0F);
  DefineOpcode(0x70,
               NACLi_SSE,
               InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeHasImmed_b),
               InstPshufw);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  /* Note: The Intel manual claims that the first operand is an
   * Mmx_G_Operand. However, this is not possible, since Opcode 6
   * is in the same corresponding bits of the ModRm byte. Test runs
   * of xed and objdump appear to match that the field should be
   * the effective address (Mmx_E_Operand) instead. -- Karl
   */
  DefineOpcodePrefix(Prefix0F);
  DefineOpcode(0x71,
               NACLi_MMX,
               InstFlag(OpcodeInModRm) | InstFlag(OpcodeHasImmed_b),
               InstPsllw);
  DefineOperand(Opcode6, OpFlag(OperandExtendsOpcode));
  DefineOperand(Mmx_E_Operand, OpFlag(OpSet) | OpFlag(OpUse)); /* G? */
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix660F);
  DefineOpcode(0x71,
               NACLi_SSE3,
               InstFlag(OpcodeInModRm) | InstFlag(OpcodeHasImmed_b),
               InstPsllw);
  DefineOperand(Opcode6, OpFlag(OperandExtendsOpcode));
  DefineOperand(Xmm_E_Operand, OpFlag(OpSet) | OpFlag(OpUse)); /* G? */
  DefineOperand(I_Operand, OpFlag(OpUse));

  /* Note: The Intel manual claims that the first operand is an
   * Mmx_G_Operand. However, this is not possible, since Opcode 6
   * is in the same corresponding bits of the ModRm byte. Test runs
   * of xed and objdump appear to match that the field should be
   * the effective address (Mmx_E_Operand) instead. -- Karl
   */
  DefineOpcodePrefix(Prefix0F);
  DefineOpcode(0x72,
               NACLi_MMX,
               InstFlag(OpcodeInModRm) | InstFlag(OpcodeHasImmed_b),
               InstPslld);
  DefineOperand(Opcode6, OpFlag(OperandExtendsOpcode));
  DefineOperand(Mmx_E_Operand, OpFlag(OpSet) | OpFlag(OpUse)); /* G? */
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix660F);
  DefineOpcode(0x72,
               NACLi_SSE2,
               InstFlag(OpcodeInModRm) | InstFlag(OpcodeHasImmed_b),
               InstPslld);
  DefineOperand(Opcode6, OpFlag(OperandExtendsOpcode));
  DefineOperand(Xmm_E_Operand, OpFlag(OpSet) | OpFlag(OpUse)); /* G? */
  DefineOperand(I_Operand, OpFlag(OpUse));

  /* Note: The Intel manual claims that the first operand is an
   * Mmx_G_Operand. However, this is not possible, since Opcode 6
   * is in the same corresponding bits of the ModRm byte. Test runs
   * of xed and objdump appear to match that the field should be
   * the effective address (Mmx_E_Operand) instead. -- Karl
   */
  DefineOpcodePrefix(Prefix0F);
  DefineOpcode(0x73,
               NACLi_MMX,
               InstFlag(OpcodeInModRm) | InstFlag(OpcodeHasImmed_b),
               InstPsllq);
  DefineOperand(Opcode6, OpFlag(OperandExtendsOpcode));
  DefineOperand(Mmx_E_Operand, OpFlag(OpSet) | OpFlag(OpUse)); /* G? */
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix660F);
  DefineOpcode(0x73,
               NACLi_SSE2,
               InstFlag(OpcodeInModRm) | InstFlag(OpcodeHasImmed_b),
               InstPsllq);
  DefineOperand(Opcode6, OpFlag(OperandExtendsOpcode));
  DefineOperand(Xmm_E_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  /* Note: The Intel manual claims that the first operand is an
   * Mmx_G_Operand. However, this is not possible, since Opcode 7
   * is in the same corresponding bits of the ModRm byte. Test runs
   * of xed and objdump appear to match that the field should be
   * the effective address (Mmx_E_Operand) instead. -- Karl
   */
  DefineOpcodePrefix(Prefix660F);
  DefineOpcode(0x73,
               NACLi_SSE2x,
               InstFlag(OpcodeInModRm) | InstFlag(OpcodeHasImmed_b),
               InstPslldq);
  DefineOperand(Opcode7, OpFlag(OperandExtendsOpcode));
  DefineOperand(Xmm_E_Operand, OpFlag(OpSet) | OpFlag(OpUse)); /* G? */
  DefineOperand(I_Operand, OpFlag(OpUse));

  /* Note: The Intel manual claims that the first operand is an
   * Mmx_G_Operand. However, this is not possible, since Opcode 4
   * is in the same corresponding bits of the ModRm byte. Test runs
   * of xed and objdump appear to match that the field should be
   * the effective address (Mmx_E_Operand) instead. -- Karl
   */
  DefineOpcodePrefix(Prefix0F);
  DefineOpcode(0x71,
               NACLi_MMX,
               InstFlag(OpcodeInModRm) | InstFlag(OpcodeHasImmed_b),
               InstPsraw);
  DefineOperand(Opcode4, OpFlag(OperandExtendsOpcode));
  DefineOperand(Mmx_E_Operand, OpFlag(OpUse) | OpFlag(OpSet)); /* G? */
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix660F);
  DefineOpcode(0x71,
               NACLi_SSE2,
               InstFlag(OpcodeInModRm) | InstFlag(OpcodeHasImmed_b),
               InstPsraw);
  DefineOperand(Opcode4, OpFlag(OperandExtendsOpcode));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse) | OpFlag(OpSet)); /* G? */
  DefineOperand(I_Operand, OpFlag(OpUse));

  /* Note: The Intel manual claims that the first operand is an
   * Mmx_G_Operand. However, this is not possible, since Opcode 4
   * is in the same corresponding bits of the ModRm byte. Test runs
   * of xed and objdump appear to match that the field should be
   * the effective address (Mmx_E_Operand) instead. -- Karl
   */
  DefineOpcodePrefix(Prefix0F);
  DefineOpcode(0x72,
               NACLi_MMX,
               InstFlag(OpcodeInModRm) | InstFlag(OpcodeHasImmed_b),
               InstPsrad);
  DefineOperand(Opcode4, OpFlag(OperandExtendsOpcode));
  DefineOperand(Mmx_E_Operand, OpFlag(OpUse) | OpFlag(OpSet)); /* G? */
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix660F);
  DefineOpcode(0x72,
               NACLi_SSE2,
               InstFlag(OpcodeInModRm) | InstFlag(OpcodeHasImmed_b),
               InstPsrad);
  DefineOperand(Opcode4, OpFlag(OperandExtendsOpcode));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse) | OpFlag(OpSet)); /* G? */
  DefineOperand(I_Operand, OpFlag(OpUse));

  /* Note: The Intel manual claims that the first operand is an
   * Mmx_G_Operand. However, this is not possible, since Opcode 3
   * is in the same corresponding bits of the ModRm byte. Test runs
   * of xed and objdump appear to match that the field should be
   * the effective address (Mmx_E_Operand) instead. -- Karl
   */
  DefineOpcodePrefix(Prefix660F);
  DefineOpcode(0x73,
               NACLi_SSE2x,
               InstFlag(OpcodeInModRm) | InstFlag(OpcodeHasImmed_b),
               InstPsrldq);
  DefineOperand(Opcode3, OpFlag(OperandExtendsOpcode));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse) | OpFlag(OpSet)); /* G? */
  DefineOperand(I_Operand, OpFlag(OpUse));

  /* Note: The Intel manual claims that the first operand is an
   * Mmx_G_Operand. However, this is not possible, since Opcode 3
   * is in the same corresponding bits of the ModRm byte. Test runs
   * of xed and objdump appear to match that the field should be
   * the effective address (Mmx_E_Operand) instead. -- Karl
   */
  DefineOpcodePrefix(Prefix0F);
  DefineOpcode(0x71,
               NACLi_MMX,
               InstFlag(OpcodeInModRm) | InstFlag(OpcodeHasImmed_b),
               InstPsrlw);
  DefineOperand(Opcode2, OpFlag(OperandExtendsOpcode));
  DefineOperand(Mmx_E_Operand, OpFlag(OpUse) | OpFlag(OpSet));  /* G? */
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix660F);
  DefineOpcode(0x71,
               NACLi_SSE2,
               InstFlag(OpcodeInModRm) | InstFlag(OpcodeHasImmed_b),
               InstPsrlw);
  DefineOperand(Opcode2, OpFlag(OperandExtendsOpcode));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse) | OpFlag(OpSet)); /* G? */
  DefineOperand(I_Operand, OpFlag(OpUse));

  /* Note: The Intel manual claims that the first operand is an
   * Mmx_G_Operand. However, this is not possible, since Opcode 2
   * is in the same corresponding bits of the ModRm byte. Test runs
   * of xed and objdump appear to match that the field should be
   * the effective address (Mmx_E_Operand) instead. -- Karl
   */
  DefineOpcodePrefix(Prefix0F);
  DefineOpcode(0x72,
               NACLi_MMX,
               InstFlag(OpcodeInModRm) | InstFlag(OpcodeHasImmed_b),
               InstPsrld);
  DefineOperand(Opcode2, OpFlag(OperandExtendsOpcode));
  DefineOperand(Mmx_E_Operand, OpFlag(OpUse) | OpFlag(OpSet)); /* G? */
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix660F);
  DefineOpcode(0x72,
               NACLi_SSE2,
               InstFlag(OpcodeInModRm) | InstFlag(OpcodeHasImmed_b),
               InstPsrld);
  DefineOperand(Opcode2, OpFlag(OperandExtendsOpcode));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse) | OpFlag(OpSet)); /* G? */
  DefineOperand(I_Operand, OpFlag(OpUse));


  /* Note: The Intel manual claims that the first operand is an
   * Mmx_G_Operand. However, this is not possible, since Opcode 2
   * is in the same corresponding bits of the ModRm byte. Test runs
   * of xed and objdump appear to match that the field should be
   * the effective address (Mmx_E_Operand) instead. -- Karl
   */
  DefineOpcodePrefix(Prefix0F);
  DefineOpcode(0x73,
               NACLi_MMX,
               InstFlag(OpcodeInModRm) | InstFlag(OpcodeHasImmed_b),
               InstPsrlq);
  DefineOperand(Opcode2, OpFlag(OperandExtendsOpcode));
  DefineOperand(Mmx_E_Operand, OpFlag(OpUse) | OpFlag(OpSet)); /* G? */
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix660F);
  DefineOpcode(0x73,
               NACLi_SSE2,
               InstFlag(OpcodeInModRm) | InstFlag(OpcodeHasImmed_b),
               InstPsrlq);
  DefineOperand(Opcode2, OpFlag(OperandExtendsOpcode));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse) | OpFlag(OpSet)); /* G? */
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix660F38);
  DefineOpcode(0x17,
               NACLi_SSE41,
               InstFlag(OpcodeUsesModRm),
               InstPtest);
  DefineOperand(Xmm_G_Operand, OpFlag(OpUse));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));

  /* f2 0f c2 /r ib  cmpsd xmm1, xmm2/m64, imm8  SSE2 RexR */
  DefineOpcodePrefix(PrefixF20F);
  DefineOpcode(0xc2,
               NACLi_SSE2,
               InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeHasImmed_b),
               InstCmpsd_xmm);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));
}

void DefineSseOpcodes() {
  DefineNarySseOpcodes();
  DefineBinarySseOpcodes();
  DefineMmxOpcodes();
}
