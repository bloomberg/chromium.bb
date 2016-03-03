// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/FontFallbackIterator.h"

#include "platform/Logging.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/FontFallbackList.h"
#include "platform/fonts/SegmentedFontData.h"
#include "platform/fonts/SimpleFontData.h"

namespace blink {

PassRefPtr<FontFallbackIterator> FontFallbackIterator::create(const FontDescription& description, PassRefPtr<FontFallbackList> fallbackList)
{
    return adoptRef(new FontFallbackIterator(description, fallbackList));
}

FontFallbackIterator::FontFallbackIterator(const FontDescription& description, PassRefPtr<FontFallbackList> fallbackList)
    : m_fontDescription(description)
    , m_fontFallbackList(fallbackList)
    , m_currentFontDataIndex(0)
    , m_segmentedIndex(0)
    , m_fallbackStage(FontGroupFonts)
{
}

bool FontFallbackIterator::needsHintList() const
{
    if (m_fallbackStage != FontGroupFonts && m_fallbackStage != SegmentedFace) {
        return false;
    }

    const FontData* fontData = m_fontFallbackList->fontDataAt(m_fontDescription, m_currentFontDataIndex);
    return fontData && fontData->isSegmented();
}

bool FontFallbackIterator::alreadyLoadingRangeForHintChar(UChar32 hintChar)
{
    for (auto it = m_loadingCustomFontForRanges.begin(); it != m_loadingCustomFontForRanges.end(); ++it) {
        if (it->contains(hintChar))
            return true;
    }
    return false;
}

bool FontFallbackIterator::rangeContributesForHint(const Vector<UChar32> hintList, const FontDataRange& fontDataRange)
{
    for (auto it = hintList.begin(); it != hintList.end(); ++it) {
        if (*it >= fontDataRange.from() && *it <= fontDataRange.to()) {
            if (!alreadyLoadingRangeForHintChar(*it))
                return true;
        }
    }
    return false;
}

void FontFallbackIterator::willUseRange(const AtomicString& family, const FontDataRange& range)
{
    FontSelector* selector = m_fontFallbackList->fontSelector();
    if (!selector)
        return;

    selector->willUseRange(m_fontDescription, family, range);
}

const FontDataRange FontFallbackIterator::next(const Vector<UChar32>& hintList)
{
    if (m_fallbackStage == OutOfLuck)
        return FontDataRange();

    const FontData* fontData = m_fontFallbackList->fontDataAt(m_fontDescription, m_currentFontDataIndex);

    // If there is no fontData coming from the fallback list, it means
    // we have reached the system fallback stage.
    if (!fontData) {
        m_fallbackStage = SystemFonts;
        // We've reached pref + system fallback.
        ASSERT(hintList.size());
        RefPtr<SimpleFontData> systemFont = uniqueSystemFontForHint(hintList[0]);
        if (systemFont)
            return FontDataRange(systemFont);

        // If we don't have options from the system fallback anymore or had
        // previously returned them, we only have the last resort font left.
        // TODO: crbug.com/42217 Improve this by doing the last run with a last
        // resort font that has glyphs for everything, for example the Unicode
        // LastResort font, not just Times or Arial.
        FontCache* fontCache = FontCache::fontCache();
        m_fallbackStage = OutOfLuck;
        RefPtr<SimpleFontData> lastResort = fontCache->getLastResortFallbackFont(m_fontDescription).get();
        RELEASE_ASSERT(lastResort);
        return FontDataRange(lastResort);
    }

    // Otherwise we've received a fontData from the font-family: set of fonts,
    // and a non-segmented one in this case.
    if (!fontData->isSegmented()) {
        // Skip forward to the next font family for the next call to next().
        m_currentFontDataIndex++;
        if (!fontData->isLoading()) {
            RefPtr<SimpleFontData> nonSegmented = const_cast<SimpleFontData*>(toSimpleFontData(fontData));
            return FontDataRange(nonSegmented);
        }
        return next(hintList);
    }

    // Iterate over ranges of a segmented font below.

    const SegmentedFontData* segmented = toSegmentedFontData(fontData);
    if (m_fallbackStage != SegmentedFace) {
        m_segmentedIndex = 0;
        m_fallbackStage = SegmentedFace;
    }

    ASSERT(m_segmentedIndex < segmented->numRanges());
    FontDataRange currentRange = segmented->rangeAt(m_segmentedIndex);
    m_segmentedIndex++;

    if (m_segmentedIndex == segmented->numRanges()) {
        // Switch from iterating over a segmented face to the next family from
        // the font-family: group of fonts.
        m_fallbackStage = FontGroupFonts;
        m_currentFontDataIndex++;
    }

    if (rangeContributesForHint(hintList, currentRange)) {
        if (currentRange.fontData()->customFontData())
            currentRange.fontData()->customFontData()->beginLoadIfNeeded();
        if (!currentRange.fontData()->isLoading())
            return currentRange;
        m_loadingCustomFontForRanges.append(currentRange);
    }

    return next(hintList);
}

const PassRefPtr<SimpleFontData> FontFallbackIterator::uniqueSystemFontForHint(UChar32 hint)
{
    FontCache* fontCache = FontCache::fontCache();

    // When we're asked for a fallback for the same characters again, we give up
    // because the shaper must have previously tried shaping with the font
    // already.
    if (m_visitedSystemFonts.find(hint) != m_visitedSystemFonts.end()) {
        return nullptr;
    }

    RefPtr<SimpleFontData> fallbackFont = fontCache->fallbackFontForCharacter(m_fontDescription, hint, m_fontFallbackList->primarySimpleFontData(m_fontDescription));

    return m_visitedSystemFonts.add(hint, fallbackFont).storedValue->value;
}

} // namespace blink
