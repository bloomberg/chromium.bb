// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/FileUploadControlPainter.h"

#include "core/layout/LayoutButton.h"
#include "core/layout/LayoutFileUploadControl.h"
#include "core/layout/PaintInfo.h"
#include "core/layout/TextRunConstructor.h"
#include "core/paint/RenderDrawingRecorder.h"
#include "platform/graphics/paint/ClipRecorder.h"

namespace blink {

const int buttonShadowHeight = 2;

void FileUploadControlPainter::paintObject(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (m_renderFileUploadControl.style()->visibility() != VISIBLE)
        return;

    // Push a clip.
    OwnPtr<ClipRecorder> clipRecorder;
    if (paintInfo.phase == PaintPhaseForeground || paintInfo.phase == PaintPhaseChildBlockBackgrounds) {
        IntRect clipRect = enclosingIntRect(LayoutRect(
            LayoutPoint(paintOffset.x() + m_renderFileUploadControl.borderLeft(), paintOffset.y() + m_renderFileUploadControl.borderTop()),
            m_renderFileUploadControl.size() + LayoutSize(0, -m_renderFileUploadControl.borderWidth() + buttonShadowHeight)));
        if (clipRect.isEmpty())
            return;
        clipRecorder = adoptPtr(new ClipRecorder(m_renderFileUploadControl.displayItemClient(), paintInfo.context, DisplayItem::ClipFileUploadControlRect, LayoutRect(clipRect)));
    }

    if (paintInfo.phase == PaintPhaseForeground) {
        const String& displayedFilename = m_renderFileUploadControl.fileTextValue();
        const Font& font = m_renderFileUploadControl.style()->font();
        TextRun textRun = constructTextRun(&m_renderFileUploadControl, font, displayedFilename, m_renderFileUploadControl.styleRef(), RespectDirection | RespectDirectionOverride);
        textRun.setExpansionBehavior(TextRun::AllowTrailingExpansion);

        // Determine where the filename should be placed
        LayoutUnit contentLeft = paintOffset.x() + m_renderFileUploadControl.borderLeft() + m_renderFileUploadControl.paddingLeft();
        Node* button = m_renderFileUploadControl.uploadButton();
        if (!button)
            return;

        int buttonWidth = (button && button->layoutBox()) ? button->layoutBox()->pixelSnappedWidth() : 0;
        LayoutUnit buttonAndSpacingWidth = buttonWidth + LayoutFileUploadControl::afterButtonSpacing;
        float textWidth = font.width(textRun);
        LayoutUnit textX;
        if (m_renderFileUploadControl.style()->isLeftToRightDirection())
            textX = contentLeft + buttonAndSpacingWidth;
        else
            textX = contentLeft + m_renderFileUploadControl.contentWidth() - buttonAndSpacingWidth - textWidth;

        LayoutUnit textY = 0;
        // We want to match the button's baseline
        // FIXME: Make this work with transforms.
        if (LayoutButton* buttonRenderer = toLayoutButton(button->layoutObject()))
            textY = paintOffset.y() + m_renderFileUploadControl.borderTop() + m_renderFileUploadControl.paddingTop() + buttonRenderer->baselinePosition(AlphabeticBaseline, true, HorizontalLine, PositionOnContainingLine);
        else
            textY = m_renderFileUploadControl.baselinePosition(AlphabeticBaseline, true, HorizontalLine, PositionOnContainingLine);
        TextRunPaintInfo textRunPaintInfo(textRun);
        // FIXME: Shouldn't these offsets be rounded? crbug.com/350474
        textRunPaintInfo.bounds = FloatRect(textX.toFloat(), textY.toFloat() - m_renderFileUploadControl.style()->fontMetrics().ascent(),
            textWidth, m_renderFileUploadControl.style()->fontMetrics().height());

        // Draw the filename.
        RenderDrawingRecorder recorder(paintInfo.context, m_renderFileUploadControl, paintInfo.phase, textRunPaintInfo.bounds);
        if (!recorder.canUseCachedDrawing()) {
            paintInfo.context->setFillColor(m_renderFileUploadControl.resolveColor(CSSPropertyColor));
            paintInfo.context->drawBidiText(font, textRunPaintInfo, FloatPoint(roundToInt(textX), roundToInt(textY)));
        }
    }

    // Paint the children.
    m_renderFileUploadControl.LayoutBlockFlow::paintObject(paintInfo, paintOffset);
}

} // namespace blink
