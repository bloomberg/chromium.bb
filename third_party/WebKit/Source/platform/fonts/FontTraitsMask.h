/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FontTraitsMask_h
#define FontTraitsMask_h

namespace WebCore {

enum {
    FontStyleNormalBit = 0,
    FontStyleItalicBit,
    FontVariantNormalBit,
    FontVariantSmallCapsBit,
    FontWeightBit1,
    FontWeightBit2,
    FontWeightBit3,
    FontWeightBit4,
    FontStretchBit1,
    FontStretchBit2,
    FontStretchBit3,
    FontStretchBit4,
    FontTraitsMaskWidth
};

enum FontTraitsMask {
    FontStyleNormalMask = 1 << FontStyleNormalBit,
    FontStyleItalicMask = 1 << FontStyleItalicBit,
    FontStyleMask = FontStyleNormalMask | FontStyleItalicMask,

    FontVariantNormalMask = 1 << FontVariantNormalBit,
    FontVariantSmallCapsMask = 1 << FontVariantSmallCapsBit,
    FontVariantMask = FontVariantNormalMask | FontVariantSmallCapsMask,

    FontWeightBit1Mask = 1 << FontWeightBit1,
    FontWeightBit2Mask = 1 << FontWeightBit2,
    FontWeightBit3Mask = 1 << FontWeightBit3,
    FontWeightBit4Mask = 1 << FontWeightBit4,
    FontWeightMask = FontWeightBit1Mask | FontWeightBit2Mask | FontWeightBit3Mask | FontWeightBit4Mask,
    FontStretchBit1Mask = 1 << FontStretchBit1,
    FontStretchBit2Mask = 1 << FontStretchBit2,
    FontStretchBit3Mask = 1 << FontStretchBit3,
    FontStretchBit4Mask = 1 << FontStretchBit4,
    FontStretchMask = FontStretchBit1Mask | FontStretchBit2Mask | FontStretchBit3Mask | FontStretchBit4Mask
};

} // namespace WebCore
#endif // FontTraitsMask_h
