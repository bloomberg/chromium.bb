/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "core/rendering/shapes/ShapeInfo.h"

#include "core/rendering/RenderBlock.h"
#include "core/rendering/RenderImage.h"

namespace WebCore {

template<class RenderType>
void ShapeInfo<RenderType>::setReferenceBoxLogicalSize(LayoutSize newReferenceBoxLogicalSize)
{
    bool isHorizontalWritingMode = this->styleForWritingMode()->isHorizontalWritingMode();
    switch (referenceBox()) {
    case MarginBox:
        if (isHorizontalWritingMode)
            newReferenceBoxLogicalSize.expand(m_renderer.marginWidth(), m_renderer.marginHeight());
        else
            newReferenceBoxLogicalSize.expand(m_renderer.marginHeight(), m_renderer.marginWidth());
        break;
    case BorderBox:
        break;
    case PaddingBox:
        if (isHorizontalWritingMode)
            newReferenceBoxLogicalSize.shrink(m_renderer.borderWidth(), m_renderer.borderHeight());
        else
            newReferenceBoxLogicalSize.shrink(m_renderer.borderHeight(), m_renderer.borderWidth());
        break;
    case ContentBox:
        if (isHorizontalWritingMode)
            newReferenceBoxLogicalSize.shrink(m_renderer.borderAndPaddingWidth(), m_renderer.borderAndPaddingHeight());
        else
            newReferenceBoxLogicalSize.shrink(m_renderer.borderAndPaddingHeight(), m_renderer.borderAndPaddingWidth());
        break;
    case BoxMissing:
        ASSERT_NOT_REACHED();
        break;
    }

    if (m_referenceBoxLogicalSize == newReferenceBoxLogicalSize)
        return;
    markShapeAsDirty();
    m_referenceBoxLogicalSize = newReferenceBoxLogicalSize;
}

bool checkShapeImageOrigin(Document& document, ImageResource& imageResource)
{
    if (imageResource.isAccessAllowed(document.securityOrigin()))
        return true;

    const KURL& url = imageResource.url();
    String urlString = url.isNull() ? "''" : url.elidedString();
    document.addConsoleMessage(SecurityMessageSource, ErrorMessageLevel, "Unsafe attempt to load URL " + urlString + ".");

    return false;
}

static void getShapeImageAndRect(const ShapeValue* shapeValue, const RenderBox* renderBox, const LayoutSize& referenceBoxSize, Image*& image, LayoutRect& rect)
{
    ASSERT(shapeValue->isImageValid());
    StyleImage* styleImage = shapeValue->image();

    const IntSize& imageSize = renderBox->calculateImageIntrinsicDimensions(styleImage, roundedIntSize(referenceBoxSize), RenderImage::ScaleByEffectiveZoom);
    styleImage->setContainerSizeForRenderer(renderBox, imageSize, renderBox->style()->effectiveZoom());

    image = styleImage->cachedImage()->imageForRenderer(renderBox);
    if (renderBox->isRenderImage())
        rect = toRenderImage(renderBox)->replacedContentRect();
    else
        rect = LayoutRect(LayoutPoint(), imageSize);
}

static LayoutRect getShapeImageMarginRect(const RenderBox& renderBox, const LayoutSize& referenceBoxLogicalSize)
{
    LayoutPoint marginBoxOrigin(-renderBox.marginLogicalLeft() - renderBox.borderAndPaddingLogicalLeft(), -renderBox.marginBefore() - renderBox.borderBefore() - renderBox.paddingBefore());
    LayoutSize marginBoxSizeDelta(renderBox.marginLogicalWidth() + renderBox.borderAndPaddingLogicalWidth(), renderBox.marginLogicalHeight() + renderBox.borderAndPaddingLogicalHeight());
    return LayoutRect(marginBoxOrigin, referenceBoxLogicalSize + marginBoxSizeDelta);
}

template<class RenderType>
const Shape& ShapeInfo<RenderType>::computedShape() const
{
    if (Shape* shape = m_shape.get())
        return *shape;

    WritingMode writingMode = this->styleForWritingMode()->writingMode();
    Length margin = m_renderer.style()->shapeMargin();
    float shapeImageThreshold = m_renderer.style()->shapeImageThreshold();
    const ShapeValue* shapeValue = this->shapeValue();
    ASSERT(shapeValue);

    switch (shapeValue->type()) {
    case ShapeValue::Shape:
        ASSERT(shapeValue->shape());
        m_shape = Shape::createShape(shapeValue->shape(), m_referenceBoxLogicalSize, writingMode, margin);
        break;
    case ShapeValue::Image: {
        Image* image;
        LayoutRect imageRect;
        getShapeImageAndRect(shapeValue, &m_renderer, m_referenceBoxLogicalSize, image, imageRect);
        const LayoutRect& marginRect = getShapeImageMarginRect(m_renderer, m_referenceBoxLogicalSize);
        m_shape = Shape::createRasterShape(image, shapeImageThreshold, imageRect, marginRect, writingMode, margin);
        break;
    }
    case ShapeValue::Box: {
        const RoundedRect& shapeRect = m_renderer.style()->getRoundedBorderFor(LayoutRect(LayoutPoint(), m_referenceBoxLogicalSize), m_renderer.view());
        m_shape = Shape::createLayoutBoxShape(shapeRect, writingMode, margin);
        break;
    }
    }

    ASSERT(m_shape);
    return *m_shape;
}

template<class RenderType>
SegmentList ShapeInfo<RenderType>::computeSegmentsForLine(LayoutUnit lineTop, LayoutUnit lineHeight) const
{
    ASSERT(lineHeight >= 0);
    SegmentList segments;

    getIntervals((lineTop - logicalTopOffset()), std::min(lineHeight, shapeLogicalBottom() - lineTop), segments);

    for (size_t i = 0; i < segments.size(); i++) {
        segments[i].logicalLeft += logicalLeftOffset();
        segments[i].logicalRight += logicalLeftOffset();
    }

    return segments;
}

template<class RenderType>
inline LayoutUnit borderBeforeInWritingMode(const RenderType& renderer, WritingMode writingMode)
{
    switch (writingMode) {
    case TopToBottomWritingMode: return renderer.borderTop();
    case BottomToTopWritingMode: return renderer.borderBottom();
    case LeftToRightWritingMode: return renderer.borderLeft();
    case RightToLeftWritingMode: return renderer.borderRight();
    }

    ASSERT_NOT_REACHED();
    return renderer.borderBefore();
}

template<class RenderType>
inline LayoutUnit borderAndPaddingBeforeInWritingMode(const RenderType& renderer, WritingMode writingMode)
{
    switch (writingMode) {
    case TopToBottomWritingMode: return renderer.borderTop() + renderer.paddingTop();
    case BottomToTopWritingMode: return renderer.borderBottom() + renderer.paddingBottom();
    case LeftToRightWritingMode: return renderer.borderLeft() + renderer.paddingLeft();
    case RightToLeftWritingMode: return renderer.borderRight() + renderer.paddingRight();
    }

    ASSERT_NOT_REACHED();
    return renderer.borderAndPaddingBefore();
}

template<class RenderType>
LayoutUnit ShapeInfo<RenderType>::logicalTopOffset() const
{
    switch (referenceBox()) {
    case MarginBox: return -m_renderer.marginBefore(styleForWritingMode());
    case BorderBox: return LayoutUnit();
    case PaddingBox: return borderBeforeInWritingMode(m_renderer, styleForWritingMode()->writingMode());
    case ContentBox: return borderAndPaddingBeforeInWritingMode(m_renderer, styleForWritingMode()->writingMode());
    case BoxMissing: break;
    }

    ASSERT_NOT_REACHED();
    return LayoutUnit();
}

template<class RenderType>
inline LayoutUnit borderStartWithStyleForWritingMode(const RenderType& renderer, const RenderStyle* style)
{
    if (style->isHorizontalWritingMode()) {
        if (style->isLeftToRightDirection())
            return renderer.borderLeft();

        return renderer.borderRight();
    }
    if (style->isLeftToRightDirection())
        return renderer.borderTop();

    return renderer.borderBottom();
}

template<class RenderType>
inline LayoutUnit borderAndPaddingStartWithStyleForWritingMode(const RenderType& renderer, const RenderStyle* style)
{
    if (style->isHorizontalWritingMode()) {
        if (style->isLeftToRightDirection())
            return renderer.borderLeft() + renderer.paddingLeft();

        return renderer.borderRight() + renderer.paddingRight();
    }
    if (style->isLeftToRightDirection())
        return renderer.borderTop() + renderer.paddingTop();

    return renderer.borderBottom() + renderer.paddingBottom();
}

template<class RenderType>
LayoutUnit ShapeInfo<RenderType>::logicalLeftOffset() const
{
    switch (referenceBox()) {
    case MarginBox: return -m_renderer.marginStart(styleForWritingMode());
    case BorderBox: return LayoutUnit();
    case PaddingBox: return borderStartWithStyleForWritingMode(m_renderer, styleForWritingMode());
    case ContentBox: return borderAndPaddingStartWithStyleForWritingMode(m_renderer, styleForWritingMode());
    case BoxMissing: break;
    }

    ASSERT_NOT_REACHED();
    return LayoutUnit();
}

template class ShapeInfo<RenderBlock>;
template class ShapeInfo<RenderBox>;
}
