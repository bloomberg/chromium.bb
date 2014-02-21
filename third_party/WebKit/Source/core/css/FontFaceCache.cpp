/*
 * Copyright (C) 2007, 2008, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "config.h"
#include "FontFaceCache.h"

#include "FontFamilyNames.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/CSSSegmentedFontFace.h"
#include "core/css/CSSValueList.h"
#include "core/css/FontFace.h"
#include "core/css/StyleRule.h"
#include "core/fetch/FontResource.h"
#include "core/fetch/ResourceFetcher.h"
#include "platform/fonts/FontDescription.h"
#include "wtf/text/AtomicString.h"

namespace WebCore {

FontFaceCache::FontFaceCache()
    : m_version(0)
{
}

void FontFaceCache::add(CSSFontSelector* cssFontSelector, const StyleRuleFontFace* fontFaceRule, PassRefPtr<FontFace> prpFontFace)
{
    RefPtr<FontFace> fontFace = prpFontFace;
    if (!m_styleRuleToFontFace.add(fontFaceRule, fontFace).isNewEntry)
        return;
    addFontFace(cssFontSelector, fontFace, true);
}

void FontFaceCache::addFontFace(CSSFontSelector* cssFontSelector, PassRefPtr<FontFace> prpFontFace, bool cssConnected)
{
    RefPtr<FontFace> fontFace = prpFontFace;

    OwnPtr<TraitsMap>& familyFontFaces = m_fontFaces.add(fontFace->family(), nullptr).storedValue->value;
    if (!familyFontFaces)
        familyFontFaces = adoptPtr(new TraitsMap);

    RefPtr<CSSSegmentedFontFace>& segmentedFontFace = familyFontFaces->add(fontFace->traitsMask(), nullptr).storedValue->value;
    if (!segmentedFontFace)
        segmentedFontFace = CSSSegmentedFontFace::create(cssFontSelector, static_cast<FontTraitsMask>(fontFace->traitsMask()));

    segmentedFontFace->addFontFace(fontFace, cssConnected);
    if (cssConnected)
        m_cssConnectedFontFaces.add(fontFace);

    ++m_version;
}

void FontFaceCache::remove(const StyleRuleFontFace* fontFaceRule)
{
    StyleRuleToFontFace::iterator it = m_styleRuleToFontFace.find(fontFaceRule);
    if (it != m_styleRuleToFontFace.end()) {
        removeFontFace(it->value.get(), true);
        m_styleRuleToFontFace.remove(it);
    }
}

void FontFaceCache::removeFontFace(FontFace* fontFace, bool cssConnected)
{
    FamilyToTraitsMap::iterator fontFacesIter = m_fontFaces.find(fontFace->family());
    if (fontFacesIter == m_fontFaces.end())
        return;
    TraitsMap* familyFontFaces = fontFacesIter->value.get();

    TraitsMap::iterator familyFontFacesIter = familyFontFaces->find(fontFace->traitsMask());
    if (familyFontFacesIter == familyFontFaces->end())
        return;
    RefPtr<CSSSegmentedFontFace> segmentedFontFace = familyFontFacesIter->value;

    segmentedFontFace->removeFontFace(fontFace);
    if (segmentedFontFace->isEmpty()) {
        familyFontFaces->remove(familyFontFacesIter);
        if (familyFontFaces->isEmpty())
            m_fontFaces.remove(fontFacesIter);
    }
    m_fonts.clear();
    if (cssConnected)
        m_cssConnectedFontFaces.remove(fontFace);

    ++m_version;
}

void FontFaceCache::clear()
{
    for (StyleRuleToFontFace::iterator it = m_styleRuleToFontFace.begin(); it != m_styleRuleToFontFace.end(); ++it)
        removeFontFace(it->value.get(), true);
    m_styleRuleToFontFace.clear();
}

static inline bool compareFontFaces(CSSSegmentedFontFace* first, CSSSegmentedFontFace* second, FontTraitsMask desiredTraitsMask)
{
    FontTraitsMask firstTraitsMask = first->traitsMask();
    FontTraitsMask secondTraitsMask = second->traitsMask();

    bool firstHasDesiredVariant = firstTraitsMask & desiredTraitsMask & FontVariantMask;
    bool secondHasDesiredVariant = secondTraitsMask & desiredTraitsMask & FontVariantMask;

    if (firstHasDesiredVariant != secondHasDesiredVariant)
        return firstHasDesiredVariant;

    // We need to check font-variant css property for CSS2.1 compatibility.
    if (desiredTraitsMask & FontVariantSmallCapsMask) {
        // Prefer a font that has indicated that it can only support small-caps to a font that claims to support
        // all variants. The specialized font is more likely to be true small-caps and not require synthesis.
        bool firstRequiresSmallCaps = (firstTraitsMask & FontVariantSmallCapsMask) && !(firstTraitsMask & FontVariantNormalMask);
        bool secondRequiresSmallCaps = (secondTraitsMask & FontVariantSmallCapsMask) && !(secondTraitsMask & FontVariantNormalMask);
        if (firstRequiresSmallCaps != secondRequiresSmallCaps)
            return firstRequiresSmallCaps;
    }

    bool firstHasDesiredStyle = firstTraitsMask & desiredTraitsMask & FontStyleMask;
    bool secondHasDesiredStyle = secondTraitsMask & desiredTraitsMask & FontStyleMask;

    if (firstHasDesiredStyle != secondHasDesiredStyle)
        return firstHasDesiredStyle;

    if (desiredTraitsMask & FontStyleItalicMask) {
        // Prefer a font that has indicated that it can only support italics to a font that claims to support
        // all styles. The specialized font is more likely to be the one the author wants used.
        bool firstRequiresItalics = (firstTraitsMask & FontStyleItalicMask) && !(firstTraitsMask & FontStyleNormalMask);
        bool secondRequiresItalics = (secondTraitsMask & FontStyleItalicMask) && !(secondTraitsMask & FontStyleNormalMask);
        if (firstRequiresItalics != secondRequiresItalics)
            return firstRequiresItalics;
    }

    if (secondTraitsMask & desiredTraitsMask & FontWeightMask)
        return false;
    if (firstTraitsMask & desiredTraitsMask & FontWeightMask)
        return true;

    // http://www.w3.org/TR/2011/WD-css3-fonts-20111004/#font-matching-algorithm says :
    //   - If the desired weight is less than 400, weights below the desired weight are checked in descending order followed by weights above the desired weight in ascending order until a match is found.
    //   - If the desired weight is greater than 500, weights above the desired weight are checked in ascending order followed by weights below the desired weight in descending order until a match is found.
    //   - If the desired weight is 400, 500 is checked first and then the rule for desired weights less than 400 is used.
    //   - If the desired weight is 500, 400 is checked first and then the rule for desired weights less than 400 is used.

    static const unsigned fallbackRuleSets = 9;
    static const unsigned rulesPerSet = 8;
    static const FontTraitsMask weightFallbackRuleSets[fallbackRuleSets][rulesPerSet] = {
        { FontWeight200Mask, FontWeight300Mask, FontWeight400Mask, FontWeight500Mask, FontWeight600Mask, FontWeight700Mask, FontWeight800Mask, FontWeight900Mask },
        { FontWeight100Mask, FontWeight300Mask, FontWeight400Mask, FontWeight500Mask, FontWeight600Mask, FontWeight700Mask, FontWeight800Mask, FontWeight900Mask },
        { FontWeight200Mask, FontWeight100Mask, FontWeight400Mask, FontWeight500Mask, FontWeight600Mask, FontWeight700Mask, FontWeight800Mask, FontWeight900Mask },
        { FontWeight500Mask, FontWeight300Mask, FontWeight200Mask, FontWeight100Mask, FontWeight600Mask, FontWeight700Mask, FontWeight800Mask, FontWeight900Mask },
        { FontWeight400Mask, FontWeight300Mask, FontWeight200Mask, FontWeight100Mask, FontWeight600Mask, FontWeight700Mask, FontWeight800Mask, FontWeight900Mask },
        { FontWeight700Mask, FontWeight800Mask, FontWeight900Mask, FontWeight500Mask, FontWeight400Mask, FontWeight300Mask, FontWeight200Mask, FontWeight100Mask },
        { FontWeight800Mask, FontWeight900Mask, FontWeight600Mask, FontWeight500Mask, FontWeight400Mask, FontWeight300Mask, FontWeight200Mask, FontWeight100Mask },
        { FontWeight900Mask, FontWeight700Mask, FontWeight600Mask, FontWeight500Mask, FontWeight400Mask, FontWeight300Mask, FontWeight200Mask, FontWeight100Mask },
        { FontWeight800Mask, FontWeight700Mask, FontWeight600Mask, FontWeight500Mask, FontWeight400Mask, FontWeight300Mask, FontWeight200Mask, FontWeight100Mask }
    };

    unsigned ruleSetIndex = 0;
    unsigned w = FontWeight100Bit;
    while (!(desiredTraitsMask & (1 << w))) {
        w++;
        ruleSetIndex++;
    }

    ASSERT(ruleSetIndex < fallbackRuleSets);
    const FontTraitsMask* weightFallbackRule = weightFallbackRuleSets[ruleSetIndex];
    for (unsigned i = 0; i < rulesPerSet; ++i) {
        if (secondTraitsMask & weightFallbackRule[i])
            return false;
        if (firstTraitsMask & weightFallbackRule[i])
            return true;
    }

    return false;
}

CSSSegmentedFontFace* FontFaceCache::get(const FontDescription& fontDescription, const AtomicString& family)
{
    TraitsMap* familyFontFaces = m_fontFaces.get(family);
    if (!familyFontFaces || familyFontFaces->isEmpty())
        return 0;

    OwnPtr<TraitsMap>& segmentedFontFaceCache = m_fonts.add(family, nullptr).storedValue->value;
    if (!segmentedFontFaceCache)
        segmentedFontFaceCache = adoptPtr(new TraitsMap);

    FontTraitsMask traitsMask = fontDescription.traitsMask();

    RefPtr<CSSSegmentedFontFace>& face = segmentedFontFaceCache->add(traitsMask, nullptr).storedValue->value;
    if (!face) {
        for (TraitsMap::const_iterator i = familyFontFaces->begin(); i != familyFontFaces->end(); ++i) {
            CSSSegmentedFontFace* candidate = i->value.get();
            unsigned candidateTraitsMask = candidate->traitsMask();
            if ((traitsMask & FontStyleNormalMask) && !(candidateTraitsMask & FontStyleNormalMask))
                continue;
            if ((traitsMask & FontVariantNormalMask) && !(candidateTraitsMask & FontVariantNormalMask))
                continue;
            if (!face || compareFontFaces(candidate, face.get(), traitsMask))
                face = candidate;
        }
    }
    return face.get();
}

}
