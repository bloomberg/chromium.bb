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
    float sizes[4];
    // FXIME: Make them fit into one word.
    uint32_t bitfields;
    uint32_t bitfields2 : 12;
};

COMPILE_ASSERT(sizeof(FontDescription) == sizeof(SameSizeAsFontDescription), FontDescription_should_stay_small);

TypesettingFeatures FontDescription::s_defaultTypesettingFeatures = 0;

bool FontDescription::s_useSubpixelTextPositioning = false;

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
            | (FontWeight100Mask << (m_weight - FontWeight100))
            | (m_stretch << FontStretchBit1));
}

void FontDescription::setTraitsMask(FontTraitsMask traitsMask)
{
    switch (traitsMask & FontWeightMask) {
    case FontWeight100Mask:
        setWeight(FontWeight100);
        break;
    case FontWeight200Mask:
        setWeight(FontWeight200);
        break;
    case FontWeight300Mask:
        setWeight(FontWeight300);
        break;
    case FontWeight400Mask:
        setWeight(FontWeight400);
        break;
    case FontWeight500Mask:
        setWeight(FontWeight500);
        break;
    case FontWeight600Mask:
        setWeight(FontWeight600);
        break;
    case FontWeight700Mask:
        setWeight(FontWeight700);
        break;
    case FontWeight800Mask:
        setWeight(FontWeight800);
        break;
    case FontWeight900Mask:
        setWeight(FontWeight900);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    setItalic((traitsMask & FontStyleItalicMask) ? FontItalicOn : FontItalicOff);
    setSmallCaps((traitsMask & FontVariantSmallCapsMask) ? FontSmallCapsOn : FontSmallCapsOff);
    setStretch(static_cast<FontStretch>((traitsMask & FontStretchMask) >> FontStretchBit1));
}

FontDescription FontDescription::makeNormalFeatureSettings() const
{
    FontDescription normalDescription(*this);
    normalDescription.setFeatureSettings(nullptr);
    return normalDescription;
}

float FontDescription::effectiveFontSize() const
{
    float size = (RuntimeEnabledFeatures::subpixelFontScalingEnabled())
        ? computedSize()
        : computedPixelSize();

    // Ensure that the effective precision matches the font-cache precision.
    // This guarantees that the same precision is used regardless of cache status.
    return floorf(size * FontCacheKey::precisionMultiplier()) / FontCacheKey::precisionMultiplier();
}

FontCacheKey FontDescription::cacheKey(const AtomicString& familyName, FontTraitsMask desiredTraits) const
{
    FontTraitsMask traits = desiredTraits
        ? desiredTraits
        : traitsMask();

    unsigned options =
        static_cast<unsigned>(m_syntheticItalic) << 8 | // bit 9
        static_cast<unsigned>(m_syntheticBold) << 7 | // bit 8
        static_cast<unsigned>(m_fontSmoothing) << 5 | // bits 6-7
        static_cast<unsigned>(m_textRendering) << 3 | // bits 4-5
        static_cast<unsigned>(m_orientation) << 2 | // bit 3
        static_cast<unsigned>(m_usePrinterFont) << 1 | // bit 2
        static_cast<unsigned>(m_subpixelTextPosition); // bit 1

    return FontCacheKey(familyName, effectiveFontSize(), options | traits << 9);
}


void FontDescription::setDefaultTypesettingFeatures(TypesettingFeatures typesettingFeatures)
{
    s_defaultTypesettingFeatures = typesettingFeatures;
}

TypesettingFeatures FontDescription::defaultTypesettingFeatures()
{
    return s_defaultTypesettingFeatures;
}

void FontDescription::updateTypesettingFeatures() const
{
    m_typesettingFeatures = s_defaultTypesettingFeatures;

    switch (textRenderingMode()) {
    case AutoTextRendering:
        break;
    case OptimizeSpeed:
        m_typesettingFeatures &= ~(WebCore::Kerning | Ligatures);
        break;
    case GeometricPrecision:
    case OptimizeLegibility:
        m_typesettingFeatures |= WebCore::Kerning | Ligatures;
        break;
    }

    switch (kerning()) {
    case FontDescription::NoneKerning:
        m_typesettingFeatures &= ~WebCore::Kerning;
        break;
    case FontDescription::NormalKerning:
        m_typesettingFeatures |= WebCore::Kerning;
        break;
    case FontDescription::AutoKerning:
        break;
    }

    switch (commonLigaturesState()) {
    case FontDescription::DisabledLigaturesState:
        m_typesettingFeatures &= ~Ligatures;
        break;
    case FontDescription::EnabledLigaturesState:
        m_typesettingFeatures |= Ligatures;
        break;
    case FontDescription::NormalLigaturesState:
        break;
    }
}

} // namespace WebCore
