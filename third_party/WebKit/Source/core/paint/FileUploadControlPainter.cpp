// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/FileUploadControlPainter.h"

#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderButton.h"
#include "core/rendering/RenderFileUploadControl.h"
#include "core/rendering/TextRunConstructor.h"

namespace blink {

const int buttonShadowHeight = 2;

void FileUploadControlPainter::paintObject(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (m_renderFileUploadControl.style()->visibility() != VISIBLE)
        return;

    // Push a clip.
    GraphicsContextStateSaver stateSaver(*paintInfo.context, false);
    if (paintInfo.phase == PaintPhaseForeground || paintInfo.phase == PaintPhaseChildBlockBackgrounds) {
        IntRect clipRect = enclosingIntRect(LayoutRect(paintOffset.x() + m_renderFileUploadControl.borderLeft(), paintOffset.y() + m_renderFileUploadControl.borderTop(),
            m_renderFileUploadControl.width() - m_renderFileUploadControl.borderLeft() - m_renderFileUploadControl.borderRight(),
            m_renderFileUploadControl.height() - m_renderFileUploadControl.borderBottom() - m_renderFileUploadControl.borderTop() + buttonShadowHeight));
        if (clipRect.isEmpty())
            return;
        stateSaver.save();
        paintInfo.context->clip(clipRect);
    }

    if (paintInfo.phase == PaintPhaseForeground) {
        const String& displayedFilename = m_renderFileUploadControl.fileTextValue();
        const Font& font = m_renderFileUploadControl.style()->font();
        TextRun textRun = constructTextRun(&m_renderFileUploadControl, font, displayedFilename, m_renderFileUploadControl.style(), TextRun::AllowTrailingExpansion, RespectDirection | RespectDirectionOverride);

        // Determine where the filename should be placed
        LayoutUnit contentLeft = paintOffset.x() + m_renderFileUploadControl.borderLeft() + m_renderFileUploadControl.paddingLeft();
        Node* button = m_renderFileUploadControl.uploadButton();
        if (!button)
            return;

        int buttonWidth = (button && button->renderBox()) ? button->renderBox()->pixelSnappedWidth() : 0;
        LayoutUnit buttonAndSpacingWidth = buttonWidth + RenderFileUploadControl::afterButtonSpacing;
        float textWidth = font.width(textRun);
        LayoutUnit textX;
        if (m_renderFileUploadControl.style()->isLeftToRightDirection())
            textX = contentLeft + buttonAndSpacingWidth;
        else
            textX = contentLeft + m_renderFileUploadControl.contentWidth() - buttonAndSpacingWidth - textWidth;

        LayoutUnit textY = 0;
        // We want to match the button's baseline
        // FIXME: Make this work with transforms.
        if (RenderButton* buttonRenderer = toRenderButton(button->renderer()))
            textY = paintOffset.y() + m_renderFileUploadControl.borderTop() + m_renderFileUploadControl.paddingTop() + buttonRenderer->baselinePosition(AlphabeticBaseline, true, HorizontalLine, PositionOnContainingLine);
        else
            textY = m_renderFileUploadControl.baselinePosition(AlphabeticBaseline, true, HorizontalLine, PositionOnContainingLine);
        TextRunPaintInfo textRunPaintInfo(textRun);
        // FIXME: Shouldn't these offsets be rounded? crbug.com/350474
        textRunPaintInfo.bounds = FloatRect(textX.toFloat(), textY.toFloat() - m_renderFileUploadControl.style()->fontMetrics().ascent(),
            textWidth, m_renderFileUploadControl.style()->fontMetrics().height());

        paintInfo.context->setFillColor(m_renderFileUploadControl.resolveColor(CSSPropertyColor));

        // Draw the filename
        paintInfo.context->drawBidiText(font, textRunPaintInfo, IntPoint(roundToInt(textX), roundToInt(textY)));
    }

    // Paint the children.
    m_renderFileUploadControl.RenderBlockFlow::paintObject(paintInfo, paintOffset);
}

} // namespace blink
