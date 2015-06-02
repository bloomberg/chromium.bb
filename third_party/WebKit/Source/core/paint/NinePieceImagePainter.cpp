// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/NinePieceImagePainter.h"

#include "core/layout/ImageQualityController.h"
#include "core/layout/LayoutBoxModelObject.h"
#include "core/paint/BoxPainter.h"
#include "core/style/ComputedStyle.h"
#include "core/style/NinePieceImage.h"
#include "platform/LengthFunctions.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/GraphicsContext.h"

namespace blink {

NinePieceImagePainter::NinePieceImagePainter(LayoutBoxModelObject& layoutObject)
    : m_layoutObject(layoutObject)
{
}

LayoutUnit NinePieceImagePainter::computeBorderImageSide(const BorderImageLength& borderSlice, LayoutUnit borderSide, LayoutUnit imageSide, LayoutUnit boxExtent)
{
    if (borderSlice.isNumber())
        return borderSlice.number() * borderSide;
    if (borderSlice.length().isAuto())
        return imageSide;
    return valueForLength(borderSlice.length(), boxExtent);
}

bool NinePieceImagePainter::paint(GraphicsContext* graphicsContext, const LayoutRect& rect, const ComputedStyle& style, const NinePieceImage& ninePieceImage, SkXfermode::Mode op) const
{
    StyleImage* styleImage = ninePieceImage.image();
    if (!styleImage)
        return false;

    if (!styleImage->isLoaded())
        return true; // Never paint a nine-piece image incrementally, but don't paint the fallback borders either.

    if (!styleImage->canRender(m_layoutObject, style.effectiveZoom()))
        return false;

    // FIXME: border-image is broken with full page zooming when tiling has to happen, since the tiling function
    // doesn't have any understanding of the zoom that is in effect on the tile.
    LayoutRect rectWithOutsets = rect;
    rectWithOutsets.expand(style.imageOutsets(ninePieceImage));
    IntRect borderImageRect = pixelSnappedIntRect(rectWithOutsets);

    IntSize imageSize = m_layoutObject.calculateImageIntrinsicDimensions(styleImage, borderImageRect.size(), LayoutBoxModelObject::DoNotScaleByEffectiveZoom);

    // If both values are 'auto' then the intrinsic width and/or height of the image should be used, if any.
    styleImage->setContainerSizeForLayoutObject(&m_layoutObject, imageSize, style.effectiveZoom());

    int imageWidth = imageSize.width();
    int imageHeight = imageSize.height();

    float imageScaleFactor = styleImage->imageScaleFactor();
    int topSlice = std::min<int>(imageHeight, valueForLength(ninePieceImage.imageSlices().top(), imageHeight)) * imageScaleFactor;
    int rightSlice = std::min<int>(imageWidth, valueForLength(ninePieceImage.imageSlices().right(), imageWidth)) * imageScaleFactor;
    int bottomSlice = std::min<int>(imageHeight, valueForLength(ninePieceImage.imageSlices().bottom(), imageHeight)) * imageScaleFactor;
    int leftSlice = std::min<int>(imageWidth, valueForLength(ninePieceImage.imageSlices().left(), imageWidth)) * imageScaleFactor;

    ENinePieceImageRule hRule = ninePieceImage.horizontalRule();
    ENinePieceImageRule vRule = ninePieceImage.verticalRule();

    int topWidth = computeBorderImageSide(ninePieceImage.borderSlices().top(), style.borderTopWidth(), topSlice, borderImageRect.height());
    int rightWidth = computeBorderImageSide(ninePieceImage.borderSlices().right(), style.borderRightWidth(), rightSlice, borderImageRect.width());
    int bottomWidth = computeBorderImageSide(ninePieceImage.borderSlices().bottom(), style.borderBottomWidth(), bottomSlice, borderImageRect.height());
    int leftWidth = computeBorderImageSide(ninePieceImage.borderSlices().left(), style.borderLeftWidth(), leftSlice, borderImageRect.width());

    // Reduce the widths if they're too large.
    // The spec says: Given Lwidth as the width of the border image area, Lheight as its height, and Wside as the border image width
    // offset for the side, let f = min(Lwidth/(Wleft+Wright), Lheight/(Wtop+Wbottom)). If f < 1, then all W are reduced by
    // multiplying them by f.
    int borderSideWidth = std::max(1, leftWidth + rightWidth);
    int borderSideHeight = std::max(1, topWidth + bottomWidth);
    float borderSideScaleFactor = std::min((float)borderImageRect.width() / borderSideWidth, (float)borderImageRect.height() / borderSideHeight);
    if (borderSideScaleFactor < 1) {
        topWidth *= borderSideScaleFactor;
        rightWidth *= borderSideScaleFactor;
        bottomWidth *= borderSideScaleFactor;
        leftWidth *= borderSideScaleFactor;
    }

    bool drawLeft = leftSlice > 0 && leftWidth > 0;
    bool drawTop = topSlice > 0 && topWidth > 0;
    bool drawRight = rightSlice > 0 && rightWidth > 0;
    bool drawBottom = bottomSlice > 0 && bottomWidth > 0;
    bool drawMiddle = ninePieceImage.fill() && (imageWidth - leftSlice - rightSlice) > 0 && (borderImageRect.width() - leftWidth - rightWidth) > 0
        && (imageHeight - topSlice - bottomSlice) > 0 && (borderImageRect.height() - topWidth - bottomWidth) > 0;

    RefPtr<Image> image = styleImage->image(&m_layoutObject, imageSize);

    float destinationWidth = borderImageRect.width() - leftWidth - rightWidth;
    float destinationHeight = borderImageRect.height() - topWidth - bottomWidth;

    float sourceWidth = imageWidth - leftSlice - rightSlice;
    float sourceHeight = imageHeight - topSlice - bottomSlice;

    float leftSideScale = drawLeft ? (float)leftWidth / leftSlice : 1;
    float rightSideScale = drawRight ? (float)rightWidth / rightSlice : 1;
    float topSideScale = drawTop ? (float)topWidth / topSlice : 1;
    float bottomSideScale = drawBottom ? (float)bottomWidth / bottomSlice : 1;

    InterpolationQuality interpolationQuality = BoxPainter::chooseInterpolationQuality(m_layoutObject, graphicsContext, image.get(), 0, rectWithOutsets.size());
    InterpolationQuality previousInterpolationQuality = graphicsContext->imageInterpolationQuality();
    graphicsContext->setImageInterpolationQuality(interpolationQuality);

    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "PaintImage", "data", InspectorPaintImageEvent::data(m_layoutObject, *styleImage));
    if (drawLeft) {
        // Paint the top and bottom left corners.

        // The top left corner rect is (tx, ty, leftWidth, topWidth)
        // The rect to use from within the image is obtained from our slice, and is (0, 0, leftSlice, topSlice)
        if (drawTop) {
            graphicsContext->drawImage(image.get(), IntRect(borderImageRect.location(), IntSize(leftWidth, topWidth)),
                LayoutRect(0, 0, leftSlice, topSlice), op);
        }

        // The bottom left corner rect is (tx, ty + h - bottomWidth, leftWidth, bottomWidth)
        // The rect to use from within the image is (0, imageHeight - bottomSlice, leftSlice, botomSlice)
        if (drawBottom) {
            graphicsContext->drawImage(image.get(), IntRect(borderImageRect.x(), borderImageRect.maxY() - bottomWidth, leftWidth, bottomWidth),
                LayoutRect(0, imageHeight - bottomSlice, leftSlice, bottomSlice), op);
        }

        // Paint the left edge.
        // Have to scale and tile into the border rect.
        if (sourceHeight > 0) {
            graphicsContext->drawTiledImage(image.get(), IntRect(borderImageRect.x(), borderImageRect.y() + topWidth, leftWidth, destinationHeight),
                IntRect(0, topSlice, leftSlice, sourceHeight), FloatSize(leftSideScale, leftSideScale), Image::StretchTile, (Image::TileRule)vRule, op);
        }
    }

    if (drawRight) {
        // Paint the top and bottom right corners
        // The top right corner rect is (tx + w - rightWidth, ty, rightWidth, topWidth)
        // The rect to use from within the image is obtained from our slice, and is (imageWidth - rightSlice, 0, rightSlice, topSlice)
        if (drawTop) {
            graphicsContext->drawImage(image.get(), IntRect(borderImageRect.maxX() - rightWidth, borderImageRect.y(), rightWidth, topWidth),
                LayoutRect(imageWidth - rightSlice, 0, rightSlice, topSlice), op);
        }

        // The bottom right corner rect is (tx + w - rightWidth, ty + h - bottomWidth, rightWidth, bottomWidth)
        // The rect to use from within the image is (imageWidth - rightSlice, imageHeight - bottomSlice, rightSlice, bottomSlice)
        if (drawBottom) {
            graphicsContext->drawImage(image.get(), IntRect(borderImageRect.maxX() - rightWidth, borderImageRect.maxY() - bottomWidth, rightWidth, bottomWidth),
                LayoutRect(imageWidth - rightSlice, imageHeight - bottomSlice, rightSlice, bottomSlice), op);
        }

        // Paint the right edge.
        if (sourceHeight > 0) {
            graphicsContext->drawTiledImage(image.get(), IntRect(borderImageRect.maxX() - rightWidth, borderImageRect.y() + topWidth, rightWidth,
                destinationHeight),
                IntRect(imageWidth - rightSlice, topSlice, rightSlice, sourceHeight),
                FloatSize(rightSideScale, rightSideScale),
                Image::StretchTile, (Image::TileRule)vRule, op);
        }
    }

    // Paint the top edge.
    if (drawTop && sourceWidth > 0) {
        graphicsContext->drawTiledImage(image.get(), IntRect(borderImageRect.x() + leftWidth, borderImageRect.y(), destinationWidth, topWidth),
            IntRect(leftSlice, 0, sourceWidth, topSlice),
            FloatSize(topSideScale, topSideScale), (Image::TileRule)hRule, Image::StretchTile, op);
    }

    // Paint the bottom edge.
    if (drawBottom && sourceWidth > 0) {
        graphicsContext->drawTiledImage(image.get(), IntRect(borderImageRect.x() + leftWidth, borderImageRect.maxY() - bottomWidth,
            destinationWidth, bottomWidth),
            IntRect(leftSlice, imageHeight - bottomSlice, sourceWidth, bottomSlice),
            FloatSize(bottomSideScale, bottomSideScale),
            (Image::TileRule)hRule, Image::StretchTile, op);
    }

    // Paint the middle.
    if (drawMiddle) {
        FloatSize middleScaleFactor(1, 1);
        if (drawTop)
            middleScaleFactor.setWidth(topSideScale);
        else if (drawBottom)
            middleScaleFactor.setWidth(bottomSideScale);
        if (drawLeft)
            middleScaleFactor.setHeight(leftSideScale);
        else if (drawRight)
            middleScaleFactor.setHeight(rightSideScale);

        // For "stretch" rules, just override the scale factor and replace. We only had to do this for the
        // center tile, since sides don't even use the scale factor unless they have a rule other than "stretch".
        // The middle however can have "stretch" specified in one axis but not the other, so we have to
        // correct the scale here.
        if (hRule == StretchImageRule)
            middleScaleFactor.setWidth(destinationWidth / sourceWidth);

        if (vRule == StretchImageRule)
            middleScaleFactor.setHeight(destinationHeight / sourceHeight);

        graphicsContext->drawTiledImage(image.get(),
            IntRect(borderImageRect.x() + leftWidth, borderImageRect.y() + topWidth, destinationWidth, destinationHeight),
            IntRect(leftSlice, topSlice, sourceWidth, sourceHeight),
            middleScaleFactor, (Image::TileRule)hRule, (Image::TileRule)vRule, op);
    }
    graphicsContext->setImageInterpolationQuality(previousInterpolationQuality);
    return true;
}

} // namespace blink
