/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that be
 * found in the LICENSE file.
 */

/*
 * Defines SSE instructions. Taken from:
 * (1) Tables A-3 and A-4 of appendix "A2 - Opcode Encodings", in AMD
 *     document 24594-Rev.3.14-September 2007: "AMD64 Architecture
 *     Programmer's manual Volume 3: General-Purpose and System Instructions".
 * (2) Table A-4 and A-5 of "appendix A.3", in Intel Document 253667-030US
 *     (March 2009): "Intel 64 and IA-32 Architectures Software Developer's
 *     Manual, Volume 2g: Instruction Set Reference, N-Z."
 *
 * Note: Invalid etries with f2 (REPNE) and f3 (REP) can be ommitted in
 * the tables, because the corresponding entry without that prefix will
 * be explicitly rejected (unless it is one of the special instructions
 * that allow such prefixes).
 */

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

#include "native_client/src/trusted/validator/x86/decoder/generator/ncdecode_forms.h"
#include "native_client/src/trusted/validator/x86/decoder/generator/ncdecode_tablegen.h"

static void NaClDefBinarySseInsts(struct NaClSymbolTable* st) {
  NaClDefPrefixInstChoices(Prefix0F, 0x12, 2);
  NaClDefPrefixInstChoices(Prefix0F, 0x16, 2);
  NaClDefPrefixInstChoices_32_64(Prefix660F, 0x6e, 1, 2);
  NaClDefPrefixInstChoices_32_64(Prefix660F, 0x7e, 1, 2);
  NaClDefine("   0f3800:     Pshufb $Pq, $Qq",     NACLi_SSSE3,   st, Binary);
  NaClDefine("   0f3801:     Phaddw $Pq, $Qq",     NACLi_SSSE3,   st, Binary);
  NaClDefine("   0f3802:     Phaddd $Pq, $Qq",     NACLi_SSSE3,   st, Binary);
  NaClDefine("   0f3803:     Phaddsw $Pq, $Qq",    NACLi_SSSE3,   st, Binary);
  NaClDefine("   0f3804:     Pmaddubsw $Pq, $Qq",  NACLi_SSSE3,   st, Binary);
  NaClDefine("   0f3805:     Phsubw $Pq, $Qq",     NACLi_SSSE3,   st, Binary);
  NaClDefine("   0f3806:     Phsubd $Pq, $Qq",     NACLi_SSSE3,   st, Binary);
  NaClDefine("   0f3807:     Phsubsw $Pq, $Qq",    NACLi_SSSE3,   st, Binary);
  NaClDefine("   0f3808:     Psignb $Pq  $Qq",     NACLi_SSSE3,   st, Binary);
  NaClDefine("   0f3809:     Psignw $Pq, $Qq",     NACLi_SSSE3,   st, Binary);
  NaClDefine("   0f380a:     Psignd $Pq, $Qq",     NACLi_SSSE3,   st, Binary);
  NaClDefine("   0f380b:     Pmulhrsw $Pq, $Qq",   NACLi_SSSE3,   st, Binary);
  NaClDefIter("  0f380c+@i:  Invalid", 0, 15,      NACLi_INVALID, st, Other);
  NaClDefine("   0f381c:     Pabsb $Pq, $Qq",      NACLi_SSSE3,   st, Move);
  NaClDefine("   0f381d:     Pabsw $Pq, $Qq",      NACLi_SSSE3,   st, Move);
  NaClDefine("   0f381e:     Pabsd $Pq, $Qq",      NACLi_SSSE3,   st, Move);
  NaClDefIter("  0f381f+@i:  Invalid", 0, 208,     NACLi_INVALID, st, Other);
  NaClDefine("   0f38f0:     Movbe $Gv, $Mv",      NACLi_MOVBE,   st, Move);
  NaClDefine("   0f38f1:     Movbe $Mv, $Gv",      NACLi_MOVBE,   st, Move);
  NaClDefIter("  0f38f2+@i:  Invalid", 0, 13,      NACLi_INVALID, st, Other);
  NaClDefine(" 660f3800:     Pshufb $Vdq, $Wdq",   NACLi_SSSE3,   st, Binary);
  NaClDefine(" 660f3801:     Phaddw $Vdq, $Wdq",   NACLi_SSSE3,   st, Binary);
  NaClDefine(" 660f3802:     Phaddd $Vdq, $Wdq",   NACLi_SSSE3,   st, Binary);
  NaClDefine(" 660f3803:     Phaddsw $Vdq, $Wdq",  NACLi_SSSE3,   st, Binary);
  NaClDefine(" 660f3804:     Pmaddubsw $Vdq, $Wdq",NACLi_SSSE3,   st, Binary);
  NaClDefine(" 660f3805:     Phsubw $Vdq, $Wdq",   NACLi_SSSE3,   st, Binary);
  NaClDefine(" 660f3806:     Phsubd $Vdq, $Wdq",   NACLi_SSSE3,   st, Binary);
  NaClDefine(" 660f3807:     Phsubsw $Vdq, $Wdq",  NACLi_SSSE3,   st, Binary);
  NaClDefine(" 660f3808:     Psignb $Vdq  $Wdq",   NACLi_SSSE3,   st, Binary);
  NaClDefine(" 660f3809:     Psignw $Vdq, $Wdq",   NACLi_SSSE3,   st, Binary);
  NaClDefine(" 660f380a:     Psignd $Vdq, $Wdq",   NACLi_SSSE3,   st, Binary);
  NaClDefine(" 660f380b:     Pmulhrsw $Vdq, $Wdq", NACLi_SSSE3,   st, Binary);
  NaClDefIter("660f380C+@i:  Invalid", 0, 3,       NACLi_INVALID, st, Other);
  NaClDefine(" 660f3810:     Pblendvb $Vdq, $Wdq, %xmm0",
                                                   NACLi_SSE41, st, Binary);
  NaClDefIter("660f3811+@i:  Invalid", 0, 2,       NACLi_INVALID, st, Other);
  NaClDefine(" 660f3814:     Blendvps $Vdq, $Wdq, %xmm0",
                                                   NACLi_SSE41, st, Binary);
  NaClDefine(" 660f3815:     Blendvpd $Vdq, $Wdq, %xmm0",
                                                   NACLi_SSE41, st, Binary);
  NaClDefine(" 660f3816:     Invalid",             NACLi_INVALID, st, Other);
  NaClDefine(" 660f3817:     Ptest $Vdq, $Wdq",    NACLi_SSE41,   st, Compare);
  NaClDefIter("660f3818+@i:  Invalid", 0, 3,       NACLi_INVALID, st, Other);
  NaClDefine(" 660f381c:     Pabsb $Vdq, $Wdq",    NACLi_SSSE3,   st, Move);
  NaClDefine(" 660f381d:     Pabsw $Vdq, $Wdq",    NACLi_SSSE3,   st, Move);
  NaClDefine(" 660f381e:     Pabsd $Vdq, $Wdq",    NACLi_SSSE3,   st, Move);
  NaClDefine(" 660f381f:     Invalid",             NACLi_INVALID, st, Other);
  NaClDefine(" 660f3820:     Pmovsxbw $Vdq, $Udq/Mq",
                                                   NACLi_SSE41,   st, Move);
  NaClDefine(" 660f3821:     Pmovsxbd $Vdq, $Udq/Md",
                                                   NACLi_SSE41,   st, Move);
  NaClDefine(" 660f3822:     Pmovsxbq $Vdq, $Udq/Mw",
                                                   NACLi_SSE41,   st, Move);
  NaClDefine(" 660f3823:     Pmovsxwd $Vdq, $Udq/Mq",
                                                   NACLi_SSE41,   st, Move);
  NaClDefine(" 660f3824:     Pmovsxwq $Vdq, $Udq/Md",
                                                   NACLi_SSE41,   st, Move);
  NaClDefine(" 660f3825:     Pmovsxdq $Vdq, $Udq/Mq",
                                                   NACLi_SSE41,   st, Move);
  NaClDefIter("660f3826+@i:  Invalid", 0, 1,       NACLi_INVALID, st, Other);
  NaClDefine(" 660f3828:     Pmuldq $Vdq, $Wdq",   NACLi_SSE41,   st, Binary);
  NaClDefine(" 660f3829:     Pcmpeqq $Vdq, $Wdq",  NACLi_SSE41,   st, Binary);
  NaClDefine(" 660f382a:     Movntdqa $Vdq, $Mdq", NACLi_SSE41,   st, Move);
  NaClDefine(" 660f382b:     Packusdw $Vdq, $Wdq", NACLi_SSE41,   st, Binary);
  NaClDefIter("660f382c+@i:  Invalid", 0, 3,       NACLi_INVALID, st, Other);
  NaClDefine(" 660f3830:     Pmovzxbw $Vdq, $Udq/Mq",
                                                   NACLi_SSE41,   st, Move);
  NaClDefine(" 660f3831:     Pmovzxbd $Vdq, $Udq/Md",
                                                   NACLi_SSE41,   st, Move);
  NaClDefine(" 660f3832:     Pmovzxbq $Vdq, $Udq/Mw",
                                                   NACLi_SSE41,   st, Move);
  NaClDefine(" 660f3833:     Pmovzxwd $Vdq, $Udq/Mq",
                                                   NACLi_SSE41,   st, Move);
  NaClDefine(" 660f3834:     Pmovzxwq $Vdq, $Udq/Md",
                                                   NACLi_SSE41,   st, Move);
  NaClDefine(" 660f3835:     Pmovzxdq $Vdq, $Udq/Mq",
                                                   NACLi_SSE41,   st, Move);
  NaClDefine(" 660f3836:     Invalid",             NACLi_INVALID, st, Other);
  NaClDefine(" 660f3837:     Pcmpgtq $Vdq, $Wdq",  NACLi_SSE42,   st, Binary);
  NaClDefine(" 660f3838:     Pminsb $Vdq, $Wdq",   NACLi_SSE41,   st, Binary);
  NaClDefine(" 660f3839:     Pminsd $Vdq, $Wdq",   NACLi_SSE41,   st, Binary);
  NaClDefine(" 660f383a:     Pminuw $Vdq, $Wdq",   NACLi_SSE41,   st, Binary);
  NaClDefine(" 660f383b:     Pminud $Vdq, $Wdq",   NACLi_SSE41,   st, Binary);
  NaClDefine(" 660f383c:     Pmaxsb $Vdq, $Wdq",   NACLi_SSE41,   st, Binary);
  NaClDefine(" 660f383d:     Pmaxsd $Vdq, $Wdq",   NACLi_SSE41,   st, Binary);
  NaClDefine(" 660f383e:     Pmaxuw $Vdq, $Wdq",   NACLi_SSE41,   st, Binary);
  NaClDefine(" 660f383f:     Pmaxud $Vdq, $Wdq",   NACLi_SSE41,   st, Binary);
  NaClDefine(" 660f3840:     Pmulld $Vdq, $Wdq",   NACLi_SSE41,   st, Binary);
  NaClDefine(" 660f3841:     Phminposuw $Vdq, $Wdq",
                                                   NACLi_SSE41,   st, Binary);
  NaClDefIter("660f3842+@i:  Invalid", 0, 61,      NACLi_INVALID, st, Other);
  NaClDef_32(" 660f3880:     Invept $Gd, $Mdq",    NACLi_VMX,     st, Uses);
  NaClDef_64(" 660f3880:     Invept $Gq, $Mdq",    NACLi_VMX,     st, Uses);
  NaClDef_32(" 660f3881:     Invvpid $Gd, $Mdq",   NACLi_VMX,     st, Uses);
  NaClDef_64(" 660f3881:     Invvpid $Gq, $Mdq",   NACLi_VMX,     st, Uses);
  NaClDefIter("660f3882+@i:  Invalid", 0, 109,     NACLi_INVALID, st, Other);
  NaClDefIter("660f38f2+@i:  Invalid", 0, 13,      NACLi_INVALID, st, Other);
  NaClDefine(" f20f38f0:     Crc32 $Gd, $Eb",      NACLi_SSE42,   st, Binary);
  NaClDefine(" f20f38f1:     Crc32 $Gd, $Ev",      NACLi_SSE42,   st, Binary);
  NaClDefine("     0f10:     Movups $Vps, $Wps",   NACLi_SSE,     st, Move);
  NaClDefine("     0f11:     Movups $Wps, $Vps",   NACLi_SSE,     st, Move);
  NaClDefine("     0f12:     Movlps $Vps, $Mq",    NACLi_SSE,     st, Move);
  NaClDefine("     0f12:     Movhlps $Vps, $VRq",  NACLi_SSE,     st, Move);
  NaClDefine("     0f13:     Movlps $Mq, $Vps",    NACLi_SSE,     st, Move);
  NaClDefine("     0f14:     Unpcklps $Vps, $Wq",  NACLi_SSE,     st, Binary);
  NaClDefine("     0f15:     Unpckhps $Vps, $Wq",  NACLi_SSE,     st, Binary);
  NaClDefine("     0f16:     Movhps $Vps, $Mq",    NACLi_SSE,     st, Move);
  NaClDefine("     0f16:     Movlhps $Vps, $VRq",  NACLi_SSE,     st, Move);
  NaClDefine("     0f17:     Movhps $Mq, $Vps",    NACLi_SSE,     st, Move);
  NaClDefine("   f30f10:     Movss $Vss, $Wss",    NACLi_SSE,     st, Move);
  NaClDefine("   f30f11:     Movss $Wss, $Vss",    NACLi_SSE,     st, Move);
  NaClDefine("   f30f12:     Movsldup $Vps, $Wps", NACLi_SSE3,    st, Move);
  NaClDefIter("  f30f13+@i:  Invalid", 0, 2,       NACLi_INVALID, st, Other);
  NaClDefine("   f30f16:     Movshdup $Vps, $Wps", NACLi_SSE3,    st, Move);
  NaClDefine("   f30f17:     Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("   660f10:     Movupd $Vpd, $Wpd",   NACLi_SSE2,    st, Move);
  NaClDefine("   660f11:     Movupd $Wpd, $Vpd",   NACLi_SSE2,    st, Move);
  NaClDefine("   660f12:     Movlpd $Vsd, $Mq",    NACLi_SSE2,    st, Move);
  NaClDefine("   660f13:     Movlpd $Mq, $Vsd",    NACLi_SSE2,    st, Move);
  NaClDefine("   660f14:     Unpcklpd $Vpd, $Wq",  NACLi_SSE2,    st, Binary);
  NaClDefine("   660f15:     Unpckhpd $Vpd, $Wq",  NACLi_SSE2,    st, Binary);
  NaClDefine("   660f16:     Movhpd $Vsd, $Mq",    NACLi_SSE2,    st, Move);
  NaClDefine("   660f17:     Movhpd $Mq, $Vsd",    NACLi_SSE2,    st, Move);
  NaClDefine("   f20f10:     Movsd $Vsd, $Wsd",    NACLi_SSE2,    st, Move);
  NaClDefine(   "f20f11:     Movsd $Wsd, $Vsd",    NACLi_SSE2,    st, Move);
  NaClDefine("   f20f12:     Movddup $Vpd, $Wsd",  NACLi_SSE3,    st, Move);
  NaClDefine("     0f28:     Movaps $Vps, $Wps",   NACLi_SSE,     st, Move);
  NaClDefine("     0f29:     Movaps $Wps, $Vps",   NACLi_SSE,     st, Move);
  NaClDefine("     0f2a:     Cvtpi2ps $Vps, $Qq",  NACLi_SSE,     st, Move);
  NaClDefine("     0f2b:     Movntps $Mdq, $Vps",  NACLi_SSE,     st, Move);
  NaClDefine("     0f2c:     Cvttps2pi $Pq, $Wps", NACLi_SSE,     st, Move);
  NaClDefine("     0f2d:     Cvtps2pi $Pq, $Wps",  NACLi_SSE,     st, Move);
  NaClDefine("     0f2e:     Ucomiss $Vss, $Wss",  NACLi_SSE,     st, Compare);
  NaClDefine("     0f2f:     Comiss $Vps, $Wps",   NACLi_SSE,     st, Compare);
  NaClDefIter("  f30f28+@i:  Invalid", 0, 1,       NACLi_INVALID, st, Other);
  NaClDefine("   f30f2a:     Cvtsi2ss $Vss, $Ed/q",NACLi_SSE,     st, Move);
  NaClDefine("   f30f2b:     Movntss $Md, $Vss",   NACLi_SSE4A,   st, Move);
  NaClDefine("   f30f2c:     Cvttss2si $Gd/q, $Wss",
                                                   NACLi_SSE,     st, Move);
  NaClDefine("   f30f2d:     Cvtss2si $Gd/q, $Wss",
                                                   NACLi_SSE,     st, Move);
  NaClDefIter("  f30f2e+@i:  Invalid", 0, 1,       NACLi_INVALID, st, Other);
  NaClDefine("   660f28:     Movapd $Vpd, $Wpd",   NACLi_SSE2,    st, Move);
  NaClDefine("   660f29:     Movapd $Wpd, $Vpd",   NACLi_SSE2,    st, Move);
  NaClDefine("   660f2a:     Cvtpi2pd $Vpd, $Qq",  NACLi_SSE2,    st, Move);
  NaClDefine("   660f2b:     Movntpd $Mdq, $Vpd",  NACLi_SSE2,    st, Move);
  NaClDefine("   660f2c:     Cvttpd2pi $Pq, $Wpd",
                                                   NACLi_SSE2,    st, Move);
  NaClDefine("   660f2d:     Cvtpd2pi $Pq, $Wpd",
                                                   NACLi_SSE2,    st, Move);
  NaClDefine("   660f2e:     Ucomisd $Vsd, $Wsd",  NACLi_SSE2,    st, Compare);
  NaClDefine("   660f2f:     Comisd $Vpd, $Wsd",   NACLi_SSE2,    st, Compare);
  NaClDefIter("  f20f28+@i:  Invalid", 0, 1,       NACLi_INVALID, st, Other);
  NaClDefine("   f20f2a:     Cvtsi2sd $Vsd, $Ed/q",
                                                   NACLi_SSE2,    st, Move);
  NaClDefine("   f20f2b:     Movntsd $Mq, $Vsd",    NACLi_SSE4A,   st, Move);
  NaClDefine("   f20f2c:     Cvttsd2si $Gd/q, $Wsd",
                                                   NACLi_SSE2,    st, Move);
  NaClDefine("   f20f2d:     Cvtsd2si $Gd/q, $Wsd",
                                                   NACLi_SSE2,    st, Move);
  NaClDefIter("  f20f2e+@i:  Invalid", 0, 1,       NACLi_INVALID, st, Other);
  NaClDefine("     0f50:     Movmskps $Gd, $VRps", NACLi_SSE,     st, Move);
  NaClDefine("     0f51:     Sqrtps $Vps, $Wps",   NACLi_SSE,     st, Move);
  NaClDefine("     0f52:     Rsqrtps $Vps, $Wps",  NACLi_SSE,     st, Move);
  NaClDefine("     0f53:     Rcpps $Vps, $Wps",    NACLi_SSE,     st, Move);
  NaClDefine("     0f54:     Andps $Vps, $Wps",    NACLi_SSE,     st, Binary);
  NaClDefine("     0f55:     Andnps $Vps, $Wps",   NACLi_SSE,     st, Binary);
  NaClDefine("     0f56:     Orps $Vps, $Wps",     NACLi_SSE,     st, Binary);
  NaClDefine("     0f57:     Xorps $Vps, $Wps",    NACLi_SSE,     st, Binary);
  NaClDefine("   f30f50:     Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("   f30f51:     Sqrtss $Vps, $Wps",   NACLi_SSE,     st, Move);
  NaClDefine("   f30f52:     Rsqrtss $Vss, $Wss",  NACLi_SSE,     st, Move);
  NaClDefine("   f30f53:     Rcpss $Vss, $Wss",    NACLi_SSE,     st, Move);
  NaClDefIter("  f30f54+@i:  Invalid", 0, 3,       NACLi_INVALID, st, Other);
  NaClDefine("   660f50:     Movmskpd $Gd, $VRpd", NACLi_SSE2,    st, Move);
  NaClDefine("   660f51:     Sqrtpd $Vps, $Wpd",   NACLi_SSE2,    st, Move);
  NaClDefIter("  660f52+@i:  Invalid", 0, 1,       NACLi_INVALID, st, Other);
  NaClDefine("   660f54:     Andpd $Vpd, $Wpd",    NACLi_SSE2,    st, Binary);
  NaClDefine("   660f55:     Andnpd $Vpd, $Wpd",   NACLi_SSE2,    st, Binary);
  NaClDefine("   660f56:     Orpd $Vpd, $Wpd",     NACLi_SSE2,    st, Binary);
  NaClDefine("   660f57:     Xorpd $Vpd, $Wpd",    NACLi_SSE2,    st, Binary);
  NaClDefine("   f20f50:     Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("   f20f51:     Sqrtsd $Vsd, $Wsd",   NACLi_SSE2,    st, Move);
  NaClDefIter("  f20f52+@i:  Invalid", 0, 5,       NACLi_INVALID, st, Other);
  NaClDefine("     0f58:     Addps $Vps, $Wps",    NACLi_SSE,     st, Binary);
  NaClDefine("     0f59:     Mulps $Vps, $Wps",    NACLi_SSE,     st, Binary);
  NaClDefine("     0f5a:     Cvtps2pd $Vpd, $Wps", NACLi_SSE2,    st, Move);
  NaClDefine("     0f5b:     Cvtdq2ps $Vps, $Wdq", NACLi_SSE2,    st, Move);
  NaClDefine("     0f5c:     Subps $Vps, $Wps",    NACLi_SSE,     st, Binary);
  NaClDefine("     0f5d:     Minps $Vps, $Wps",    NACLi_SSE,     st, Binary);
  NaClDefine("     0f5e:     Divps $Vps, $Wps",    NACLi_SSE,     st, Binary);
  NaClDefine("     0f5f:     Maxps $Vps, $Wps",    NACLi_SSE,     st, Binary);
  NaClDefine("   f30f58:     Addss $Vss, $Wss",    NACLi_SSE,     st, Binary);
  NaClDefine("   f30f59:     Mulss $Vss, $Wss",    NACLi_SSE,     st, Binary);
  NaClDefine("   f30f5a:     Cvtss2sd $Vsd, $Wss", NACLi_SSE2,    st, Move);
  NaClDefine("   f30f5b:     Cvttps2dq $Vdq, $Wps",
                                                   NACLi_SSE2,    st, Move);
  NaClDefine("   f30f5c:     Subss $Vss, $Wss",    NACLi_SSE,     st, Binary);
  NaClDefine("   f30f5d:     Minss $Vss, $Wss",    NACLi_SSE,     st, Binary);
  NaClDefine("   f30f5e:     Divss $Vss, $Wss",    NACLi_SSE,     st, Binary);
  NaClDefine("   f30f5f:     Maxss $Vss, $Wss",    NACLi_SSE,     st, Binary);
  NaClDefine("   660f58:     Addpd $Vpd, $Wpd",    NACLi_SSE2,    st, Binary);
  NaClDefine("   660f59:     Mulpd $Vpd, $Wpd",    NACLi_SSE2,    st, Binary);
  NaClDefine("   660f5a:     Cvtpd2ps $Vps, $Wpd", NACLi_SSE2,    st, Move);
  NaClDefine("   660f5b:     Cvtps2dq $Vdq, $Wps", NACLi_SSE2,    st, Move);
  NaClDefine("   660f5c:     Subpd $Vpd, $Wpd",    NACLi_SSE2,    st, Binary);
  NaClDefine("   660f5d:     Minpd $Vpd, $Wpd",    NACLi_SSE2,    st, Binary);
  NaClDefine("   660f5e:     Divpd $Vpd, $Wpd",    NACLi_SSE2,    st, Binary);
  NaClDefine("   660f5f:     Maxpd $Vpd, $Wpd",    NACLi_SSE2,    st, Binary);
  NaClDefine("   f20f58:     Addsd $Vsd, $Wsd",    NACLi_SSE2,    st, Binary);
  NaClDefine("   f20f59:     Mulsd $Vsd, $Wsd",    NACLi_SSE2,    st, Binary);
  NaClDefine("   f20f5a:     Cvtsd2ss $Vss, $Wsd", NACLi_SSE2,    st, Move);
  NaClDefine("   f20f5b:     Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("   f20f5c:     Subsd $Vsd, $Wsd",    NACLi_SSE2,    st, Binary);
  NaClDefine("   f20f5d:     Minsd $Vsd, $Wsd",    NACLi_SSE2,    st, Binary);
  NaClDefine("   f20f5e:     Divsd $Vsd, $Wsd",    NACLi_SSE2,    st, Binary);
  NaClDefine("   f20f5f:     Maxsd $Vsd, $Wsd",    NACLi_SSE2,    st, Binary);
  NaClDefine("   660f60:     Punpcklbw $Vdq, $Wq", NACLi_SSE2,    st, Binary);
  NaClDefine("   660f61:     Punpcklwd $Vdq, $Wq", NACLi_SSE2,    st, Binary);
  NaClDefine("   660f62:     Punpckldq $Vdq, $Wq", NACLi_SSE2,    st, Binary);
  NaClDefine("   660f63:     Packsswb $Vdq, $Wdq", NACLi_SSE2,    st, Binary);
  NaClDefine("   660f64:     Pcmpgtb $Vdq, $Wdq",  NACLi_SSE2,    st, Binary);
  NaClDefine("   660f65:     Pcmpgtw $Vdq, $Wdq",  NACLi_SSE2,    st, Binary);
  NaClDefine("   660f66:     Pcmpgtd $Vdq, $Wdq",  NACLi_SSE2,    st, Binary);
  NaClDefine("   660f67:     Packuswb $Vdq, $Wdq", NACLi_SSE2,    st, Binary);
  NaClDefIter("  f20f60+@i:  Invalid", 0, 7,       NACLi_INVALID, st, Other);
  NaClDefIter("  f30f60+@i:  Invalid", 0, 7,       NACLi_INVALID, st, Other);
  NaClDefIter("  f30f68+@i:  Invalid", 0, 6,       NACLi_INVALID, st, Other);
  NaClDefine("   f30f6f:     Movdqu $Vdq, $Wdq",   NACLi_SSE2,    st, Move);
  NaClDefine("   660f68:     Punpckhbw $Vdq, $Wq", NACLi_SSE2,    st, Binary);
  NaClDefine("   660f69:     Punpckhwd $Vdq, $Wq", NACLi_SSE2,    st, Binary);
  NaClDefine("   660f6a:     Punpckhdq $Vdq, $Wq", NACLi_SSE2,    st, Binary);
  NaClDefine("   660f6b:     Packssdw $Vdq, $Wdq", NACLi_SSE2,    st, Binary);
  NaClDefine("   660f6c:     Punpcklqdq $Vdq, $Wq",
                                                   NACLi_SSE2,    st, Binary);
  NaClDefine("   660f6d:     Punpckhqdq $Vdq, $Wq",
                                                   NACLi_SSE2,    st, Binary);
  NaClDefine("   660f6e:     Movd $Vdq, $Ed/q/d",    NACLi_SSE2,    st, Move);
  NaClDef_64("   660f6e:     Movq $Vdq, $Ed/q/q",    NACLi_SSE2,    st, Move);
  NaClDefine("   660f6f:     Movdqa $Vdq, $Wdq",   NACLi_SSE2,    st, Move);
  NaClDefIter("  f20f68+@i:  Invalid", 0, 7,       NACLi_INVALID, st, Other);
  NaClDefine("   660f70:     Pshufd $Vdq, $Wdq, $Ib",
                                                   NACLi_SSE2,    st, Binary);
  NaClDefIter("  660f71/@i:  Invalid", 0, 1,       NACLi_INVALID, st, Other);
  NaClDefine("   660f71/2:   Psrlw $VRdq, $Ib",    NACLi_SSE2,    st, Binary);
  NaClDefine("   660f71/3:   Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("   660f71/4:   Psraw $VRdq, $Ib",    NACLi_SSE2,    st, Binary);
  NaClDefine("   660f71/5:   Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("   660f71/6:   Psllw $VRdq, $Ib",    NACLi_SSE2,    st, Binary);
  NaClDefine("   660f71/7:   Invalid",             NACLi_INVALID, st, Other);
  NaClDefIter("  660f72/@i:  Invalid", 0, 1,       NACLi_INVALID, st, Other);
  NaClDefine("   660f72/2:   Psrld $VRdq, $Ib",    NACLi_SSE2,    st, Binary);
  NaClDefine("   660f72/3:   Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("   660f72/4:   Psrad $VRdq, $Ib",    NACLi_SSE2,    st, Binary);
  NaClDefine("   660f72/5:   Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("   660f72/6:   Pslld $VRdq, $Ib",    NACLi_SSE2,    st, Binary);
  NaClDefine("   660f72/7:   Invalid",             NACLi_INVALID, st, Other);
  NaClDefIter("  660f73/@i:  Invalid", 0, 1,       NACLi_INVALID, st, Other);
  NaClDefine("   660f73/2:   Psrlq $VRdq, $Ib",    NACLi_SSE2,    st, Binary);
  NaClDefine("   660f73/3:   Psrldq $VRdq, $Ib",   NACLi_SSE2,    st, Binary);
  NaClDefIter("  660f73/@i:  Invalid", 4, 5,       NACLi_INVALID, st, Other);
  NaClDefine("   660f73/6:   Psllq $VRdq, $Ib",    NACLi_SSE2,    st, Binary);
  NaClDefine("   660f73/7:   Pslldq $VRdq, $Ib",   NACLi_SSE2,    st, Binary);
  NaClDefine("   660f74:     Pcmpeqb $Vdq, $Wdq",  NACLi_SSE2,    st, Binary);
  NaClDefine("   660f75:     Pcmpeqw $Vdq, $Wdq",  NACLi_SSE2,    st, Binary);
  NaClDefine("   660f76:     Pcmpeqd $Vdq, $Wdq",  NACLi_SSE2,    st, Binary);
  NaClDefine("   660f77:     Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("   f20f70:     Pshuflw $Vq, $Wq, $Ib",
                                                   NACLi_SSE2,    st, Binary);
  NaClDefIter("  f20f71+@i:  Invalid", 0, 6,       NACLi_INVALID, st, Other);
  NaClDefine("   f30f70:     Pshufhw $Vq, $Wq, $Ib",
                                                   NACLi_SSE2,    st, Binary);
  NaClDefIter("  f30f71+@i:  Invalid", 0, 6,       NACLi_INVALID, st, Other);
  NaClBegDef("   660f78/0:   Extrq $Vdq, $Ib, $Ib",NACLi_SSE4A,   st);
  NaClAddOpFlags(0, NACL_OPFLAG(AllowGOperandWithOpcodeInModRm));
  NaClEndDef(                                                         Nary);
  NaClDefine("   660f78/r:   Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("   660f79:     Extrq $Vdq, $VRdq",   NACLi_SSE4A,   st, Binary);
  NaClDefIter("  660f7a+@i:  Invalid", 0, 1,       NACLi_INVALID, st, Other);
  NaClDefine("   660f7c:     Haddpd $Vpd, $Wpd",   NACLi_SSE2,    st, Binary);
  NaClDefine("   660f7d:     Hsubpd $Vpd, $Wpd",   NACLi_SSE2,    st, Binary);
  NaClDefine("   660f7e:     Movd $Ed/q/d, $Vd/q/d",
                                                   NACLi_SSE2,    st, Move);
  NaClDef_64("   660f7e:     Movq $Ed/q/q, $Vd/q/q",
                                                   NACLi_SSE2,    st, Move);
  NaClDefine("   660f7f:     Movdqa $Wdq, $Vdq",   NACLi_SSE2,    st, Move);
  NaClDefine("   f20f78:     Insertq $Vdq, $VRq, $Ib, $Ib",
                                                   NACLi_SSE4A,   st, Nary);
  NaClDefine("   f20f79:     Insertq $Vdq, $VRdq", NACLi_SSE4A,   st, Binary);
  NaClDefIter("  f20f7a+@i:  Invalid", 0, 1,       NACLi_INVALID, st, Other);
  NaClDefine("   f20f7c:     Haddps $Vps, $Wps",   NACLi_SSE3,    st, Binary);
  NaClDefine("   f20f7d:     Hsubps $Vps, $Wps",   NACLi_SSE3,    st, Binary);
  NaClDefIter("  f20f7e+@i:  Invalid", 0, 1,       NACLi_INVALID, st, Other);
  NaClDefIter("  f30f78+@i:  Invalid", 0, 5,       NACLi_INVALID, st, Other);
  NaClDefine("   f30f7e:     Movq $Vq, $Wq",       NACLi_SSE2,    st, Move);
  NaClDefine("   f30f7f:     Movdqu $Wdq, $Vdq",   NACLi_SSE2,    st, Move);
  NaClDefIter("  f20fb8+@i:  Invalid", 0, 7,       NACLi_INVALID, st, Other);
  NaClDefine("   f30fb8:     Popcnt $Gv, $Ev",     NACLi_POPCNT,  st, Move);
  NaClDefIter("  f30fb9+@i:  Invalid", 0, 2,       NACLi_INVALID, st, Other);
  /* tzcnt is treated as a bsf on machines that don't have tzcnt.
   * Hence, even though its conditional on NACLi_LZCNT, we act
   * like it can be used on all processors.
   */
  NaClDefine("   f30fbc:     Tzcnt $Gv, $Ev",      NACLi_386,     st, Move);
  /* lzcnt is treated as a bsr on machines that don't have lzcnt.
   * Hence, even though its conditional on NACLi_LZCNT, we act
   * like it can be used on all processors.
   */
  NaClDefine("   f30fbd:     Lzcnt $Gv, $Ev",      NACLi_386,     st, Move);
  NaClDefIter("  f30fbe+@i:  Invalid", 0, 1,       NACLi_INVALID, st, Other);
  NaClDefine("     0fc2:     Cmpps $Vps, $Wps, $Ib",
                                                   NACLi_SSE,     st, Nary);
  NaClDefine("     0fc3:     Movnti $Md/q $Gd/q",  NACLi_SSE2,    st, Move);
  /* Note: Pinsrw follows Intel's table, not ADM, which uses:
   *    Pinsrw $Pq, $Ew, $Ib
   */
  NaClDefine("     0fc4:     Pinsrw $Pq, $Rd/q/Mw, $Ib",
                                                   NACLi_SSE,     st, Nary);
  NaClDefine("     0fc5:     Pextrw $Gd, $PRq, $Ib",
                                                   NACLi_SSE41,   st, Move);
  NaClDefine("     0fc6:     Shufps $Vps, $Wps, $Ib",
                                                   NACLi_SSE,     st, Nary);
  NaClDefine("   660fc2:     Cmppd $Vpd, $Wpd, $Ib",
                                                   NACLi_SSE2,    st, Nary);
  NaClDefine("   660fc3:     Invalid",             NACLi_INVALID, st, Other);
  /* Note: Pinsrw follows Intel's table, not ADM, which uses:
   * Pinsrw $Vdq, $Ew, $Ib
   */
  NaClDefine("   660fc4:     Pinsrw $Vdq, $Rd/q/Mw, $Ib",
                                                   NACLi_SSE,     st, Binary);
  NaClDefine("   660fc5:     Pextrw $Gd $VRdq, $Ib",
                                                   NACLi_SSE41,   st, Binary);
  NaClDefine("   660fc6:     Shufpd $Vpd, $Wpd, $Ib",
                                                   NACLi_SSE2,    st, Nary);
  NaClDefine("   f20fc2:     Cmpsd_xmm $Vsd, $Wsd, $Ib",
                                                   NACLi_SSE2,    st, Nary);
  NaClDefine("   f20fc3:     Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("   f20fc4:     Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("   f20fc5:     Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("   f20fc6:     Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("   f30fc2:     Cmpss $Vss, $Wss, $Ib",
                                                   NACLi_SSE,     st, Nary);
  NaClDefine("   f30fc3:     Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("   f30fc4:     Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("   f30fc5:     Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("   f30fc6:     Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("   f20fd0:     Addsubps $Vpd, $Wpd", NACLi_SSE3,    st, Binary);
  NaClDefIter("  f20fd1+@i:  Invalid", 0, 4,       NACLi_INVALID, st, Other);
  NaClDefine("   f20fd6:     Movdq2q $Pq, $VRq",   NACLi_SSE2,    st, Move);
  NaClDefine("   f20fd7:     Invalid",             NACLi_INVALID, st, Other);
  NaClDefIter("  f30fd0+@i:  Invalid", 0, 5,       NACLi_INVALID, st, Other);
  NaClDefine("   f30fd6:     Movq2dq $Vdq, $PRq",  NACLi_SSE2,    st, Move);
  NaClDefine("   f30fd7:     Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("   660fd0:     Addsubpd $Vpd, $Wpd", NACLi_SSE3,    st, Binary);
  NaClDefine("   660fd1:     Psrlw $Vdq, $Wdq",    NACLi_SSE2,    st, Binary);
  NaClDefine("   660fd2:     Psrld $Vdq, $Wdq",    NACLi_SSE2,    st, Binary);
  NaClDefine("   660fd3:     Psrlq $Vdq, $Wdq",    NACLi_SSE2,    st, Binary);
  NaClDefine("   660fd4:     Paddq $Vdq, $Wdq",    NACLi_SSE2,    st, Binary);
  NaClDefine("   660fd5:     Pmullw $Vdq, $Wdq",   NACLi_SSE2,    st, Binary);
  NaClDefine("   660fd6:     Movq $Wq, $Vq",       NACLi_SSE2,    st, Move);
  NaClDefine("   660fd7:     Pmovmskb $Gd, $VRdq", NACLi_SSE2,    st, Move);
  NaClDefine("   660fd8:     Psubusb $Vdq, $Wdq",  NACLi_SSE2,    st, Binary);
  NaClDefine("   660fd9:     Psubusw $Vdq, $Wdq",  NACLi_SSE2,    st, Binary);
  NaClDefine("   660fda:     Pminub $Vdq, $Wdq",   NACLi_SSE2,    st, Binary);
  NaClDefine("   660fdb:     Pand $Vdq, $Wdq",     NACLi_SSE2,    st, Binary);
  NaClDefine("   660fdc:     Paddusb $Vdq, $Wdq",  NACLi_SSE2,    st, Binary);
  NaClDefine("   660fdd:     Paddusw $Vdq, $Wdq",  NACLi_SSE2,    st, Binary);
  NaClDefine("   660fde:     Pmaxub $Vdq, $Wdq",   NACLi_SSE2,    st, Binary);
  NaClDefine("   660fdf:     Pandn $Vdq, $Wdq",    NACLi_SSE2,    st, Binary);
  NaClDefIter("  f20fd8+@i:  Invalid", 0, 7,       NACLi_INVALID, st, Other);
  NaClDefIter("  f30fd8+@i:  Invalid", 0, 7,       NACLi_INVALID, st, Other);
  NaClDefine("   660fe0:     Pavgb $Vdq, $Wdq",    NACLi_SSE2,    st, Binary);
  NaClDefine("   660fe1:     Psraw $Vdq, $Wdq",    NACLi_SSE2,    st, Binary);
  NaClDefine("   660fe2:     Psrad $Vdq, $Wdq",    NACLi_SSE2,    st, Binary);
  NaClDefine("   660fe3:     Pavgw $Vdq, $Wdq",    NACLi_SSE2,    st, Binary);
  NaClDefine("   660fe4:     Pmulhuw $Vdq, $Wdq",  NACLi_SSE2,    st, Binary);
  NaClDefine("   660fe5:     Pmulhw $Vdq, $Wdq",   NACLi_SSE2,    st, Binary);
  NaClDefine("   660fe6:     Cvttpd2dq $Vq, $Wdq", NACLi_SSE2,    st, Move);
  NaClDefine("   660fe7:     Movntdq $Mdq, $Vdq",  NACLi_SSE2,    st, Move);
  NaClDefIter("  f30fe0+@i:  Invalid", 0, 5,       NACLi_INVALID, st, Other);
  NaClDefine("   f30fe6:     Cvtdq2pd $Vpd, $Wq",  NACLi_SSE2,    st, Move);
  NaClDefine("   f30fe7:     Invalid",             NACLi_INVALID, st, Other);
  NaClDefIter("  f20fe0+@i:  Invalid", 0, 5,       NACLi_INVALID, st, Other);
  NaClDefine("   f20fe6:     Cvtpd2dq $Vq, $Wpd",  NACLi_SSE2,    st, Move);
  NaClDefine("   f20fe7:     Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("   660fe8:     Psubsb $Vdq, $Wdq",   NACLi_SSE2,    st, Binary);
  NaClDefine("   660fe9:     Psubsw $Vdq, $Wdq",   NACLi_SSE2,    st, Binary);
  NaClDefine("   660fea:     Pminsw $Vdq, $Wdq",   NACLi_SSE2,    st, Binary);
  NaClDefine("   660feb:     Por $Vdq, $Wdq",      NACLi_SSE2,    st, Binary);
  NaClDefine("   660fec:     Paddsb $Vdq, $Wdq",   NACLi_SSE2,    st, Binary);
  NaClDefine("   660fed:     Paddsw $Vdq, $Wdq",   NACLi_SSE2,    st, Binary);
  NaClDefine("   660fee:     Pmaxsw $Vdq, $Wdq",   NACLi_SSE2,    st, Binary);
  NaClDefine("   660fef:     Pxor $Vdq, $Wdq",     NACLi_SSE2,    st, Binary);
  NaClDefIter("  f20fe8+@i:  Invalid", 0, 7,       NACLi_INVALID, st, Other);
  NaClDefIter("  f30fe8+@i:  Invalid", 0, 7,       NACLi_INVALID, st, Other);
  NaClDefine("   660ff0:     Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("   660ff1:     Psllw $Vdq, $Wdq",    NACLi_SSE2,    st, Binary);
  NaClDefine("   660ff2:     Pslld $Vdq, $Wdq",    NACLi_SSE2,    st, Binary);
  NaClDefine("   660ff3:     Psllq $Vdq, $Wdq",    NACLi_SSE2,    st, Binary);
  NaClDefine("   660ff4:     Pmuludq $Vdq, $Wdq",  NACLi_SSE2,    st, Binary);
  NaClDefine("   660ff5:     Pmaddwd $Vdq, $Wdq",  NACLi_SSE2,    st, Binary);
  NaClDefine("   660ff6:     Psadbw $Vdq, $Wdq",   NACLi_SSE2,    st, Binary);
  NaClDefine("   660ff7:     Maskmovdqu {$Zvd}, $Vdq, $VRdq",
                                                   NACLi_SSE2,    st, Binary);
  NaClDefine("   f20ff0:     Lddqu $Vdq, $Mdq",    NACLi_SSE3,    st, Move);
  NaClDefIter("  f20ff1+@i:  Invalid", 0, 6,       NACLi_INVALID, st, Other);
  NaClDefIter("  f30ff0+@i:  Invalid", 0, 7,       NACLi_INVALID, st, Other);
  NaClDefine("   660ff8:     Psubb $Vdq, $Wdq",    NACLi_SSE2,    st, Binary);
  NaClDefine("   660ff9:     Psubw $Vdq, $Wdq",    NACLi_SSE2,    st, Binary);
  NaClDefine("   660ffa:     Psubd $Vdq, $Wdq",    NACLi_SSE2,    st, Binary);
  NaClDefine("   660ffb:     Psubq $Vdq, $Wdq",    NACLi_SSE2,    st, Binary);
  NaClDefine("   660ffc:     Paddb $Vdq, $Wdq",    NACLi_SSE2,    st, Binary);
  NaClDefine("   660ffd:     Paddw $Vdq, $Wdq",    NACLi_SSE2,    st, Binary);
  NaClDefine("   660ffe:     Paddd $Vdq, $Wdq",    NACLi_SSE2,    st, Binary);
  NaClDefine("   660fff:     Invalid",             NACLi_INVALID, st, Other);
  NaClDefIter("  f20ff8+@i:  Invalid", 0, 7,       NACLi_INVALID, st, Other);
  NaClDefIter("  f30ff8+@i:  Invalid", 0, 7,       NACLi_INVALID, st, Other);
}

static void NaClDefMmxInsts(struct NaClSymbolTable* st) {
  NaClDefPrefixInstChoices_32_64(Prefix0F, 0x6e, 1, 2);
  NaClDefPrefixInstChoices_32_64(Prefix0F, 0x7e, 1, 2);
  NaClDefine("     0f60:     Punpcklbw $Pq, $Qq",  NACLi_MMX,     st, Binary);
  NaClDefine("     0f61:     Punpcklwd $Pq, $Qq",  NACLi_MMX,     st, Binary);
  NaClDefine("     0f62:     Punpckldq $Pq, $Qq",  NACLi_MMX,     st, Binary);
  NaClDefine("     0f63:     Packsswb $Pq, $Qq",   NACLi_MMX,     st, Binary);
  NaClDefine("     0f64:     Pcmpgtb $Pq, $Qq",    NACLi_MMX,     st, Binary);
  NaClDefine("     0f65:     Pcmpgtw $Pq, $Qq",    NACLi_MMX,     st, Binary);
  NaClDefine("     0f66:     Pcmpgtd $Pq, $Qq",    NACLi_MMX,     st, Binary);
  NaClDefine("     0f67:     Packuswb $Pq, $Qq",   NACLi_MMX,     st, Binary);
  NaClDefine("     0f68:     Punpckhbw $Pq, $Qd",  NACLi_MMX,     st, Binary);
  NaClDefine("     0f69:     Punpckhwd $Pq, $Qd",  NACLi_MMX,     st, Binary);
  NaClDefine("     0f6a:     Punpckhdq $Pq, $Qd",  NACLi_MMX,     st, Binary);
  NaClDefine("     0f6b:     Packssdw $Pq, $Qq",   NACLi_MMX,     st, Binary);
  NaClDefIter("    0f6c+@i:  Invalid", 0, 1,       NACLi_INVALID, st, Other);
  NaClDefine("     0f6e:     Movd $Pq, $Ed/q/d",   NACLi_MMX,     st, Move);
  NaClDef_64("     0f6e:     Movq $Pq, $Ed/q/q",   NACLi_MMX,     st, Move);
  NaClDefine("     0f6f:     Movq $Pq, $Qq",       NACLi_MMX,     st, Move);
  NaClDefine("     0f70:     Pshufw $Pq, $Qq, $Ib",NACLi_MMX,     st, Binary);
  NaClDefIter("    0f71/@i:  Invalid", 0, 1,       NACLi_INVALID, st, Other);
  NaClDefine("     0f71/2:   Psrlw $PRq, $Ib",     NACLi_MMX,     st, Binary);
  NaClDefine("     0f71/3:   Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("     0f71/4:   Psraw $PRq, $Ib",     NACLi_MMX,     st, Binary);
  NaClDefine("     0f71/5:   Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("     0f71/6:   Psllw $PRq, $Ib",     NACLi_MMX,     st, Binary);
  NaClDefine("     0f71/7:   Invalid",             NACLi_INVALID, st, Other);
  NaClDefIter("    0f72/@i:  Invalid", 0, 1,       NACLi_INVALID, st, Other);
  NaClDefine("     0f72/2:   Psrld $PRq, $Ib",     NACLi_MMX,     st, Binary);
  NaClDefine("     0f72/3:   Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("     0f72/4:   Psrad $PRq, $Ib",     NACLi_MMX,     st, Binary);
  NaClDefine("     0f72/5:   Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("     0f72/6:   Pslld $PRq, $Ib",     NACLi_MMX,     st, Binary);
  NaClDefine("     0f72/7:   Invalid",             NACLi_INVALID, st, Other);
  NaClDefIter("    0f73/@i:  Invalid", 0, 1,       NACLi_INVALID, st, Other);
  NaClDefine("     0f73/2:   Psrlq $PRq, $Ib",     NACLi_MMX,     st, Binary);
  NaClDefIter("    0f73/@i:  Invalid", 3, 5,       NACLi_INVALID, st, Other);
  NaClDefine("     0f73/6:   Psllq $PRq, $Ib",     NACLi_MMX,     st, Binary);
  NaClDefine("     0f73/7:   Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("     0f74:     Pcmpeqb $Pq, $Qq",    NACLi_MMX,     st, Binary);
  NaClDefine("     0f75:     Pcmpeqw $Pq, $Qq",    NACLi_MMX,     st, Binary);
  NaClDefine("     0f76:     Pcmpeqd $Pq, $Qq",    NACLi_MMX,     st, Binary);
  NaClDefine("     0f77:     Emms",                NACLi_MMX,     st, Binary);
  NaClDefIter("    0f78+@i:  Invalid", 0, 5,       NACLi_INVALID, st, Other);
  NaClDefine("     0f7e:     Movd $Ed/q/d, $Pd/q/d",
                                                   NACLi_MMX,     st, Move);
  NaClDef_64("     0f7e:     Movq $Ed/q/q, $Pd/q/q",
                                                   NACLi_MMX,     st, Move);
  NaClDefine("     0f7f:     Movq $Qq, $Pq",       NACLi_MMX,     st, Move);
  NaClDefine("     0fd0:     Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("     0fd1:     Psrlw $Pq, $Qq",      NACLi_MMX,     st, Binary);
  NaClDefine("     0fd2:     Psrld $Pq, $Qq",      NACLi_MMX,     st, Binary);
  NaClDefine("     0fd3:     Psrlq $Pq, $Qq",      NACLi_MMX,     st, Binary);
  NaClDefine("     0fd4:     Paddq $Pq, $Qq",      NACLi_MMX,     st, Binary);
  NaClDefine("     0fd5:     Pmullw $Pq, $Qq",     NACLi_MMX,     st, Binary);
  NaClDefine("     0fd6:     Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("     0fd7:     Pmovmskb $Gd, $PRq",  NACLi_MMX,     st, Move);
  NaClDefine("     0fd8:     Psubusb $Pq, $Qq",    NACLi_MMX,     st, Binary);
  NaClDefine("     0fd9:     Psubusw $Pq, $Qq",    NACLi_MMX,     st, Binary);
  NaClDefine("     0fda:     Pminub $Pq, $Qq",     NACLi_MMX,     st, Binary);
  NaClDefine("     0fdb:     Pand $Pq, $Qq",       NACLi_MMX,     st, Binary);
  NaClDefine("     0fdc:     Paddusb $Pq, $Qq",    NACLi_MMX,     st, Binary);
  NaClDefine("     0fdd:     Paddusw $Pq, $Qq",    NACLi_MMX,     st, Binary);
  NaClDefine("     0fde:     Pmaxub $Pq, $Qq",     NACLi_MMX,     st, Binary);
  NaClDefine("     0fdf:     Pandn $Pq, $Qq",      NACLi_MMX,     st, Binary);
  NaClDefine("     0fe0:     Pavgb $Pq, $Qq",      NACLi_MMX,     st, Binary);
  NaClDefine("     0fe1:     Psraw $Pq, $Qq",      NACLi_MMX,     st, Binary);
  NaClDefine("     0fe2:     Psrad $Pq, $Qq",      NACLi_MMX,     st, Binary);
  NaClDefine("     0fe3:     Pavgw $Pq, $Qq",      NACLi_MMX,     st, Binary);
  NaClDefine("     0fe4:     Pmulhuw $Pq, $Qq",    NACLi_MMX,     st, Binary);
  NaClDefine("     0fe5:     Pmulhw $Pq, $Qq",     NACLi_MMX,     st, Binary);
  NaClDefine("     0fe6:     Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("     0fe7:     Movntq $Mq, $Pq",     NACLi_MMX,     st, Move);
  NaClDefine("     0fe8:     Psubsb $Pq, $Qq",     NACLi_MMX,     st, Binary);
  NaClDefine("     0fe9:     Psubsw $Pq, $Qq",     NACLi_MMX,     st, Binary);
  NaClDefine("     0fea:     Pminsw $Pq, $Qq",     NACLi_MMX,     st, Binary);
  NaClDefine("     0feb:     Por $Pq, $Qq",        NACLi_MMX,     st, Binary);
  NaClDefine("     0fec:     Paddsb $Pq, $Qq",     NACLi_MMX,     st, Binary);
  NaClDefine("     0fed:     Paddsw $Pq, $Qq",     NACLi_MMX,     st, Binary);
  NaClDefine("     0fee:     Pmaxsw $Pq, $Qq",     NACLi_MMX,     st, Binary);
  NaClDefine("     0fef:     Pxor $Pq, $Qq",       NACLi_MMX,     st, Binary);
  NaClDefine("     0ff0:     Invalid",             NACLi_INVALID, st, Other);
  NaClDefine("     0ff1:     Psllw $Pq, $Qq",      NACLi_MMX,     st, Binary);
  NaClDefine("     0ff2:     Pslld $Pq, $Qq",      NACLi_MMX,     st, Binary);
  NaClDefine("     0ff3:     Psllq $Pq, $Qq",      NACLi_MMX,     st, Binary);
  NaClDefine("     0ff4:     Pmuludq $Pq, $Qq",    NACLi_MMX,     st, Binary);
  NaClDefine("     0ff5:     Pmaddwd $Pq, $Qq",    NACLi_MMX,     st, Binary);
  NaClDefine("     0ff6:     Psadbw $Pq, $Qq",     NACLi_MMX,     st, Binary);
  NaClDefine("     0ff7:     Maskmovq {$Zvd}, $Pq, $PRq",
                                                   NACLi_MMX,     st, Binary);
  NaClDefine("     0ff8:     Psubb $Pq, $Qq",      NACLi_MMX,     st, Binary);
  NaClDefine("     0ff9:     Psubw $Pq, $Qq",      NACLi_MMX,     st, Binary);
  NaClDefine("     0ffa:     Psubd $Pq, $Qq",      NACLi_MMX,     st, Binary);
  NaClDefine("     0ffb:     Psubq $Pq, $Qq",      NACLi_MMX,     st, Binary);
  NaClDefine("     0ffc:     Paddb $Pq, $Qq",      NACLi_MMX,     st, Binary);
  NaClDefine("     0ffd:     Paddw $Pq, $Qq",      NACLi_MMX,     st, Binary);
  NaClDefine("     0ffe:     Paddd $Pq, $Qq",      NACLi_MMX,     st, Binary);
  NaClDefine("     0fff:     Invalid",             NACLi_INVALID, st, Binary);
}

static void NaClDefNarySseInsts(struct NaClSymbolTable* st) {
  /* Define other forms of MMX and XMM operations, i.e. those in 660f3a. */
  NaClDefInstPrefix(Prefix660F3A);
  NaClDefPrefixInstChoices_32_64(Prefix660F3A, 0x16, 1, 2);
  NaClDefPrefixInstChoices_32_64(Prefix660F3A, 0x22, 1, 2);
  NaClDefine(" 660f3a08:     Roundps $Vdq, $Wdq, $Ib",
                                                   NACLi_SSE41,   st, O1Nary);
  NaClDefine(" 660f3a09:     Roundpd $Vdq, $Wdq, $Ib",
                                                   NACLi_SSE41,   st, O1Nary);
  NaClDefine(" 660f3a0a:     Roundss $Vss, $Wss, $Ib",
                                                   NACLi_SSE41,   st, O1Nary);
  NaClDefine(" 660f3a0b:     Roundsd $Vsd, $Wsd, $Ib",
                                                   NACLi_SSE41,   st, O1Nary);
  NaClDefine(" 660f3a0c:     Blendps $Vdq, $Wdq, $Ib",
                                                   NACLi_SSE41,   st, Nary);
  NaClDefine(" 660f3a0d:     Blendpd $Vdq, $Wdq, $Ib",
                                                   NACLi_SSE41,   st, Nary);
  NaClDefine(" 660f3a0e:     Pblendw $Vdq, $Wdq, $Ib",
                                                   NACLi_SSE41,   st, Nary);
  NaClDefine("   0f3a0f:     Palignr $Pq, $Qq, $Ib",
                                                   NACLi_SSSE3,   st, Nary);
  NaClDefine(" 660f3a0f:     Palignr $Vdq, $Wdq, $Ib",
                                                   NACLi_SSSE3,   st, Nary);
  NaClDefine(" 660f3a14:     Pextrb $Rd/Mb, $Vdq, $Ib",
                                                   NACLi_SSE41,   st, O1Nary);
  NaClDefine(" 660f3a15:     Pextrw $Rd/Mw, $Vdq, $Ib",
                                                   NACLi_SSE41,   st, O1Nary);
  NaClDefine(" 660f3a16:     Pextrd $Ed/q/d, $Vdq, $Ib",
                                                   NACLi_SSE41,   st, O1Nary);
  NaClDef_64(" 660f3a16:     Pextrq $Ed/q/q, $Vdq, $Ib",
                                                   NACLi_SSE41,   st, O1Nary);
  NaClDefine(" 660f3a17:     Extractps $Ed, $Vdq, $Ib",
                                                   NACLi_SSE41,   st, O1Nary);
  NaClDefine(" 660f3a20:     Pinsrb $Vdq, $Rd/q/Mb, $Ib",
                                                   NACLi_SSE41,   st, O1Nary);
  NaClDefine(" 660f3a21:     Insertps $Vdq, $Udq/Md, $Ib",
                                                   NACLi_SSE41,   st, O1Nary);
  NaClDefine(" 660f3a22:     Pinsrd $Vdq, $Ed/q/d, $Ib",
                                                   NACLi_SSE41,   st, O1Nary);
  NaClDef_64(" 660f3a22:     Pinsrq $Vdq, $Ed/q/q, $Ib",
                                                   NACLi_SSE41,   st, O1Nary);
  NaClDefine(" 660f3a40:     Dpps $Vdq, $Wdq, $Ib",
                                                   NACLi_SSE41,   st, Nary);
  NaClDefine(" 660f3a41:     Dppd $Vdq, $Wdq, $Ib",
                                                   NACLi_SSE41,   st, Nary);
  NaClDefine(" 660f3a42:     Mpsadbw $Vdq, $Wdq, $Ib",
                                                   NACLi_SSE41,   st, Nary);
  NaClDefine(" 660f3a60:     Pcmpestrm {%xmm0}, {$rAXv}, {$rDXv}, "
                                      "$Vdq, $Wdq, $Ib",
                                                   NACLi_SSE42,   st, O3Nary);
  NaClDefine(" 660f3a61:     Pcmpestri {$rCXv}, {$rAXv}, {$rDXv}, "
                                      "$Vdq, $Wdq, $Ib",
                                                   NACLi_SSE42,   st, O3Nary);
  NaClDefine(" 660f3a62:     Pcmpistrm {%xmm0}, $Vdq, $Wdq, $Ib",
                                                   NACLi_SSE42,   st, O1Nary);
  NaClDefine(" 660f3a63:     Pcmpistri {$rCXv}, $Vdq, $Wdq, $Ib",
                                                   NACLi_SSE42,   st, O1Nary);
}

void NaClDefSseInsts(struct NaClSymbolTable* st) {
  NaClDefNarySseInsts(st);
  NaClDefBinarySseInsts(st);
  NaClDefMmxInsts(st);
}
