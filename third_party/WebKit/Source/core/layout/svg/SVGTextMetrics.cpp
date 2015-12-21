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

#include "core/layout/svg/SVGTextMetrics.h"

#include "core/layout/api/LineLayoutSVGInlineText.h"
#include "core/layout/svg/LayoutSVGInlineText.h"
#include "platform/fonts/FontOrientation.h"

namespace blink {

SVGTextMetrics::SVGTextMetrics()
    : m_width(0)
    , m_height(0)
    , m_length(0)
{
}

SVGTextMetrics::SVGTextMetrics(SVGTextMetrics::MetricsType)
    : m_width(0)
    , m_height(0)
    , m_length(1)
{
}

SVGTextMetrics::SVGTextMetrics(LineLayoutSVGInlineText textLayoutItem, const TextRun& run)
{
    ASSERT(textLayoutItem);

    float scalingFactor = textLayoutItem.scalingFactor();
    ASSERT(scalingFactor);

    const Font& scaledFont = textLayoutItem.scaledFont();

    // Calculate width/height using the scaled font, divide this result by the scalingFactor afterwards.
    m_width = scaledFont.width(run) / scalingFactor;
    m_height = scaledFont.fontMetrics().floatHeight() / scalingFactor;

    ASSERT(run.length() >= 0);
    m_length = static_cast<unsigned>(run.length());
}

TextRun SVGTextMetrics::constructTextRun(LineLayoutSVGInlineText textLayoutItem, unsigned position, unsigned length, TextDirection textDirection)
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

SVGTextMetrics SVGTextMetrics::measureCharacterRange(LineLayoutSVGInlineText textLayoutItem, unsigned position, unsigned length, TextDirection textDirection)
{
    ASSERT(textLayoutItem);
    return SVGTextMetrics(textLayoutItem, constructTextRun(textLayoutItem, position, length, textDirection));
}

SVGTextMetrics::SVGTextMetrics(LineLayoutSVGInlineText textLayoutItem, unsigned length, float width)
{
    ASSERT(textLayoutItem);

    float scalingFactor = textLayoutItem.scalingFactor();
    ASSERT(scalingFactor);

    m_width = width / scalingFactor;
    m_height = textLayoutItem.scaledFont().fontMetrics().floatHeight() / scalingFactor;

    m_length = length;
}

float SVGTextMetrics::advance(FontOrientation orientation) const
{
    switch (orientation) {
    case FontOrientation::Horizontal:
    case FontOrientation::VerticalRotated:
        return width();
    case FontOrientation::VerticalUpright:
        return height();
    default:
        ASSERT_NOT_REACHED();
        return width();
    }
}

}
