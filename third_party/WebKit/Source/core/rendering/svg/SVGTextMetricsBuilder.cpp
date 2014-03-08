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

#include "config.h"

#include "core/rendering/svg/SVGTextMetricsBuilder.h"

#include "core/rendering/svg/RenderSVGInlineText.h"
#include "core/rendering/svg/RenderSVGText.h"
#include "core/rendering/svg/SVGTextMetrics.h"
#include "platform/fonts/WidthIterator.h"
#include "platform/text/TextPath.h"
#include "platform/text/TextRun.h"
#include "wtf/Vector.h"

namespace WebCore {

class SVGTextMetricsCalculator {
public:
    SVGTextMetricsCalculator(RenderSVGInlineText*);

    SVGTextMetrics computeMetricsForCharacter(unsigned textPosition);
    unsigned textLength() const { return static_cast<unsigned>(m_run.charactersLength()); }

    bool characterStartsSurrogatePair(unsigned textPosition) const
    {
        return U16_IS_LEAD(m_run[textPosition]) && textPosition + 1 < textLength() && U16_IS_TRAIL(m_run[textPosition + 1]);
    }
    bool characterIsWhiteSpace(unsigned textPosition) const
    {
        return m_run[textPosition] == ' ';
    }

private:
    SVGTextMetrics computeMetricsForCharacterSimple(unsigned textPosition);
    SVGTextMetrics computeMetricsForCharacterComplex(unsigned textPosition);

    RenderSVGInlineText* m_text;
    TextRun m_run;
    bool m_isComplexText;
    float m_totalWidth;

    // Simple text only.
    OwnPtr<WidthIterator> m_simpleWidthIterator;
};

SVGTextMetricsCalculator::SVGTextMetricsCalculator(RenderSVGInlineText* text)
    : m_text(text)
    , m_run(SVGTextMetrics::constructTextRun(text, 0, text->textLength()))
    , m_isComplexText(false)
    , m_totalWidth(0)
{
    const Font& scaledFont = text->scaledFont();
    CodePath codePath = scaledFont.codePath(m_run);
    m_isComplexText = codePath == ComplexPath;
    m_run.setCharacterScanForCodePath(!m_isComplexText);

    if (!m_isComplexText)
        m_simpleWidthIterator = adoptPtr(new WidthIterator(&scaledFont, m_run));
}

SVGTextMetrics SVGTextMetricsCalculator::computeMetricsForCharacterSimple(unsigned textPosition)
{
    GlyphBuffer glyphBuffer;
    unsigned metricsLength = m_simpleWidthIterator->advance(textPosition + 1, &glyphBuffer);
    if (!metricsLength)
        return SVGTextMetrics();

    float currentWidth = m_simpleWidthIterator->runWidthSoFar() - m_totalWidth;
    m_totalWidth = m_simpleWidthIterator->runWidthSoFar();

    Glyph glyphId = glyphBuffer.glyphAt(0);
    return SVGTextMetrics(m_text, textPosition, metricsLength, currentWidth, glyphId);
}

SVGTextMetrics SVGTextMetricsCalculator::computeMetricsForCharacterComplex(unsigned textPosition)
{
    unsigned metricsLength = characterStartsSurrogatePair(textPosition) ? 2 : 1;
    SVGTextMetrics metrics = SVGTextMetrics::measureCharacterRange(m_text, textPosition, metricsLength);
    ASSERT(metrics.length() == metricsLength);

    SVGTextMetrics complexStartToCurrentMetrics = SVGTextMetrics::measureCharacterRange(m_text, 0, textPosition + metricsLength);
    // Frequent case for Arabic text: when measuring a single character the arabic isolated form is taken
    // when rendering the glyph "in context" (with it's surrounding characters) it changes due to shaping.
    // So whenever currentWidth != currentMetrics.width(), we are processing a text run whose length is
    // not equal to the sum of the individual lengths of the glyphs, when measuring them isolated.
    float currentWidth = complexStartToCurrentMetrics.width() - m_totalWidth;
    if (currentWidth != metrics.width())
        metrics.setWidth(currentWidth);

    m_totalWidth = complexStartToCurrentMetrics.width();
    return metrics;
}

SVGTextMetrics SVGTextMetricsCalculator::computeMetricsForCharacter(unsigned textPosition)
{
    if (m_isComplexText)
        return computeMetricsForCharacterComplex(textPosition);

    return computeMetricsForCharacterSimple(textPosition);
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

static void measureTextRenderer(RenderSVGInlineText* text, MeasureTextData* data, bool processRenderer)
{
    ASSERT(text);

    SVGTextLayoutAttributes* attributes = text->layoutAttributes();
    Vector<SVGTextMetrics>* textMetricsValues = &attributes->textMetricsValues();
    if (processRenderer) {
        if (data->allCharactersMap)
            attributes->clear();
        else
            textMetricsValues->clear();
    }

    SVGTextMetricsCalculator calculator(text);
    bool preserveWhiteSpace = text->style()->whiteSpace() == PRE;
    unsigned surrogatePairCharacters = 0;
    unsigned skippedCharacters = 0;
    unsigned textPosition = 0;
    unsigned textLength = calculator.textLength();

    SVGTextMetrics currentMetrics;
    for (; textPosition < textLength; textPosition += currentMetrics.length()) {
        currentMetrics = calculator.computeMetricsForCharacter(textPosition);
        if (!currentMetrics.length())
            break;

        bool characterIsWhiteSpace = calculator.characterIsWhiteSpace(textPosition);
        if (characterIsWhiteSpace && !preserveWhiteSpace && data->lastCharacterWasWhiteSpace) {
            if (processRenderer)
                textMetricsValues->append(SVGTextMetrics(SVGTextMetrics::SkippedSpaceMetrics));
            if (data->allCharactersMap)
                skippedCharacters += currentMetrics.length();
            continue;
        }

        if (processRenderer) {
            if (data->allCharactersMap) {
                const SVGCharacterDataMap::const_iterator it = data->allCharactersMap->find(data->valueListPosition + textPosition - skippedCharacters - surrogatePairCharacters + 1);
                if (it != data->allCharactersMap->end())
                    attributes->characterDataMap().set(textPosition + 1, it->value);
            }
            textMetricsValues->append(currentMetrics);
        }

        if (data->allCharactersMap && calculator.characterStartsSurrogatePair(textPosition))
            surrogatePairCharacters++;

        data->lastCharacterWasWhiteSpace = characterIsWhiteSpace;
    }

    if (!data->allCharactersMap)
        return;

    data->valueListPosition += textPosition - skippedCharacters;
}

static void walkTree(RenderObject* start, RenderSVGInlineText* stopAtLeaf, MeasureTextData* data)
{
    RenderObject* child = start->firstChild();
    while (child) {
        if (child->isSVGInlineText()) {
            RenderSVGInlineText* text = toRenderSVGInlineText(child);
            measureTextRenderer(text, data, !stopAtLeaf || stopAtLeaf == text);
            if (stopAtLeaf && stopAtLeaf == text)
                return;
        } else if (child->isSVGInline()) {
            // Visit children of text content elements.
            if (RenderObject* inlineChild = child->firstChild()) {
                child = inlineChild;
                continue;
            }
        }
        child = child->nextInPreOrderAfterChildren(start);
    }
}

void SVGTextMetricsBuilder::measureTextRenderer(RenderSVGInlineText* text)
{
    ASSERT(text);

    RenderSVGText* textRoot = RenderSVGText::locateRenderSVGTextAncestor(text);
    if (!textRoot)
        return;

    MeasureTextData data(0);
    walkTree(textRoot, text, &data);
}

void SVGTextMetricsBuilder::buildMetricsAndLayoutAttributes(RenderSVGText* textRoot, RenderSVGInlineText* stopAtLeaf, SVGCharacterDataMap& allCharactersMap)
{
    ASSERT(textRoot);
    MeasureTextData data(&allCharactersMap);
    walkTree(textRoot, stopAtLeaf, &data);
}

}
