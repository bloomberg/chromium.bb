/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "core/platform/DragImage.h"

#include "core/platform/graphics/Font.h"
#include "core/platform/graphics/FontCache.h"
#include "core/platform/graphics/FontDescription.h"
#include "core/platform/graphics/FontSelector.h"
#include "core/platform/graphics/GraphicsContext.h"
#include "core/platform/graphics/ImageBuffer.h"
#include "core/platform/graphics/StringTruncator.h"
#include "core/platform/graphics/TextRun.h"
#include "weborigin/KURL.h"

namespace WebCore {
    
const float kDragLabelBorderX = 4;
// Keep border_y in synch with DragController::LinkDragBorderInset.
const float kDragLabelBorderY = 2;
const float kDragLabelRadius = 5;
const float kLabelBorderYOffset = 2;

const float kMinDragLabelWidthBeforeClip = 120;
const float kMaxDragLabelWidth = 300;
const float kMaxDragLabelStringWidth = (kMaxDragLabelWidth - 2 * kDragLabelBorderX);

const float kDragLinkLabelFontSize = 11;
const float kDragLinkUrlFontSize = 10;

static Font deriveDragLabelFont(int size, FontWeight fontWeight, const FontDescription& systemFont)
{
    FontDescription description = systemFont;
    description.setWeight(fontWeight);

    description.setSpecifiedSize(size);
    description.setComputedSize(size);
    Font result(description, 0, 0);
    result.update(0);
    return result;
}

DragImageRef fitDragImageToMaxSize(DragImageRef image, const IntSize& srcSize, const IntSize& size)
{
    float heightResizeRatio = 0.0f;
    float widthResizeRatio = 0.0f;
    float resizeRatio = -1.0f;
    IntSize originalSize = dragImageSize(image);
    
    if (srcSize.width() > size.width()) {
        widthResizeRatio = size.width() / (float)srcSize.width();
        resizeRatio = widthResizeRatio;
    }
    
    if (srcSize.height() > size.height()) {
        heightResizeRatio = size.height() / (float)srcSize.height();
        if ((resizeRatio < 0.0f) || (resizeRatio > heightResizeRatio))
            resizeRatio = heightResizeRatio;
    }
    
    if (srcSize == originalSize)
        return resizeRatio > 0.0f ? scaleDragImage(image, FloatSize(resizeRatio, resizeRatio)) : image;
    
    // The image was scaled in the webpage so at minimum we must account for that scaling
    float scalex = srcSize.width() / (float)originalSize.width();
    float scaley = srcSize.height() / (float)originalSize.height();
    if (resizeRatio > 0.0f) {
        scalex *= resizeRatio;
        scaley *= resizeRatio;
    }
    
    return scaleDragImage(image, FloatSize(scalex, scaley));
}
    
DragImageRef createDragImageForSelection(DragImageRef image, float dragImageAlpha)
{
    if (image)
        image = dissolveDragImageToFraction(image, dragImageAlpha);
    return image;
}

DragImageRef createDragImageForLink(const KURL& url, const String& inLabel, const FontDescription& systemFont, float deviceScaleFactor)
{
    const Font labelFont = deriveDragLabelFont(kDragLinkLabelFontSize, FontWeightBold, systemFont);
    const Font urlFont = deriveDragLabelFont(kDragLinkUrlFontSize, FontWeightNormal, systemFont);
    FontCachePurgePreventer fontCachePurgePreventer;

    bool drawURLString = true;
    bool clipURLString = false;
    bool clipLabelString = false;

    String urlString = url.string();
    String label = inLabel;
    if (label.isEmpty()) {
        drawURLString = false;
        label = urlString;
    }

    // First step is drawing the link drag image width.
    TextRun labelRun(label.impl());
    TextRun urlRun(urlString.impl());
    IntSize labelSize(labelFont.width(labelRun), labelFont.fontMetrics().ascent() + labelFont.fontMetrics().descent());

    if (labelSize.width() > kMaxDragLabelStringWidth) {
        labelSize.setWidth(kMaxDragLabelStringWidth);
        clipLabelString = true;
    }

    IntSize urlStringSize;
    IntSize imageSize(labelSize.width() + kDragLabelBorderX * 2, labelSize.height() + kDragLabelBorderY * 2);

    if (drawURLString) {
        urlStringSize.setWidth(urlFont.width(urlRun));
        urlStringSize.setHeight(urlFont.fontMetrics().ascent() + urlFont.fontMetrics().descent());
        imageSize.setHeight(imageSize.height() + urlStringSize.height());
        if (urlStringSize.width() > kMaxDragLabelStringWidth) {
            imageSize.setWidth(kMaxDragLabelWidth);
            clipURLString = true;
        } else
            imageSize.setWidth(std::max(labelSize.width(), urlStringSize.width()) + kDragLabelBorderX * 2);
    }

    // We now know how big the image needs to be, so we create and
    // fill the background
    IntSize scaledImageSize = imageSize;
    scaledImageSize.scale(deviceScaleFactor);
    OwnPtr<ImageBuffer> buffer(ImageBuffer::create(scaledImageSize, deviceScaleFactor));
    if (!buffer)
        return 0;

    const float DragLabelRadius = 5;
    const IntSize radii(DragLabelRadius, DragLabelRadius);
    IntRect rect(IntPoint(), imageSize);
    const Color backgroundColor(140, 140, 140);
    buffer->context()->fillRoundedRect(rect, radii, radii, radii, radii, backgroundColor);

    // Draw the text
    if (drawURLString) {
        if (clipURLString)
            urlString = StringTruncator::centerTruncate(urlString, imageSize.width() - (kDragLabelBorderX * 2.0f), urlFont, StringTruncator::EnableRoundingHacks);
        IntPoint textPos(kDragLabelBorderX, imageSize.height() - (kLabelBorderYOffset + urlFont.fontMetrics().descent()));
        TextRun textRun(urlString);
        buffer->context()->drawText(urlFont, TextRunPaintInfo(textRun), textPos);
    }

    if (clipLabelString)
        label = StringTruncator::rightTruncate(label, imageSize.width() - (kDragLabelBorderX * 2.0f), labelFont, StringTruncator::EnableRoundingHacks);

    IntPoint textPos(kDragLabelBorderX, kDragLabelBorderY + labelFont.pixelSize());
    TextRun textRun(label);
    buffer->context()->drawText(urlFont, TextRunPaintInfo(textRun), textPos);

    RefPtr<Image> image = buffer->copyImage();
    return createDragImageFromImage(image.get());
}

} // namespace WebCore

