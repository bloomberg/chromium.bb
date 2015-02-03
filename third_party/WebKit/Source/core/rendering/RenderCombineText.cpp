/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 *
 */

#include "config.h"
#include "core/rendering/RenderCombineText.h"

#include "core/rendering/TextRunConstructor.h"
#include "platform/graphics/GraphicsContext.h"

namespace blink {

const float textCombineMargin = 1.1f; // Allow em + 10% margin

RenderCombineText::RenderCombineText(Node* node, PassRefPtr<StringImpl> string)
    : RenderText(node, string)
    , m_combinedTextWidth(0)
    , m_scaleX(1.0f)
    , m_isCombined(false)
    , m_needsFontUpdate(false)
{
}

void RenderCombineText::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    setStyleInternal(RenderStyle::clone(style()));
    RenderText::styleDidChange(diff, oldStyle);

    updateIsCombined();
}

void RenderCombineText::setTextInternal(PassRefPtr<StringImpl> text)
{
    RenderText::setTextInternal(text);

    updateIsCombined();
}

float RenderCombineText::width(unsigned from, unsigned length, const Font& font, float xPosition, TextDirection direction, HashSet<const SimpleFontData*>* fallbackFonts, GlyphOverflow* glyphOverflow) const
{
    if (!length)
        return 0;

    if (hasEmptyText())
        return 0;

    if (m_isCombined)
        return font.fontDescription().computedSize();

    return RenderText::width(from, length, font, xPosition, direction, fallbackFonts, glyphOverflow);
}

void RenderCombineText::adjustTextOrigin(FloatPoint& textOrigin, const FloatRect& boxRect) const
{
    ASSERT(!m_needsFontUpdate);
    if (!m_isCombined)
        return;
    float renderingWidth = m_combinedTextWidth / m_scaleX;
    textOrigin.move(boxRect.height() / 2 - renderingWidth / 2, style()->font().fontDescription().computedPixelSize());
}

void RenderCombineText::transform(GraphicsContext& context, const FloatRect& boxRect) const
{
    ASSERT(!m_needsFontUpdate);
    ASSERT(isTransformNeeded());
    auto centerX = boxRect.x() + boxRect.width() / 2;
    AffineTransform transform(m_scaleX, 0, 0, 1, centerX * (1 - m_scaleX), 0);
    context.concatCTM(transform);
}

void RenderCombineText::updateIsCombined()
{
    // CSS3 spec says text-combine works only in vertical writing mode.
    m_isCombined = !style()->isHorizontalWritingMode()
        // Nothing to combine.
        && !hasEmptyText();

    if (m_isCombined)
        m_needsFontUpdate = true;
}

void RenderCombineText::updateFont()
{
    if (!m_needsFontUpdate)
        return;

    m_needsFontUpdate = false;

    if (!m_isCombined)
        return;

    TextRun run = constructTextRun(this, originalFont(), this, style(), style()->direction());
    FontDescription description = originalFont().fontDescription();
    float emWidth = description.computedSize();
    if (!(style()->textDecorationsInEffect() & (TextDecorationUnderline | TextDecorationOverline)))
        emWidth *= textCombineMargin;

    description.setOrientation(Horizontal); // We are going to draw combined text horizontally.
    m_combinedTextWidth = originalFont().width(run);

    FontSelector* fontSelector = style()->font().fontSelector();

    bool shouldUpdateFont = style()->setFontDescription(description); // Need to change font orientation to horizontal.

    if (m_combinedTextWidth <= emWidth) {
        m_scaleX = 1.0f;
    } else {
        // Need to try compressed glyphs.
        static const FontWidthVariant widthVariants[] = { HalfWidth, ThirdWidth, QuarterWidth };
        for (size_t i = 0 ; i < WTF_ARRAY_LENGTH(widthVariants) ; ++i) {
            description.setWidthVariant(widthVariants[i]);
            Font compressedFont = Font(description);
            compressedFont.update(fontSelector);
            float runWidth = compressedFont.width(run);
            if (runWidth <= emWidth) {
                m_combinedTextWidth = runWidth;

                // Replace my font with the new one.
                shouldUpdateFont = style()->setFontDescription(description);
                break;
            }
        }

        // If width > ~1em, shrink to fit within ~1em, otherwise render without scaling (no expansion)
        // http://dev.w3.org/csswg/css-writing-modes-3/#text-combine-compression
        if (m_combinedTextWidth > emWidth) {
            m_scaleX = emWidth / m_combinedTextWidth;
            m_combinedTextWidth = emWidth;
        } else {
            m_scaleX = 1.0f;
        }
    }

    if (shouldUpdateFont)
        style()->font().update(fontSelector);
}

} // namespace blink
