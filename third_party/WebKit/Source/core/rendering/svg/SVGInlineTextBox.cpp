/**
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
#include "core/rendering/svg/SVGInlineTextBox.h"

#include "core/dom/DocumentMarkerController.h"
#include "core/dom/RenderedDocumentMarker.h"
#include "core/editing/Editor.h"
#include "core/frame/LocalFrame.h"
#include "core/paint/SVGInlineTextBoxPainter.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/InlineFlowBox.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/PointerEventsHitRules.h"
#include "core/rendering/RenderInline.h"
#include "core/rendering/RenderTheme.h"
#include "core/rendering/svg/RenderSVGInlineText.h"
#include "core/rendering/svg/RenderSVGResource.h"
#include "core/rendering/svg/RenderSVGResourceSolidColor.h"
#include "core/rendering/svg/SVGTextRunRenderingContext.h"
#include "platform/FloatConversion.h"
#include "platform/fonts/FontCache.h"
#include "platform/graphics/GraphicsContextStateSaver.h"

namespace blink {

struct ExpectedSVGInlineTextBoxSize : public InlineTextBox {
    float float1;
    uint32_t bitfields : 1;
    Vector<SVGTextFragment> vector;
};

COMPILE_ASSERT(sizeof(SVGInlineTextBox) == sizeof(ExpectedSVGInlineTextBoxSize), SVGInlineTextBox_is_not_of_expected_size);

SVGInlineTextBox::SVGInlineTextBox(RenderObject& object)
    : InlineTextBox(object)
    , m_logicalHeight(0)
    , m_startsNewTextChunk(false)
{
}

void SVGInlineTextBox::dirtyLineBoxes()
{
    InlineTextBox::dirtyLineBoxes();

    // Clear the now stale text fragments
    clearTextFragments();

    // And clear any following text fragments as the text on which they
    // depend may now no longer exist, or glyph positions may be wrong
    InlineTextBox* nextBox = nextTextBox();
    if (nextBox)
        nextBox->dirtyLineBoxes();
}

int SVGInlineTextBox::offsetForPosition(float, bool) const
{
    // SVG doesn't use the standard offset <-> position selection system, as it's not suitable for SVGs complex needs.
    // vertical text selection, inline boxes spanning multiple lines (contrary to HTML, etc.)
    ASSERT_NOT_REACHED();
    return 0;
}

int SVGInlineTextBox::offsetForPositionInFragment(const SVGTextFragment& fragment, float position, bool includePartialGlyphs) const
{
    RenderSVGInlineText& textRenderer = toRenderSVGInlineText(this->renderer());

    float scalingFactor = textRenderer.scalingFactor();
    ASSERT(scalingFactor);

    RenderStyle* style = textRenderer.style();
    ASSERT(style);

    TextRun textRun = constructTextRun(style, fragment);

    // Eventually handle lengthAdjust="spacingAndGlyphs".
    // FIXME: Handle vertical text.
    AffineTransform fragmentTransform;
    fragment.buildFragmentTransform(fragmentTransform);
    if (!fragmentTransform.isIdentity())
        textRun.setHorizontalGlyphStretch(narrowPrecisionToFloat(fragmentTransform.xScale()));

    return fragment.characterOffset - start() + textRenderer.scaledFont().offsetForPosition(textRun, position * scalingFactor, includePartialGlyphs);
}

float SVGInlineTextBox::positionForOffset(int) const
{
    // SVG doesn't use the offset <-> position selection system.
    ASSERT_NOT_REACHED();
    return 0;
}

FloatRect SVGInlineTextBox::selectionRectForTextFragment(const SVGTextFragment& fragment, int startPosition, int endPosition, RenderStyle* style)
{
    ASSERT(startPosition < endPosition);
    ASSERT(style);

    FontCachePurgePreventer fontCachePurgePreventer;

    RenderSVGInlineText& textRenderer = toRenderSVGInlineText(this->renderer());

    float scalingFactor = textRenderer.scalingFactor();
    ASSERT(scalingFactor);

    const Font& scaledFont = textRenderer.scaledFont();
    const FontMetrics& scaledFontMetrics = scaledFont.fontMetrics();
    FloatPoint textOrigin(fragment.x, fragment.y);
    if (scalingFactor != 1)
        textOrigin.scale(scalingFactor, scalingFactor);

    textOrigin.move(0, -scaledFontMetrics.floatAscent());

    FloatRect selectionRect = scaledFont.selectionRectForText(constructTextRun(style, fragment), textOrigin, fragment.height * scalingFactor, startPosition, endPosition);
    if (scalingFactor == 1)
        return selectionRect;

    selectionRect.scale(1 / scalingFactor);
    return selectionRect;
}

LayoutRect SVGInlineTextBox::localSelectionRect(int startPosition, int endPosition)
{
    int boxStart = start();
    startPosition = std::max(startPosition - boxStart, 0);
    endPosition = std::min(endPosition - boxStart, static_cast<int>(len()));
    if (startPosition >= endPosition)
        return LayoutRect();

    RenderStyle* style = renderer().style();
    ASSERT(style);

    AffineTransform fragmentTransform;
    FloatRect selectionRect;
    int fragmentStartPosition = 0;
    int fragmentEndPosition = 0;

    unsigned textFragmentsSize = m_textFragments.size();
    for (unsigned i = 0; i < textFragmentsSize; ++i) {
        const SVGTextFragment& fragment = m_textFragments.at(i);

        fragmentStartPosition = startPosition;
        fragmentEndPosition = endPosition;
        if (!mapStartEndPositionsIntoFragmentCoordinates(fragment, fragmentStartPosition, fragmentEndPosition))
            continue;

        FloatRect fragmentRect = selectionRectForTextFragment(fragment, fragmentStartPosition, fragmentEndPosition, style);
        fragment.buildFragmentTransform(fragmentTransform);
        fragmentRect = fragmentTransform.mapRect(fragmentRect);

        selectionRect.unite(fragmentRect);
    }

    return enclosingIntRect(selectionRect);
}

void SVGInlineTextBox::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset, LayoutUnit, LayoutUnit)
{

    SVGInlineTextBoxPainter(*this).paint(paintInfo, paintOffset);
}

TextRun SVGInlineTextBox::constructTextRun(RenderStyle* style, const SVGTextFragment& fragment) const
{
    ASSERT(style);

    RenderText* text = &renderer();

    // FIXME(crbug.com/264211): This should not be necessary but can occur if we
    //                          layout during layout. Remove this when 264211 is fixed.
    RELEASE_ASSERT(!text->needsLayout());

    TextRun run(static_cast<const LChar*>(0) // characters, will be set below if non-zero.
                , 0 // length, will be set below if non-zero.
                , 0 // xPos, only relevant with allowTabs=true
                , 0 // padding, only relevant for justified text, not relevant for SVG
                , TextRun::AllowTrailingExpansion
                , direction()
                , dirOverride() || style->rtlOrdering() == VisualOrder /* directionalOverride */);

    if (fragment.length) {
        if (text->is8Bit())
            run.setText(text->characters8() + fragment.characterOffset, fragment.length);
        else
            run.setText(text->characters16() + fragment.characterOffset, fragment.length);
    }

    if (textRunNeedsRenderingContext(style->font()))
        run.setRenderingContext(SVGTextRunRenderingContext::create(text));

    // We handle letter & word spacing ourselves.
    run.disableSpacing();

    // Propagate the maximum length of the characters buffer to the TextRun, even when we're only processing a substring.
    run.setCharactersLength(text->textLength() - fragment.characterOffset);
    ASSERT(run.charactersLength() >= run.length());
    return run;
}

bool SVGInlineTextBox::mapStartEndPositionsIntoFragmentCoordinates(const SVGTextFragment& fragment, int& startPosition, int& endPosition) const
{
    if (startPosition >= endPosition)
        return false;

    int offset = static_cast<int>(fragment.characterOffset) - start();
    int length = static_cast<int>(fragment.length);

    if (startPosition >= offset + length || endPosition <= offset)
        return false;

    if (startPosition < offset)
        startPosition = 0;
    else
        startPosition -= offset;

    if (endPosition > offset + length)
        endPosition = length;
    else {
        ASSERT(endPosition >= offset);
        endPosition -= offset;
    }

    ASSERT(startPosition < endPosition);
    return true;
}

void SVGInlineTextBox::paintDocumentMarker(GraphicsContext*, const FloatPoint&, DocumentMarker*, RenderStyle*, const Font&, bool)
{
    // SVG does not have support for generic document markers (e.g., spellchecking, etc).
}

void SVGInlineTextBox::paintTextMatchMarker(GraphicsContext* context, const FloatPoint& point, DocumentMarker* marker, RenderStyle* style, const Font& font)
{
    SVGInlineTextBoxPainter(*this).paintTextMatchMarker(context, point, marker, style, font);
}

FloatRect SVGInlineTextBox::calculateBoundaries() const
{
    FloatRect textRect;

    RenderSVGInlineText& textRenderer = toRenderSVGInlineText(this->renderer());

    float scalingFactor = textRenderer.scalingFactor();
    ASSERT(scalingFactor);

    float baseline = textRenderer.scaledFont().fontMetrics().floatAscent() / scalingFactor;

    AffineTransform fragmentTransform;
    unsigned textFragmentsSize = m_textFragments.size();
    for (unsigned i = 0; i < textFragmentsSize; ++i) {
        const SVGTextFragment& fragment = m_textFragments.at(i);
        FloatRect fragmentRect(fragment.x, fragment.y - baseline, fragment.width, fragment.height);
        fragment.buildFragmentTransform(fragmentTransform);
        fragmentRect = fragmentTransform.mapRect(fragmentRect);

        textRect.unite(fragmentRect);
    }

    return textRect;
}

bool SVGInlineTextBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit, LayoutUnit)
{
    // FIXME: integrate with InlineTextBox::nodeAtPoint better.
    ASSERT(!isLineBreak());

    PointerEventsHitRules hitRules(PointerEventsHitRules::SVG_TEXT_HITTESTING, request, renderer().style()->pointerEvents());
    bool isVisible = renderer().style()->visibility() == VISIBLE;
    if (isVisible || !hitRules.requireVisible) {
        if (hitRules.canHitBoundingBox
            || (hitRules.canHitStroke && (renderer().style()->svgStyle().hasStroke() || !hitRules.requireStroke))
            || (hitRules.canHitFill && (renderer().style()->svgStyle().hasFill() || !hitRules.requireFill))) {
            FloatPoint boxOrigin(x(), y());
            boxOrigin.moveBy(accumulatedOffset);
            FloatRect rect(boxOrigin, size());
            if (locationInContainer.intersects(rect)) {
                renderer().updateHitTestResult(result, locationInContainer.point() - toLayoutSize(accumulatedOffset));
                if (!result.addNodeToRectBasedTestResult(renderer().node(), request, locationInContainer, rect))
                    return true;
             }
        }
    }
    return false;
}

} // namespace blink
