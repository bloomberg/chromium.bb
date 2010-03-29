/*
 * Copyright (c) 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that be
 * found in the LICENSE file.
 */

/*
 * Defines SSE instructions.
 */

#include "native_client/src/trusted/validator_x86/ncdecode_forms.h"
#include "native_client/src/trusted/validator_x86/ncdecode_tablegen.h"

/* Define a generic way for computing the array size of a declared array. */
#if !defined(ARRAYSIZE)
#define ARRAYSIZE(a) sizeof(a)/sizeof(a[0])
#endif

/* See Appendex A of the AMD manual for tables describing the formats
 * of these instructions.
 */

static void NaClDefBinarySseInsts() {
  DEF_BINST(Vdq, Wdq)(NACLi_SSSE3, 0x08, Prefix660F38, InstPsignb, Binary);
  /* DefineVdqWdqInst(NACLi_SSSE3, 0x08, Prefix660F38, InstPsignb, Binary); */
  DEF_BINST(Vdq, Wdq)(NACLi_SSSE3, 0x09, Prefix660F38, InstPsignw, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSSE3, 0x0A, Prefix660F38, InstPsignd, Binary);

  DEF_BINST(Vps, Wps)(NACLi_SSE, 0x10, Prefix0F, InstMovups, Move);
  DEF_BINST(Wps, Vps)(NACLi_SSE, 0x11, Prefix0F, InstMovups, Move);
  /* TODO(karl): Turn off recognition of instruction for now, until we
   * can distinquish between:
   * DEF_BINST(Vps, _Mq)t(NACLi_SSE, 0x12, Prefix0F, InstMovlps, Move);
   * DEF_BINST(Vps, Uq_)(NACLi_SSE, 0x12, Prefix0F, InstMovhlps, Move);
   */
  DEF_BINST(Vps, Mq_)(NACLi_SSE, 0x12, Prefix0F, InstMovlps, Move);
  NaClAddIFlags(NACL_IFLAG(NaClIllegal));
  DEF_BINST(Mq_, Vps)(NACLi_SSE, 0x13, Prefix0F, InstMovlps, Move);
  DEF_BINST(Vps, Wq_)(NACLi_SSE, 0x14, Prefix0F, InstUnpcklps, Binary);
  DEF_BINST(Vps, Wq_)(NACLi_SSE, 0x15, Prefix0F, InstUnpckhps, Binary);
  /* TODO(karl): Turn off recognition of instruction for now, until we
   * can distinquish between:
   * DEF_BINST(Vps, Mq_)(NACLi_SSE, 0x16, Prefix0F, InstMovhps, Move);
   * DEF_BINST(Vps, Uq_)(NACLi_SSE, 0x16, Prefix0F, InstMovlhps, Move);
   */
  DEF_BINST(Vps, Mq_)(NACLi_SSE, 0x16, Prefix0F, InstMovhps, Move);
  NaClAddIFlags(NACL_IFLAG(NaClIllegal));
  DEF_BINST(Mq_, Vps)(NACLi_SSE, 0x17, Prefix0F, InstMovhps, Move);

  DEF_BINST(Vss, Wss)(NACLi_SSE, 0x10, PrefixF30F, InstMovss, Move);
  DEF_BINST(Wss, Vss)(NACLi_SSE, 0x11, PrefixF30F, InstMovss, Move);

  DEF_BINST(Vpd, Wpd)(NACLi_SSE2, 0x10, Prefix660F, InstMovupd, Move);
  DEF_BINST(Wpd, Vpd)(NACLi_SSE2, 0x11, Prefix660F, InstMovupd, Move);
  DEF_BINST(Vsd, Mq_)(NACLi_SSE2, 0x12, Prefix660F, InstMovlpd, Move);
  DEF_BINST(Mq_, Vsd)(NACLi_SSE2, 0x13, Prefix660F, InstMovlpd, Move);
  DEF_BINST(Vpd, Wq_)(NACLi_SSE2, 0x14, Prefix660F, InstUnpcklpd, Binary);
  DEF_BINST(Vpd, Wq_)(NACLi_SSE2, 0x15, Prefix660F, InstUnpckhpd, Binary);
  DEF_BINST(Vsd, Mq_)(NACLi_SSE2, 0x16, Prefix660F, InstMovhpd, Move);
  DEF_BINST(Mq_, Vsd)(NACLi_SSE2, 0x17, Prefix660F, InstMovhpd, Move);

  DEF_BINST(Vsd, Wsd)(NACLi_SSE2, 0x10, PrefixF20F, InstMovsd, Move);
  DEF_BINST(Wsd, Vsd)(NACLi_SSE2, 0x11, PrefixF20F, InstMovsd, Move);
  DEF_BINST(Vpd, Wsd)(NACLi_SSE3, 0x12, PrefixF20F, InstMovddup, Move);

  DEF_BINST(Vps, Wps)(NACLi_SSE3, 0x12, PrefixF30F, InstMovsldup, Move);
  DEF_BINST(Vps, Wps)(NACLi_SSE3, 0x16, PrefixF30F, InstMovshdup, Move);

  /* OF 20/21/22/23 (MOV) to/from control/debug registers not implemented,
   * and illegal for native client.
   */

  DEF_BINST(Vps, Wps)(NACLi_SSE, 0x28, Prefix0F, InstMovaps, Move);
  DEF_BINST(Wps, Vps)(NACLi_SSE, 0x29, Prefix0F, InstMovaps, Move);
  DEF_BINST(Vps, Qq_)(NACLi_SSE, 0x2a, Prefix0F, InstCvtpi2ps, Move);
  DEF_BINST(Mdq, Vps)(NACLi_SSE, 0x2b, Prefix0F, InstMovntps, Move);
  DEF_BINST(Pq_, Wps)(NACLi_SSE, 0x2c, Prefix0F, InstCvttps2pi, Move);
  DEF_BINST(Pq_, Wps)(NACLi_SSE, 0x2d, Prefix0F, InstCvtps2pi, Move);
  DEF_BINST(Vss, Wss)(NACLi_SSE, 0x2e, Prefix0F, InstUcomiss, Compare);
  DEF_BINST(Vps, Wps)(NACLi_SSE, 0x2f, Prefix0F, InstComiss, Compare);

  DEF_BINST(Vss, Edq)(NACLi_SSE, 0x2a, PrefixF30F, InstCvtsi2ss, Move);
  DEF_BINST(Md_, Vss)(NACLi_SSE4A, 0x2b, PrefixF30F, InstMovntss, Move);
  DEF_BINST(Gdq, Wss)(NACLi_SSE, 0x2c, PrefixF30F, InstCvttss2si, Move);
  DEF_BINST(Gdq, Wss)(NACLi_SSE, 0x2d, PrefixF30F, InstCvtss2si, Move);

  DEF_BINST(Vpd, Wpd)(NACLi_SSE2, 0x28, Prefix660F, InstMovapd, Move);
  DEF_BINST(Wpd, Vpd)(NACLi_SSE2, 0x29, Prefix660F, InstMovapd, Move);
  DEF_BINST(Vpd, Qq_)(NACLi_SSE2, 0x2a, Prefix660F, InstCvtpi2pd, Move);
  DEF_BINST(Mdq, Vpd)(NACLi_SSE2, 0x2b, Prefix660F, InstMovntpd, Move);
  DEF_BINST(Pq_, Wpd)(NACLi_SSE2, 0x2c, Prefix660F, InstCvttpd2pi, Move);
  DEF_BINST(Pq_, Wpd)(NACLi_SSE2, 0x2d, Prefix660F, InstCvtpd2pi, Move);
  DEF_BINST(Vsd, Wsd)(NACLi_SSE2, 0x2e, Prefix660F, InstUcomisd, Compare);
  DEF_BINST(Vsd, Wsd)(NACLi_SSE2, 0x2f, Prefix660F, InstComisd, Compare);

  DEF_BINST(Vsd, Edq)(NACLi_SSE2, 0x2a, PrefixF20F, InstCvtsi2sd, Move);
  DEF_BINST(Mq_, Vsd)(NACLi_SSE4A, 0x2b, PrefixF20F, InstMovntsd, Move);
  DEF_BINST(Gdq, Wsd)(NACLi_SSE2, 0x2c, PrefixF20F, InstCvttsd2si, Move);
  DEF_BINST(Gdq, Wsd)(NACLi_SSE2, 0x2d, PrefixF20F, InstCvtsd2si, Move);

  DEF_BINST(Gd_, Ups)(NACLi_SSE, 0x50, Prefix0F, InstMovmskps, Move);
  DEF_BINST(Vps, Wps)(NACLi_SSE, 0x51, Prefix0F, InstSqrtps, Move);
  DEF_BINST(Vps, Wps)(NACLi_SSE, 0x52, Prefix0F, InstRsqrtps, Move);
  DEF_BINST(Vps, Wps)(NACLi_SSE, 0x53, Prefix0F, InstRcpps, Move);
  DEF_BINST(Vps, Wps)(NACLi_SSE, 0x54, Prefix0F, InstAndps, Binary);
  DEF_BINST(Vps, Wps)(NACLi_SSE, 0x55, Prefix0F, InstAndnps, Binary);
  DEF_BINST(Vps, Wps)(NACLi_SSE, 0x56, Prefix0F, InstOrps, Binary);
  DEF_BINST(Vps, Wps)(NACLi_SSE, 0x57, Prefix0F, InstXorps, Binary);

  DEF_BINST(Gd_, Upd)(NACLi_SSE2, 0x50, Prefix660F, InstMovmskpd, Move);
  NaClAddIFlags(NACL_IFLAG(OperandSizeDefaultIs64));
  DEF_BINST(Vpd, Wpd)(NACLi_SSE2, 0x51, Prefix660F, InstSqrtpd, Move);
  DEF_BINST(Vpd, Wpd)(NACLi_SSE2, 0x54, Prefix660F, InstAndpd, Binary);
  DEF_BINST(Vpd, Wpd)(NACLi_SSE2, 0x55, Prefix660F, InstAndnpd, Binary);
  DEF_BINST(Vpd, Wpd)(NACLi_SSE2, 0x56, Prefix660F, InstOrpd, Binary);
  DEF_BINST(Vpd, Wpd)(NACLi_SSE2, 0x57, Prefix660F, InstXorpd, Binary);

  DEF_BINST(Vsd, Wsd)(NACLi_SSE2, 0x51, PrefixF20F, InstSqrtsd, Move);
  NaClAddIFlags(NACL_IFLAG(OperandSizeDefaultIs64));
  DEF_BINST(Vss, Wss)(NACLi_SSE, 0x51, PrefixF30F, InstSqrtss, Move);
  DEF_BINST(Vss, Wss)(NACLi_SSE, 0x52, PrefixF30F, InstRsqrtss, Move);
  DEF_BINST(Vss, Wss)(NACLi_SSE, 0x53, PrefixF30F, InstRcpss, Move);

  DEF_BINST(Vps, Wps)(NACLi_SSE, 0x58, Prefix0F, InstAddps, Binary);
  DEF_BINST(Vps, Wps)(NACLi_SSE, 0x59, Prefix0F, InstMulps, Binary);
  DEF_BINST(Vps, Wps)(NACLi_SSE2, 0x5a, Prefix0F, InstCvtps2pd, Move);
  DEF_BINST(Vps, Wps)(NACLi_SSE2, 0x5b, Prefix0F, InstCvtdq2ps, Move);
  DEF_BINST(Vps, Wps)(NACLi_SSE, 0x5c, Prefix0F, InstSubps, Binary);
  DEF_BINST(Vps, Wps)(NACLi_SSE, 0x5d, Prefix0F, InstMinps, Binary);
  DEF_BINST(Vps, Wps)(NACLi_SSE, 0x5e, Prefix0F, InstDivps, Binary);
  DEF_BINST(Vps, Wps)(NACLi_SSE, 0x5f, Prefix0F, InstMaxps, Binary);

  DEF_BINST(Vss, Wss)(NACLi_SSE, 0x58, PrefixF30F, InstAddss, Binary);
  DEF_BINST(Vss, Wss)(NACLi_SSE, 0x59, PrefixF30F, InstMulss, Binary);
  DEF_BINST(Vsd, Wss)(NACLi_SSE2, 0x5a, PrefixF30F, InstCvtss2sd, Move);
  DEF_BINST(Vdq, Wps)(NACLi_SSE2, 0x5b, PrefixF30F, InstCvttps2dq, Move);
  DEF_BINST(Vss, Wss)(NACLi_SSE, 0x5c, PrefixF30F, InstSubss, Binary);
  DEF_BINST(Vss, Wss)(NACLi_SSE, 0x5d, PrefixF30F, InstMinss, Binary);
  DEF_BINST(Vss, Wss)(NACLi_SSE, 0x5e, PrefixF30F, InstDivss, Binary);
  DEF_BINST(Vss, Wss)(NACLi_SSE, 0x5f, PrefixF30F, InstMaxss, Binary);

  DEF_BINST(Vpd, Wpd)(NACLi_SSE2, 0x58, Prefix660F, InstAddpd, Binary);
  DEF_BINST(Vpd, Wpd)(NACLi_SSE2, 0x59, Prefix660F, InstMulpd, Binary);
  DEF_BINST(Vps, Wpd)(NACLi_SSE2, 0x5a, Prefix660F, InstCvtpd2ps, Move);
  DEF_BINST(Vdq, Wps)(NACLi_SSE2, 0x5b, Prefix660F, InstCvtps2dq, Move);
  DEF_BINST(Vpd, Wpd)(NACLi_SSE2, 0x5c, Prefix660F, InstSubpd, Binary);
  DEF_BINST(Vpd, Wpd)(NACLi_SSE2, 0x5d, Prefix660F, InstMinpd, Binary);
  DEF_BINST(Vpd, Wpd)(NACLi_SSE2, 0x5e, Prefix660F, InstDivpd, Binary);
  DEF_BINST(Vpd, Wpd)(NACLi_SSE2, 0x5f, Prefix660F, InstMaxpd, Binary);

  DEF_BINST(Vsd, Wsd)(NACLi_SSE2, 0x58, PrefixF20F, InstAddsd, Binary);
  DEF_BINST(Vsd, Wsd)(NACLi_SSE2, 0x59, PrefixF20F, InstMulsd, Binary);
  DEF_BINST(Vss, Wsd)(NACLi_SSE2, 0x5a, PrefixF20F, InstCvtsd2ss, Move);
  /* hole at 5B */
  DEF_BINST(Vsd, Wsd)(NACLi_SSE2, 0x5c, PrefixF20F, InstSubsd, Binary);
  DEF_BINST(Vsd, Wsd)(NACLi_SSE2, 0x5d, PrefixF20F, InstMinsd, Binary);
  DEF_BINST(Vsd, Wsd)(NACLi_SSE2, 0x5e, PrefixF20F, InstDivsd, Binary);
  DEF_BINST(Vsd, Wsd)(NACLi_SSE2, 0x5f, PrefixF20F, InstMaxsd, Binary);

  DEF_BINST(Vdq, Wq_)(NACLi_SSE2, 0x60, Prefix660F, InstPunpcklbw, Binary);
  DEF_BINST(Vdq, Wq_)(NACLi_SSE2, 0x61, Prefix660F, InstPunpcklwd, Binary);
  DEF_BINST(Vdq, Wq_)(NACLi_SSE2, 0x62, Prefix660F, InstPunpckldq, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0x63, Prefix660F, InstPacksswb, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0x64, Prefix660F, InstPcmpgtb, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0x65, Prefix660F, InstPcmpgtw, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0x66, Prefix660F, InstPcmpgtd, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0x67, Prefix660F, InstPackuswb, Binary);

  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0x6f, PrefixF30F, InstMovdqu, Move);

  DEF_BINST(Vdq, Wq_)(NACLi_SSE2, 0x68, Prefix660F, InstPunpckhbw, Binary);
  DEF_BINST(Vdq, Wq_)(NACLi_SSE2, 0x69, Prefix660F, InstPunpckhwd, Binary);
  DEF_BINST(Vdq, Wq_)(NACLi_SSE2, 0x6a, Prefix660F, InstPunpckhdq, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0x6b, Prefix660F, InstPackssdw, Binary);
  DEF_BINST(Vdq, Wq_)(NACLi_SSE2, 0x6c, Prefix660F, InstPunpcklqdq, Binary);
  DEF_BINST(Vdq, Wq_)(NACLi_SSE2, 0x6d, Prefix660F, InstPunpckhqdq, Binary);
  DEF_BINST(Vdq, EdQ)(NACLi_SSE2, 0x6e, Prefix660F, InstMovd, Move);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0x6f, Prefix660F, InstMovdqa, Move);

  DEF_BINST(Pq_, Qq_)(NACLi_SSE, 0x70, Prefix0F, InstPshufw, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0x70, Prefix660F, InstPshufd, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  DEF_OINST(Udq, I__)(NACLi_SSE2, 0x71, Prefix660F, Opcode2, InstPsrlw, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  DEF_OINST(Udq, I__)(NACLi_SSE2, 0x71, Prefix660F, Opcode4, InstPsraw, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  DEF_OINST(Udq, I__)(NACLi_SSE3, 0x71, Prefix660F, Opcode6, InstPsllw, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  DEF_OINST(Udq, I__)(NACLi_SSE2, 0x72, Prefix660F, Opcode2, InstPsrld, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  DEF_OINST(Udq, I__)(NACLi_SSE2, 0x72, Prefix660F, Opcode4, InstPsrad, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  DEF_OINST(Udq, I__)(NACLi_SSE2, 0x72, Prefix660F, Opcode6, InstPslld, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  DEF_OINST(Udq, I__)(NACLi_SSE2, 0x73, Prefix660F, Opcode2, InstPsrlq, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  DEF_OINST(Udq, I__)(NACLi_SSE2x, 0x73, Prefix660F, Opcode3, InstPsrldq,
                      Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  DEF_OINST(Udq, I__)(NACLi_SSE2, 0x73, Prefix660F, Opcode6, InstPsllq, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  DEF_OINST(Udq, I__)(NACLi_SSE2x, 0x73, Prefix660F, Opcode7, InstPslldq,
                      Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0x74, Prefix660F, InstPcmpeqb, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0x75, Prefix660F, InstPcmpeqw, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0x76, Prefix660F, InstPcmpeqd, Binary);

  DEF_BINST(Vq_, Wq_)(NACLi_SSE2, 0x70, PrefixF20F, InstPshuflw, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  DEF_BINST(Vq_, Wq_)(NACLi_SSE2, 0x70, PrefixF30F, InstPshufhw, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDelaySanityChecks();
  /* Note: opcode also defines register to use (i.e. xmm0). */
  DEF_OINST(Vdq, I__)(NACLi_SSE4A, 0x78, Prefix660F, Opcode0, InstExtrq,
                      Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeHasImmed2_b));
  NaClAddOpFlags(1, NACL_OPFLAG(AllowGOperandWithOpcodeInModRm));
  NaClDefOp(I2_Operand, NACL_IFLAG(OpUse));
  NaClApplySanityChecks();
  DEF_BINST(Vdq, Uq_)(NACLi_SSE4A, 0x79, Prefix660F, InstExtrq, Binary);
  DEF_BINST(Vpd, Wpd)(NACLi_SSE2, 0x7c, Prefix660F, InstHaddpd, Binary);
  DEF_BINST(Vpd, Wpd)(NACLi_SSE2, 0x7d, Prefix660F, InstHsubpd, Binary);
  DEF_BINST(EdQ, VdQ)(NACLi_SSE2, 0x7e, Prefix660F, InstMovd, Move);
  DEF_BINST(Wdq, Vdq)(NACLi_SSE2, 0x7f, Prefix660F, InstMovdqa, Move);

  DEF_BINST(Vdq, Uq_)(NACLi_SSE4A, 0x78, PrefixF20F, InstInsertq, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeHasImmed2_b));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(I2_Operand, NACL_OPFLAG(OpUse));
  DEF_BINST(Vdq, Udq)(NACLi_SSE4A, 0x79, PrefixF20F, InstInsertq, Binary);
  DEF_BINST(Vps, Wps)(NACLi_SSE3, 0x7c, PrefixF20F, InstHaddps, Binary);
  DEF_BINST(Vps, Wps)(NACLi_SSE3, 0x7d, PrefixF20F, InstHsubps, Binary);

  DEF_BINST(Vq_, Wq_)(NACLi_SSE2, 0x7e, PrefixF30F, InstMovq, Move);
  DEF_BINST(Wdq, Vdq)(NACLi_SSE2, 0x7f, PrefixF30F, InstMovdqu, Move);

  DEF_BINST(Vps, Wps)(NACLi_SSE3, 0xd0, PrefixF20F, InstAddsubps, Binary);
  DEF_BINST(Pq_, Uq_)(NACLi_SSE2, 0xd6, PrefixF20F, InstMovdq2q, Move);

  DEF_BINST(Vdq, Uq_)(NACLi_SSE2, 0xd6, PrefixF30F, InstMovq2dq, Move);

  DEF_BINST(Vpd, Wpd)(NACLi_SSE3, 0xd0, Prefix660F, InstAddsubpd, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xd1, Prefix660F, InstPsrlw, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xd2, Prefix660F, InstPsrld, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xd3, Prefix660F, InstPsrlq, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xd4, Prefix660F, InstPaddq, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xd5, Prefix660F, InstPmullw, Binary);
  DEF_BINST(Wq_, Vq_)(NACLi_SSE2, 0xd6, Prefix660F, InstMovq, Binary);
  DEF_BINST(Gd_, Udq)(NACLi_SSE2, 0xd7, Prefix660F, InstPmovmskb, Move);

  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xd8, Prefix660F, InstPsubusb, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xd9, Prefix660F, InstPsubusw, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xda, Prefix660F, InstPminub, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xdb, Prefix660F, InstPand, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xdc, Prefix660F, InstPaddusb, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xdd, Prefix660F, InstPaddusw, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xde, Prefix660F, InstPmaxub, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xdf, Prefix660F, InstPandn, Binary);

  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xe0, Prefix660F, InstPavgb, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xe1, Prefix660F, InstPsraw, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xe2, Prefix660F, InstPsrad, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xe3, Prefix660F, InstPavgw, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xe4, Prefix660F, InstPmulhuw, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xe5, Prefix660F, InstPmulhw, Binary);
  DEF_BINST(Vq_, Wdq)(NACLi_SSE2, 0xe6, Prefix660F, InstCvttpd2dq, Move);
  DEF_BINST(Mdq, Vdq)(NACLi_SSE2, 0xe7, Prefix660F, InstMovntdq, Move);

  DEF_BINST(Vpd, Wq_)(NACLi_SSE2, 0xe6, PrefixF30F, InstCvttdq2pd, Move);

  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xe8, Prefix660F, InstPsubsb, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xe9, Prefix660F, InstPsubsw, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xea, Prefix660F, InstPminsw, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xeb, Prefix660F, InstPor, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xec, Prefix660F, InstPaddsb, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xed, Prefix660F, InstPaddsw, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xee, Prefix660F, InstPmaxsw, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xef, Prefix660F, InstPxor, Binary);

  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xf1, Prefix660F, InstPsllw, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xf2, Prefix660F, InstPslld, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xf3, Prefix660F, InstPsllq, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xf4, Prefix660F, InstPmuludq, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xf5, Prefix660F, InstPmaddwd, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xf6, Prefix660F, InstPsadbw, Binary);
  DEF_BINST(Vdq, Udq)(NACLi_SSE2, 0xf7, Prefix660F, InstMaskmovdqu, Compare);
  NaClDefOp(RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));

  DEF_BINST(Vdq, Mdq)(NACLi_SSE3, 0xf0, PrefixF20F, InstLddqu, Move);

  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xf8, Prefix660F, InstPsubb, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xf9, Prefix660F, InstPsubw, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xfa, Prefix660F, InstPsubd, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xfb, Prefix660F, InstPsubq, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xfc, Prefix660F, InstPaddb, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xfd, Prefix660F, InstPaddw, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xfe, Prefix660F, InstPaddd, Binary);
}

static void NaClDefMmxInsts() {
  DEF_BINST(Pq_, Qq_)(NACLi_SSSE3, 0x08, Prefix0F38, InstPsignb, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_SSSE3, 0x09, Prefix0F38, InstPsignw, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_SSSE3, 0x0A, Prefix0F38, InstPsignd, Binary);

  DEF_BINST(Pq_, Qd_)(NACLi_MMX, 0x60, Prefix0F, InstPunpcklbw, Binary);
  DEF_BINST(Pq_, Qd_)(NACLi_MMX, 0x61, Prefix0F, InstPunpcklwd, Binary);
  DEF_BINST(Pq_, Qd_)(NACLi_MMX, 0x62, Prefix0F, InstPunpckldq, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0x63, Prefix0F, InstPacksswb, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0x64, Prefix0F, InstPcmpgtb, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0x65, Prefix0F, InstPcmpgtw, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0x66, Prefix0F, InstPcmpgtd, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0x67, Prefix0F, InstPackuswb, Binary);

  DEF_BINST(Pq_, Qd_)(NACLi_MMX, 0x68, Prefix0F, InstPunpckhbw, Binary);
  DEF_BINST(Pq_, Qd_)(NACLi_MMX, 0x69, Prefix0F, InstPunpckhwd, Binary);
  DEF_BINST(Pq_, Qd_)(NACLi_MMX, 0x6a, Prefix0F, InstPunpckhdq, Binary);
  DEF_BINST(Pq_, Qd_)(NACLi_MMX, 0x6b, Prefix0F, InstPackssdw, Binary);
  DEF_BINST(Pq_, EdQ)(NACLi_MMX, 0x6e, Prefix0F, InstMovd, Move);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0x6f, Prefix0F, InstMovq, Move);

  DEF_OINST(Nq_, I__)(NACLi_MMX, 0x71, Prefix0F, Opcode2, InstPsrlw, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  DEF_OINST(Nq_, I__)(NACLi_MMX, 0x71, Prefix0F, Opcode4, InstPsraw, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  DEF_OINST(Nq_, I__)(NACLi_MMX, 0x71, Prefix0F, Opcode6, InstPsllw, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  DEF_OINST(Nq_, I__)(NACLi_MMX, 0x72, Prefix0F, Opcode2, InstPsrld, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  DEF_OINST(Nq_, I__)(NACLi_MMX, 0x72, Prefix0F, Opcode4, InstPsrad, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  DEF_OINST(Nq_, I__)(NACLi_MMX, 0x72, Prefix0F, Opcode6, InstPslld, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  DEF_OINST(Nq_, I__)(NACLi_MMX, 0x73, Prefix0F, Opcode2, InstPsrlq, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  DEF_OINST(Nq_, I__)(NACLi_MMX, 0x73, Prefix0F, Opcode6, InstPsllq, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));

  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0x74, Prefix0F, InstPcmpeqb, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0x75, Prefix0F, InstPcmpeqw, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0x76, Prefix0F, InstPcmpeqd, Binary);
  DEF_NULL_OPRDS_INST(NACLi_MMX, 0x77, Prefix0F, InstEmms);

  DEF_BINST(EdQ, PdQ)(NACLi_MMX, 0x7e, Prefix0F, InstMovd, Move);
  DEF_BINST(Qq_, Pq_)(NACLi_MMX, 0x7f, Prefix0F, InstMovq, Move);

  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xd1, Prefix0F, InstPsrlw, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xd2, Prefix0F, InstPsrld, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xd3, Prefix0F, InstPsrlq, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xd4, Prefix0F, InstPaddq, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xd5, Prefix0F, InstPmullw, Binary);
  DEF_BINST(Gd_, Nq_)(NACLi_MMX, 0xd7, Prefix0F, InstPmovmskb, Move);

  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xd8, Prefix0F, InstPsubusb, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xd9, Prefix0F, InstPsubusw, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xda, Prefix0F, InstPminub, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xdb, Prefix0F, InstPand, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xdc, Prefix0F, InstPaddusb, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xdd, Prefix0F, InstPaddusw, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xde, Prefix0F, InstPmaxub, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xdf, Prefix0F, InstPandn, Binary);

  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xe0, Prefix0F, InstPavgb, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xe1, Prefix0F, InstPsraw, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xe2, Prefix0F, InstPsrad, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xe3, Prefix0F, InstPavgw, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xe4, Prefix0F, InstPmulhuw, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xe5, Prefix0F, InstPmulhw, Binary);
  DEF_BINST(Mq_, Pq_)(NACLi_MMX, 0xe7, Prefix0F, InstMovntq, Move);

  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xe8, Prefix0F, InstPsubsb, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xe9, Prefix0F, InstPsubsw, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xea, Prefix0F, InstPminsw, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xeb, Prefix0F, InstPor, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xec, Prefix0F, InstPaddsb, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xed, Prefix0F, InstPaddsw, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xee, Prefix0F, InstPmaxsw, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xef, Prefix0F, InstPxor, Binary);

  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xf1, Prefix0F, InstPsllw, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xf2, Prefix0F, InstPslld, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xf3, Prefix0F, InstPsllq, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xf4, Prefix0F, InstPmuludq, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xf5, Prefix0F, InstPmaddwd, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xf6, Prefix0F, InstPsadbw, Binary);
  DEF_BINST(Pq_, Nq_)(NACLi_MMX, 0xf7, Prefix0F, InstMaskmovq, Compare);
  NaClDefOp(RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));

  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xf8, Prefix0F, InstPsubb, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xf9, Prefix0F, InstPsubw, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xfa, Prefix0F, InstPsubd, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xfb, Prefix0F, InstPsubq, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xfc, Prefix0F, InstPaddb, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xfd, Prefix0F, InstPaddw, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0xfe, Prefix0F, InstPaddd, Binary);
}

static void NaClDefNarySseInsts() {
  /* TODO(karl) - Check what other instructions should be using g_OperandSize_v
   * and g_OpreandSize_o to check sizes (using new flag OperandSizeIgnore66 to
   * help differentiate sizes).
   */

  /* Define other forms of MMX and XMM operations. */

  NaClDefInstPrefix(Prefix660F3A);
  NaClDefInstChoices(0x61, 2);
  NaClDefInst(0x61,
               NACLi_SSE42,
               NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) |
               NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
               InstPcmpestri);
  NaClDefOp(RegECX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegEAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegEDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(Xmm_G_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(Xmm_E_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInstPrefix(Prefix660F3A);
  NaClDefInst(0x61,
               NACLi_SSE42,
               NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) |
               NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OpcodeUsesRexW),
               InstPcmpestri);
  NaClDefOp(RegRCX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegRAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegRDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(Xmm_G_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(Xmm_E_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInstPrefix(Prefix660F3A);
  NaClDefInstChoices(0x60, 2);
  NaClDefInst(0x60,
               NACLi_SSE42,
               NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) |
               NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
               InstPcmpestrm);
  NaClDefOp(RegXMM0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegEAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegEDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(Xmm_G_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(Xmm_E_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInstPrefix(Prefix660F3A);
  NaClDefInst(0x60,
               NACLi_SSE42,
               NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b) |
               NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OpcodeUsesRexW),
               InstPcmpestrm);
  NaClDefOp(RegXMM0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegRAX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(RegRDX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(Xmm_G_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(Xmm_E_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInstPrefix(Prefix660F3A);
  NaClDefInstChoices(0x63, 2);
  NaClDefInst(0x63,
               NACLi_SSE42,
               NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b)|
               NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v),
               InstPcmpistri);
  NaClDefOp(RegECX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(Xmm_G_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(Xmm_E_Operand, NACL_OPFLAG(OpUse));

  NaClDefInstPrefix(Prefix660F3A);
  NaClDefInst(0x63,
               NACLi_SSE42,
               NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b)|
               NACL_IFLAG(OperandSize_o) | NACL_IFLAG(OpcodeUsesRexW),
               InstPcmpistri);
  NaClDefOp(RegRCX, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(Xmm_G_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(Xmm_E_Operand, NACL_OPFLAG(OpUse));

  NaClDefInstPrefix(Prefix660F3A);
  NaClDefInst(0x62,
               NACLi_SSE42,
               NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
               InstPcmpistrm);
  NaClDefOp(RegXMM0, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OpImplicit));
  NaClDefOp(Xmm_G_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(Xmm_E_Operand, NACL_OPFLAG(OpUse));

  NaClDefInstPrefix(Prefix660F3A);
  /* Note: shares opcode with pextrw (uses different operand sizes). */
  NaClDefInst(0x14,
               NACLi_SSE41,
               NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b) |
               NACL_IFLAG(OpcodeHasImmed),
               InstPextrb);
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OperandZeroExtends_v));
  NaClDefOp(Xmm_G_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInstPrefix(Prefix660F3A);
  NaClDefInstChoices(0x16, 2);
  NaClDefInst(0x16,
               NACLi_SSE41,
               NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_v) |
               NACL_IFLAG(OpcodeHasImmed_b),
               InstPextrd);
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(Xmm_G_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInstPrefix(Prefix660F3A);
  NaClDefInst(0x16,
               NACLi_SSE41,
               NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_o) |
               NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeUsesRexW),
               InstPextrq);
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(Xmm_G_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  DEF_BINST(Gd_, Nq_)(NACLi_SSE41, 0xc5, Prefix0F, InstPextrw, Move);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  NaClAddOpFlags(0, NACL_OPFLAG(OperandZeroExtends_v));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  DEF_BINST(Gd_, Udq)(NACLi_SSE41, 0xc5, Prefix660F, InstPextrw, Move);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  NaClAddOpFlags(0, NACL_OPFLAG(OperandZeroExtends_v));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInstPrefix(Prefix660F3A);
  NaClDefInst(0x15,
               NACLi_SSE41,
               NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_w) |
               NACL_IFLAG(OpcodeHasImmed_b),
               InstPextrw);
  NaClDefOp(E_Operand, NACL_OPFLAG(OpSet) | NACL_OPFLAG(OperandZeroExtends_v));
  NaClDefOp(Xmm_G_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInstPrefix(Prefix660F3A);
  NaClDefInst(0x20,
               NACLi_SSE41,
               NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_b) |
               NACL_IFLAG(OpcodeHasImmed),
               InstPinsrb);
  NaClDefOp(Xmm_G_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInstPrefix(Prefix660F3A);
  /* Note: opcode shares with pinsrq (uses different operand sizes). */
  NaClDefInstChoices(0x22, 2);
  NaClDefInst(0x22,
               NACLi_SSE41,
               NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_v) |
               NACL_IFLAG(OpcodeHasImmed_b),
               InstPinsrd);
  NaClDefOp(Xmm_G_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInstPrefix(Prefix660F3A);
  NaClDefInst(0x22,
               NACLi_SSE41,
               NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_o) |
               NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeUsesRexW),
               InstPinsrq);
  NaClDefOp(Xmm_G_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInstPrefix(Prefix0F);
  NaClDefInst(0xC4,
               NACLi_SSE,
               NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_w) |
               NACL_IFLAG(OpcodeHasImmed_b),
               InstPinsrw);
  NaClDefOp(Mmx_G_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInstPrefix(Prefix660F);
  NaClDefInst(0xC4,
               NACLi_SSE,
               NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OperandSize_w) |
               NACL_IFLAG(OpcodeHasImmed_b),
               InstPinsrw);
  NaClDefOp(Xmm_G_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(E_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInstPrefix(Prefix660F38);
  NaClDefInst(0x17,
               NACLi_SSE41,
               NACL_IFLAG(OpcodeUsesModRm),
               InstPtest);
  NaClDefOp(Xmm_G_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(Xmm_E_Operand, NACL_OPFLAG(OpUse));

  /* f2 0f c2 /r ib  cmpsd xmm1, xmm2/m64, imm8  SSE2 RexR */
  NaClDefInstPrefix(PrefixF20F);
  NaClDefInst(0xc2,
              NACLi_SSE2,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
              InstCmpsd_xmm);
  NaClDefOp(Xmm_G_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(Xmm_E_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));
}

void NaClDefSseInsts() {
  NaClDefDefaultInstPrefix(NoPrefix);
  NaClDefNarySseInsts();
  NaClDefBinarySseInsts();
  NaClDefMmxInsts();
}
