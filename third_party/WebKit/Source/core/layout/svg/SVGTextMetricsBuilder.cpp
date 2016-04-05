/*
 * Copyright (C) Research In Motion Limited 2010-2012. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/layout/svg/SVGTextMetricsBuilder.h"

#include "core/layout/api/LineLayoutSVGInlineText.h"
#include "core/layout/svg/LayoutSVGInline.h"
#include "core/layout/svg/LayoutSVGInlineText.h"
#include "core/layout/svg/LayoutSVGText.h"
#include "core/layout/svg/SVGTextMetrics.h"
#include "platform/fonts/CharacterRange.h"
#include "platform/text/BidiCharacterRun.h"
#include "platform/text/BidiResolver.h"
#include "platform/text/TextDirection.h"
#include "platform/text/TextRun.h"
#include "platform/text/TextRunIterator.h"
#include "wtf/Vector.h"

namespace blink {

namespace {

inline bool characterStartsSurrogatePair(const TextRun& run, unsigned index)
{
    if (!U16_IS_LEAD(run[index]))
        return false;
    if (index + 1 >= static_cast<unsigned>(run.charactersLength()))
        return false;
    return U16_IS_TRAIL(run[index + 1]);
}

class SVGTextMetricsCalculator {
public:
    SVGTextMetricsCalculator(LineLayoutSVGInlineText);
    ~SVGTextMetricsCalculator();

    bool advancePosition();
    unsigned currentPosition() const { return m_currentPosition; }

    SVGTextMetrics currentCharacterMetrics();

    // TODO(pdr): Character-based iteration is ambiguous and error-prone. It
    // should be unified under a single concept. See: https://crbug.com/593570
    bool currentCharacterStartsSurrogatePair() const
    {
        return characterStartsSurrogatePair(m_run, m_currentPosition);
    }
    bool currentCharacterIsWhiteSpace() const
    {
        return m_run[m_currentPosition] == ' ';
    }
    unsigned characterCount() const
    {
        return static_cast<unsigned>(m_run.charactersLength());
    }

private:
    void setupBidiRuns();

    static TextRun constructTextRun(LineLayoutSVGInlineText, unsigned position, unsigned length, TextDirection);

    // Ensure |m_subrunRanges| is updated for the current bidi run, or the
    // complete m_run if no bidi runs are present. Returns the current position
    // in the subrun which can be used to index into |m_subrunRanges|.
    unsigned updateSubrunRangesForCurrentPosition();

    // Current character position in m_text.
    unsigned m_currentPosition;

    LineLayoutSVGInlineText m_text;
    float m_fontScalingFactor;
    float m_cachedFontHeight;
    TextRun m_run;

    BidiCharacterRun* m_bidiRun;
    BidiResolver<TextRunIterator, BidiCharacterRun> m_bidiResolver;

    // Ranges for the current bidi run if present, or the entire run otherwise.
    Vector<CharacterRange> m_subrunRanges;
};

TextRun SVGTextMetricsCalculator::constructTextRun(LineLayoutSVGInlineText textLayoutItem, unsigned position, unsigned length, TextDirection textDirection)
{
    const ComputedStyle& style = textLayoutItem.styleRef();

    TextRun run(static_cast<const LChar*>(nullptr) // characters, will be set below if non-zero.
        , 0 // length, will be set below if non-zero.
        , 0 // xPos, only relevant with allowTabs=true
        , 0 // padding, only relevant for justified text, not relevant for SVG
        , TextRun::AllowTrailingExpansion
        , textDirection
        , isOverride(style.unicodeBidi()) /* directionalOverride */);

    if (length) {
        if (textLayoutItem.is8Bit())
            run.setText(textLayoutItem.characters8() + position, length);
        else
            run.setText(textLayoutItem.characters16() + position, length);
    }

    // We handle letter & word spacing ourselves.
    run.disableSpacing();

    // Propagate the maximum length of the characters buffer to the TextRun, even when we're only processing a substring.
    run.setCharactersLength(textLayoutItem.textLength() - position);
    ASSERT(run.charactersLength() >= run.length());
    return run;
}

SVGTextMetricsCalculator::SVGTextMetricsCalculator(LineLayoutSVGInlineText text)
    : m_currentPosition(0)
    , m_text(text)
    , m_fontScalingFactor(m_text.scalingFactor())
    , m_cachedFontHeight(m_text.scaledFont().getFontMetrics().floatHeight() / m_fontScalingFactor)
    , m_run(constructTextRun(m_text, 0, m_text.textLength(), m_text.styleRef().direction()))
    , m_bidiRun(nullptr)
{
    setupBidiRuns();
}

SVGTextMetricsCalculator::~SVGTextMetricsCalculator()
{
    m_bidiResolver.runs().deleteRuns();
}

void SVGTextMetricsCalculator::setupBidiRuns()
{
    BidiRunList<BidiCharacterRun>& bidiRuns = m_bidiResolver.runs();
    bool bidiOverride = isOverride(m_text.styleRef().unicodeBidi());
    BidiStatus status(LTR, bidiOverride);
    if (m_run.is8Bit() || bidiOverride) {
        WTF::Unicode::CharDirection direction = WTF::Unicode::LeftToRight;
        // If BiDi override is in effect, use the specified direction.
        if (bidiOverride && !m_text.styleRef().isLeftToRightDirection())
            direction = WTF::Unicode::RightToLeft;
        bidiRuns.addRun(new BidiCharacterRun(0, m_run.charactersLength(), status.context.get(), direction));
    } else {
        status.last = status.lastStrong = WTF::Unicode::OtherNeutral;
        m_bidiResolver.setStatus(status);
        m_bidiResolver.setPositionIgnoringNestedIsolates(TextRunIterator(&m_run, 0));
        const bool hardLineBreak = false;
        const bool reorderRuns = false;
        m_bidiResolver.createBidiRunsForLine(TextRunIterator(&m_run, m_run.length()), NoVisualOverride, hardLineBreak, reorderRuns);
    }
    m_bidiRun = bidiRuns.firstRun();
}

bool SVGTextMetricsCalculator::advancePosition()
{
    m_currentPosition += currentCharacterStartsSurrogatePair() ? 2 : 1;
    return m_currentPosition < characterCount();
}

// TODO(pdr): We only have per-glyph data so we need to synthesize per-grapheme
// data. E.g., if 'fi' is shaped into a single glyph, we do not know the 'i'
// position. The code below synthesizes an average glyph width when characters
// share a single position. This will incorrectly split combining diacritics.
// See: https://crbug.com/473476.
static void synthesizeGraphemeWidths(const TextRun& run, Vector<CharacterRange>& ranges)
{
    unsigned distributeCount = 0;
    for (int rangeIndex = static_cast<int>(ranges.size()) - 1; rangeIndex >= 0; --rangeIndex) {
        CharacterRange& currentRange = ranges[rangeIndex];
        if (currentRange.width() == 0) {
            distributeCount++;
        } else if (distributeCount != 0) {
            // Only count surrogate pairs as a single character.
            bool surrogatePair = characterStartsSurrogatePair(run, rangeIndex);
            if (!surrogatePair)
                distributeCount++;

            float newWidth = currentRange.width() / distributeCount;
            currentRange.end = currentRange.start + newWidth;
            float lastEndPosition = currentRange.end;
            for (unsigned distribute = 1; distribute < distributeCount; distribute++) {
                // This surrogate pair check will skip processing of the second
                // character forming the surrogate pair.
                unsigned distributeIndex = rangeIndex + distribute + (surrogatePair ? 1 : 0);
                ranges[distributeIndex].start = lastEndPosition;
                ranges[distributeIndex].end = lastEndPosition + newWidth;
                lastEndPosition = ranges[distributeIndex].end;
            }

            distributeCount = 0;
        }
    }
}

unsigned SVGTextMetricsCalculator::updateSubrunRangesForCurrentPosition()
{
    ASSERT(m_bidiRun);
    if (m_currentPosition >= static_cast<unsigned>(m_bidiRun->stop())) {
        m_bidiRun = m_bidiRun->next();
        // Ensure new subrange ranges are computed below.
        m_subrunRanges.clear();
    }
    ASSERT(m_bidiRun);
    ASSERT(static_cast<int>(m_currentPosition) < m_bidiRun->stop());

    unsigned positionInRun = m_currentPosition - m_bidiRun->start();
    if (positionInRun >= m_subrunRanges.size()) {
        TextRun subRun = constructTextRun(m_text, m_bidiRun->start(),
            m_bidiRun->stop() - m_bidiRun->start(), m_bidiRun->direction());
        m_subrunRanges = m_text.scaledFont().individualCharacterRanges(subRun);
        synthesizeGraphemeWidths(subRun, m_subrunRanges);
    }

    ASSERT(m_subrunRanges.size() && positionInRun < m_subrunRanges.size());
    return positionInRun;
}

SVGTextMetrics SVGTextMetricsCalculator::currentCharacterMetrics()
{
    unsigned currentSubrunPosition = updateSubrunRangesForCurrentPosition();
    unsigned length = currentCharacterStartsSurrogatePair() ? 2 : 1;
    float width = m_subrunRanges[currentSubrunPosition].width();
    return SVGTextMetrics(length, width / m_fontScalingFactor, m_cachedFontHeight);
}

struct TreeWalkTextState {
    TreeWalkTextState()
        : lastCharacterWasWhiteSpace(true)
        , valueListPosition(0) { }

    bool lastCharacterWasWhiteSpace;
    unsigned valueListPosition;
};

// Struct for updating SVGTextLayoutAttributes. If an SVGCharacterDataMap is
// available, the attribute's character data map will also be updated.
// TreeWalkTextState should be used to maintain the value list position which
// indexes into the SVGCharacterDataMap of all characters.
struct UpdateAttributes {
    UpdateAttributes(
        SVGTextLayoutAttributes& textAttributes,
        Vector<SVGTextMetrics>& metricsList,
        const SVGCharacterDataMap* allCharacters)
        : metricsList(metricsList)
        , attributes(textAttributes)
        , allCharactersMap(allCharacters) { }

    void clearExistingAttributes()
    {
        metricsList.clear();

        if (allCharactersMap)
            attributes.clear();
    }

    void addMetrics(SVGTextMetrics metrics)
    {
        metricsList.append(metrics);
    }

    void updateCharacterDataMap(unsigned valueListPosition, unsigned currentTextPosition)
    {
        if (!allCharactersMap)
            return;
        const SVGCharacterDataMap::const_iterator it = allCharactersMap->find(valueListPosition);
        if (it != allCharactersMap->end())
            attributes.characterDataMap().set(currentTextPosition, it->value);
    }

    Vector<SVGTextMetrics>& metricsList;
    SVGTextLayoutAttributes& attributes;
    const SVGCharacterDataMap* allCharactersMap;
};

void walkInlineText(LayoutSVGInlineText* text, TreeWalkTextState& textState, UpdateAttributes* attributesToUpdate = nullptr)
{
    LineLayoutSVGInlineText textLayoutItem(text);
    if (attributesToUpdate)
        attributesToUpdate->clearExistingAttributes();

    if (!textLayoutItem.textLength())
        return;

    // TODO(pdr): This loop is too tightly coupled to SVGTextMetricsCalculator.
    // We should refactor SVGTextMetricsCalculator to be a simple bidi run
    // iterator and move all subrun logic to a single function.
    SVGTextMetricsCalculator calculator(textLayoutItem);
    bool preserveWhiteSpace = textLayoutItem.styleRef().whiteSpace() == PRE;
    unsigned surrogatePairCharacters = 0;
    unsigned skippedWhitespace = 0;
    do {
        bool currentCharacterIsWhiteSpace = calculator.currentCharacterIsWhiteSpace();
        if (currentCharacterIsWhiteSpace && !preserveWhiteSpace && textState.lastCharacterWasWhiteSpace) {
            if (attributesToUpdate)
                attributesToUpdate->addMetrics(SVGTextMetrics(SVGTextMetrics::SkippedSpaceMetrics));
            ASSERT(calculator.currentCharacterMetrics().length() == 1);
            skippedWhitespace++;
            continue;
        }

        if (attributesToUpdate) {
            attributesToUpdate->updateCharacterDataMap(textState.valueListPosition - skippedWhitespace - surrogatePairCharacters + calculator.currentPosition() + 1, calculator.currentPosition() + 1);
            attributesToUpdate->addMetrics(calculator.currentCharacterMetrics());
        }

        if (calculator.currentCharacterStartsSurrogatePair())
            surrogatePairCharacters++;
        textState.lastCharacterWasWhiteSpace = currentCharacterIsWhiteSpace;
    } while (calculator.advancePosition());

    textState.valueListPosition += calculator.currentPosition() - skippedWhitespace;
}

void walkTree(LayoutSVGText& start, LayoutSVGInlineText* stopAtText, SVGCharacterDataMap* allCharactersMap = nullptr)
{
    TreeWalkTextState textState;
    LayoutObject* child = start.firstChild();
    while (child) {
        if (child->isSVGInlineText()) {
            LayoutSVGInlineText* text = toLayoutSVGInlineText(child);
            OwnPtr<UpdateAttributes> attributesToUpdate = nullptr;
            if (!stopAtText || stopAtText == text)
                attributesToUpdate = adoptPtr(new UpdateAttributes(*text->layoutAttributes(), text->metricsList(), allCharactersMap));
            walkInlineText(text, textState, attributesToUpdate.get());
            if (stopAtText == text)
                return;
        } else if (child->isSVGInline()) {
            // Visit children of text content elements.
            if (LayoutObject* inlineChild = toLayoutSVGInline(child)->firstChild()) {
                child = inlineChild;
                continue;
            }
        }
        child = child->nextInPreOrderAfterChildren(&start);
    }
}

} // namespace

void SVGTextMetricsBuilder::measureTextLayoutObject(LayoutSVGText& textRoot, LayoutSVGInlineText& text)
{
    walkTree(textRoot, &text);
}

void SVGTextMetricsBuilder::buildMetricsAndLayoutAttributes(LayoutSVGText& textRoot, SVGCharacterDataMap& allCharactersMap)
{
    walkTree(textRoot, nullptr, &allCharactersMap);
}

} // namespace blink
