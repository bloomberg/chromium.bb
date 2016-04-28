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

PassRefPtr<FontFallbackIterator> FontFallbackIterator::create(
    const FontDescription& description,
    PassRefPtr<FontFallbackList> fallbackList,
    FontFallbackPriority fontFallbackPriority)
{
    return adoptRef(new FontFallbackIterator(
        description, fallbackList, fontFallbackPriority));
}

FontFallbackIterator::FontFallbackIterator(const FontDescription& description,
    PassRefPtr<FontFallbackList> fallbackList,
    FontFallbackPriority fontFallbackPriority)
    : m_fontDescription(description)
    , m_fontFallbackList(fallbackList)
    , m_currentFontDataIndex(0)
    , m_segmentedFaceIndex(0)
    , m_fallbackStage(FontGroupFonts)
    , m_fontFallbackPriority(fontFallbackPriority)
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
    for (auto it = m_trackedLoadingRangeSets.begin(); it != m_trackedLoadingRangeSets.end(); ++it) {
        if (it->contains(hintChar))
            return true;
    }
    return false;
}

bool FontFallbackIterator::rangeSetContributesForHint(const Vector<UChar32> hintList, const FontDataForRangeSet& segmentedFace)
{
    for (auto it = hintList.begin(); it != hintList.end(); ++it) {
        if (segmentedFace.contains(*it)) {
            if (!alreadyLoadingRangeForHintChar(*it))
                return true;
        }
    }
    return false;
}

void FontFallbackIterator::willUseRange(const AtomicString& family, const FontDataForRangeSet& rangeSet)
{
    FontSelector* selector = m_fontFallbackList->getFontSelector();
    if (!selector)
        return;

    selector->willUseRange(m_fontDescription, family, rangeSet);
}

const FontDataForRangeSet FontFallbackIterator::next(const Vector<UChar32>& hintList)
{
    if (m_fallbackStage == OutOfLuck)
        return FontDataForRangeSet();

    if (m_fallbackStage == FallbackPriorityFonts) {
        // Only try one fallback priority font,
        // then proceed to regular system fallback.
        m_fallbackStage = SystemFonts;
        FontDataForRangeSet fallbackPriorityFontRange(fallbackPriorityFont(hintList[0]));
        if (fallbackPriorityFontRange.hasFontData())
            return fallbackPriorityFontRange;
        return next(hintList);
    }

    if (m_fallbackStage == SystemFonts) {
        // We've reached pref + system fallback.
        ASSERT(hintList.size());
        RefPtr<SimpleFontData> systemFont = uniqueSystemFontForHint(hintList[0]);
        if (systemFont)
            return FontDataForRangeSet(systemFont);

        // If we don't have options from the system fallback anymore or had
        // previously returned them, we only have the last resort font left.
        // TODO: crbug.com/42217 Improve this by doing the last run with a last
        // resort font that has glyphs for everything, for example the Unicode
        // LastResort font, not just Times or Arial.
        FontCache* fontCache = FontCache::fontCache();
        m_fallbackStage = OutOfLuck;
        RefPtr<SimpleFontData> lastResort = fontCache->getLastResortFallbackFont(m_fontDescription).get();
        RELEASE_ASSERT(lastResort);
        return FontDataForRangeSet(lastResort);
    }

    ASSERT(m_fallbackStage == FontGroupFonts
        || m_fallbackStage == SegmentedFace);
    const FontData* fontData = m_fontFallbackList->fontDataAt(
        m_fontDescription, m_currentFontDataIndex);

    if (!fontData) {
        // If there is no fontData coming from the fallback list, it means
        // we are now looking at system fonts, either for prioritized symbol
        // or emoji fonts or by calling system fallback API.
        m_fallbackStage = isNonTextFallbackPriority(m_fontFallbackPriority)
            ? FallbackPriorityFonts
            : SystemFonts;
        return next(hintList);
    }

    // Otherwise we've received a fontData from the font-family: set of fonts,
    // and a non-segmented one in this case.
    if (!fontData->isSegmented()) {
        // Skip forward to the next font family for the next call to next().
        m_currentFontDataIndex++;
        if (!fontData->isLoading()) {
            RefPtr<SimpleFontData> nonSegmented = const_cast<SimpleFontData*>(toSimpleFontData(fontData));
            return FontDataForRangeSet(nonSegmented);
        }
        return next(hintList);
    }

    // Iterate over ranges of a segmented font below.

    const SegmentedFontData* segmented = toSegmentedFontData(fontData);
    if (m_fallbackStage != SegmentedFace) {
        m_segmentedFaceIndex = 0;
        m_fallbackStage = SegmentedFace;
    }

    ASSERT(m_segmentedFaceIndex < segmented->numFaces());
    FontDataForRangeSet currentSegmentedFace = segmented->faceAt(m_segmentedFaceIndex);
    m_segmentedFaceIndex++;

    if (m_segmentedFaceIndex == segmented->numFaces()) {
        // Switch from iterating over a segmented face to the next family from
        // the font-family: group of fonts.
        m_fallbackStage = FontGroupFonts;
        m_currentFontDataIndex++;
    }

    if (rangeSetContributesForHint(hintList, currentSegmentedFace)) {
        const SimpleFontData* fontData = currentSegmentedFace.fontData();
        if (const CustomFontData* customFontData = fontData->customFontData())
            customFontData->beginLoadIfNeeded();
        if (!fontData->isLoading())
            return currentSegmentedFace;
        m_trackedLoadingRangeSets.append(currentSegmentedFace);
    }

    return next(hintList);
}

const PassRefPtr<SimpleFontData> FontFallbackIterator::fallbackPriorityFont(
    UChar32 hint)
{
    return FontCache::fontCache()->fallbackFontForCharacter(
        m_fontDescription,
        hint,
        m_fontFallbackList->primarySimpleFontData(m_fontDescription),
        m_fontFallbackPriority);
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
