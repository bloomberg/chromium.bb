/*
 * Copyright (c) 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that be
 * found in the LICENSE file.
 */

/*
 * Defines SSE instructions. Taken from:
 * (1) Tables A-3 and A-4 of appendix "A2 - Opcode Encodings", in AMD
 *     document 24594-Rev.3.14-September 2007: "AMD64 Architecture
 *     Programmer's manual Volume 3: General-Purpose and System Instructions".
 * (2) Table A-4 of "appendix A.3", in Intel Document 253667-030US (March 2009):
 *     "Intel 64 and IA-32 Architectures Software Developer's Manual,
 *     Volume 2g: Instruction Set Reference, N-Z."
 */

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

#include "native_client/src/trusted/validator_x86/ncdecode_forms.h"
#include "native_client/src/trusted/validator_x86/ncdecode_tablegen.h"

/* Define a generic way for computing the array size of a declared array. */
#if !defined(ARRAYSIZE)
#define ARRAYSIZE(a) sizeof(a)/sizeof(a[0])
#endif

/* See Appendex A of the AMD manual for tables describing the formats
 * of these instructions.
 */

static void NaClDefBinarySseInsts(struct NaClSymbolTable* st) {

  NaClDefine("660f3800: Pshufb $Vdq, $Wdq", NACLi_SSSE3, st, Binary);
  NaClDefine("660f3801: Phaddw $Vdq, $Wdq", NACLi_SSSE3, st, Binary);
  NaClDefine("660f3802: Phaddd $Vdq, $Wdq", NACLi_SSSE3, st, Binary);
  NaClDefine("660f3803: Phaddsw $Vdq, $Wdq", NACLi_SSSE3, st, Binary);
  NaClDefine("660f3804: Pmaddubsw $Vdq, $Wdq", NACLi_SSSE3, st, Binary);
  NaClDefine("660f3805: Phsubw $Vdq, $Wdq", NACLi_SSSE3, st, Binary);
  NaClDefine("660f3806: Phsubd $Vdq, $Wdq", NACLi_SSSE3, st, Binary);
  NaClDefine("660f3807: Phsubsw $Vdq, $Wdq", NACLi_SSSE3, st, Binary);
  NaClDefine("660f3808: Psignb $Vdq  $Wdq", NACLi_SSSE3, st, Binary);
  NaClDefine("660f3809: Psignw $Vdq, $Wdq", NACLi_SSSE3, st, Binary);
  NaClDefine("660f380a: Psignd $Vdq, $Wdq", NACLi_SSSE3, st, Binary);
  NaClDefine("660f380b: Pmulhrsw $Vdq, $Wdq", NACLi_SSSE3, st, Binary);
  NaClDefIter("660f380C+@i: Invalid", 0, 3, NACLi_INVALID, st, Other);
  NaClDefine("660f3810: Pblendvb $Vdq, $Wdq, %xmm0", NACLi_SSE41, st, Binary);
  NaClDefIter("660f3811+@i: Invalid", 0, 2, NACLi_INVALID, st, Other);
  NaClDefine("660f3814: Blendvps $Vdq, $Wdq, %xmm0", NACLi_SSE41, st, Binary);
  NaClDefine("660f3815: Blendvpd $Vdq, $Wdq, %xmm0", NACLi_SSE41, st, Binary);
  NaClDefine("660f3816: Invalid", NACLi_INVALID, st, Other);
  NaClDefine("660f3817: Ptest $Vdq, $Wdq", NACLi_SSE41, st, Compare);
  NaClDefIter("660f3818+@i: Invalid", 0, 3, NACLi_INVALID, st, Other);
  NaClDefine("660f381c: Pabsb $Vdq, $Wdq", NACLi_SSSE3, st, Move);
  NaClDefine("660f381d: Pabsw $Vdq, $Wdq", NACLi_SSSE3, st, Move);
  NaClDefine("660f381e: Pabsd $Vdq, $Wdq", NACLi_SSSE3, st, Move);
  NaClDefine("660f381f: Invalid", NACLi_INVALID, st, Other);
  NaClDefine("660f3820: Pmovsxbw $Vdq, $Udq/Mq", NACLi_SSE41, st, Move);
  NaClDefine("660f3821: Pmovsxbd $Vdq, $Udq/Md", NACLi_SSE41, st, Move);
  NaClDefine("660f3822: Pmovsxbq $Vdq, $Udq/Mw", NACLi_SSE41, st, Move);
  NaClDefine("660f3823: Pmovsxwd $Vdq, $Udq/Mq", NACLi_SSE41, st, Move);
  NaClDefine("660f3824: Pmovsxwq $Vdq, $Udq/Md", NACLi_SSE41, st, Move);
  NaClDefine("660f3825: Pmovsxdq $Vdq, $Udq/Mq", NACLi_SSE41, st, Move);
  NaClDefIter("660f3826+@i: Invalid", 0, 1, NACLi_INVALID, st, Other);
  NaClDefine("660f3828: Pmuldq $Vdq, $Wdq", NACLi_SSE41, st, Binary);
  NaClDefine("660f3829: Pcmpeqq $Vdq, $Wdq", NACLi_SSE41, st, Binary);
  NaClDefine("660f382a: Movntdqa $Vdq, $Wdq", NACLi_SSE41, st, Move);
  NaClDefine("660f382b: Packusdw $Vdq, $Wdq", NACLi_SSE41, st, Binary);
  NaClDefIter("660f382c+@i: Invalid", 0, 3, NACLi_INVALID, st, Other);
  NaClDefine("660f3830: Pmovzxbw $Vdq, $Udq/Mq", NACLi_SSE41, st, Move);
  NaClDefine("660f3831: Pmovzxbd $Vdq, $Udq/Md", NACLi_SSE41, st, Move);
  NaClDefine("660f3832: Pmovzxbq $Vdq, $Udq/Mw", NACLi_SSE41, st, Move);
  NaClDefine("660f3833: Pmovzxwd $Vdq, $Udq/Mq", NACLi_SSE41, st, Move);
  NaClDefine("660f3834: Pmovzxwq $Vdq, $Udq/Md", NACLi_SSE41, st, Move);
  NaClDefine("660f3835: Pmovzxdq $Vdq, $Udq/Mq", NACLi_SSE41, st, Move);
  NaClDefine("660f3836: Invalid", NACLi_INVALID, st, Other);
  NaClDefine("660f3837: Pcmpgtq $Vdq, $Wdq", NACLi_SSE42, st, Binary);
  NaClDefine("660f3838: Pminsb $Vdq, $Wdq", NACLi_SSE41, st, Binary);
  NaClDefine("660f3839: Pminsd $Vdq, $Wdq", NACLi_SSE41, st, Binary);
  NaClDefine("660f383a: Pminuw $Vdq, $Wdq", NACLi_SSE41, st, Binary);
  NaClDefine("660f383b: Pminud $Vdq, $Wdq", NACLi_SSE41, st, Binary);
  NaClDefine("660f383c: Pmaxsb $Vdq, $Wdq", NACLi_SSE41, st, Binary);
  NaClDefine("660f383d: Pmaxsd $Vdq, $Wdq", NACLi_SSE41, st, Binary);
  NaClDefine("660f383e: Pmaxuw $Vdq, $Wdq", NACLi_SSE41, st, Binary);
  NaClDefine("660f383f: Pmaxud $Vdq, $Wdq", NACLi_SSE41, st, Binary);
  NaClDefine("660f3840: Pmulld $Vdq, $Wdq", NACLi_SSE41, st, Binary);
  NaClDefine("660f3841: Phminposuw $Vdq, $Wdq", NACLi_SSE41, st, Binary);
  NaClDefIter("660f3842+@i: Invalid", 0, 61, NACLi_INVALID, st, Other);
  NaClDef_32("660f3880: Invept $Gd, $Mdq", NACLi_VMX, st, Uses);
  NaClDef_64("660f3880: Invept $Gq, $Mdq", NACLi_VMX, st, Uses);
  NaClDef_32("660f3881: Invvpid $Gd, $Mdq", NACLi_VMX, st, Uses);
  NaClDef_64("660f3881: Invvpid $Gq, $Mdq", NACLi_VMX, st, Uses);
  NaClDefIter("660f3882+@i: Invalid", 0, 109, NACLi_INVALID, st, Other);
  /* Let 660f38f0 and 660f38f1 default to 0f38f0 and 0f38f1! */
  /* TODO(karl): Verify that assumption above is true! */
  NaClDefIter("660f38f2+@i: Invalid", 0, 13, NACLi_INVALID, st, Other);

  /* Note: Since 0f38XX instructions don't allow a REPNE prefix, we
   * can skip filling blank entries in the table. The corresponding
   * 0f38XX instruction will be explicitly rejected.
   */
  NaClDefine("f20f38f0: Crc32 $Gd, $Eb", NACLi_SSE42, st, Binary);
  NaClDefine("f20f38f1: Crc32 $Gd, $Ev", NACLi_SSE42, st, Binary);

  DEF_BINST(Vps, Wps)(NACLi_SSE, 0x10, Prefix0F, InstMovups, Move);
  DEF_BINST(Wps, Vps)(NACLi_SSE, 0x11, Prefix0F, InstMovups, Move);

  NaClDefPrefixInstChoices(Prefix0F, 0x12, 2);
  DEF_BINST(Vps, Mq_)(NACLi_SSE, 0x12, Prefix0F, InstMovlps, Move);
  NaClAddIFlags(NACL_IFLAG(ModRmModIsnt0x3));
  DEF_BINST(Vps, Uq_)(NACLi_SSE, 0x12, Prefix0F, InstMovhlps, Move);
  NaClAddIFlags(NACL_IFLAG(ModRmModIs0x3));

  DEF_BINST(Mq_, Vps)(NACLi_SSE, 0x13, Prefix0F, InstMovlps, Move);
  DEF_BINST(Vps, Wq_)(NACLi_SSE, 0x14, Prefix0F, InstUnpcklps, Binary);
  DEF_BINST(Vps, Wq_)(NACLi_SSE, 0x15, Prefix0F, InstUnpckhps, Binary);

  NaClDefPrefixInstChoices(Prefix0F, 0x16, 2);
  DEF_BINST(Vps, Mq_)(NACLi_SSE, 0x16, Prefix0F, InstMovhps, Move);
  NaClAddIFlags(NACL_IFLAG(ModRmModIsnt0x3));
  DEF_BINST(Vps, Uq_)(NACLi_SSE, 0x16, Prefix0F, InstMovlhps, Move);
  NaClAddIFlags(NACL_IFLAG(ModRmModIs0x3));

  DEF_BINST(Mq_, Vps)(NACLi_SSE, 0x17, Prefix0F, InstMovhps, Move);

  DEF_BINST(Vss, Wss)(NACLi_SSE, 0x10, PrefixF30F, InstMovss, Move);
  DEF_BINST(Wss, Vss)(NACLi_SSE, 0x11, PrefixF30F, InstMovss, Move);
  DEF_BINST(Vps, Wps)(NACLi_SSE3, 0x12, PrefixF30F, InstMovsldup, Move);
  NaClDefInvalidIcode(PrefixF30F, 0x13);
  NaClDefInvalidIcode(PrefixF30F, 0x14);
  NaClDefInvalidIcode(PrefixF30F, 0x15);
  DEF_BINST(Vps, Wps)(NACLi_SSE3, 0x16, PrefixF30F, InstMovshdup, Move);
  NaClDefInvalidIcode(PrefixF30F, 0x17);

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
  NaClDefInvalidIcode(PrefixF20F, 0x13);
  NaClDefInvalidIcode(PrefixF20F, 0x14);
  NaClDefInvalidIcode(PrefixF20F, 0x15);
  NaClDefInvalidIcode(PrefixF20F, 0x16);
  NaClDefInvalidIcode(PrefixF20F, 0x17);


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

  NaClDefInvalidIcode(PrefixF30F, 0x28);
  NaClDefInvalidIcode(PrefixF30F, 0x29);
  DEF_BINST(Vss, EdQ)(NACLi_SSE, 0x2a, PrefixF30F, InstCvtsi2ss, Move);
  DEF_BINST(Md_, Vss)(NACLi_SSE4A, 0x2b, PrefixF30F, InstMovntss, Move);
  DEF_BINST(GdQ, Wss)(NACLi_SSE, 0x2c, PrefixF30F, InstCvttss2si, Move);
  DEF_BINST(GdQ, Wss)(NACLi_SSE, 0x2d, PrefixF30F, InstCvtss2si, Move);
  NaClDefInvalidIcode(PrefixF30F, 0x2e);
  NaClDefInvalidIcode(PrefixF30F, 0x2f);

  DEF_BINST(Vpd, Wpd)(NACLi_SSE2, 0x28, Prefix660F, InstMovapd, Move);
  DEF_BINST(Wpd, Vpd)(NACLi_SSE2, 0x29, Prefix660F, InstMovapd, Move);
  DEF_BINST(Vpd, Qq_)(NACLi_SSE2, 0x2a, Prefix660F, InstCvtpi2pd, Move);
  DEF_BINST(Mdq, Vpd)(NACLi_SSE2, 0x2b, Prefix660F, InstMovntpd, Move);
  DEF_BINST(Pq_, Wpd)(NACLi_SSE2, 0x2c, Prefix660F, InstCvttpd2pi, Move);
  DEF_BINST(Pq_, Wpd)(NACLi_SSE2, 0x2d, Prefix660F, InstCvtpd2pi, Move);
  DEF_BINST(Vsd, Wsd)(NACLi_SSE2, 0x2e, Prefix660F, InstUcomisd, Compare);
  DEF_BINST(Vsd, Wsd)(NACLi_SSE2, 0x2f, Prefix660F, InstComisd, Compare);

  NaClDefInvalidIcode(PrefixF20F, 0x28);
  NaClDefInvalidIcode(PrefixF20F, 0x29);
  DEF_BINST(Vsd, EdQ)(NACLi_SSE2, 0x2a, PrefixF20F, InstCvtsi2sd, Move);
  DEF_BINST(Mq_, Vsd)(NACLi_SSE4A, 0x2b, PrefixF20F, InstMovntsd, Move);
  DEF_BINST(GdQ, Wsd)(NACLi_SSE2, 0x2c, PrefixF20F, InstCvttsd2si, Move);
  DEF_BINST(GdQ, Wsd)(NACLi_SSE2, 0x2d, PrefixF20F, InstCvtsd2si, Move);
  NaClDefInvalidIcode(PrefixF20F, 0x2e);
  NaClDefInvalidIcode(PrefixF20F, 0x2f);

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
  NaClDefInvalidIcode(Prefix660F, 0x52);
  NaClDefInvalidIcode(Prefix660F, 0x53);
  DEF_BINST(Vpd, Wpd)(NACLi_SSE2, 0x54, Prefix660F, InstAndpd, Binary);
  DEF_BINST(Vpd, Wpd)(NACLi_SSE2, 0x55, Prefix660F, InstAndnpd, Binary);
  DEF_BINST(Vpd, Wpd)(NACLi_SSE2, 0x56, Prefix660F, InstOrpd, Binary);
  DEF_BINST(Vpd, Wpd)(NACLi_SSE2, 0x57, Prefix660F, InstXorpd, Binary);

  NaClDefInvalidIcode(PrefixF20F, 0x50);
  DEF_BINST(Vsd, Wsd)(NACLi_SSE2, 0x51, PrefixF20F, InstSqrtsd, Move);
  NaClAddIFlags(NACL_IFLAG(OperandSizeDefaultIs64));
  NaClDefInvalidIcode(PrefixF20F, 0x52);
  NaClDefInvalidIcode(PrefixF20F, 0x53);
  NaClDefInvalidIcode(PrefixF20F, 0x54);
  NaClDefInvalidIcode(PrefixF20F, 0x55);
  NaClDefInvalidIcode(PrefixF20F, 0x56);
  NaClDefInvalidIcode(PrefixF20F, 0x57);

  NaClDefInvalidIcode(PrefixF30F, 0x50);
  DEF_BINST(Vss, Wss)(NACLi_SSE, 0x51, PrefixF30F, InstSqrtss, Move);
  DEF_BINST(Vss, Wss)(NACLi_SSE, 0x52, PrefixF30F, InstRsqrtss, Move);
  DEF_BINST(Vss, Wss)(NACLi_SSE, 0x53, PrefixF30F, InstRcpss, Move);
  NaClDefInvalidIcode(PrefixF30F, 0x54);
  NaClDefInvalidIcode(PrefixF30F, 0x55);
  NaClDefInvalidIcode(PrefixF30F, 0x56);
  NaClDefInvalidIcode(PrefixF30F, 0x57);

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
  NaClDefInvalidIcode(PrefixF20F, 0x5b);
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

  NaClDefInvalidIcode(PrefixF20F, 0x60);
  NaClDefInvalidIcode(PrefixF20F, 0x61);
  NaClDefInvalidIcode(PrefixF20F, 0x62);
  NaClDefInvalidIcode(PrefixF20F, 0x63);
  NaClDefInvalidIcode(PrefixF20F, 0x64);
  NaClDefInvalidIcode(PrefixF20F, 0x65);
  NaClDefInvalidIcode(PrefixF20F, 0x66);
  NaClDefInvalidIcode(PrefixF20F, 0x67);

  NaClDefInvalidIcode(PrefixF30F, 0x60);
  NaClDefInvalidIcode(PrefixF30F, 0x61);
  NaClDefInvalidIcode(PrefixF30F, 0x62);
  NaClDefInvalidIcode(PrefixF30F, 0x63);
  NaClDefInvalidIcode(PrefixF30F, 0x64);
  NaClDefInvalidIcode(PrefixF30F, 0x65);
  NaClDefInvalidIcode(PrefixF30F, 0x66);
  /* TODO(karl) Implement MOVDQU for F3 0F 67 */
  NaClDefInvalidIcode(PrefixF30F, 0x67);

  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0x6f, PrefixF30F, InstMovdqu, Move);

  DEF_BINST(Vdq, Wq_)(NACLi_SSE2, 0x68, Prefix660F, InstPunpckhbw, Binary);
  DEF_BINST(Vdq, Wq_)(NACLi_SSE2, 0x69, Prefix660F, InstPunpckhwd, Binary);
  DEF_BINST(Vdq, Wq_)(NACLi_SSE2, 0x6a, Prefix660F, InstPunpckhdq, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0x6b, Prefix660F, InstPackssdw, Binary);
  DEF_BINST(Vdq, Wq_)(NACLi_SSE2, 0x6c, Prefix660F, InstPunpcklqdq, Binary);
  DEF_BINST(Vdq, Wq_)(NACLi_SSE2, 0x6d, Prefix660F, InstPunpckhqdq, Binary);
  DEF_BINST(Vdq, EdQ)(NACLi_SSE2, 0x6e, Prefix660F, InstMovd, Move);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0x6f, Prefix660F, InstMovdqa, Move);

  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0x70, Prefix660F, InstPshufd, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));
  NaClDefInvModRmInst(Prefix660F, 0x71, 0);
  NaClDefInvModRmInst(Prefix660F, 0x71, 1);
  DEF_OINST(Udq, I__)(NACLi_SSE2, 0x71, Prefix660F, 2, InstPsrlw, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  NaClDefInvModRmInst(Prefix660F, 0x71, 3);
  DEF_OINST(Udq, I__)(NACLi_SSE2, 0x71, Prefix660F, 4, InstPsraw, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  NaClDefInvModRmInst(Prefix660F, 0x71, 5);
  DEF_OINST(Udq, I__)(NACLi_SSE3, 0x71, Prefix660F, 6, InstPsllw, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  NaClDefInvModRmInst(Prefix660F, 0x71, 7);
  NaClDefInvModRmInst(Prefix660F, 0x72, 0);
  NaClDefInvModRmInst(Prefix660F, 0x72, 1);
  DEF_OINST(Udq, I__)(NACLi_SSE2, 0x72, Prefix660F, 2, InstPsrld, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  NaClDefInvModRmInst(Prefix660F, 0x72, 3);
  DEF_OINST(Udq, I__)(NACLi_SSE2, 0x72, Prefix660F, 4, InstPsrad, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  NaClDefInvModRmInst(Prefix660F, 0x72, 5);
  DEF_OINST(Udq, I__)(NACLi_SSE2, 0x72, Prefix660F, 6, InstPslld, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  NaClDefInvModRmInst(Prefix660F, 0x72, 7);
  NaClDefInvModRmInst(Prefix660F, 0x73, 0);
  NaClDefInvModRmInst(Prefix660F, 0x73, 1);
  DEF_OINST(Udq, I__)(NACLi_SSE2, 0x73, Prefix660F, 2, InstPsrlq, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  DEF_OINST(Udq, I__)(NACLi_SSE2x, 0x73, Prefix660F, 3, InstPsrldq,
                      Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  NaClDefInvModRmInst(Prefix660F, 0x73, 4);
  NaClDefInvModRmInst(Prefix660F, 0x73, 5);
  DEF_OINST(Udq, I__)(NACLi_SSE2, 0x73, Prefix660F, 6, InstPsllq, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  DEF_OINST(Udq, I__)(NACLi_SSE2x, 0x73, Prefix660F, 7, InstPslldq,
                      Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0x74, Prefix660F, InstPcmpeqb, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0x75, Prefix660F, InstPcmpeqw, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0x76, Prefix660F, InstPcmpeqd, Binary);
  NaClDefInvalidIcode(Prefix660F, 0x77);

  DEF_BINST(Vq_, Wq_)(NACLi_SSE2, 0x70, PrefixF20F, InstPshuflw, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));
  NaClDefInvalidIcode(PrefixF20F, 0x71);
  NaClDefInvalidIcode(PrefixF20F, 0x72);
  NaClDefInvalidIcode(PrefixF20F, 0x73);
  NaClDefInvalidIcode(PrefixF20F, 0x74);
  NaClDefInvalidIcode(PrefixF20F, 0x75);
  NaClDefInvalidIcode(PrefixF20F, 0x76);
  NaClDefInvalidIcode(PrefixF20F, 0x77);

  DEF_BINST(Vq_, Wq_)(NACLi_SSE2, 0x70, PrefixF30F, InstPshufhw, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));
  NaClDefInvalidIcode(PrefixF30F, 0x71);
  NaClDefInvalidIcode(PrefixF30F, 0x72);
  NaClDefInvalidIcode(PrefixF30F, 0x73);
  NaClDefInvalidIcode(PrefixF30F, 0x74);
  NaClDefInvalidIcode(PrefixF30F, 0x75);
  NaClDefInvalidIcode(PrefixF30F, 0x76);
  NaClDefInvalidIcode(PrefixF30F, 0x77);

  NaClDelaySanityChecks();
  /* Note: opcode also defines register to use (i.e. xmm0). */
  DEF_OINST(Vdq, I__)(NACLi_SSE4A, 0x78, Prefix660F, 0, InstExtrq,
                      Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeHasImmed2_b));
  NaClAddOpFlags(0, NACL_OPFLAG(AllowGOperandWithOpcodeInModRm));
  NaClDefOp(I2_Operand, NACL_IFLAG(OpUse));
  NaClApplySanityChecks();
  NaClDefInvModRmInst(Prefix660F, 0x78, 1);
  NaClDefInvModRmInst(Prefix660F, 0x78, 2);
  NaClDefInvModRmInst(Prefix660F, 0x78, 3);
  NaClDefInvModRmInst(Prefix660F, 0x78, 4);
  NaClDefInvModRmInst(Prefix660F, 0x78, 5);
  NaClDefInvModRmInst(Prefix660F, 0x78, 6);
  NaClDefInvModRmInst(Prefix660F, 0x78, 7);
  DEF_BINST(Vdq, Uq_)(NACLi_SSE4A, 0x79, Prefix660F, InstExtrq, Binary);
  NaClDefInvalidIcode(Prefix660F, 0x7a);
  NaClDefInvalidIcode(Prefix660F, 0x7b);
  DEF_BINST(Vpd, Wpd)(NACLi_SSE2, 0x7c, Prefix660F, InstHaddpd, Binary);
  DEF_BINST(Vpd, Wpd)(NACLi_SSE2, 0x7d, Prefix660F, InstHsubpd, Binary);
  DEF_BINST(EdQ, VdQ)(NACLi_SSE2, 0x7e, Prefix660F, InstMovd, Move);
  DEF_BINST(Wdq, Vdq)(NACLi_SSE2, 0x7f, Prefix660F, InstMovdqa, Move);

  DEF_BINST(Vdq, Uq_)(NACLi_SSE4A, 0x78, PrefixF20F, InstInsertq, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OpcodeHasImmed2_b));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(I2_Operand, NACL_OPFLAG(OpUse));
  DEF_BINST(Vdq, Udq)(NACLi_SSE4A, 0x79, PrefixF20F, InstInsertq, Binary);
  NaClDefInvalidIcode(PrefixF20F, 0x7a);
  NaClDefInvalidIcode(PrefixF20F, 0x7b);
  DEF_BINST(Vps, Wps)(NACLi_SSE3, 0x7c, PrefixF20F, InstHaddps, Binary);
  DEF_BINST(Vps, Wps)(NACLi_SSE3, 0x7d, PrefixF20F, InstHsubps, Binary);
  NaClDefInvalidIcode(PrefixF20F, 0x7e);
  NaClDefInvalidIcode(PrefixF20F, 0x7f);

  NaClDefInvalidIcode(PrefixF30F, 0x78);
  NaClDefInvalidIcode(PrefixF30F, 0x79);
  NaClDefInvalidIcode(PrefixF30F, 0x7a);
  NaClDefInvalidIcode(PrefixF30F, 0x7b);
  NaClDefInvalidIcode(PrefixF30F, 0x7c);
  NaClDefInvalidIcode(PrefixF30F, 0x7d);
  DEF_BINST(Vq_, Wq_)(NACLi_SSE2, 0x7e, PrefixF30F, InstMovq, Move);
  DEF_BINST(Wdq, Vdq)(NACLi_SSE2, 0x7f, PrefixF30F, InstMovdqu, Move);

  NaClDefInvalidIcode(Prefix660F, 0xb8);
  NaClDefInvalidIcode(Prefix660F, 0xb9);
  NaClDefInvalidIcode(Prefix660F, 0xba);
  NaClDefInvalidIcode(Prefix660F, 0xbb);
  NaClDefInvalidIcode(Prefix660F, 0xbc);
  NaClDefInvalidIcode(Prefix660F, 0xbd);
  NaClDefInvalidIcode(Prefix660F, 0xbf);

  NaClDefInvalidIcode(PrefixF20F, 0xb8);
  NaClDefInvalidIcode(PrefixF20F, 0xb9);
  NaClDefInvalidIcode(PrefixF20F, 0xba);
  NaClDefInvalidIcode(PrefixF20F, 0xbb);
  NaClDefInvalidIcode(PrefixF20F, 0xbc);
  NaClDefInvalidIcode(PrefixF20F, 0xbd);
  NaClDefInvalidIcode(PrefixF20F, 0xbe);
  NaClDefInvalidIcode(PrefixF20F, 0xbf);

  NaClDefInvalidIcode(PrefixF30F, 0xb9);
  NaClDefInvalidIcode(PrefixF30F, 0xba);
  NaClDefInvalidIcode(PrefixF30F, 0xbb);
  NaClDefInvalidIcode(PrefixF30F, 0xbc);
  NaClDefInvalidIcode(PrefixF30F, 0xbd);
  NaClDefInvalidIcode(PrefixF30F, 0xbe);
  NaClDefInvalidIcode(PrefixF30F, 0xbf);

  DEF_BINST(MdQ, GdQ)(NACLi_SSE2, 0xc3, Prefix0F, InstMovnti, Move);
  DEF_BINST(Pq_, E__)(NACLi_SSE, 0xc4, Prefix0F, InstPinsrw, Move);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OperandSize_w));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));
  DEF_BINST(Gd_, Nq_)(NACLi_SSE41, 0xc5, Prefix0F, InstPextrw, Move);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  NaClAddOpFlags(0, NACL_OPFLAG(OperandZeroExtends_v));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));
  DEF_BINST(Vps, Wps)(NACLi_SSE, 0xc6, Prefix0F, InstShufps, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInvalidIcode(Prefix660F, 0xc3);
  DEF_BINST(Vdq, E__)(NACLi_SSE, 0xc4, Prefix660F, InstPinsrw, Move);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b) | NACL_IFLAG(OperandSize_w));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));
  DEF_BINST(Gd_, Udq)(NACLi_SSE41, 0xc5, Prefix660F, InstPextrw, Move);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  NaClAddOpFlags(0, NACL_OPFLAG(OperandZeroExtends_v));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));
  DEF_BINST(Vpd, Wpd)(NACLi_SSE2, 0xC6, Prefix660F, InstShufpd, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  NaClDefInvalidIcode(PrefixF20F, 0xc3);
  NaClDefInvalidIcode(PrefixF20F, 0xc4);
  NaClDefInvalidIcode(PrefixF20F, 0xc5);
  NaClDefInvalidIcode(PrefixF20F, 0xc6);

  NaClDefInvalidIcode(PrefixF30F, 0xc3);
  NaClDefInvalidIcode(PrefixF30F, 0xc4);
  NaClDefInvalidIcode(PrefixF30F, 0xc5);
  NaClDefInvalidIcode(PrefixF30F, 0xc6);

  NaClDefInvalidIcode(Prefix0F, 0xd0);
  NaClDefInvalidIcode(Prefix0F, 0xd6);

  DEF_BINST(Vps, Wps)(NACLi_SSE3, 0xd0, PrefixF20F, InstAddsubps, Binary);
  NaClDefInvalidIcode(PrefixF20F, 0xd1);
  NaClDefInvalidIcode(PrefixF20F, 0xd2);
  NaClDefInvalidIcode(PrefixF20F, 0xd3);
  NaClDefInvalidIcode(PrefixF20F, 0xd4);
  NaClDefInvalidIcode(PrefixF20F, 0xd5);
  DEF_BINST(Pq_, Uq_)(NACLi_SSE2, 0xd6, PrefixF20F, InstMovdq2q, Move);
  NaClDefInvalidIcode(PrefixF20F, 0xd7);

  NaClDefInvalidIcode(PrefixF30F, 0xd0);
  NaClDefInvalidIcode(PrefixF30F, 0xd1);
  NaClDefInvalidIcode(PrefixF30F, 0xd2);
  NaClDefInvalidIcode(PrefixF30F, 0xd3);
  NaClDefInvalidIcode(PrefixF30F, 0xd4);
  NaClDefInvalidIcode(PrefixF30F, 0xd5);
  DEF_BINST(Vdq, Uq_)(NACLi_SSE2, 0xd6, PrefixF30F, InstMovq2dq, Move);
  NaClDefInvalidIcode(PrefixF30F, 0xd7);

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

  NaClDefInvalidIcode(Prefix0F, 0xe6);

  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xe0, Prefix660F, InstPavgb, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xe1, Prefix660F, InstPsraw, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xe2, Prefix660F, InstPsrad, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xe3, Prefix660F, InstPavgw, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xe4, Prefix660F, InstPmulhuw, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xe5, Prefix660F, InstPmulhw, Binary);
  DEF_BINST(Vq_, Wdq)(NACLi_SSE2, 0xe6, Prefix660F, InstCvttpd2dq, Move);
  DEF_BINST(Mdq, Vdq)(NACLi_SSE2, 0xe7, Prefix660F, InstMovntdq, Move);

  NaClDefInvalidIcode(PrefixF20F, 0xd8);
  NaClDefInvalidIcode(PrefixF20F, 0xd9);
  NaClDefInvalidIcode(PrefixF20F, 0xda);
  NaClDefInvalidIcode(PrefixF20F, 0xdb);
  NaClDefInvalidIcode(PrefixF20F, 0xdc);
  NaClDefInvalidIcode(PrefixF20F, 0xdd);
  NaClDefInvalidIcode(PrefixF20F, 0xde);
  NaClDefInvalidIcode(PrefixF20F, 0xdf);

  NaClDefInvalidIcode(PrefixF30F, 0xd8);
  NaClDefInvalidIcode(PrefixF30F, 0xd9);
  NaClDefInvalidIcode(PrefixF30F, 0xda);
  NaClDefInvalidIcode(PrefixF30F, 0xdb);
  NaClDefInvalidIcode(PrefixF30F, 0xdc);
  NaClDefInvalidIcode(PrefixF30F, 0xdd);
  NaClDefInvalidIcode(PrefixF30F, 0xde);
  NaClDefInvalidIcode(PrefixF30F, 0xdf);

  NaClDefInvalidIcode(PrefixF30F, 0xe0);
  NaClDefInvalidIcode(PrefixF30F, 0xe1);
  NaClDefInvalidIcode(PrefixF30F, 0xe2);
  NaClDefInvalidIcode(PrefixF30F, 0xe3);
  NaClDefInvalidIcode(PrefixF30F, 0xe4);
  NaClDefInvalidIcode(PrefixF30F, 0xe5);
  DEF_BINST(Vpd, Wq_)(NACLi_SSE2, 0xe6, PrefixF30F, InstCvtdq2pd, Move);
  NaClDefInvalidIcode(PrefixF30F, 0xe7);

  NaClDefInvalidIcode(PrefixF20F, 0xe0);
  NaClDefInvalidIcode(PrefixF20F, 0xe1);
  NaClDefInvalidIcode(PrefixF20F, 0xe2);
  NaClDefInvalidIcode(PrefixF20F, 0xe3);
  NaClDefInvalidIcode(PrefixF20F, 0xe4);
  NaClDefInvalidIcode(PrefixF20F, 0xe5);
  DEF_BINST(Vq_, Wpd)(NACLi_SSE2, 0xe6, PrefixF20F, InstCvtpd2dq, Move);
  NaClDefInvalidIcode(PrefixF20F, 0xe7);

  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xe8, Prefix660F, InstPsubsb, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xe9, Prefix660F, InstPsubsw, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xea, Prefix660F, InstPminsw, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xeb, Prefix660F, InstPor, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xec, Prefix660F, InstPaddsb, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xed, Prefix660F, InstPaddsw, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xee, Prefix660F, InstPmaxsw, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xef, Prefix660F, InstPxor, Binary);

  NaClDefInvalidIcode(PrefixF20F, 0xe8);
  NaClDefInvalidIcode(PrefixF20F, 0xe9);
  NaClDefInvalidIcode(PrefixF20F, 0xea);
  NaClDefInvalidIcode(PrefixF20F, 0xeb);
  NaClDefInvalidIcode(PrefixF20F, 0xec);
  NaClDefInvalidIcode(PrefixF20F, 0xed);
  NaClDefInvalidIcode(PrefixF20F, 0xee);
  NaClDefInvalidIcode(PrefixF20F, 0xef);

  NaClDefInvalidIcode(PrefixF30F, 0xe8);
  NaClDefInvalidIcode(PrefixF30F, 0xe9);
  NaClDefInvalidIcode(PrefixF30F, 0xea);
  NaClDefInvalidIcode(PrefixF30F, 0xeb);
  NaClDefInvalidIcode(PrefixF30F, 0xec);
  NaClDefInvalidIcode(PrefixF30F, 0xed);
  NaClDefInvalidIcode(PrefixF30F, 0xee);
  NaClDefInvalidIcode(PrefixF30F, 0xef);

  NaClDefInvalidIcode(Prefix0F, 0xf0);

  NaClDefInvalidIcode(Prefix660F, 0xf0);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xf1, Prefix660F, InstPsllw, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xf2, Prefix660F, InstPslld, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xf3, Prefix660F, InstPsllq, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xf4, Prefix660F, InstPmuludq, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xf5, Prefix660F, InstPmaddwd, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xf6, Prefix660F, InstPsadbw, Binary);
  DEF_BINST(Vdq, Udq)(NACLi_SSE2, 0xf7, Prefix660F, InstMaskmovdqu, Compare);
  NaClDefOp(RegDS_EDI, NACL_OPFLAG(OpUse) | NACL_OPFLAG(OpImplicit));

  DEF_BINST(Vdq, Mdq)(NACLi_SSE3, 0xf0, PrefixF20F, InstLddqu, Move);
  NaClDefInvalidIcode(PrefixF20F, 0xf1);
  NaClDefInvalidIcode(PrefixF20F, 0xf2);
  NaClDefInvalidIcode(PrefixF20F, 0xf3);
  NaClDefInvalidIcode(PrefixF20F, 0xf4);
  NaClDefInvalidIcode(PrefixF20F, 0xf5);
  NaClDefInvalidIcode(PrefixF20F, 0xf6);
  NaClDefInvalidIcode(PrefixF20F, 0xf7);

  NaClDefInvalidIcode(PrefixF30F, 0xf0);
  NaClDefInvalidIcode(PrefixF30F, 0xf1);
  NaClDefInvalidIcode(PrefixF30F, 0xf2);
  NaClDefInvalidIcode(PrefixF30F, 0xf3);
  NaClDefInvalidIcode(PrefixF30F, 0xf4);
  NaClDefInvalidIcode(PrefixF30F, 0xf5);
  NaClDefInvalidIcode(PrefixF30F, 0xf6);
  NaClDefInvalidIcode(PrefixF30F, 0xf7);

  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xf8, Prefix660F, InstPsubb, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xf9, Prefix660F, InstPsubw, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xfa, Prefix660F, InstPsubd, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xfb, Prefix660F, InstPsubq, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xfc, Prefix660F, InstPaddb, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xfd, Prefix660F, InstPaddw, Binary);
  DEF_BINST(Vdq, Wdq)(NACLi_SSE2, 0xfe, Prefix660F, InstPaddd, Binary);
  NaClDefInvalidIcode(Prefix660F, 0xff);

  NaClDefInvalidIcode(PrefixF20F, 0xf8);
  NaClDefInvalidIcode(PrefixF20F, 0xf9);
  NaClDefInvalidIcode(PrefixF20F, 0xfa);
  NaClDefInvalidIcode(PrefixF20F, 0xfb);
  NaClDefInvalidIcode(PrefixF20F, 0xfc);
  NaClDefInvalidIcode(PrefixF20F, 0xfd);
  NaClDefInvalidIcode(PrefixF20F, 0xfe);
  NaClDefInvalidIcode(PrefixF20F, 0xff);

  NaClDefInvalidIcode(PrefixF30F, 0xf8);
  NaClDefInvalidIcode(PrefixF30F, 0xf9);
  NaClDefInvalidIcode(PrefixF30F, 0xfa);
  NaClDefInvalidIcode(PrefixF30F, 0xfb);
  NaClDefInvalidIcode(PrefixF30F, 0xfc);
  NaClDefInvalidIcode(PrefixF30F, 0xfd);
  NaClDefInvalidIcode(PrefixF30F, 0xfe);
  NaClDefInvalidIcode(PrefixF30F, 0xff);
}

static void NaClDefMmxInsts(struct NaClSymbolTable* st) {
  NaClDefine("0f3800: Pshufb $Pq, $Qq", NACLi_SSSE3, st, Binary);
  NaClDefine("0f3801: Phaddw $Pq, $Qq", NACLi_SSSE3, st, Binary);
  NaClDefine("0f3802: Phaddd $Pq, $Qq", NACLi_SSSE3, st, Binary);
  NaClDefine("0f3803: Phaddsw $Pq, $Qq", NACLi_SSSE3, st, Binary);
  NaClDefine("0f3804: Pmaddubsw $Pq, $Qq", NACLi_SSSE3, st, Binary);
  NaClDefine("0f3805: Phsubw $Pq, $Qq", NACLi_SSSE3, st, Binary);
  NaClDefine("0f3806: Phsubd $Pq, $Qq", NACLi_SSSE3, st, Binary);
  NaClDefine("0f3807: Phsubsw $Pq, $Qq", NACLi_SSSE3, st, Binary);
  NaClDefine("0f3808: Psignb $Pq  $Qq", NACLi_SSSE3, st, Binary);
  NaClDefine("0f3809: Psignw $Pq, $Qq", NACLi_SSSE3, st, Binary);
  NaClDefine("0f380a: Psignd $Pq, $Qq", NACLi_SSSE3, st, Binary);
  NaClDefine("0f380b: Pmulhrsw $Pq, $Qq", NACLi_SSSE3, st, Binary);
  NaClDefIter("0f380c+@i: Invalid", 0, 15, NACLi_INVALID, st, Other);
  NaClDefine("0f381c: Pabsb $Pq, $Qq", NACLi_SSSE3, st, Move);
  NaClDefine("0f381d: Pabsw $Pq, $Qq", NACLi_SSSE3, st, Move);
  NaClDefine("0f381e: Pabsd $Pq, $Qq", NACLi_SSSE3, st, Move);
  NaClDefIter("0f381f+@i: Invalid", 0, 208, NACLi_INVALID, st, Other);
  NaClDefine("0f38f0: Movbe $Gv, $Mv", NACLi_MOVBE, st, Move);
  NaClDefine("0f38f1: Movbe $Mv, $Gv", NACLi_MOVBE, st, Move);
  NaClDefIter("0f38f2+@i: Invalid", 0, 13, NACLi_INVALID, st, Other);

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

  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0x70, Prefix0F, InstPshufw, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));
  NaClDefInvModRmInst(Prefix0F, 0x71, 0);
  NaClDefInvModRmInst(Prefix0F, 0x71, 1);
  DEF_OINST(Nq_, I__)(NACLi_MMX, 0x71, Prefix0F, 2, InstPsrlw, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  NaClDefInvModRmInst(Prefix0F, 0x71, 3);
  DEF_OINST(Nq_, I__)(NACLi_MMX, 0x71, Prefix0F, 4, InstPsraw, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  NaClDefInvModRmInst(Prefix0F, 0x71, 5);
  DEF_OINST(Nq_, I__)(NACLi_MMX, 0x71, Prefix0F, 6, InstPsllw, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  NaClDefInvModRmInst(Prefix0F, 0x71, 7);
  NaClDefInvModRmInst(Prefix0F, 0x72, 0);
  NaClDefInvModRmInst(Prefix0F, 0x72, 1);
  DEF_OINST(Nq_, I__)(NACLi_MMX, 0x72, Prefix0F, 2, InstPsrld, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  NaClDefInvModRmInst(Prefix0F, 0x72, 3);
  DEF_OINST(Nq_, I__)(NACLi_MMX, 0x72, Prefix0F, 4, InstPsrad, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  NaClDefInvModRmInst(Prefix0F, 0x72, 5);
  DEF_OINST(Nq_, I__)(NACLi_MMX, 0x72, Prefix0F, 6, InstPslld, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  NaClDefInvModRmInst(Prefix0F, 0x72, 7);
  NaClDefInvModRmInst(Prefix0F, 0x73, 0);
  NaClDefInvModRmInst(Prefix0F, 0x73, 1);
  DEF_OINST(Nq_, I__)(NACLi_MMX, 0x73, Prefix0F, 2, InstPsrlq, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  NaClDefInvModRmInst(Prefix0F, 0x73, 3);
  NaClDefInvModRmInst(Prefix0F, 0x73, 4);
  NaClDefInvModRmInst(Prefix0F, 0x73, 5);
  DEF_OINST(Nq_, I__)(NACLi_MMX, 0x73, Prefix0F, 6, InstPsllq, Binary);
  NaClAddIFlags(NACL_IFLAG(OpcodeHasImmed_b));
  NaClDefInvModRmInst(Prefix0F, 0x73, 7);

  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0x74, Prefix0F, InstPcmpeqb, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0x75, Prefix0F, InstPcmpeqw, Binary);
  DEF_BINST(Pq_, Qq_)(NACLi_MMX, 0x76, Prefix0F, InstPcmpeqd, Binary);
  DEF_NULL_OPRDS_INST(NACLi_MMX, 0x77, Prefix0F, InstEmms);

  NaClDefInvalidIcode(Prefix0F, 0x78);
  NaClDefInvalidIcode(Prefix0F, 0x79);
  NaClDefInvalidIcode(Prefix0F, 0x7a);
  NaClDefInvalidIcode(Prefix0F, 0x7b);
  NaClDefInvalidIcode(Prefix0F, 0x7c);
  NaClDefInvalidIcode(Prefix0F, 0x7d);
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
  NaClDefInvalidIcode(Prefix0F, 0xff);
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

  /* f2 0f c2 /r ib  cmpsd xmm1, xmm2/m64, imm8  SSE2 RexR */
  NaClDefInstPrefix(PrefixF20F);
  NaClDefInst(0xc2,
              NACLi_SSE2,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
              InstCmpsd_xmm);
  NaClDefOp(Xmm_G_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(Xmm_E_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));

  /* f3 0f c2 /r ib  cmpss xmm1, xmm2/m64, imm8  SSE RexR */
  NaClDefInstPrefix(PrefixF30F);
  NaClDefInst(0xc2,
              NACLi_SSE,
              NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(OpcodeHasImmed_b),
              InstCmpss);
  NaClDefOp(Xmm_G_Operand, NACL_OPFLAG(OpSet));
  NaClDefOp(Xmm_E_Operand, NACL_OPFLAG(OpUse));
  NaClDefOp(I_Operand, NACL_OPFLAG(OpUse));
}

void NaClDefSseInsts(struct NaClSymbolTable* st) {
  NaClDefDefaultInstPrefix(NoPrefix);
  NaClDefNarySseInsts();
  NaClDefBinarySseInsts(st);
  NaClDefMmxInsts(st);
}
