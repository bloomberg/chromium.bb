/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

/* Define prefix array that covers opcodes with OF and 66OF prefices. */
static OpcodePrefix g_prefix0F_pair[2] = {
  Prefix0F,
  Prefix660F
};

/* Define prefix array that covers opcodes with 0F38 and 660F38 preficies. */
static OpcodePrefix g_prefix0F38_pair[2] = {
  Prefix0F38,
  Prefix660F38
};

/* Define prefix array that covers opcodew with preifx 0F3A and 660F3A
 * prefices.
 */
static OpcodePrefix g_prefix0F3A_pair[2] = {
  Prefix0F3A,
  Prefix660F3A
};

/* Define prefix array that only applies for the prefix 660F. */
static OpcodePrefix g_prefix660F_only[2] = {
  OpcodePrefixEnumSize,
  Prefix660F,
};

/* Define prefix array that only applies for the prefix 660F38. */
static OpcodePrefix g_prefix660F38_only[2] = {
  OpcodePrefixEnumSize,
  Prefix660F38,
};

/* Define prefix array that only applies for the prefix F20F. */
static OpcodePrefix g_prefixF20F_only[2] = {
  PrefixF20F,
  OpcodePrefixEnumSize
};

/* Define prefix array that only applies for the prefix F30F. */
static OpcodePrefix g_prefixF30F_only[2] = {
  PrefixF30F,
  OpcodePrefixEnumSize
};

/* Define an operand kind pair that uses the G_Operand value for
 * prefix opcodes, independent of the appearance of prefix 66.
 */
static OperandKind g_G_Operand[2] = {
  G_Operand,
  G_Operand
};

/* Define an operand kind pair that uses Mmx_G_Operand for the
 * prefix opcodes without prefix 66, and uses the Xmm_G_Operand
 * for the prefix opcodes with prefix 66.
 */
static OperandKind g_Mm_G_Operand[2] = {
  Mmx_G_Operand,
  Xmm_G_Operand
};

/* Define an operand kind pair that uses Mmx_E_Operand for the
 * prefix opcodes without prefix 66, and uses the Xmm_E_Operand
 * for the prefix opcodes with prefix 66.
 */
static OperandKind g_Mm_E_Operand[2] = {
  Mmx_E_Operand,
  Xmm_E_Operand
};

/* Define an operand kind pair that uses Xmm_G_Operand for both forms. */
static OperandKind g_Xmm_G_Operand[2] = {
  Xmm_G_Operand,
  Xmm_G_Operand
};

/* Define an operand kind pair that uses Xmm_E_Operand for both forms. */
static OperandKind g_Xmm_E_Operand[2] = {
  Xmm_E_Operand,
  Xmm_E_Operand
};

/* Define the NaCL instruction type as NACLi_SSE for both kinds of opcode
 * prefices.
 */
static NaClInstType g_SSE[2] = {
  NACLi_SSE,
  NACLi_SSE
};

/* Define the NaCL instruction type as NACLi_SSE2 for both kinds of opcode
 * prefices.
 */
static NaClInstType g_SSE2[2] = {
  NACLi_SSE2,
  NACLi_SSE2
};

/* Define the NaCl instruction type as NACLi_SSSE3 for both kinds of opcode
 * prefices.
 */
static NaClInstType g_SSSE3[2] = {
  NACLi_SSSE3,
  NACLi_SSSE3
};

/* Define the Nacl instruction type as NACLi_SSE41 for both kinds of opcode
 * prefices.
 */
static NaClInstType g_SSE41[2] = {
  NACLi_SSE41,
  NACLi_SSE41
};

/* Define the Nacl instruction type as NACLi_SSE42 for both kinds of opcode
 * prefices.
 */
static NaClInstType g_SSE42[2] = {
  NACLi_SSE41,
  NACLi_SSE41
};

/* Define the Nacl instruction type as NACLi_MMX for opcode prefices not
 * containing the 66 prefix, and NACLi_SSE2 for the opcode prefices containing
 * the 66 prefix.
 */
static NaClInstType g_MMX_or_SSE2[2] = {
  NACLi_MMX,
  NACLi_SSE2
};

/* Define the NaCl instruction type as NACLi_SSE for opcode prefices not
 * containing the 66 prefix, and NACLi_SSE2 for the opcode prefices containing
 * th e66 prefix.
 */
static NaClInstType g_SSE_or_SSE2[2] = {
  NACLi_SSE,
  NACLi_SSE2
};

/* Define no additional (opcode) flags being specified for either of the opcode
 * prefices.
 */
static OpcodeFlags g_no_flags[2] = {
  0,
  0
};

/* Define an additional (opcode) flag OperandSize_v for all opcode prefices. */
static OpcodeFlags g_OperandSize_v[2] = {
  InstFlag(OperandSize_v),
  InstFlag(OperandSize_v)
};

/* Define additional (opcode) flags OperandSize_o and OpcodeUseRexW for all
 * opcode prefices.
 */
static OpcodeFlags g_OperandSize_o[2] = {
  InstFlag(OperandSize_o) | InstFlag(OpcodeUsesRexW),
  InstFlag(OperandSize_o) | InstFlag(OpcodeUsesRexW)
};

/* Define a structure containing information on the SSE instructions it is
 * to define. Each field is a two-element array specifying the value to use
 * when a 66 doesn't appear, and when a 66 prefix does appear.
 */
typedef struct {
  /* Define the instruction opcode prefices being specified. */
  OpcodePrefix* prefix;
  /* Define the last byte of the instruction opcode being defined. */
  uint8_t opcode;
  /* Define the instruction (mnemonic) that is being specified. */
  InstMnemonic inst;
  /* Define the operand kind to use for the first operand of the instruction. */
  OperandKind* op1kind;
  /* Define the operand kind to use for the second operand of the
   * instruction.
   */
  OperandKind* op2kind;
  /* Define the NaCL instruction type for the instruction being specified. */
  NaClInstType* insttype;
  /* Define any additional opcode flags that should be specified for the
   * instruction.
   */
  OpcodeFlags* flags;
} InstOpcodeMnemonic;

/* Specify binary instructions that act as move instructions, (i.e. copy data
 * from thier second operand, to thier first operand.
 */
static InstOpcodeMnemonic g_MoveOps[] = {
  {g_prefix0F38_pair, 0xC1, InstPabsb, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSSE3, g_no_flags},
  {g_prefix0F38_pair, 0x1D, InstPabsw, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSSE3, g_no_flags},
  {g_prefix0F38_pair, 0x1E, InstPabsd, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSSE3, g_no_flags},
  {g_prefix660F38_only, 0x41, InstPhminposuw, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE41, g_no_flags},
  {g_prefixF20F_only, 0x10, InstMovsd, g_Xmm_G_Operand, g_Xmm_E_Operand,
   g_SSE2, g_no_flags},
  {g_prefixF20F_only, 0x11, InstMovsd, g_Xmm_E_Operand, g_Xmm_G_Operand,
   g_SSE2, g_no_flags},
  {g_prefixF30F_only, 0x10, InstMovss, g_Xmm_G_Operand, g_Xmm_E_Operand,
   g_SSE, g_no_flags},
  {g_prefixF30F_only, 0x11, InstMovss, g_Xmm_E_Operand, g_Xmm_G_Operand,
   g_SSE, g_no_flags},
};

/* Specify binary instructions that apply a binary operation to both
 * the first and second arguments, and stores the result in the first
 * argument.
 */
static InstOpcodeMnemonic g_BinaryOps[] = {
  {g_prefix0F_pair, 0x63, InstPacksswb, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0x6B, InstPackssdw, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix660F38_only, 0x2b, InstPackusdw, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE41, g_no_flags},
  {g_prefix0F_pair, 0x67, InstPackuswb, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0xFC, InstPaddb, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0xFD, InstPaddw, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0xFE, InstPaddd, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0xD4, InstPaddq, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE2, g_no_flags},
  {g_prefix0F_pair, 0xEC, InstPaddsb, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0xED, InstPaddsw, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0xDC, InstPaddusb, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0xDD, InstPaddusw, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix0F3A_pair, 0x0F, InstPalignr, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSSE3, g_no_flags},
  {g_prefix0F_pair, 0xDB, InstPand, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0xDF, InstPandn, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0xE0, InstPavgb, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0xE3, InstPavgw, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE_or_SSE2, g_no_flags},
  {g_prefix660F38_only, 0x10, InstPblendvb, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE41, g_no_flags},
  {g_prefix660F38_only, 0x3A, InstPblendw, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE41, g_no_flags},
  {g_prefix0F_pair, 0x74, InstPcmpeqb, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0x75, InstPcmpeqw, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0x76, InstPcmpeqd, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix660F38_only, 0x29, InstPcmpeqq, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE41, g_no_flags},
  {g_prefix0F_pair, 0x64, InstPcmpgtb, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0x65, InstPcmpgtw, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0x66, InstPcmpgtd, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix660F38_only, 0x37, InstPcmpgtq, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE42, g_no_flags},
  {g_prefix0F38_pair, 0x01, InstPhaddw, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSSE3, g_no_flags},
  {g_prefix0F38_pair, 0x02, InstPhaddd, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSSE3, g_no_flags},
  {g_prefix0F38_pair, 0x03, InstPhaddsw, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSSE3, g_no_flags},
  {g_prefix0F38_pair, 0x05, InstPhsubw, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSSE3, g_no_flags},
  {g_prefix0F38_pair, 0x06, InstPhsubd, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSSE3, g_no_flags},
  {g_prefix0F38_pair, 0x07, InstPhsubsw, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSSE3, g_no_flags},
  {g_prefix0F38_pair, 0x04, InstPmaddubsw, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSSE3, g_no_flags},
  {g_prefix0F_pair, 0xF5, InstPmaddwd, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix660F38_only, 0x3c, InstPmaxsb, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE41, g_no_flags},
  {g_prefix660F38_only, 0x3D, InstPmaxsd, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE41, g_no_flags},
  {g_prefix0F_pair, 0xEE, InstPmaxsw, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0xDE, InstPmaxub, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE_or_SSE2, g_no_flags},
  {g_prefix660F38_only, 0x3F, InstPmaxud, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE41, g_no_flags},
  {g_prefix660F38_only, 0x3E, InstPmaxuw, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE41, g_no_flags},
  {g_prefix660F38_only, 0x39, InstPminsb, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE41, g_no_flags},
  {g_prefix660F38_only, 0x38, InstPminsd, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE41, g_no_flags},
  {g_prefix0F_pair, 0xEA, InstPminsw, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0xDA, InstPminub, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE_or_SSE2, g_no_flags},
  {g_prefix660F38_only, 0x3B, InstPminud, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE41, g_no_flags},
  {g_prefix660F38_only, 0x3A, InstPminuw, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE41, g_no_flags},
  /* TODO(karl) The second operand of pmovmskb can only b a MM register. Does
   * the instruction use the Effective address to compute?
   */
  {g_prefix0F_pair, 0xD7, InstPmovmskb, g_G_Operand, g_Mm_E_Operand,
   g_SSE_or_SSE2, g_OperandSize_v},
  {g_prefix0F_pair, 0xD7, InstPmovmskb, g_G_Operand, g_Mm_E_Operand,
   g_SSE_or_SSE2, g_OperandSize_o},
  {g_prefix660F38_only, 0x20, InstPmovsxbw, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE41, g_no_flags},
  {g_prefix660F38_only, 0x21, InstPmovsxbd, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE41, g_no_flags},
  {g_prefix660F38_only, 0x22, InstPmovsxbq, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE41, g_no_flags},
  {g_prefix660F38_only, 0x23, InstPmovsxwd, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE41, g_no_flags},
  {g_prefix660F38_only, 0x24, InstPmovsxwq, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE41, g_no_flags},
  {g_prefix660F38_only, 0x25, InstPmovsxdq, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE41, g_no_flags},
  {g_prefix660F38_only, 0x30, InstPmovzxbw, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE41, g_no_flags},
  {g_prefix660F38_only, 0x31, InstPmovzxbd, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE41, g_no_flags},
  {g_prefix660F38_only, 0x32, InstPmovzxbq, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE41, g_no_flags},
  {g_prefix660F38_only, 0x33, InstPmovzxwd, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE41, g_no_flags},
  {g_prefix660F38_only, 0x34, InstPmovzxwq, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE41, g_no_flags},
  {g_prefix660F38_only, 0x35, InstPmovzxdq, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE41, g_no_flags},
  {g_prefix660F38_only, 0x28, InstPmuldq, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE41, g_no_flags},
  {g_prefix0F38_pair, 0x0B, InstPmulhrsw, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSSE3, g_no_flags},
  {g_prefix0F_pair, 0xE4, InstPmulhuw, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0xE5, InstPmulhw, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix660F38_only, 0x40, InstPmulld, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE41, g_no_flags},
  {g_prefix0F_pair, 0xD5, InstPmullw, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0xF4, InstPmuludq, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE2, g_no_flags},
  {g_prefix0F_pair, 0xEB, InstPor, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0xF6, InstPsadbw, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE_or_SSE2, g_no_flags},
  {g_prefix0F38_pair, 0x00, InstPshufb, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSSE3, g_no_flags},
  {g_prefix0F38_pair, 0x08, InstPsignb, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSSE3, g_no_flags},
  {g_prefix0F38_pair, 0x09, InstPsignw, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSSE3, g_no_flags},
  {g_prefix0F38_pair, 0x0A, InstPsignd, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSSE3, g_no_flags},
  {g_prefix0F_pair, 0xF1, InstPsllw, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0xF2, InstPslld, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0xF3, InstPsllq, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0xE1, InstPsraw, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0xE2, InstPsrad, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0xD1, InstPsrlw, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0xD2, InstPsrld, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0xD3, InstPsrlq, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0xF8, InstPsubb, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0xF9, InstPsubw, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0xFA, InstPsubd, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0xFB, InstPsubq, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0xE8, InstPsubsb, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0xE9, InstPsubsw, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0xD8, InstPsubusb, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0xD9, InstPsubusw, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0x68, InstPunpckhbw, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0x69, InstPunpckhbd, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0x6A, InstPunpckhbq, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0x60, InstPunpcklbw, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0x61, InstPunpcklwd, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix0F_pair, 0x62, InstPunpckldq, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
  {g_prefix660F_only, 0x6C, InstPunpcklqdq, g_Mm_G_Operand, g_Mm_E_Operand,
   g_SSE2, g_no_flags},
  {g_prefix0F_pair, 0xEF, InstPxor, g_Mm_G_Operand, g_Mm_E_Operand,
   g_MMX_or_SSE2, g_no_flags},
};

void DefineSseOpcodes() {
  int i;
  int j;

  /* Define General Move and Binary operations */
  for (i = 0; i < 2; ++i) {
    /* Define move (and possibly modify) instructions. */
    for (j = 0; j < ARRAYSIZE(g_MoveOps); ++j) {
      if (OpcodePrefixEnumSize != g_MoveOps[j].prefix[i]) {
        DefineOpcodePrefix(g_MoveOps[j].prefix[i]);
        DefineOpcode(g_MoveOps[j].opcode,
                     g_MoveOps[j].insttype[i],
                     g_MoveOps[j].flags[i] | InstFlag(OpcodeUsesModRm),
                     g_MoveOps[j].inst);
        DefineOperand(g_MoveOps[j].op1kind[i], OpFlag(OpSet));
        DefineOperand(g_MoveOps[j].op2kind[i], OpFlag(OpUse));
      }
    }

    /* Define binary instructions. */
    for (j = 0; j < ARRAYSIZE(g_BinaryOps); ++j) {
      if (OpcodePrefixEnumSize != g_BinaryOps[j].prefix[i]) {
        DefineOpcodePrefix(g_BinaryOps[j].prefix[i]);
        DefineOpcode(g_BinaryOps[j].opcode,
                     g_BinaryOps[j].insttype[i],
                     g_BinaryOps[j].flags[i] | InstFlag(OpcodeUsesModRm),
                     g_BinaryOps[j].inst);
        DefineOperand(g_BinaryOps[j].op1kind[i], OpFlag(OpSet) | OpFlag(OpUse));
        DefineOperand(g_BinaryOps[j].op2kind[i], OpFlag(OpUse));
      }
    }
  }

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

  DefineOpcodePrefix(Prefix660F);
  DefineOpcode(0x73,
               NACLi_SSE2x,
               InstFlag(OpcodeInModRm) | InstFlag(OpcodeHasImmed_b),
               InstPslldq);
  DefineOperand(Opcode7, OpFlag(OperandExtendsOpcode));
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix0F);
  DefineOpcode(0x71,
               NACLi_MMX,
               InstFlag(OpcodeInModRm) | InstFlag(OpcodeHasImmed_b),
               InstPsllw);
  DefineOperand(Opcode6, OpFlag(OperandExtendsOpcode));
  DefineOperand(Mmx_G_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix660F);
  DefineOpcode(0x71,
               NACLi_SSE3,
               InstFlag(OpcodeInModRm) | InstFlag(OpcodeHasImmed_b),
               InstPsllw);
  DefineOperand(Opcode6, OpFlag(OperandExtendsOpcode));
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix0F);
  DefineOpcode(0x72,
               NACLi_MMX,
               InstFlag(OpcodeInModRm) | InstFlag(OpcodeHasImmed_b),
               InstPslld);
  DefineOperand(Opcode6, OpFlag(OperandExtendsOpcode));
  DefineOperand(Mmx_G_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix660F);
  DefineOpcode(0x72,
               NACLi_SSE2,
               InstFlag(OpcodeInModRm) | InstFlag(OpcodeHasImmed_b),
               InstPslld);
  DefineOperand(Opcode6, OpFlag(OperandExtendsOpcode));
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix0F);
  DefineOpcode(0x73,
               NACLi_MMX,
               InstFlag(OpcodeInModRm) | InstFlag(OpcodeHasImmed_b),
               InstPsllq);
  DefineOperand(Opcode6, OpFlag(OperandExtendsOpcode));
  DefineOperand(Mmx_G_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix660F);
  DefineOpcode(0x73,
               NACLi_SSE2,
               InstFlag(OpcodeInModRm) | InstFlag(OpcodeHasImmed_b),
               InstPsllq);
  DefineOperand(Opcode6, OpFlag(OperandExtendsOpcode));
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet) | OpFlag(OpUse));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix0F);
  DefineOpcode(0x71,
               NACLi_MMX,
               InstFlag(OpcodeInModRm) | InstFlag(OpcodeHasImmed_b),
               InstPsraw);
  DefineOperand(Opcode4, OpFlag(OperandExtendsOpcode));
  DefineOperand(Mmx_G_Operand, OpFlag(OpUse) | OpFlag(OpSet));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix660F);
  DefineOpcode(0x71,
               NACLi_SSE2,
               InstFlag(OpcodeInModRm) | InstFlag(OpcodeHasImmed_b),
               InstPsraw);
  DefineOperand(Opcode4, OpFlag(OperandExtendsOpcode));
  DefineOperand(Xmm_G_Operand, OpFlag(OpUse) | OpFlag(OpSet));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix0F);
  DefineOpcode(0x72,
               NACLi_MMX,
               InstFlag(OpcodeInModRm) | InstFlag(OpcodeHasImmed_b),
               InstPsrad);
  DefineOperand(Opcode4, OpFlag(OperandExtendsOpcode));
  DefineOperand(Mmx_G_Operand, OpFlag(OpUse) | OpFlag(OpSet));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix660F);
  DefineOpcode(0x72,
               NACLi_SSE2,
               InstFlag(OpcodeInModRm) | InstFlag(OpcodeHasImmed_b),
               InstPsrad);
  DefineOperand(Opcode4, OpFlag(OperandExtendsOpcode));
  DefineOperand(Xmm_G_Operand, OpFlag(OpUse) | OpFlag(OpSet));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix660F);
  DefineOpcode(0x73,
               NACLi_SSE2x,
               InstFlag(OpcodeInModRm) | InstFlag(OpcodeHasImmed_b),
               InstPsrldq);
  DefineOperand(Opcode3, OpFlag(OperandExtendsOpcode));
  DefineOperand(Xmm_G_Operand, OpFlag(OpUse) | OpFlag(OpSet));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix0F);
  DefineOpcode(0x71,
               NACLi_MMX,
               InstFlag(OpcodeInModRm) | InstFlag(OpcodeHasImmed_b),
               InstPsrlw);
  DefineOperand(Opcode2, OpFlag(OperandExtendsOpcode));
  DefineOperand(Mmx_G_Operand, OpFlag(OpUse) | OpFlag(OpSet));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix660F);
  DefineOpcode(0x71,
               NACLi_SSE2,
               InstFlag(OpcodeInModRm) | InstFlag(OpcodeHasImmed_b),
               InstPsrlw);
  DefineOperand(Opcode2, OpFlag(OperandExtendsOpcode));
  DefineOperand(Xmm_G_Operand, OpFlag(OpUse) | OpFlag(OpSet));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix0F);
  DefineOpcode(0x72,
               NACLi_MMX,
               InstFlag(OpcodeInModRm) | InstFlag(OpcodeHasImmed_b),
               InstPsrld);
  DefineOperand(Opcode2, OpFlag(OperandExtendsOpcode));
  DefineOperand(Mmx_G_Operand, OpFlag(OpUse) | OpFlag(OpSet));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix660F);
  DefineOpcode(0x72,
               NACLi_SSE2,
               InstFlag(OpcodeInModRm) | InstFlag(OpcodeHasImmed_b),
               InstPsrld);
  DefineOperand(Opcode2, OpFlag(OperandExtendsOpcode));
  DefineOperand(Xmm_G_Operand, OpFlag(OpUse) | OpFlag(OpSet));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix0F);
  DefineOpcode(0x73,
               NACLi_MMX,
               InstFlag(OpcodeInModRm) | InstFlag(OpcodeHasImmed_b),
               InstPsrlq);
  DefineOperand(Opcode2, OpFlag(OperandExtendsOpcode));
  DefineOperand(Mmx_G_Operand, OpFlag(OpUse) | OpFlag(OpSet));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix660F);
  DefineOpcode(0x73,
               NACLi_SSE2,
               InstFlag(OpcodeInModRm) | InstFlag(OpcodeHasImmed_b),
               InstPsrlq);
  DefineOperand(Opcode2, OpFlag(OperandExtendsOpcode));
  DefineOperand(Xmm_G_Operand, OpFlag(OpUse) | OpFlag(OpSet));
  DefineOperand(I_Operand, OpFlag(OpUse));

  DefineOpcodePrefix(Prefix660F38);
  DefineOpcode(0x17,
               NACLi_SSE41,
               InstFlag(OpcodeUsesModRm),
               InstPtest);
  DefineOperand(Xmm_G_Operand, OpFlag(OpUse));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));

  /* f2 0f 2a /r    CVTSI2SD xmm, r/m32    */
  DefineOpcodePrefix(PrefixF20F);
  DefineOpcode(0x2a,
               NACLi_SSE2,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_v),
               InstCvtsi2sd);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(E_Operand, OpFlag(OpUse));

  /* f2 REX.W 0f 2a /r    CVTSI2SD xmm, r/m64    */
  DefineOpcodePrefix(PrefixF20F);
  DefineOpcode(0x2a,
               NACLi_SSE2,
               InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeUsesRexW),
               InstCvtsi2sd);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(E_Operand, OpFlag(OpUse));

  /* f2 0f 2d /r    CVTSD2SI r32, xmm/m64    */
  DefineOpcodePrefix(PrefixF20F);
  DefineOpcode(0x2d,
               NACLi_SSE2,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_v),
               InstCvtsd2si);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));

  /* f2 REX.W 0f 2d /r    CVTSD2SI r64, xmm/m64    */
  DefineOpcodePrefix(PrefixF20F);
  DefineOpcode(0x2d,
               NACLi_SSE2,
               InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeUsesRexW),
               InstCvtsd2si);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));


  /* f2 0f 2c /r    CVTTSD2SI r32, xmm/m64    */
  DefineOpcodePrefix(PrefixF20F);
  DefineOpcode(0x2c,
               NACLi_SSE2,
               InstFlag(OpcodeUsesModRm) | InstFlag(OperandSize_v),
               InstCvttsd2si);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));

  /* f2 REX.W 0f 2c /r    CVTTSD2SI r64, xmm/m64    */
  DefineOpcodePrefix(PrefixF20F);
  DefineOpcode(0x2c,
               NACLi_SSE2,
               InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeUsesRexW),
               InstCvttsd2si);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));

  /* f2 0f 58 /r  addsd xmm1, xmm2/m64  SSE2 RexR */
  DefineOpcodePrefix(PrefixF20F);
  DefineOpcode(0x58,
               NACLi_SSE2,
               InstFlag(OpcodeUsesModRm),
               InstAddsd);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));

  /* f3 0f 58 /r  addss xmm1, xmm2/m32  SSE RexR */
  DefineOpcodePrefix(PrefixF30F);
  DefineOpcode(0x58,
               NACLi_SSE,
               InstFlag(OpcodeUsesModRm),
               InstAddss);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));

  /* 66 0f 55 /r  andnpd xmm1, xmm2/m128   SSE2 RexR */
  DefineOpcodePrefix(Prefix660F);
  DefineOpcode(0x55,
               NACLi_SSE2,
               InstFlag(OpcodeUsesModRm),
               InstAndnpd);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));

  /* 66 0f 54 /r  andpd xmm1, xmm2/m128  SSE2 RexR */
  DefineOpcodePrefix(Prefix660F);
  DefineOpcode(0x54,
               NACLi_SSE2,
               InstFlag(OpcodeUsesModRm),
               InstAndpd);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));

  /* 0f 54 /r  andps xmm1, xmm2/m128  SSE RexR */
  DefineOpcodePrefix(Prefix0F);
  DefineOpcode(0x54,
               NACLi_SSE,
               InstFlag(OpcodeUsesModRm),
               InstAndps);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));

  /* f2 0f c2 /r ib  cmpsd xmm1, xmm2/m64, imm8  SSE2 RexR */
  DefineOpcodePrefix(PrefixF20F);
  DefineOpcode(0xc2,
               NACLi_SSE2,
               InstFlag(OpcodeUsesModRm),
               InstCmpsd);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));

  /* f2 0f 5a /r  cvtsd2ss xmm1, xmm2/m64  SSE2 */
  DefineOpcodePrefix(PrefixF20F);
  DefineOpcode(0x5a,
               NACLi_SSE2,
               InstFlag(OpcodeUsesModRm),
               InstCvtsd2ss);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));

  /* f3 0f 2a /r  cvtsi2ss xmm, r/m32  SSE */
  DefineOpcodePrefix(PrefixF30F);
  DefineOpcode(0x2a,
               NACLi_SSE,
               InstFlag(OpcodeUsesModRm),
               InstCvtsi2ss);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(E_Operand, OpFlag(OpUse));

  /* f3 REX.W 0f 2a /r cvtsi2ss xmm, r/m64  SSE */
  DefineOpcodePrefix(PrefixF30F);
  DefineOpcode(0x2a,
               NACLi_SSE,
               InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeUsesRexW),
               InstCvtsi2ss);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(E_Operand, OpFlag(OpUse));

  /* f3 0f 5a /r  cvtss2sd xmm1, xmm2/m32  SSE2 RexR */
  DefineOpcodePrefix(PrefixF30F);
  DefineOpcode(0x5a,
               NACLi_SSE2,
               InstFlag(OpcodeUsesModRm),
               InstCvtss2sd);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));

  /* f3 0f 2c /r  cvttss2si r32,xmm/m32  SSE RexR */
  DefineOpcodePrefix(PrefixF30F);
  DefineOpcode(0x2c,
               NACLi_SSE,
               InstFlag(OpcodeUsesModRm),
               InstCvttss2si);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));

  /* f3 REX.W 0f 2c /r  cvttss2si r64,xmm/m32  SSE RexR */
  DefineOpcodePrefix(PrefixF30F);
  DefineOpcode(0x2c,
               NACLi_SSE,
               InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeUsesRexW),
               InstCvttss2si);
  DefineOperand(G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));

  /* f2 0f 5e /r  divsd xmm1, xmm2/m64  SSE2 RexR */
  DefineOpcodePrefix(PrefixF20F);
  DefineOpcode(0x5e,
               NACLi_SSE2,
               InstFlag(OpcodeUsesModRm),
               InstDivsd);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));

  /* f3 0f 5e /r  divss xmm1, xmm2/m32  SSE RexR */
  DefineOpcodePrefix(PrefixF30F);
  DefineOpcode(0x5e,
               NACLi_SSE,
               InstFlag(OpcodeUsesModRm),
               InstDivss);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));

  /* f3 0f 5f /r  maxss xmm1, xmm2/m32  SSE RexR */
  DefineOpcodePrefix(PrefixF30F);
  DefineOpcode(0x5f,
               NACLi_SSE,
               InstFlag(OpcodeUsesModRm),
               InstMaxss);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));

  /* f2 0f 5d /r  minsd xmm1, xmm2/m64  SSE2 RexR */
  DefineOpcodePrefix(PrefixF20F);
  DefineOpcode(0x5d,
               NACLi_SSE2,
               InstFlag(OpcodeUsesModRm),
               InstMinsd);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));

  /* f3 0f 5d /r  minss xmm1, xmm2/m32  SSE RexR */
  DefineOpcodePrefix(PrefixF30F);
  DefineOpcode(0x5d,
               NACLi_SSE,
               InstFlag(OpcodeUsesModRm),
               InstMinss);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));

  /* 66 0f 28 /r  movapd xmm1, xmm2/m128  SSE2 RexR */
  DefineOpcodePrefix(Prefix660F);
  DefineOpcode(0x28,
               NACLi_SSE2,
               InstFlag(OpcodeUsesModRm),
               InstMovapd);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));

  /* 66 0f 29 /r  movapd xmm2/m128, xmm1  SSE2 RexR */
  DefineOpcodePrefix(Prefix660F);
  DefineOpcode(0x29,
               NACLi_SSE2,
               InstFlag(OpcodeUsesModRm),
               InstMovapd);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));

  /* 0f 28 /r  movaps xmm1, xmm2/m128  SSE RexR */
  DefineOpcodePrefix(Prefix0F);
  DefineOpcode(0x28,
               NACLi_SSE,
               InstFlag(OpcodeUsesModRm),
               InstMovaps);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));

  /* 0f 29 /r  movaps xmm2/m128, xmm1  SSE RexR */
  DefineOpcodePrefix(Prefix0F);
  DefineOpcode(0x29,
               NACLi_SSE,
               InstFlag(OpcodeUsesModRm),
               InstMovaps);
  DefineOperand(Xmm_E_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_G_Operand, OpFlag(OpUse));

  /* f3 0f 10 /r  movss xmm1, xmm2/m32  SSE RexR */
  DefineOpcodePrefix(PrefixF30F);
  DefineOpcode(0x10,
               NACLi_SSE,
               InstFlag(OpcodeUsesModRm),
               InstMovss);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));

  /* f3 0f 11 /r  movss xmm2/m32, xmm1  SSE RexR */
  DefineOpcodePrefix(PrefixF30F);
  DefineOpcode(0x11,
               NACLi_SSE,
               InstFlag(OpcodeUsesModRm),
               InstMovss);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));

  /* f2 0f 59 /r  mulsd xmm1, xmm2/m64  SSE2 RexR */
  DefineOpcodePrefix(PrefixF20F);
  DefineOpcode(0x59,
               NACLi_SSE2,
               InstFlag(OpcodeUsesModRm),
               InstMulsd);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));

  /* f3 0f 59 /r  mulss xmm1, xmm2/m32  SSE RexR */
  DefineOpcodePrefix(PrefixF30F);
  DefineOpcode(0x59,
               NACLi_SSE,
               InstFlag(OpcodeUsesModRm),
               InstMulss);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));

  /* 66 0f 56 /r  orpd xmm1, xmm2/m128  SSE2 RexR */
  DefineOpcodePrefix(Prefix660F);
  DefineOpcode(0x56,
               NACLi_SSE2,
               InstFlag(OpcodeUsesModRm),
               InstOrpd);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));

  /* f2 0f 51 /r  sqrtsd xmm1, xmm2/64  SSE2 RexR */
  DefineOpcodePrefix(PrefixF20F);
  DefineOpcode(0x51,
               NACLi_SSE2,
               InstFlag(OpcodeUsesModRm),
               InstSqrtsd);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));

  /* f2 0f 5c /r  subsd xmm1, xmm2/m64  SSE2 RexR */
  DefineOpcodePrefix(PrefixF20F);
  DefineOpcode(0x5c,
               NACLi_SSE2,
               InstFlag(OpcodeUsesModRm),
               InstSubsd);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));

  /* F3 0f 5C /r  subss xmm1, xmm2/m32  SSE RexR */
  DefineOpcodePrefix(PrefixF30F);
  DefineOpcode(0x5c,
               NACLi_SSE,
               InstFlag(OpcodeUsesModRm),
               InstSubss);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));

  /* 66 0f 2e /r  ucomisd xmm1, xmm2/m64 SSE2 RexR */
  DefineOpcodePrefix(Prefix660F);
  DefineOpcode(0x2e,
               NACLi_SSE2,
               InstFlag(OpcodeUsesModRm),
               InstUcomisd);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));

  /* 0f 2e /r  ucomiss xmm1, xmm2/m32  SSE RexR */
  DefineOpcodePrefix(Prefix0F);
  DefineOpcode(0x2e,
               NACLi_SSE,
               InstFlag(OpcodeUsesModRm),
               InstUcomiss);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));

  /* 66 0f 57 /r  xorpd xmm1, xmm2/m128  SSE2 RexR */
  DefineOpcodePrefix(Prefix660F);
  DefineOpcode(0x57,
               NACLi_SSE2,
               InstFlag(OpcodeUsesModRm),
               InstXorpd);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));

  /* 0f 57 /r  xorps xmm1, xmm2/m128  SSE RexR */
  DefineOpcodePrefix(Prefix0F);
  DefineOpcode(0x57,
               NACLi_SSE2,
               InstFlag(OpcodeUsesModRm),
               InstXorps);
  DefineOperand(Xmm_G_Operand, OpFlag(OpSet));
  DefineOperand(Xmm_E_Operand, OpFlag(OpUse));
}
