/*
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Rob Buis <buis@kde.org>
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

#ifndef LayoutSVGInlineText_h
#define LayoutSVGInlineText_h

#include "core/layout/LayoutText.h"
#include "core/layout/svg/SVGTextLayoutAttributes.h"

namespace blink {

class LayoutSVGInlineText final : public LayoutText {
public:
    LayoutSVGInlineText(Node*, PassRefPtr<StringImpl>);

    bool characterStartsNewTextChunk(int position) const;
    SVGTextLayoutAttributes* layoutAttributes() { return &m_layoutAttributes; }
    const SVGTextLayoutAttributes* layoutAttributes() const { return &m_layoutAttributes; }

    float scalingFactor() const { return m_scalingFactor; }
    const Font& scaledFont() const { return m_scaledFont; }
    void updateScaledFont();
    static void computeNewScaledFontForStyle(LayoutObject*, float& scalingFactor, Font& scaledFont);

    // Preserves floating point precision for the use in DRT. It knows how to round and does a better job than enclosingIntRect.
    FloatRect floatLinesBoundingBox() const;

    PassRefPtr<StringImpl> originalText() const override;

    const char* name() const override { return "LayoutSVGInlineText"; }

private:
    void setTextInternal(PassRefPtr<StringImpl>) override;
    void styleDidChange(StyleDifference, const ComputedStyle*) override;

    FloatRect objectBoundingBox() const override { return floatLinesBoundingBox(); }

    bool isOfType(LayoutObjectType type) const override { return type == LayoutObjectSVG || type == LayoutObjectSVGInlineText || LayoutText::isOfType(type); }

    PositionWithAffinity positionForPoint(const LayoutPoint&) override;
    LayoutRect localCaretRect(InlineBox*, int caretOffset, LayoutUnit* extraWidthToEndOfLine = nullptr) override;
    IntRect linesBoundingBox() const override;
    InlineTextBox* createTextBox(int start, unsigned short length) override;

    LayoutRect clippedOverflowRectForPaintInvalidation(const LayoutBoxModelObject* paintInvalidationContainer, const PaintInvalidationState*) const final;

    float m_scalingFactor;
    Font m_scaledFont;
    SVGTextLayoutAttributes m_layoutAttributes;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutSVGInlineText, isSVGInlineText());

}

#endif // LayoutSVGInlineText_h
