/*
 * Copyright (C) 2007 Nicholas Shanks <contact@nickshanks.com>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "platform/fonts/FontDescription.h"

#include "RuntimeEnabledFeatures.h"
#include "wtf/text/AtomicStringHash.h"
#include "wtf/text/StringHash.h"

namespace WebCore {

struct SameSizeAsFontDescription {
    FontFamily familyList;
    RefPtr<FontFeatureSettings> m_featureSettings;
    float sizes[2];
    // FXIME: Make them fit into one word.
    uint32_t bitfields;
    uint32_t bitfields2 : 8;
};

COMPILE_ASSERT(sizeof(FontDescription) == sizeof(SameSizeAsFontDescription), FontDescription_should_stay_small);

FontWeight FontDescription::lighterWeight(void) const
{
    switch (m_weight) {
        case FontWeight100:
        case FontWeight200:
        case FontWeight300:
        case FontWeight400:
        case FontWeight500:
            return FontWeight100;

        case FontWeight600:
        case FontWeight700:
            return FontWeight400;

        case FontWeight800:
        case FontWeight900:
            return FontWeight700;
    }
    ASSERT_NOT_REACHED();
    return FontWeightNormal;
}

FontWeight FontDescription::bolderWeight(void) const
{
    switch (m_weight) {
        case FontWeight100:
        case FontWeight200:
        case FontWeight300:
            return FontWeight400;

        case FontWeight400:
        case FontWeight500:
            return FontWeight700;

        case FontWeight600:
        case FontWeight700:
        case FontWeight800:
        case FontWeight900:
            return FontWeight900;
    }
    ASSERT_NOT_REACHED();
    return FontWeightNormal;
}

FontTraitsMask FontDescription::traitsMask() const
{
    return static_cast<FontTraitsMask>((m_italic ? FontStyleItalicMask : FontStyleNormalMask)
            | (m_smallCaps ? FontVariantSmallCapsMask : FontVariantNormalMask)
            | (FontWeight100Mask << (m_weight - FontWeight100)));

}

FontDescription FontDescription::makeNormalFeatureSettings() const
{
    FontDescription normalDescription(*this);
    normalDescription.setFeatureSettings(0);
    return normalDescription;
}

float FontDescription::effectiveFontSize() const
{
    return (RuntimeEnabledFeatures::subpixelFontScalingEnabled())
        ? computedSize()
        : computedPixelSize();
}

FontCacheKey FontDescription::cacheKey(const AtomicString& familyName, FontTraitsMask desiredTraits) const
{
    FontTraitsMask traits = desiredTraits
        ? desiredTraits
        : traitsMask();

    unsigned options =
        // synthetic bold, italics - bits 7-8
        static_cast<unsigned>(m_fontSmoothing) << 4 | // bits 5-6
        static_cast<unsigned>(m_textRendering) << 2 | // bits 3-4
        static_cast<unsigned>(m_orientation) << 1 | // bit 2
        static_cast<unsigned>(m_usePrinterFont); // bit 1

    return FontCacheKey(familyName, effectiveFontSize(), options | traits << 8);
}

} // namespace WebCore
