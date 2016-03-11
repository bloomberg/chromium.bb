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
#include "platform/text/TextPath.h"
#include "platform/text/TextRun.h"
#include "platform/text/TextRunIterator.h"
#include "wtf/Vector.h"

namespace blink {

class SVGTextMetricsCalculator {
public:
    SVGTextMetricsCalculator(LayoutSVGInlineText*);
    ~SVGTextMetricsCalculator();

    bool advancePosition();
    unsigned currentPosition() const { return m_currentPosition; }

    SVGTextMetrics currentCharacterMetrics();

    // TODO(pdr): Character-based iteration is ambiguous and error-prone. It
    // should be unified under a single concept. See: https://crbug.com/593570
    bool currentCharacterStartsSurrogatePair() const
    {
        return U16_IS_LEAD(m_run[m_currentPosition]) && m_currentPosition + 1 < characterCount() && U16_IS_TRAIL(m_run[m_currentPosition + 1]);
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

    // Ensure |m_subrunRanges| is updated for the current bidi run, or the
    // complete m_run if no bidi runs are present. Returns the current position
    // in the subrun which can be used to index into |m_subrunRanges|.
    unsigned updateSubrunRangesForCurrentPosition();

    // Current character position in m_text.
    unsigned m_currentPosition;

    LineLayoutSVGInlineText m_text;
    TextRun m_run;

    BidiCharacterRun* m_bidiRun;
    BidiResolver<TextRunIterator, BidiCharacterRun> m_bidiResolver;

    // Ranges for the current bidi run if present, or the entire run otherwise.
    Vector<CharacterRange> m_subrunRanges;
};

SVGTextMetricsCalculator::SVGTextMetricsCalculator(LayoutSVGInlineText* text)
    : m_currentPosition(0)
    , m_text(LineLayoutSVGInlineText(text))
    , m_run(SVGTextMetrics::constructTextRun(m_text, 0, m_text.textLength(), m_text.styleRef().direction()))
    , m_bidiRun(nullptr)
{
    setupBidiRuns();
}

SVGTextMetricsCalculator::~SVGTextMetricsCalculator()
{
    if (m_bidiRun)
        m_bidiResolver.runs().deleteRuns();
}

void SVGTextMetricsCalculator::setupBidiRuns()
{
    if (isOverride(m_text.styleRef().unicodeBidi()))
        return;
    BidiStatus status(LTR, false);
    status.last = status.lastStrong = WTF::Unicode::OtherNeutral;
    m_bidiResolver.setStatus(status);
    m_bidiResolver.setPositionIgnoringNestedIsolates(TextRunIterator(&m_run, 0));
    const bool hardLineBreak = false;
    const bool reorderRuns = false;
    m_bidiResolver.createBidiRunsForLine(TextRunIterator(&m_run, m_run.length()), NoVisualOverride, hardLineBreak, reorderRuns);
    BidiRunList<BidiCharacterRun>& bidiRuns = m_bidiResolver.runs();
    m_bidiRun = bidiRuns.firstRun();
}

bool SVGTextMetricsCalculator::advancePosition()
{
    m_currentPosition += currentCharacterStartsSurrogatePair() ? 2 : 1;
    return m_currentPosition < characterCount();
}

unsigned SVGTextMetricsCalculator::updateSubrunRangesForCurrentPosition()
{
    if (m_bidiRun) {
        if (m_currentPosition >= static_cast<unsigned>(m_bidiRun->stop())) {
            m_bidiRun = m_bidiRun->next();
            // Ensure new subrange ranges are computed below.
            m_subrunRanges.clear();
        }
        ASSERT(m_bidiRun);
        ASSERT(static_cast<int>(m_currentPosition) < m_bidiRun->stop());
    }

    unsigned positionInRun = m_bidiRun ? m_currentPosition - m_bidiRun->start() : m_currentPosition;
    if (positionInRun >= m_subrunRanges.size()) {
        unsigned subrunStart = m_bidiRun ? m_bidiRun->start() : 0;
        unsigned subrunEnd = m_bidiRun ? m_bidiRun->stop() : m_run.charactersLength();
        TextDirection subrunDirection = m_bidiRun ? m_bidiRun->direction() : m_text.styleRef().direction();
        TextRun subRun = SVGTextMetrics::constructTextRun(m_text, subrunStart, subrunEnd - subrunStart, subrunDirection);
        m_subrunRanges = m_text.scaledFont().individualCharacterRanges(subRun, 0, subRun.length() - 1);

        // TODO(pdr): We only have per-glyph data so we need to synthesize per-
        // grapheme data. E.g., if 'fi' is shaped into a single glyph, we do not
        // know the 'i' position. The code below synthesizes an average glyph
        // width when characters share a position. This will incorrectly split
        // combining diacritics. See: https://crbug.com/473476.
        unsigned distributeCount = 0;
        for (int rangeIndex = static_cast<int>(m_subrunRanges.size()) - 1; rangeIndex >= 0; --rangeIndex) {
            CharacterRange& currentRange = m_subrunRanges[rangeIndex];
            if (currentRange.width() == 0) {
                distributeCount++;
            } else if (distributeCount != 0) {
                distributeCount++;
                float newWidth = currentRange.width() / distributeCount;
                currentRange.end = currentRange.start + newWidth;
                for (unsigned distribute = 1; distribute < distributeCount; distribute++) {
                    unsigned distributeIndex = rangeIndex + distribute;
                    float newStartPosition = m_subrunRanges[distributeIndex - 1].end;
                    m_subrunRanges[distributeIndex].start = newStartPosition;
                    m_subrunRanges[distributeIndex].end = newStartPosition + newWidth;
                }
                distributeCount = 0;
            }
        }
    }

    return positionInRun;
}

SVGTextMetrics SVGTextMetricsCalculator::currentCharacterMetrics()
{
    unsigned currentSubrunPosition = updateSubrunRangesForCurrentPosition();
    unsigned length = currentCharacterStartsSurrogatePair() ? 2 : 1;
    float width = m_subrunRanges[currentSubrunPosition].width();
    return SVGTextMetrics(m_text, length, width);
}

struct MeasureTextData {
    MeasureTextData(SVGCharacterDataMap* characterDataMap)
        : allCharactersMap(characterDataMap)
        , lastCharacterWasWhiteSpace(true)
        , valueListPosition(0)
    {
    }

    SVGCharacterDataMap* allCharactersMap;
    bool lastCharacterWasWhiteSpace;
    unsigned valueListPosition;
};

static void measureTextLayoutObject(LayoutSVGInlineText* text, MeasureTextData* data, bool processLayoutObject)
{
    ASSERT(text);

    SVGTextLayoutAttributes* attributes = text->layoutAttributes();
    Vector<SVGTextMetrics>* textMetricsValues = &attributes->textMetricsValues();
    if (processLayoutObject) {
        if (data->allCharactersMap)
            attributes->clear();
        else
            textMetricsValues->clear();
    }

    // TODO(pdr): This loop is too tightly coupled to SVGTextMetricsCalculator.
    // We should refactor SVGTextMetricsCalculator to be a simple bidi run
    // iterator and move all subrun logic to a single function.
    SVGTextMetricsCalculator calculator(text);
    bool preserveWhiteSpace = text->style()->whiteSpace() == PRE;
    unsigned surrogatePairCharacters = 0;
    unsigned skippedCharacters = 0;
    do {
        SVGTextMetrics metrics = calculator.currentCharacterMetrics();
        if (!metrics.length())
            break;

        bool characterIsWhiteSpace = calculator.currentCharacterIsWhiteSpace();
        if (characterIsWhiteSpace && !preserveWhiteSpace && data->lastCharacterWasWhiteSpace) {
            if (processLayoutObject)
                textMetricsValues->append(SVGTextMetrics(SVGTextMetrics::SkippedSpaceMetrics));
            if (data->allCharactersMap)
                skippedCharacters += metrics.length();
            continue;
        }

        if (processLayoutObject) {
            if (data->allCharactersMap) {
                const SVGCharacterDataMap::const_iterator it = data->allCharactersMap->find(data->valueListPosition + calculator.currentPosition() - skippedCharacters - surrogatePairCharacters + 1);
                if (it != data->allCharactersMap->end())
                    attributes->characterDataMap().set(calculator.currentPosition() + 1, it->value);
            }
            textMetricsValues->append(metrics);
        }

        if (data->allCharactersMap && calculator.currentCharacterStartsSurrogatePair())
            surrogatePairCharacters++;

        data->lastCharacterWasWhiteSpace = characterIsWhiteSpace;
    } while (calculator.advancePosition());

    if (!data->allCharactersMap)
        return;

    data->valueListPosition += calculator.currentPosition() - skippedCharacters;
}

static void walkTree(LayoutSVGText* start, LayoutSVGInlineText* stopAtLeaf, MeasureTextData* data)
{
    LayoutObject* child = start->firstChild();
    while (child) {
        if (child->isSVGInlineText()) {
            LayoutSVGInlineText* text = toLayoutSVGInlineText(child);
            measureTextLayoutObject(text, data, !stopAtLeaf || stopAtLeaf == text);
            if (stopAtLeaf && stopAtLeaf == text)
                return;
        } else if (child->isSVGInline()) {
            // Visit children of text content elements.
            if (LayoutObject* inlineChild = toLayoutSVGInline(child)->firstChild()) {
                child = inlineChild;
                continue;
            }
        }
        child = child->nextInPreOrderAfterChildren(start);
    }
}

void SVGTextMetricsBuilder::measureTextLayoutObject(LayoutSVGInlineText* text)
{
    ASSERT(text);

    LayoutSVGText* textRoot = LayoutSVGText::locateLayoutSVGTextAncestor(text);
    if (!textRoot)
        return;

    MeasureTextData data(0);
    walkTree(textRoot, text, &data);
}

void SVGTextMetricsBuilder::buildMetricsAndLayoutAttributes(LayoutSVGText* textRoot, LayoutSVGInlineText* stopAtLeaf, SVGCharacterDataMap& allCharactersMap)
{
    ASSERT(textRoot);
    MeasureTextData data(&allCharactersMap);
    walkTree(textRoot, stopAtLeaf, &data);
}

} // namespace blink
