// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGLImageConversionMSA_h
#define WebGLImageConversionMSA_h

#if HAVE(MIPS_MSA_INTRINSICS)

#include "platform/cpu/mips/CommonMacrosMSA.h"

namespace blink {

namespace SIMD {

#define SEPERATE_RGBA_FRM_16BIT_5551INPUT(in, out_r, out_g, out_b, out_a)  \
    cnst31 = (v8u16)__msa_ldi_h(0x1F);                                     \
    cnst7 = (v8u16)__msa_ldi_h(0x7);                                       \
    cnst1 = (v8u16)__msa_ldi_h(0x1);                                       \
    out_r = (v8u16)SRLI_H(in, 11);                                         \
    out_g = ((v8u16)SRLI_H(in, 6)) & cnst31;                               \
    out_b = ((v8u16)SRLI_H(in, 1)) & cnst31;                               \
    out_a = in & cnst1;                                                    \
    out_r = ((v8u16)SLLI_H(out_r, 3)) | (out_r & cnst7);                   \
    out_g = ((v8u16)SLLI_H(out_g, 3)) | (out_g & cnst7);                   \
    out_b = ((v8u16)SLLI_H(out_b, 3)) | (out_b & cnst7);                   \
    out_a = (v8u16)CEQI_H((v8i16)out_a, 1);                                \

ALWAYS_INLINE void unpackOneRowOfRGBA5551ToRGBA8MSA(const uint16_t*& source, uint8_t*& destination, unsigned& pixelsPerRow)
{
    unsigned i;
    v8u16 src0, src1, src2, src3;
    v8u16 src0r, src0g, src0b, src0a, src1r, src1g, src1b, src1a;
    v8u16 src2r, src2g, src2b, src2a, src3r, src3g, src3b, src3a;
    v8u16 cnst31, cnst7, cnst1;
    v16u8 dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7;
    v16u8 dst8, dst9, dst10, dst11, dst12, dst13, dst14, dst15;
    v16u8 out0, out1, out2, out3, out4, out5, out6, out7;

    for (i = (pixelsPerRow >> 5); i--;) {
        LD_UH4(source, 8, src0, src1, src2, src3);
        SEPERATE_RGBA_FRM_16BIT_5551INPUT(src0, src0r, src0g, src0b, src0a);
        SEPERATE_RGBA_FRM_16BIT_5551INPUT(src1, src1r, src1g, src1b, src1a);
        SEPERATE_RGBA_FRM_16BIT_5551INPUT(src2, src2r, src2g, src2b, src2a);
        SEPERATE_RGBA_FRM_16BIT_5551INPUT(src3, src3r, src3g, src3b, src3a);
        ILVRL_B2_UB(src0g, src0r, dst0, dst1);
        ILVRL_B2_UB(src0a, src0b, dst2, dst3);
        ILVRL_B2_UB(src1g, src1r, dst4, dst5);
        ILVRL_B2_UB(src1a, src1b, dst6, dst7);
        ILVRL_B2_UB(src2g, src2r, dst8, dst9);
        ILVRL_B2_UB(src2a, src2b, dst10, dst11);
        ILVRL_B2_UB(src3g, src3r, dst12, dst13);
        ILVRL_B2_UB(src3a, src3b, dst14, dst15);
        ILVEV_H2_UB(dst0, dst2, dst1, dst3, out0, out1);
        ILVEV_H2_UB(dst4, dst6, dst5, dst7, out2, out3);
        ILVEV_H2_UB(dst8, dst10, dst9, dst11, out4, out5);
        ILVEV_H2_UB(dst12, dst14, dst13, dst15, out6, out7);
        ST_UB8(out0, out1, out2, out3, out4, out5, out6, out7, destination, 16);
    }

    if (pixelsPerRow & 31) {
        if ((pixelsPerRow & 16) && (pixelsPerRow & 8)) {
            LD_UH3(source, 8, src0, src1, src2);
            SEPERATE_RGBA_FRM_16BIT_5551INPUT(src0, src0r, src0g, src0b, src0a);
            SEPERATE_RGBA_FRM_16BIT_5551INPUT(src1, src1r, src1g, src1b, src1a);
            SEPERATE_RGBA_FRM_16BIT_5551INPUT(src2, src2r, src2g, src2b, src2a);
            ILVRL_B2_UB(src0g, src0r, dst0, dst1);
            ILVRL_B2_UB(src0a, src0b, dst2, dst3);
            ILVRL_B2_UB(src1g, src1r, dst4, dst5);
            ILVRL_B2_UB(src1a, src1b, dst6, dst7);
            ILVRL_B2_UB(src2g, src2r, dst8, dst9);
            ILVRL_B2_UB(src2a, src2b, dst10, dst11);
            ILVEV_H2_UB(dst0, dst2, dst1, dst3, out0, out1);
            ILVEV_H2_UB(dst4, dst6, dst5, dst7, out2, out3);
            ILVEV_H2_UB(dst8, dst10, dst9, dst11, out4, out5);
            ST_UB6(out0, out1, out2, out3, out4, out5, destination, 16);
        } else if (pixelsPerRow & 16) {
            LD_UH2(source, 8, src0, src1);
            SEPERATE_RGBA_FRM_16BIT_5551INPUT(src0, src0r, src0g, src0b, src0a);
            SEPERATE_RGBA_FRM_16BIT_5551INPUT(src1, src1r, src1g, src1b, src1a);
            ILVRL_B2_UB(src0g, src0r, dst0, dst1);
            ILVRL_B2_UB(src0a, src0b, dst2, dst3);
            ILVRL_B2_UB(src1g, src1r, dst4, dst5);
            ILVRL_B2_UB(src1a, src1b, dst6, dst7);
            ILVEV_H2_UB(dst0, dst2, dst1, dst3, out0, out1);
            ILVEV_H2_UB(dst4, dst6, dst5, dst7, out2, out3);
            ST_UB4(out0, out1, out2, out3, destination, 16);
        } else if (pixelsPerRow & 8) {
            src0 = LD_UH(source);
            source += 8;
            SEPERATE_RGBA_FRM_16BIT_5551INPUT(src0, src0r, src0g, src0b, src0a);
            ILVRL_B2_UB(src0g, src0r, dst0, dst1);
            ILVRL_B2_UB(src0a, src0b, dst2, dst3);
            ILVEV_H2_UB(dst0, dst2, dst1, dst3, out0, out1);
            ST_UB2(out0, out1, destination, 16);
        }
    }

    pixelsPerRow &= 7;
}

} // namespace SIMD

} // namespace blink

#endif // HAVE(MIPS_MSA_INTRINSICS)

#endif // WebGLImageConversionMSA_h
