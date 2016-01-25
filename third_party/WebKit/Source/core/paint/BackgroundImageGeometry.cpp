// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/BackgroundImageGeometry.h"

#include "core/frame/FrameView.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/LayoutView.h"
#include "core/layout/compositing/CompositedLayerMapping.h"
#include "core/paint/PaintLayer.h"
#include "platform/LayoutUnit.h"
#include "platform/geometry/LayoutRect.h"

namespace blink {

namespace {

// Return the amount of space to leave between image tiles for the background-repeat: space property.
inline LayoutUnit getSpaceBetweenImageTiles(LayoutUnit areaSize, LayoutUnit tileSize)
{
    int numberOfTiles = areaSize / tileSize;
    LayoutUnit space = -1;

    if (numberOfTiles > 1) {
        // Spec doesn't specify rounding, so use the same method as for background-repeat: round.
        space = (areaSize - numberOfTiles * tileSize) / (numberOfTiles - 1);
    }

    return space;
}

bool fixedBackgroundPaintsInLocalCoordinates(const LayoutObject& obj, const GlobalPaintFlags globalPaintFlags)
{
    if (!obj.isLayoutView())
        return false;

    const LayoutView& view = toLayoutView(obj);

    if (globalPaintFlags & GlobalPaintFlattenCompositingLayers)
        return false;

    PaintLayer* rootLayer = view.layer();
    if (!rootLayer || rootLayer->compositingState() == NotComposited)
        return false;

    return rootLayer->compositedLayerMapping()->backgroundLayerPaintsFixedRootBackground();
}

LayoutSize calculateFillTileSize(const LayoutBoxModelObject& obj, const FillLayer& fillLayer, const LayoutSize& positioningAreaSize)
{
    StyleImage* image = fillLayer.image();
    EFillSizeType type = fillLayer.size().type;

    LayoutSize imageIntrinsicSize = obj.calculateImageIntrinsicDimensions(image, positioningAreaSize, LayoutBoxModelObject::ScaleByEffectiveZoom);
    imageIntrinsicSize.scale(1 / image->imageScaleFactor(), 1 / image->imageScaleFactor());
    switch (type) {
    case SizeLength: {
        LayoutSize tileSize(positioningAreaSize);

        Length layerWidth = fillLayer.size().size.width();
        Length layerHeight = fillLayer.size().size.height();

        if (layerWidth.isFixed())
            tileSize.setWidth(layerWidth.value());
        else if (layerWidth.hasPercent())
            tileSize.setWidth(valueForLength(layerWidth, positioningAreaSize.width()));

        if (layerHeight.isFixed())
            tileSize.setHeight(layerHeight.value());
        else if (layerHeight.hasPercent())
            tileSize.setHeight(valueForLength(layerHeight, positioningAreaSize.height()));

        // If one of the values is auto we have to use the appropriate
        // scale to maintain our aspect ratio.
        if (layerWidth.isAuto() && !layerHeight.isAuto()) {
            if (imageIntrinsicSize.height()) {
                LayoutUnit adjustedWidth = imageIntrinsicSize.width() * tileSize.height() / imageIntrinsicSize.height();
                if (imageIntrinsicSize.width() >= 1 && adjustedWidth < 1)
                    adjustedWidth = 1;
                tileSize.setWidth(adjustedWidth);
            }
        } else if (!layerWidth.isAuto() && layerHeight.isAuto()) {
            if (imageIntrinsicSize.width()) {
                LayoutUnit adjustedHeight = imageIntrinsicSize.height() * tileSize.width() / imageIntrinsicSize.width();
                if (imageIntrinsicSize.height() >= 1 && adjustedHeight < 1)
                    adjustedHeight = 1;
                tileSize.setHeight(adjustedHeight);
            }
        } else if (layerWidth.isAuto() && layerHeight.isAuto()) {
            // If both width and height are auto, use the image's intrinsic size.
            tileSize = imageIntrinsicSize;
        }

        tileSize.clampNegativeToZero();
        return tileSize;
    }
    case SizeNone: {
        // If both values are 'auto' then the intrinsic width and/or height of the image should be used, if any.
        if (!imageIntrinsicSize.isEmpty())
            return imageIntrinsicSize;

        // If the image has neither an intrinsic width nor an intrinsic height, its size is determined as for 'contain'.
        type = Contain;
    }
    case Contain:
    case Cover: {
        float horizontalScaleFactor = imageIntrinsicSize.width()
            ? positioningAreaSize.width().toFloat() / imageIntrinsicSize.width() : 1;
        float verticalScaleFactor = imageIntrinsicSize.height()
            ? positioningAreaSize.height().toFloat() / imageIntrinsicSize.height() : 1;
        float scaleFactor = type == Contain ? std::min(horizontalScaleFactor, verticalScaleFactor) : std::max(horizontalScaleFactor, verticalScaleFactor);
        return LayoutSize(std::max<LayoutUnit>(1, imageIntrinsicSize.width() * scaleFactor), std::max<LayoutUnit>(1, imageIntrinsicSize.height() * scaleFactor));
    }
    }

    ASSERT_NOT_REACHED();
    return LayoutSize();
}

IntPoint accumulatedScrollOffsetForFixedBackground(const LayoutBoxModelObject& object, const LayoutBoxModelObject* container)
{
    IntPoint result;
    if (&object == container)
        return result;
    for (const LayoutBlock* block = object.containingBlock(); block; block = block->containingBlock()) {
        if (block->hasOverflowClip())
            result += block->scrolledContentOffset();
        if (block == container)
            break;
    }
    return result;
}

// When we match the sub-pixel fraction of the destination rect in a dimension, we
// snap the same way. This commonly occurs when the background is meant to fill the
// padding box but there's a border (which in Blink is always stored as an integer).
// Otherwise we floor to avoid growing our tile size. Often these tiles are from a
// sprite map, and bleeding adjactent sprites is visually worse than clipping the
// intenteded one.
LayoutSize applySubPixelHeuristicToImageSize(const LayoutSize& size, const LayoutRect& destination)
{
    LayoutSize snappedSize = LayoutSize(
        size.width().fraction() == destination.width().fraction() ? snapSizeToPixel(size.width(), destination.x()) : size.width().floor(),
        size.height().fraction() == destination.height().fraction() ? snapSizeToPixel(size.height(), destination.y()) : size.height().floor());
    return snappedSize;
}

} // anonymous namespace

void BackgroundImageGeometry::setNoRepeatX(LayoutUnit xOffset)
{
    m_destRect.move(std::max(xOffset, LayoutUnit()), LayoutUnit());
    m_phase.setX(-std::min(xOffset, LayoutUnit()));
    m_destRect.setWidth(m_tileSize.width() + std::min(xOffset, LayoutUnit()));
}

void BackgroundImageGeometry::setNoRepeatY(LayoutUnit yOffset)
{
    m_destRect.move(LayoutUnit(), std::max(yOffset, LayoutUnit()));
    m_phase.setY(-std::min(yOffset, LayoutUnit()));
    m_destRect.setHeight(m_tileSize.height() + std::min(yOffset, LayoutUnit()));
}

void BackgroundImageGeometry::useFixedAttachment(const LayoutPoint& attachmentPoint)
{
    LayoutPoint alignedPoint = attachmentPoint;
    m_phase.move(std::max(alignedPoint.x() - m_destRect.x(), LayoutUnit()), std::max(alignedPoint.y() - m_destRect.y(), LayoutUnit()));
}

void BackgroundImageGeometry::clip(const LayoutRect& clipRect)
{
    m_destRect.intersect(clipRect);
}

void BackgroundImageGeometry::pixelSnapGeometry()
{
    setTileSize(applySubPixelHeuristicToImageSize(m_tileSize, m_destRect));
    setImageContainerSize(applySubPixelHeuristicToImageSize(m_imageContainerSize, m_destRect));
    setSpaceSize(LayoutSize(roundedIntSize(m_repeatSpacing)));
    setDestRect(LayoutRect(pixelSnappedIntRect(m_destRect)));
    setPhase(LayoutPoint(flooredIntPoint(m_phase)));
}

void BackgroundImageGeometry::calculate(const LayoutBoxModelObject& obj, const LayoutBoxModelObject* paintContainer,
    const GlobalPaintFlags globalPaintFlags, const FillLayer& fillLayer, const LayoutRect& paintRect)
{
    LayoutUnit left;
    LayoutUnit top;
    LayoutSize positioningAreaSize;
    bool isLayoutView = obj.isLayoutView();
    const LayoutBox* rootBox = nullptr;
    if (isLayoutView) {
        // It is only possible reach here when root element has a box.
        Element* documentElement = obj.document().documentElement();
        ASSERT(documentElement);
        ASSERT(documentElement->layoutObject());
        ASSERT(documentElement->layoutObject()->isBox());
        rootBox = toLayoutBox(documentElement->layoutObject());
    }
    const LayoutBoxModelObject& positioningBox = isLayoutView ? static_cast<const LayoutBoxModelObject&>(*rootBox) : obj;

    // Determine the background positioning area and set destRect to the background painting area.
    // destRect will be adjusted later if the background is non-repeating.
    // FIXME: transforms spec says that fixed backgrounds behave like scroll inside transforms.
    bool fixedAttachment = fillLayer.attachment() == FixedBackgroundAttachment;

    if (RuntimeEnabledFeatures::fastMobileScrollingEnabled()) {
        // As a side effect of an optimization to blit on scroll, we do not honor the CSS
        // property "background-attachment: fixed" because it may result in rendering
        // artifacts. Note, these artifacts only appear if we are blitting on scroll of
        // a page that has fixed background images.
        fixedAttachment = false;
    }

    if (!fixedAttachment) {
        setDestRect(paintRect);

        LayoutUnit right;
        LayoutUnit bottom;
        // Scroll and Local.
        if (fillLayer.origin() != BorderFillBox) {
            left = positioningBox.borderLeft();
            right = positioningBox.borderRight();
            top = positioningBox.borderTop();
            bottom = positioningBox.borderBottom();
            if (fillLayer.origin() == ContentFillBox) {
                left += positioningBox.paddingLeft();
                right += positioningBox.paddingRight();
                top += positioningBox.paddingTop();
                bottom += positioningBox.paddingBottom();
            }
        }

        if (isLayoutView) {
            // The background of the box generated by the root element covers the entire canvas and will
            // be painted by the view object, but the we should still use the root element box for
            // positioning.
            positioningAreaSize = rootBox->size() - LayoutSize(left + right, top + bottom), rootBox->location();
            // The input paint rect is specified in root element local coordinate (i.e. a transform
            // is applied on the context for painting), and is expanded to cover the whole canvas.
            // Since left/top is relative to the paint rect, we need to offset them back.
            left -= paintRect.x();
            top -= paintRect.y();
        } else {
            positioningAreaSize = paintRect.size() - LayoutSize(left + right, top + bottom);
        }
    } else {
        setHasNonLocalGeometry();

        LayoutRect viewportRect = obj.viewRect();
        if (fixedBackgroundPaintsInLocalCoordinates(obj, globalPaintFlags)) {
            viewportRect.setLocation(LayoutPoint());
        } else {
            if (FrameView* frameView = obj.view()->frameView())
                viewportRect.setLocation(frameView->scrollPosition());
            // Compensate the translations created by ScrollRecorders.
            // TODO(trchen): Fix this for SP phase 2. crbug.com/529963.
            viewportRect.moveBy(accumulatedScrollOffsetForFixedBackground(obj, paintContainer));
        }

        if (paintContainer)
            viewportRect.moveBy(LayoutPoint(-paintContainer->localToAbsolute(FloatPoint())));

        setDestRect(viewportRect);
        positioningAreaSize = destRect().size();
    }

    LayoutSize fillTileSize(calculateFillTileSize(positioningBox, fillLayer, positioningAreaSize));
    // It's necessary to apply the heuristic here prior to any further calculations to avoid
    // incorrectly using sub-pixel values that won't be present in the painted tile.
    fillTileSize = applySubPixelHeuristicToImageSize(fillTileSize, m_destRect);
    setTileSize(fillTileSize);
    setImageContainerSize(fillTileSize);

    EFillRepeat backgroundRepeatX = fillLayer.repeatX();
    EFillRepeat backgroundRepeatY = fillLayer.repeatY();
    positioningAreaSize = LayoutSize(snapSizeToPixel(positioningAreaSize.width(), m_destRect.x()), snapSizeToPixel(positioningAreaSize.height(), m_destRect.y()));
    LayoutUnit availableWidth = positioningAreaSize.width() - tileSize().width();
    LayoutUnit availableHeight = positioningAreaSize.height() - tileSize().height();

    LayoutUnit computedXPosition = roundedMinimumValueForLength(fillLayer.xPosition(), availableWidth);
    if (backgroundRepeatX == RoundFill && positioningAreaSize.width() > LayoutUnit() && fillTileSize.width() > LayoutUnit()) {
        int nrTiles = std::max(1, roundToInt(positioningAreaSize.width() / fillTileSize.width()));

        fillTileSize.setWidth(positioningAreaSize.width() / nrTiles);

        // Maintain aspect ratio if background-size: auto is set
        if (fillLayer.size().size.height().isAuto() && backgroundRepeatY != RoundFill) {
            fillTileSize.setHeight(fillTileSize.height() * positioningAreaSize.width() / (nrTiles * fillTileSize.width()));
        }

        setTileSize(fillTileSize);
        setImageContainerSize(fillTileSize);
        setPhaseX(tileSize().width() ? tileSize().width() - fmodf((computedXPosition + left), tileSize().width()) : 0.0f);
        setSpaceSize(LayoutSize());
    }

    LayoutUnit computedYPosition = roundedMinimumValueForLength(fillLayer.yPosition(), availableHeight);
    if (backgroundRepeatY == RoundFill && positioningAreaSize.height() > LayoutUnit() && fillTileSize.height() > LayoutUnit()) {
        int nrTiles = std::max(1, roundToInt(positioningAreaSize.height() / fillTileSize.height()));

        fillTileSize.setHeight(positioningAreaSize.height() / nrTiles);

        // Maintain aspect ratio if background-size: auto is set
        if (fillLayer.size().size.width().isAuto() && backgroundRepeatX != RoundFill) {
            fillTileSize.setWidth(fillTileSize.width() * positioningAreaSize.height() / (nrTiles * fillTileSize.height()));
        }

        setTileSize(fillTileSize);
        setImageContainerSize(fillTileSize);
        setPhaseY(tileSize().height() ? tileSize().height() - fmodf((computedYPosition + top), tileSize().height()) : 0.0f);
        setSpaceSize(LayoutSize());
    }

    if (backgroundRepeatX == RepeatFill) {
        LayoutUnit xOffset = fillLayer.backgroundXOrigin() == RightEdge ? availableWidth - computedXPosition : computedXPosition;
        setPhaseX(tileSize().width() ? tileSize().width() - fmodf((xOffset + left), tileSize().width()) : 0.0f);
        setSpaceSize(LayoutSize());
    } else if (backgroundRepeatX == SpaceFill && fillTileSize.width() > LayoutUnit()) {
        LayoutUnit space = getSpaceBetweenImageTiles(positioningAreaSize.width(), tileSize().width());
        LayoutUnit actualWidth = tileSize().width() + space;

        if (space >= LayoutUnit()) {
            computedXPosition = roundedMinimumValueForLength(Length(), availableWidth);
            setSpaceSize(LayoutSize(space, LayoutUnit()));
            setPhaseX(actualWidth ? actualWidth - fmodf((computedXPosition + left), actualWidth) : 0.0f);
        } else {
            backgroundRepeatX = NoRepeatFill;
        }
    }
    if (backgroundRepeatX == NoRepeatFill) {
        LayoutUnit xOffset = fillLayer.backgroundXOrigin() == RightEdge ? availableWidth - computedXPosition : computedXPosition;
        setNoRepeatX(left + xOffset);
        setSpaceSize(LayoutSize(LayoutUnit(), spaceSize().height()));
    }

    if (backgroundRepeatY == RepeatFill) {
        LayoutUnit yOffset = fillLayer.backgroundYOrigin() == BottomEdge ? availableHeight - computedYPosition : computedYPosition;
        setPhaseY(tileSize().height() ? tileSize().height() - fmodf((yOffset + top), tileSize().height()) : 0.0f);
        setSpaceSize(LayoutSize(spaceSize().width(), LayoutUnit()));
    } else if (backgroundRepeatY == SpaceFill && fillTileSize.height() > LayoutUnit()) {
        LayoutUnit space = getSpaceBetweenImageTiles(positioningAreaSize.height(), tileSize().height());
        LayoutUnit actualHeight = tileSize().height() + space;

        if (space >= LayoutUnit()) {
            computedYPosition = roundedMinimumValueForLength(Length(), availableHeight);
            setSpaceSize(LayoutSize(spaceSize().width(), space));
            setPhaseY(actualHeight ? actualHeight - fmodf((computedYPosition + top), actualHeight) : 0.0f);
        } else {
            backgroundRepeatY = NoRepeatFill;
        }
    }
    if (backgroundRepeatY == NoRepeatFill) {
        LayoutUnit yOffset = fillLayer.backgroundYOrigin() == BottomEdge ? availableHeight - computedYPosition : computedYPosition;
        setNoRepeatY(top + yOffset);
        setSpaceSize(LayoutSize(spaceSize().width(), LayoutUnit()));
    }

    if (fixedAttachment)
        useFixedAttachment(paintRect.location());

    clip(paintRect);
    pixelSnapGeometry();
}

} // namespace blink
