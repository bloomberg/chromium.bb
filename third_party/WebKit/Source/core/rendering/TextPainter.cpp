// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/rendering/TextPainter.h"

#include "core/rendering/InlineTextBox.h"
#include "core/rendering/RenderCombineText.h"
#include "core/rendering/style/ShadowList.h"
#include "platform/fonts/Font.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/text/TextRun.h"
#include "wtf/Assertions.h"
#include "wtf/unicode/CharacterNames.h"

namespace blink {

TextPainter::TextPainter(GraphicsContext* context, const Font& font, const TextRun& run, const FloatPoint& textOrigin, const FloatRect& textBounds, bool horizontal)
    : m_graphicsContext(context)
    , m_font(font)
    , m_run(run)
    , m_textOrigin(textOrigin)
    , m_textBounds(textBounds)
    , m_horizontal(horizontal)
    , m_emphasisMarkOffset(0)
    , m_combinedText(0)
{
}

TextPainter::~TextPainter()
{
}

void TextPainter::setEmphasisMark(const AtomicString& emphasisMark, TextEmphasisPosition position)
{
    m_emphasisMark = emphasisMark;

    if (emphasisMark.isNull()) {
        m_emphasisMarkOffset = 0;
    } else if (position == TextEmphasisPositionOver) {
        m_emphasisMarkOffset = -m_font.fontMetrics().ascent() - m_font.emphasisMarkDescent(emphasisMark);
    } else {
        ASSERT(position == TextEmphasisPositionUnder);
        m_emphasisMarkOffset = m_font.fontMetrics().descent() + m_font.emphasisMarkAscent(emphasisMark);
    }
}

void TextPainter::paint(int startOffset, int endOffset, int length, const Style& textStyle, TextBlobPtr* cachedTextBlob)
{
    GraphicsContextStateSaver stateSaver(*m_graphicsContext, false);
    updateGraphicsContext(textStyle, stateSaver);
    paintInternal<PaintText>(startOffset, endOffset, length, cachedTextBlob);

    if (!m_emphasisMark.isEmpty()) {
        if (textStyle.emphasisMarkColor != textStyle.fillColor)
            m_graphicsContext->setFillColor(textStyle.emphasisMarkColor);

        if (m_combinedText)
            paintEmphasisMarkForCombinedText();
        else
            paintInternal<PaintEmphasisMark>(startOffset, endOffset, length);
    }
}

// static
void TextPainter::updateGraphicsContext(GraphicsContext* context, const Style& textStyle, bool horizontal, GraphicsContextStateSaver& stateSaver)
{
    TextDrawingModeFlags mode = context->textDrawingMode();
    if (textStyle.strokeWidth > 0) {
        TextDrawingModeFlags newMode = mode | TextModeStroke;
        if (mode != newMode) {
            if (!stateSaver.saved())
                stateSaver.save();
            context->setTextDrawingMode(newMode);
            mode = newMode;
        }
    }

    if (mode & TextModeFill && textStyle.fillColor != context->fillColor())
        context->setFillColor(textStyle.fillColor);

    if (mode & TextModeStroke) {
        if (textStyle.strokeColor != context->strokeColor())
            context->setStrokeColor(textStyle.strokeColor);
        if (textStyle.strokeWidth != context->strokeThickness())
            context->setStrokeThickness(textStyle.strokeWidth);
    }

    // Text shadows are disabled when printing. http://crbug.com/258321
    if (textStyle.shadow && !context->printing()) {
        if (!stateSaver.saved())
            stateSaver.save();
        context->setDrawLooper(textStyle.shadow->createDrawLooper(DrawLooperBuilder::ShadowIgnoresAlpha, horizontal));
    }
}

static bool graphicsContextAllowsTextBlobs(GraphicsContext* context)
{
    // Text blobs affect the shader coordinate space.
    // FIXME: Fix this, most likely in Skia.
    return !context->strokeGradient() && !context->strokePattern() && !context->fillGradient() && !context->fillPattern();
}

template <TextPainter::PaintInternalStep step>
void TextPainter::paintInternalRun(TextRunPaintInfo& textRunPaintInfo, int from, int to, TextBlobPtr* cachedTextBlob)
{
    textRunPaintInfo.from = from;
    textRunPaintInfo.to = to;

    if (step == PaintEmphasisMark) {
        m_graphicsContext->drawEmphasisMarks(m_font, textRunPaintInfo, m_emphasisMark, m_textOrigin + IntSize(0, m_emphasisMarkOffset));
        return;
    }

    ASSERT(step == PaintText);

    TextBlobPtr localTextBlob;
    TextBlobPtr& textBlob = cachedTextBlob ? *cachedTextBlob : localTextBlob;
    bool canUseTextBlobs = RuntimeEnabledFeatures::textBlobEnabled() && graphicsContextAllowsTextBlobs(m_graphicsContext);

    if (canUseTextBlobs && !textBlob)
        textBlob = m_font.buildTextBlob(textRunPaintInfo, m_textOrigin, m_graphicsContext->couldUseLCDRenderedText());

    if (canUseTextBlobs && textBlob)
        m_font.drawTextBlob(m_graphicsContext, textBlob.get(), m_textOrigin.data());
    else
        m_graphicsContext->drawText(m_font, textRunPaintInfo, m_textOrigin);
}

template <TextPainter::PaintInternalStep Step>
void TextPainter::paintInternal(int startOffset, int endOffset, int truncationPoint, TextBlobPtr* cachedTextBlob)
{
    // FIXME: We should be able to use cachedTextBlob in more cases.
    TextRunPaintInfo textRunPaintInfo(m_run);
    textRunPaintInfo.bounds = m_textBounds;
    if (startOffset <= endOffset) {
        paintInternalRun<Step>(textRunPaintInfo, startOffset, endOffset, cachedTextBlob);
    } else {
        if (endOffset > 0)
            paintInternalRun<Step>(textRunPaintInfo, 0, endOffset);
        if (startOffset < truncationPoint)
            paintInternalRun<Step>(textRunPaintInfo, startOffset, truncationPoint);
    }
}

void TextPainter::paintEmphasisMarkForCombinedText()
{
    ASSERT(m_combinedText);
    DEFINE_STATIC_LOCAL(TextRun, objectReplacementCharacterTextRun, (&objectReplacementCharacter, 1));
    FloatPoint emphasisMarkTextOrigin(m_textBounds.x() + m_textBounds.width() / 2, m_textBounds.y() + m_font.fontMetrics().ascent() + m_emphasisMarkOffset);
    TextRunPaintInfo textRunPaintInfo(objectReplacementCharacterTextRun);
    textRunPaintInfo.bounds = m_textBounds;
    m_graphicsContext->concatCTM(InlineTextBox::rotation(m_textBounds, InlineTextBox::Clockwise));
    m_graphicsContext->drawEmphasisMarks(m_combinedText->originalFont(), textRunPaintInfo, m_emphasisMark, emphasisMarkTextOrigin);
    m_graphicsContext->concatCTM(InlineTextBox::rotation(m_textBounds, InlineTextBox::Counterclockwise));
}

} // namespace blink
