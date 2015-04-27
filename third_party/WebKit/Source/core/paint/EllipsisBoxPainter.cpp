// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/EllipsisBoxPainter.h"

#include "core/layout/TextRunConstructor.h"
#include "core/layout/line/EllipsisBox.h"
#include "core/layout/line/RootInlineBox.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/TextPainter.h"
#include "platform/graphics/GraphicsContextStateSaver.h"

namespace blink {

void EllipsisBoxPainter::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset, LayoutUnit lineTop, LayoutUnit lineBottom)
{
    const ComputedStyle& style = m_ellipsisBox.layoutObject().styleRef(m_ellipsisBox.isFirstLineStyle());
    paintEllipsis(paintInfo, paintOffset, lineTop, lineBottom, style);
}

void EllipsisBoxPainter::paintEllipsis(const PaintInfo& paintInfo, const LayoutPoint& paintOffset, LayoutUnit lineTop, LayoutUnit lineBottom, const ComputedStyle& style)
{
    GraphicsContext* context = paintInfo.context;
    FloatPoint boxOrigin = m_ellipsisBox.locationIncludingFlipping().toFloatPoint();
    boxOrigin.moveBy(FloatPoint(paintOffset));
    FloatRect boxRect(boxOrigin, FloatSize(m_ellipsisBox.logicalWidth(), m_ellipsisBox.virtualLogicalHeight()));

    DrawingRecorder recorder(*context, m_ellipsisBox, DisplayItem::paintPhaseToDrawingType(paintInfo.phase), boxRect);
    if (recorder.canUseCachedDrawing())
        return;

    GraphicsContextStateSaver stateSaver(*context);
    if (!m_ellipsisBox.isHorizontal())
        context->concatCTM(TextPainter::rotation(boxRect, TextPainter::Clockwise));
    const Font& font = style.font();
    FloatPoint textOrigin(boxOrigin.x(), boxOrigin.y() + font.fontMetrics().ascent());

    bool isPrinting = m_ellipsisBox.layoutObject().document().printing();
    bool haveSelection = !isPrinting && paintInfo.phase != PaintPhaseTextClip && m_ellipsisBox.selectionState() != LayoutObject::SelectionNone;

    if (haveSelection)
        paintSelection(context, boxOrigin, style, font);
    else if (paintInfo.phase == PaintPhaseSelection)
        return;

    TextPainter::Style textStyle = TextPainter::textPaintingStyle(m_ellipsisBox.layoutObject(), style, paintInfo.forceBlackText(), isPrinting);
    if (haveSelection)
        textStyle = TextPainter::selectionPaintingStyle(m_ellipsisBox.layoutObject(), true, paintInfo.forceBlackText(), isPrinting, textStyle);

    TextRun textRun = constructTextRun(&m_ellipsisBox.layoutObject(), font, m_ellipsisBox.ellipsisStr(), style, TextRun::AllowTrailingExpansion);
    TextPainter textPainter(context, font, textRun, textOrigin, boxRect, m_ellipsisBox.isHorizontal());
    textPainter.paint(0, m_ellipsisBox.ellipsisStr().length(), m_ellipsisBox.ellipsisStr().length(), textStyle);
}

void EllipsisBoxPainter::paintSelection(GraphicsContext* context, const FloatPoint& boxOrigin, const ComputedStyle& style, const Font& font)
{
    Color textColor = m_ellipsisBox.layoutObject().resolveColor(style, CSSPropertyColor);
    Color c = m_ellipsisBox.layoutObject().selectionBackgroundColor();
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
    const int deltaY = roundToInt(m_ellipsisBox.layoutObject().styleRef().isFlippedLinesWritingMode() ? selectionBottom - m_ellipsisBox.logicalBottom() : m_ellipsisBox.logicalTop() - top);
    const FloatPoint localOrigin(boxOrigin.x(), boxOrigin.y() - deltaY);
    FloatRect clipRect(localOrigin, FloatSize(m_ellipsisBox.logicalWidth(), h.toFloat()));
    context->clip(clipRect);
    context->drawHighlightForText(font, constructTextRun(&m_ellipsisBox.layoutObject(), font, m_ellipsisBox.ellipsisStr(), style, TextRun::AllowTrailingExpansion), localOrigin, h, c);
}

} // namespace blink
