// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/EllipsisBoxPainter.h"

#include "core/rendering/EllipsisBox.h"
#include "core/rendering/InlineTextBox.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RootInlineBox.h"
#include "core/rendering/TextPainter.h"
#include "core/rendering/TextRunConstructor.h"
#include "platform/graphics/GraphicsContextStateSaver.h"

namespace blink {

void EllipsisBoxPainter::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset, LayoutUnit lineTop, LayoutUnit lineBottom)
{
    GraphicsContext* context = paintInfo.context;
    RenderStyle* style = m_ellipsisBox.renderer().style(m_ellipsisBox.isFirstLineStyle());
    const Font& font = style->font();
    FloatPoint boxOrigin = m_ellipsisBox.locationIncludingFlipping();
    boxOrigin.moveBy(FloatPoint(paintOffset));
    if (!m_ellipsisBox.isHorizontal())
        boxOrigin.move(0, -m_ellipsisBox.virtualLogicalHeight());
    FloatRect boxRect(boxOrigin, LayoutSize(m_ellipsisBox.logicalWidth(), m_ellipsisBox.virtualLogicalHeight()));
    GraphicsContextStateSaver stateSaver(*context);
    if (!m_ellipsisBox.isHorizontal())
        context->concatCTM(InlineTextBox::rotation(boxRect, InlineTextBox::Clockwise));
    FloatPoint textOrigin(boxOrigin.x(), boxOrigin.y() + font.fontMetrics().ascent());

    bool isPrinting = m_ellipsisBox.renderer().document().printing();
    bool haveSelection = !isPrinting && paintInfo.phase != PaintPhaseTextClip && m_ellipsisBox.selectionState() != RenderObject::SelectionNone;

    if (haveSelection)
        paintSelection(context, boxOrigin, style, font);
    else if (paintInfo.phase == PaintPhaseSelection)
        return;

    TextPainter::Style textStyle = TextPainter::textPaintingStyle(m_ellipsisBox.renderer(), style, paintInfo.forceBlackText(), isPrinting);
    if (haveSelection)
        textStyle = TextPainter::selectionPaintingStyle(m_ellipsisBox.renderer(), true, paintInfo.forceBlackText(), isPrinting, textStyle);

    TextRun textRun = constructTextRun(&m_ellipsisBox.renderer(), font, m_ellipsisBox.ellipsisStr(), style, TextRun::AllowTrailingExpansion);
    TextPainter textPainter(context, font, textRun, textOrigin, boxRect, m_ellipsisBox.isHorizontal());
    textPainter.paint(0, m_ellipsisBox.ellipsisStr().length(), m_ellipsisBox.ellipsisStr().length(), textStyle);

    paintMarkupBox(paintInfo, paintOffset, lineTop, lineBottom, style);
}

void EllipsisBoxPainter::paintMarkupBox(PaintInfo& paintInfo, const LayoutPoint& paintOffset, LayoutUnit lineTop, LayoutUnit lineBottom, RenderStyle* style)
{
    InlineBox* markupBox = m_ellipsisBox.markupBox();
    if (!markupBox)
        return;

    LayoutPoint adjustedPaintOffset = paintOffset;
    adjustedPaintOffset.move(m_ellipsisBox.x() + m_ellipsisBox.logicalWidth() - markupBox->x(),
        m_ellipsisBox.y() + style->fontMetrics().ascent() - (markupBox->y() + markupBox->renderer().style(m_ellipsisBox.isFirstLineStyle())->fontMetrics().ascent()));
    markupBox->paint(paintInfo, adjustedPaintOffset, lineTop, lineBottom);
}

void EllipsisBoxPainter::paintSelection(GraphicsContext* context, const FloatPoint& boxOrigin, RenderStyle* style, const Font& font)
{
    Color textColor = m_ellipsisBox.renderer().resolveColor(style, CSSPropertyColor);
    Color c = m_ellipsisBox.renderer().selectionBackgroundColor();
    if (!c.alpha())
        return;

    // If the text color ends up being the same as the selection background, invert the selection
    // background.
    if (textColor == c)
        c = Color(0xff - c.red(), 0xff - c.green(), 0xff - c.blue());

    GraphicsContextStateSaver stateSaver(*context);
    LayoutUnit selectionBottom = m_ellipsisBox.root().selectionBottom();
    LayoutUnit top = m_ellipsisBox.root().selectionTop();
    LayoutUnit h = m_ellipsisBox.root().selectionHeight();
    const int deltaY = roundToInt(m_ellipsisBox.renderer().style()->isFlippedLinesWritingMode() ? selectionBottom - m_ellipsisBox.logicalBottom() : m_ellipsisBox.logicalTop() - top);
    const FloatPoint localOrigin(boxOrigin.x(), boxOrigin.y() - deltaY);
    FloatRect clipRect(localOrigin, FloatSize(m_ellipsisBox.logicalWidth(), h.toFloat()));
    context->clip(clipRect);
    context->drawHighlightForText(font, constructTextRun(&m_ellipsisBox.renderer(), font, m_ellipsisBox.ellipsisStr(), style, TextRun::AllowTrailingExpansion), localOrigin, h, c);
}

} // namespace blink
