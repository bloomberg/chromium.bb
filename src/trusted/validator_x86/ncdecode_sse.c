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

/* Here's a slightly different strategy for how to encode some of */
/* the SSE* instructions.                                         */
static void DefineVpsWpsInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));
}

static void DefineWpsVpsInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_E_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_G_Operand, OpFlag(OpUse));
}

static void DefineVq_Mq_Inst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(M_Operand, OpFlag(OpUse));
}

static void DefineMq_Vq_Inst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(M_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_G_Operand, OpFlag(OpUse));
}

static void DefineVpsWq_Inst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));
}

static void DefineWssVssInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_E_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_G_Operand, OpFlag(OpUse));
}

static void DefineVssWssInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));
}

static void DefineVpdWpdInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));
}

static void DefineWpdVpdInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_E_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_G_Operand, OpFlag(OpUse));
}

static void DefineVpdWq_Inst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));
}

static void DefineVsdWsdInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));
}

static void DefineWsdVsdInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_E_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_G_Operand, OpFlag(OpUse));
}

static void DefineVq_Wq_Inst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));
}

static void DefineVpsQpiInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(E_Operand, OpFlag(OpUse));
}

static void DefineMpsVpsInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(M_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
}

static void DefinePpiWpsInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));
}

static void DefineVpdQpiInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Mmx_E_Operand, OpFlag(OpUse));
}

static void DefineMpdVpdInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(M_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_G_Operand, OpFlag(OpUse));
}

static void DefinePpiWpdInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Mmx_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));
}

static void DefineQpiWpdInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Mmx_E_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));
}

static void DefineVsdEdqInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(E_Operand, OpFlag(OpUse));
}

static void DefineGdqWsdInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));
}

static void DefineGdqUpsInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));
}

static void DefineGdqUpdInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));
}

static void DefineVdqWdqInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));
}

static void DefineGd_UdqInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));
}

static void DefineMdqVdqInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(M_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_G_Operand, OpFlag(OpUse));
}

static void DefineVpdWdqInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));
}

static void DefineVdqUdqInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));
}

static void DefineVdqMdqInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(M_Operand, OpFlag(OpUse));
}


static void DefineEdqVdqInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(E_Operand, OpFlag(OpUse));
  DefineOperand(Xmm_G_Operand, OpFlag(OpUse));
}

static void DefinePq_Qd_Inst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Mmx_G_Operand, OpFlag(OpSet));
  DefineOperand(Mmx_E_Operand, OpFlag(OpUse));
}

static void DefinePq_Qq_Inst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Mmx_G_Operand, OpFlag(OpSet));
  DefineOperand(Mmx_E_Operand, OpFlag(OpUse));
}

static void DefineEdqPd_Inst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(E_Operand, OpFlag(OpSet));
  DefineOperand(Mmx_G_Operand, OpFlag(OpUse));
}

static void DefineQq_Pq_Inst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Mmx_E_Operand, OpFlag(OpSet));
  DefineOperand(Mmx_G_Operand, OpFlag(OpUse));
}

static void DefineMq_Pq_Inst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(M_Operand, OpFlag(OpSet));
  DefineOperand(Mmx_G_Operand, OpFlag(OpUse));
}

static void DefineGd_Nq_Inst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(Mmx_E_Operand, OpFlag(OpUse));
}

static void DefinePq_Nq_Inst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Mmx_E_Operand, OpFlag(OpSet));
  DefineOperand(Mmx_G_Operand, OpFlag(OpUse));
}

static void DefinePd_EdqInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Mmx_G_Operand, OpFlag(OpSet));
  DefineOperand(E_Operand, OpFlag(OpUse));
}

static void DefineWdqVdqInst(NaClInstType itype, uint8_t opbyte,
                             OpcodePrefix prefix, InstMnemonic inst) {
  DefineOpcodePrefix(prefix);
  DefineOpcode(opbyte, itype, InstFlag(OpcodeUsesModRm), inst);
  DefineOperand(Xmm_E_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_G_Operand, OpFlag(OpUse));
}

static void DefineBinarySseOpcodes() {
  DefineVpsWpsInst(NACLi_SSE, 0x10, Prefix0F, InstMovups);
  DefineWpsVpsInst(NACLi_SSE, 0x11, Prefix0F, InstMovups);
  DefineVq_Mq_Inst(NACLi_SSE, 0x12, Prefix0F, InstMovlps);
  DefineMq_Vq_Inst(NACLi_SSE, 0x13, Prefix0F, InstMovlps);
  DefineVpsWq_Inst(NACLi_SSE, 0x14, Prefix0F, InstUnpcklps);
  DefineVpsWq_Inst(NACLi_SSE, 0x15, Prefix0F, InstUnpckhps);
  DefineVq_Mq_Inst(NACLi_SSE, 0x16, Prefix0F, InstMovhps);
  DefineMq_Vq_Inst(NACLi_SSE, 0x17, Prefix0F, InstMovhps);
  DefineVssWssInst(NACLi_SSE, 0x10, PrefixF30F, InstMovss);
  DefineWssVssInst(NACLi_SSE, 0x11, PrefixF30F, InstMovss);

  DefineVpdWpdInst(NACLi_SSE2, 0x10, Prefix660F, InstMovupd);
  DefineWpdVpdInst(NACLi_SSE2, 0x11, Prefix660F, InstMovupd);
  DefineVq_Mq_Inst(NACLi_SSE2, 0x12, Prefix660F, InstMovlpd);
  DefineMq_Vq_Inst(NACLi_SSE2, 0x13, Prefix660F, InstMovlpd);
  DefineVpdWq_Inst(NACLi_SSE2, 0x14, Prefix660F, InstUnpcklpd);
  DefineVpdWq_Inst(NACLi_SSE2, 0x15, Prefix660F, InstUnpckhpd);
  DefineVq_Mq_Inst(NACLi_SSE2, 0x16, Prefix660F, InstMovhpd);
  DefineMq_Vq_Inst(NACLi_SSE2, 0x17, Prefix660F, InstMovhpd);

  DefineVsdWsdInst(NACLi_SSE2, 0x10, PrefixF20F, InstMovsd);
  DefineWsdVsdInst(NACLi_SSE2, 0x11, PrefixF20F, InstMovsd);

  DefineVq_Wq_Inst(NACLi_SSE3, 0x12, PrefixF20F, InstMovddup);
  DefineVq_Wq_Inst(NACLi_SSE3, 0x12, PrefixF30F, InstMovsldup);
  DefineVq_Wq_Inst(NACLi_SSE3, 0x16, PrefixF30F, InstMovshdup);

  DefineVpsWpsInst(NACLi_SSE, 0x28, Prefix0F, InstMovaps);
  DefineWpsVpsInst(NACLi_SSE, 0x29, Prefix0F, InstMovaps);
  DefineVpsQpiInst(NACLi_SSE, 0x2a, Prefix0F, InstCvtpi2ps);
  DefineMpsVpsInst(NACLi_SSE, 0x2b, Prefix0F, InstMovntps);
  DefinePpiWpsInst(NACLi_SSE, 0x2c, Prefix0F, InstCvttps2pi);
  DefinePpiWpsInst(NACLi_SSE, 0x2d, Prefix0F, InstCvtps2pi);
  DefineVssWssInst(NACLi_SSE, 0x2e, Prefix0F, InstUcomiss);
  DefineVssWssInst(NACLi_SSE, 0x2f, Prefix0F, InstComiss);

  DefineVpsQpiInst(NACLi_SSE, 0x2a, PrefixF30F, InstCvtsi2ss);
  DefineMpsVpsInst(NACLi_SSE4A, 0x2b, PrefixF30F, InstMovntss);
  DefinePpiWpsInst(NACLi_SSE, 0x2c, PrefixF30F, InstCvttss2si);
  DefinePpiWpsInst(NACLi_SSE, 0x2d, PrefixF30F, InstCvtss2si);

  DefineVpdWpdInst(NACLi_SSE2, 0x28, Prefix660F, InstMovapd);
  DefineWpdVpdInst(NACLi_SSE2, 0x29, Prefix660F, InstMovapd);
  DefineVpdQpiInst(NACLi_SSE2, 0x2a, Prefix660F, InstCvtpi2pd);
  DefineMpdVpdInst(NACLi_SSE2, 0x2b, Prefix660F, InstMovntpd);
  DefinePpiWpdInst(NACLi_SSE2, 0x2c, Prefix660F, InstCvttpd2pi);
  DefineQpiWpdInst(NACLi_SSE2, 0x2d, Prefix660F, InstCvtpd2pi);
  DefineVsdWsdInst(NACLi_SSE2, 0x2e, Prefix660F, InstUcomisd);
  DefineVsdWsdInst(NACLi_SSE2, 0x2f, Prefix660F, InstComisd);

  DefineVsdEdqInst(NACLi_SSE2, 0x2a, PrefixF20F, InstCvtsi2sd);
  DefineGdqWsdInst(NACLi_SSE2, 0x2c, PrefixF20F, InstCvttsd2si);
  DefineGdqWsdInst(NACLi_SSE2, 0x2d, PrefixF20F, InstCvtsd2si);

  DefineGdqUpsInst(NACLi_SSE, 0x50, Prefix0F, InstMovmskps);
  DefineVpsWpsInst(NACLi_SSE, 0x51, Prefix0F, InstSqrtps);
  DefineVpsWpsInst(NACLi_SSE, 0x52, Prefix0F, InstRsqrtps);
  DefineVpsWpsInst(NACLi_SSE, 0x53, Prefix0F, InstRcpps);
  DefineVpsWpsInst(NACLi_SSE, 0x54, Prefix0F, InstAndps);
  DefineVpsWpsInst(NACLi_SSE, 0x55, Prefix0F, InstAndnps);
  DefineVpsWpsInst(NACLi_SSE, 0x56, Prefix0F, InstOrps);
  DefineVpsWpsInst(NACLi_SSE, 0x57, Prefix0F, InstXorps);

  DefineGdqUpdInst(NACLi_SSE2, 0x50, Prefix660F, InstMovmskpd);
  DefineVpdWpdInst(NACLi_SSE2, 0x51, Prefix660F, InstSqrtpd);
  DefineVpdWpdInst(NACLi_SSE2, 0x54, Prefix660F, InstAndpd);
  DefineVpdWpdInst(NACLi_SSE2, 0x55, Prefix660F, InstAndnpd);
  DefineVpdWpdInst(NACLi_SSE2, 0x56, Prefix660F, InstOrpd);
  DefineVpdWpdInst(NACLi_SSE2, 0x57, Prefix660F, InstXorpd);

  DefineVsdWsdInst(NACLi_SSE2, 0x51, PrefixF20F, InstSqrtsd);

  DefineVssWssInst(NACLi_SSE, 0x51, PrefixF30F, InstSqrtss);
  DefineVssWssInst(NACLi_SSE, 0x52, PrefixF30F, InstRsqrtps);
  DefineVssWssInst(NACLi_SSE, 0x53, PrefixF30F, InstRcpps);

  DefineVpsWpsInst(NACLi_SSE, 0x58, Prefix0F, InstAddps);
  DefineVpsWpsInst(NACLi_SSE, 0x59, Prefix0F, InstMulps);
  DefineVpsWpsInst(NACLi_SSE2, 0x5a, Prefix0F, InstCvtps2pd);
  DefineVpsWpsInst(NACLi_SSE2, 0x5b, Prefix0F, InstCvtdq2ps);
  DefineVpsWpsInst(NACLi_SSE, 0x5c, Prefix0F, InstSubps);
  DefineVpsWpsInst(NACLi_SSE, 0x5d, Prefix0F, InstMinps);
  DefineVpsWpsInst(NACLi_SSE, 0x5e, Prefix0F, InstDivps);
  DefineVpsWpsInst(NACLi_SSE, 0x5f, Prefix0F, InstMaxps);

  DefineVssWssInst(NACLi_SSE, 0x58, PrefixF30F, InstAddss);
  DefineVssWssInst(NACLi_SSE, 0x59, PrefixF30F, InstMulss);
  DefineVssWssInst(NACLi_SSE2, 0x5a, PrefixF30F, InstCvtss2sd);
  DefineVssWssInst(NACLi_SSE2, 0x5b, PrefixF30F, InstCvttps2dq);
  DefineVssWssInst(NACLi_SSE, 0x5c, PrefixF30F, InstSubss);
  DefineVssWssInst(NACLi_SSE, 0x5d, PrefixF30F, InstMinss);
  DefineVssWssInst(NACLi_SSE, 0x5e, PrefixF30F, InstDivss);
  DefineVssWssInst(NACLi_SSE, 0x5f, PrefixF30F, InstMaxss);

  DefineVpdWpdInst(NACLi_SSE2, 0x58, Prefix660F, InstAddpd);
  DefineVpdWpdInst(NACLi_SSE2, 0x59, Prefix660F, InstMulpd);
  DefineVpdWpdInst(NACLi_SSE2, 0x5a, Prefix660F, InstCvtss2sd);
  DefineVpdWpdInst(NACLi_SSE2, 0x5b, Prefix660F, InstCvtps2dq);
  DefineVpdWpdInst(NACLi_SSE2, 0x5c, Prefix660F, InstSubpd);
  DefineVpdWpdInst(NACLi_SSE2, 0x5d, Prefix660F, InstMinpd);
  DefineVpdWpdInst(NACLi_SSE2, 0x5e, Prefix660F, InstDivpd);
  DefineVpdWpdInst(NACLi_SSE2, 0x5f, Prefix660F, InstMaxpd);

  DefineVsdWsdInst(NACLi_SSE2, 0x58, PrefixF20F, InstAddsd);
  DefineVsdWsdInst(NACLi_SSE2, 0x59, PrefixF20F, InstMulsd);
  DefineVssWssInst(NACLi_SSE2, 0x5a, PrefixF20F, InstCvtsd2ss);
  /* hole at 5D */
  DefineVsdWsdInst(NACLi_SSE2, 0x5c, PrefixF20F, InstSubsd);
  DefineVsdWsdInst(NACLi_SSE2, 0x5d, PrefixF20F, InstMinsd);
  DefineVsdWsdInst(NACLi_SSE2, 0x5e, PrefixF20F, InstDivsd);
  DefineVsdWsdInst(NACLi_SSE2, 0x5f, PrefixF20F, InstMaxsd);

  /* pshufw is done below */

  DefineVdqWdqInst(NACLi_SSE2, 0x60, Prefix660F, InstPunpcklbw);
  DefineVdqWdqInst(NACLi_SSE2, 0x61, Prefix660F, InstPunpcklwd);
  DefineVdqWdqInst(NACLi_SSE2, 0x62, Prefix660F, InstPunpckldq);
  DefineVdqWdqInst(NACLi_SSE2, 0x63, Prefix660F, InstPacksswb);
  DefineVdqWdqInst(NACLi_SSE2, 0x64, Prefix660F, InstPcmpgtb);
  DefineVdqWdqInst(NACLi_SSE2, 0x65, Prefix660F, InstPcmpgtw);
  DefineVdqWdqInst(NACLi_SSE2, 0x66, Prefix660F, InstPcmpgtd);
  DefineVdqWdqInst(NACLi_SSE2, 0x67, Prefix660F, InstPackuswb);

  /* a bunch of MMX instructions at 0x68-0x6f with no prefix */
  DefineVdqWdqInst(NACLi_SSE2, 0x6f, PrefixF30F, InstMovdqu);
  DefineVdqWdqInst(NACLi_SSE2, 0x68, Prefix660F, InstPunpckhbw);
  DefineVdqWdqInst(NACLi_SSE2, 0x69, Prefix660F, InstPunpckhwd);
  DefineVdqWdqInst(NACLi_SSE2, 0x6a, Prefix660F, InstPunpckhdq);
  DefineVdqWdqInst(NACLi_SSE2, 0x6b, Prefix660F, InstPackssdw);
  DefineVdqWdqInst(NACLi_SSE2, 0x6c, Prefix660F, InstPunpcklqdq);
  DefineVdqWdqInst(NACLi_SSE2, 0x6d, Prefix660F, InstPunpckhqdq);
  DefineVdqWdqInst(NACLi_SSE2, 0x6e, Prefix660F, InstMovd);
  DefineVdqWdqInst(NACLi_SSE2, 0x6f, Prefix660F, InstMovdqa);

  DefineVdqWdqInst(NACLi_SSE2, 0x74, Prefix660F, InstPcmpeqb);
  DefineVdqWdqInst(NACLi_SSE2, 0x75, Prefix660F, InstPcmpeqw);
  DefineVdqWdqInst(NACLi_SSE2, 0x76, Prefix660F, InstPcmpeqd);

  DefineVpdWpdInst(NACLi_SSE2, 0x7c, Prefix660F, InstHaddpd);
  DefineVpdWpdInst(NACLi_SSE2, 0x7d, Prefix660F, InstHsubpd);
  DefineVpsWpsInst(NACLi_SSE3, 0x7c, PrefixF20F, InstHaddps);
  DefineVpsWpsInst(NACLi_SSE3, 0x7d, PrefixF20F, InstHsubps);
  /* 0x7e and 0x7f are for some move opcodes that are done elsewhere */
  DefineEdqVdqInst(NACLi_SSE2, 0x7e, Prefix660F, InstMovd);
  DefineVq_Wq_Inst(NACLi_SSE2, 0x7e, PrefixF30F, InstMovq);
  DefineWdqVdqInst(NACLi_SSE2, 0x7f, Prefix660F, InstMovdqa);
  DefineWdqVdqInst(NACLi_SSE2, 0x7f, PrefixF30F, InstMovdqu);

  DefineVpsWpsInst(NACLi_SSE3, 0xd0, PrefixF20F, InstAddsubps);
  DefineVpdWpdInst(NACLi_SSE3, 0xd0, Prefix660F, InstAddsubpd);
  DefineVdqWdqInst(NACLi_SSE2, 0xd1, Prefix660F, InstPsrlw);
  DefineVdqWdqInst(NACLi_SSE2, 0xd2, Prefix660F, InstPsrld);
  DefineVdqWdqInst(NACLi_SSE2, 0xd3, Prefix660F, InstPsrlq);
  DefineVdqWdqInst(NACLi_SSE2, 0xd4, Prefix660F, InstPaddq);
  DefineVdqWdqInst(NACLi_SSE2, 0xd5, Prefix660F, InstPmullw);
  /* 0xd6 - movq */
  DefineGd_UdqInst(NACLi_SSE2, 0xd7, Prefix660F, InstPmovmskb);

  DefineVdqWdqInst(NACLi_SSE2, 0xd8, Prefix660F, InstPsubusb);
  DefineVdqWdqInst(NACLi_SSE2, 0xd9, Prefix660F, InstPsubusw);
  DefineVdqWdqInst(NACLi_SSE2, 0xda, Prefix660F, InstPminub);
  DefineVdqWdqInst(NACLi_SSE2, 0xdb, Prefix660F, InstPand);
  DefineVdqWdqInst(NACLi_SSE2, 0xdc, Prefix660F, InstPaddusb);
  DefineVdqWdqInst(NACLi_SSE2, 0xdd, Prefix660F, InstPaddusw);
  DefineVdqWdqInst(NACLi_SSE2, 0xde, Prefix660F, InstPmaxub);
  DefineVdqWdqInst(NACLi_SSE2, 0xdf, Prefix660F, InstPandn);

  DefineVdqWdqInst(NACLi_SSE2, 0xe0, Prefix660F, InstPavgb);
  DefineVdqWdqInst(NACLi_SSE2, 0xe1, Prefix660F, InstPsraw);
  DefineVdqWdqInst(NACLi_SSE2, 0xe2, Prefix660F, InstPsrad);
  DefineVdqWdqInst(NACLi_SSE2, 0xe3, Prefix660F, InstPavgw);
  DefineVdqWdqInst(NACLi_SSE2, 0xe4, Prefix660F, InstPmulhuw);
  DefineVdqWdqInst(NACLi_SSE2, 0xe5, Prefix660F, InstPmulhw);
  DefineVdqWdqInst(NACLi_SSE2, 0xe6, Prefix660F, InstCvttpd2dq);
  DefineMdqVdqInst(NACLi_SSE2, 0xe7, Prefix660F, InstMovntdq);
  DefineVpdWdqInst(NACLi_SSE2, 0xe6, PrefixF30F, InstCvttdq2pd);

  DefineVdqWdqInst(NACLi_SSE2, 0xe8, Prefix660F, InstPsubsb);
  DefineVdqWdqInst(NACLi_SSE2, 0xe9, Prefix660F, InstPsubsw);
  DefineVdqWdqInst(NACLi_SSE2, 0xea, Prefix660F, InstPminsw);
  DefineVdqWdqInst(NACLi_SSE2, 0xeb, Prefix660F, InstPor);
  DefineVdqWdqInst(NACLi_SSE2, 0xec, Prefix660F, InstPaddsb);
  DefineVdqWdqInst(NACLi_SSE2, 0xed, Prefix660F, InstPaddsw);
  DefineVdqWdqInst(NACLi_SSE2, 0xee, Prefix660F, InstPmaxsw);
  DefineVdqWdqInst(NACLi_SSE2, 0xef, Prefix660F, InstPxor);

  DefineVdqWdqInst(NACLi_SSE2, 0xf1, Prefix660F, InstPsllw);
  DefineVdqWdqInst(NACLi_SSE2, 0xf2, Prefix660F, InstPslld);
  DefineVdqWdqInst(NACLi_SSE2, 0xf3, Prefix660F, InstPsllq);
  DefineVdqWdqInst(NACLi_SSE2, 0xf4, Prefix660F, InstPmuludq);
  DefineVdqWdqInst(NACLi_SSE2, 0xf5, Prefix660F, InstPmaddwd);
  DefineVdqWdqInst(NACLi_SSE2, 0xf6, Prefix660F, InstPsadbw);
  DefineVdqUdqInst(NACLi_SSE2, 0xf7, Prefix660F, InstMaskmovdqu);

  DefineVdqMdqInst(NACLi_SSE3, 0xf0, PrefixF20F, InstLddqu);

  DefineVdqWdqInst(NACLi_SSE2, 0xf8, Prefix660F, InstPsubb);
  DefineVdqWdqInst(NACLi_SSE2, 0xf9, Prefix660F, InstPsubw);
  DefineVdqWdqInst(NACLi_SSE2, 0xfa, Prefix660F, InstPsubd);
  DefineVdqWdqInst(NACLi_SSE2, 0xfb, Prefix660F, InstPsubq);
  DefineVdqWdqInst(NACLi_SSE2, 0xfc, Prefix660F, InstPaddb);
  DefineVdqWdqInst(NACLi_SSE2, 0xfd, Prefix660F, InstPaddw);
  DefineVdqWdqInst(NACLi_SSE2, 0xfe, Prefix660F, InstPaddd);
}

static void DefineMmxOpcodes() {
  /*  0x77 NACLi_MMX Prefix0f InstEmms */

  DefinePq_Qd_Inst(NACLi_MMX, 0x60, Prefix0F, InstPunpcklbw);
  DefinePq_Qd_Inst(NACLi_MMX, 0x61, Prefix0F, InstPunpcklwd);
  DefinePq_Qd_Inst(NACLi_MMX, 0x62, Prefix0F, InstPunpckldq);
  DefinePq_Qq_Inst(NACLi_MMX, 0x63, Prefix0F, InstPacksswb);
  DefinePq_Qq_Inst(NACLi_MMX, 0x64, Prefix0F, InstPcmpgtb);
  DefinePq_Qq_Inst(NACLi_MMX, 0x65, Prefix0F, InstPcmpgtw);
  DefinePq_Qq_Inst(NACLi_MMX, 0x66, Prefix0F, InstPcmpgtd);
  DefinePq_Qq_Inst(NACLi_MMX, 0x67, Prefix0F, InstPackuswb);

  DefinePq_Qd_Inst(NACLi_MMX, 0x68, Prefix0F, InstPunpckhbw);
  DefinePq_Qd_Inst(NACLi_MMX, 0x69, Prefix0F, InstPunpckhwd);
  DefinePq_Qd_Inst(NACLi_MMX, 0x6a, Prefix0F, InstPunpckhdq);
  DefinePq_Qd_Inst(NACLi_MMX, 0x6b, Prefix0F, InstPackssdw);
  DefinePd_EdqInst(NACLi_MMX, 0x6e, Prefix0F, InstMovd);
  DefinePq_Qq_Inst(NACLi_MMX, 0x6f, Prefix0F, InstMovq);

  DefinePq_Qq_Inst(NACLi_MMX, 0x74, Prefix0F, InstPcmpeqb);
  DefinePq_Qq_Inst(NACLi_MMX, 0x75, Prefix0F, InstPcmpeqw);
  DefinePq_Qq_Inst(NACLi_MMX, 0x76, Prefix0F, InstPcmpeqd);

  DefineEdqPd_Inst(NACLi_MMX, 0x7e, Prefix0F, InstMovd);
  DefineQq_Pq_Inst(NACLi_MMX, 0x7f, Prefix0F, InstMovq);

  DefinePq_Qq_Inst(NACLi_MMX, 0xd1, Prefix0F, InstPsrlw);
  DefinePq_Qq_Inst(NACLi_MMX, 0xd2, Prefix0F, InstPsrld);
  DefinePq_Qq_Inst(NACLi_MMX, 0xd3, Prefix0F, InstPsrlq);
  DefinePq_Qq_Inst(NACLi_MMX, 0xd4, Prefix0F, InstPaddq);
  DefinePq_Qq_Inst(NACLi_MMX, 0xd5, Prefix0F, InstPmullw);
  DefineGd_Nq_Inst(NACLi_MMX, 0xd7, Prefix0F, InstPmovmskb);

  DefinePq_Qq_Inst(NACLi_MMX, 0xd8, Prefix0F, InstPsubusb);
  DefinePq_Qq_Inst(NACLi_MMX, 0xd9, Prefix0F, InstPsubusw);
  DefinePq_Qq_Inst(NACLi_MMX, 0xda, Prefix0F, InstPminub);
  DefinePq_Qq_Inst(NACLi_MMX, 0xdb, Prefix0F, InstPand);
  DefinePq_Qq_Inst(NACLi_MMX, 0xdc, Prefix0F, InstPaddusb);
  DefinePq_Qq_Inst(NACLi_MMX, 0xdd, Prefix0F, InstPaddusw);
  DefinePq_Qq_Inst(NACLi_MMX, 0xde, Prefix0F, InstPmaxub);
  DefinePq_Qq_Inst(NACLi_MMX, 0xdf, Prefix0F, InstPandn);

  DefinePq_Qq_Inst(NACLi_MMX, 0xe0, Prefix0F, InstPavgb);
  DefinePq_Qq_Inst(NACLi_MMX, 0xe1, Prefix0F, InstPsraw);
  DefinePq_Qq_Inst(NACLi_MMX, 0xe2, Prefix0F, InstPsrad);
  DefinePq_Qq_Inst(NACLi_MMX, 0xe3, Prefix0F, InstPavgw);
  DefinePq_Qq_Inst(NACLi_MMX, 0xe4, Prefix0F, InstPmulhuw);
  DefinePq_Qq_Inst(NACLi_MMX, 0xe5, Prefix0F, InstPmulhw);
  DefineMq_Pq_Inst(NACLi_MMX, 0xe7, Prefix0F, InstMovntq);

  DefinePq_Qq_Inst(NACLi_MMX, 0xe8, Prefix0F, InstPsubsb);
  DefinePq_Qq_Inst(NACLi_MMX, 0xe9, Prefix0F, InstPsubsw);
  DefinePq_Qq_Inst(NACLi_MMX, 0xea, Prefix0F, InstPminsw);
  DefinePq_Qq_Inst(NACLi_MMX, 0xeb, Prefix0F, InstPor);
  DefinePq_Qq_Inst(NACLi_MMX, 0xec, Prefix0F, InstPaddwb);
  DefinePq_Qq_Inst(NACLi_MMX, 0xed, Prefix0F, InstPaddsw);
  DefinePq_Qq_Inst(NACLi_MMX, 0xee, Prefix0F, InstPmaxsw);
  DefinePq_Qq_Inst(NACLi_MMX, 0xef, Prefix0F, InstPxor);

  DefinePq_Qq_Inst(NACLi_MMX, 0xf1, Prefix0F, InstPsllw);
  DefinePq_Qq_Inst(NACLi_MMX, 0xf2, Prefix0F, InstPslld);
  DefinePq_Qq_Inst(NACLi_MMX, 0xf3, Prefix0F, InstPsllq);
  DefinePq_Qq_Inst(NACLi_MMX, 0xf4, Prefix0F, InstPsmuludq);
  DefinePq_Qq_Inst(NACLi_MMX, 0xf5, Prefix0F, InstPmaddwd);
  DefinePq_Qq_Inst(NACLi_MMX, 0xf6, Prefix0F, InstPsadbw);
  DefinePq_Nq_Inst(NACLi_MMX, 0xf7, Prefix0F, InstMasmovq);

  DefinePq_Qq_Inst(NACLi_MMX, 0xf8, Prefix0F, InstPsubb);
  DefinePq_Qq_Inst(NACLi_MMX, 0xf9, Prefix0F, InstPsubw);
  DefinePq_Qq_Inst(NACLi_MMX, 0xfa, Prefix0F, InstPsubd);
  DefinePq_Qq_Inst(NACLi_MMX, 0xfb, Prefix0F, InstPsubq);
  DefinePq_Qq_Inst(NACLi_MMX, 0xfc, Prefix0F, InstPaddb);
  DefinePq_Qq_Inst(NACLi_MMX, 0xfd, Prefix0F, InstPaddw);
  DefinePq_Qq_Inst(NACLi_MMX, 0xfe, Prefix0F, InstPaddd);
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
