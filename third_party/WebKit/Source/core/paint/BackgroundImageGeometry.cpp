// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
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

inline void applySubPixelHeuristicForTileSize(LayoutSize& tileSize, const IntSize& positioningAreaSize)
{
    tileSize.setWidth(positioningAreaSize.width() - tileSize.width() <= 1 ? tileSize.width().ceil() : tileSize.width().floor());
    tileSize.setHeight(positioningAreaSize.height() - tileSize.height() <= 1 ? tileSize.height().ceil() : tileSize.height().floor());
}

// Return the amount of space to leave between image tiles for the background-repeat: space property.
inline int getSpaceBetweenImageTiles(int areaSize, int tileSize)
{
    int numberOfTiles = areaSize / tileSize;
    int space = -1;

    if (numberOfTiles > 1) {
        // Spec doesn't specify rounding, so use the same method as for background-repeat: round.
        space = lroundf((areaSize - numberOfTiles * tileSize) / (float)(numberOfTiles - 1));
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

IntSize calculateFillTileSize(const LayoutBoxModelObject& obj, const FillLayer& fillLayer, const IntSize& positioningAreaSize)
{
    StyleImage* image = fillLayer.image();
    EFillSizeType type = fillLayer.size().type;

    IntSize imageIntrinsicSize = obj.calculateImageIntrinsicDimensions(image, positioningAreaSize, LayoutBoxModelObject::ScaleByEffectiveZoom);
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

        applySubPixelHeuristicForTileSize(tileSize, positioningAreaSize);

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
            tileSize = LayoutSize(imageIntrinsicSize);
        }

        tileSize.clampNegativeToZero();
        return flooredIntSize(tileSize);
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
            ? static_cast<float>(positioningAreaSize.width()) / imageIntrinsicSize.width() : 1;
        float verticalScaleFactor = imageIntrinsicSize.height()
            ? static_cast<float>(positioningAreaSize.height()) / imageIntrinsicSize.height() : 1;
        float scaleFactor = type == Contain ? std::min(horizontalScaleFactor, verticalScaleFactor) : std::max(horizontalScaleFactor, verticalScaleFactor);
        return IntSize(std::max(1l, lround(imageIntrinsicSize.width() * scaleFactor)), std::max(1l, lround(imageIntrinsicSize.height() * scaleFactor)));
    }
    }

    ASSERT_NOT_REACHED();
    return IntSize();
}

IntPoint accumulatedScrollOffset(const LayoutBoxModelObject& object, const LayoutBoxModelObject* container)
{
    const LayoutBlock* block = object.isLayoutBlock() ? toLayoutBlock(&object) : object.containingBlock();
    IntPoint result;
    while (block) {
        if (block->hasOverflowClip())
            result += block->scrolledContentOffset();
        if (block == container)
            break;
        block = block->containingBlock();
    }
    return result;
}

} // anonymous namespace

void BackgroundImageGeometry::setNoRepeatX(int xOffset)
{
    m_destRect.move(std::max(xOffset, 0), 0);
    m_phase.setX(-std::min(xOffset, 0));
    m_destRect.setWidth(m_tileSize.width() + std::min(xOffset, 0));
}

void BackgroundImageGeometry::setNoRepeatY(int yOffset)
{
    m_destRect.move(0, std::max(yOffset, 0));
    m_phase.setY(-std::min(yOffset, 0));
    m_destRect.setHeight(m_tileSize.height() + std::min(yOffset, 0));
}

void BackgroundImageGeometry::useFixedAttachment(const IntPoint& attachmentPoint)
{
    IntPoint alignedPoint = attachmentPoint;
    m_phase.move(std::max(alignedPoint.x() - m_destRect.x(), 0), std::max(alignedPoint.y() - m_destRect.y(), 0));
}

void BackgroundImageGeometry::clip(const IntRect& clipRect)
{
    m_destRect.intersect(clipRect);
}

void BackgroundImageGeometry::calculate(const LayoutBoxModelObject& obj, const LayoutBoxModelObject* paintContainer,
    const GlobalPaintFlags globalPaintFlags, const FillLayer& fillLayer, const LayoutRect& paintRect,
    const LayoutObject* backgroundObject)
{
    LayoutUnit left = 0;
    LayoutUnit top = 0;
    IntSize positioningAreaSize;
    IntRect snappedPaintRect = pixelSnappedIntRect(paintRect);
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
        setDestRect(snappedPaintRect);

        LayoutUnit right = 0;
        LayoutUnit bottom = 0;
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
            positioningAreaSize = pixelSnappedIntSize(rootBox->size() - LayoutSize(left + right, top + bottom), rootBox->location());
            // The input paint rect is specified in root element local coordinate (i.e. a transform
            // is applied on the context for painting), and is expanded to cover the whole canvas.
            // Since left/top is relative to the paint rect, we need to offset them back.
            left -= paintRect.x();
            top -= paintRect.y();
        } else {
            positioningAreaSize = pixelSnappedIntSize(paintRect.size() - LayoutSize(left + right, top + bottom), paintRect.location());
        }
    } else {
        setHasNonLocalGeometry();

        IntRect viewportRect = pixelSnappedIntRect(obj.viewRect());
        if (fixedBackgroundPaintsInLocalCoordinates(obj, globalPaintFlags)) {
            viewportRect.setLocation(IntPoint());
        } else {
            if (FrameView* frameView = obj.view()->frameView())
                viewportRect.setLocation(frameView->scrollPosition());
            // Compensate the translations created by ScrollRecorders.
            // TODO(trchen): Fix this for SP phase 2. crbug.com/529963.
            viewportRect.moveBy(accumulatedScrollOffset(obj, paintContainer));
        }

        if (paintContainer) {
            IntPoint absoluteContainerOffset = roundedIntPoint(paintContainer->localToAbsolute(FloatPoint()));
            viewportRect.moveBy(-absoluteContainerOffset);
        }

        setDestRect(viewportRect);
        positioningAreaSize = destRect().size();
    }

    const LayoutObject* clientForBackgroundImage = backgroundObject ? backgroundObject : &obj;
    IntSize fillTileSize = calculateFillTileSize(positioningBox, fillLayer, positioningAreaSize);
    fillLayer.image()->setContainerSizeForLayoutObject(clientForBackgroundImage, fillTileSize, obj.style()->effectiveZoom());
    setTileSize(fillTileSize);

    EFillRepeat backgroundRepeatX = fillLayer.repeatX();
    EFillRepeat backgroundRepeatY = fillLayer.repeatY();
    int availableWidth = positioningAreaSize.width() - tileSize().width();
    int availableHeight = positioningAreaSize.height() - tileSize().height();

    LayoutUnit computedXPosition = roundedMinimumValueForLength(fillLayer.xPosition(), availableWidth);
    if (backgroundRepeatX == RoundFill && positioningAreaSize.width() > 0 && fillTileSize.width() > 0) {
        long nrTiles = std::max(1l, lroundf((float)positioningAreaSize.width() / fillTileSize.width()));

        // Round tile size per css3-background spec.
        fillTileSize.setWidth(lroundf(positioningAreaSize.width() / (float)nrTiles));

        // Maintain aspect ratio if background-size: auto is set
        if (fillLayer.size().size.height().isAuto() && backgroundRepeatY != RoundFill) {
            fillTileSize.setHeight(fillTileSize.height() * positioningAreaSize.width() / (nrTiles * fillTileSize.width()));
        }

        setTileSize(fillTileSize);
        setPhaseX(tileSize().width() ? tileSize().width() - roundToInt(computedXPosition + left) % tileSize().width() : 0);
        setSpaceSize(IntSize());
    }

    LayoutUnit computedYPosition = roundedMinimumValueForLength(fillLayer.yPosition(), availableHeight);
    if (backgroundRepeatY == RoundFill && positioningAreaSize.height() > 0 && fillTileSize.height() > 0) {
        long nrTiles = std::max(1l, lroundf((float)positioningAreaSize.height() / fillTileSize.height()));

        // Round tile size per css3-background spec.
        fillTileSize.setHeight(lroundf(positioningAreaSize.height() / (float)nrTiles));

        // Maintain aspect ratio if background-size: auto is set
        if (fillLayer.size().size.width().isAuto() && backgroundRepeatX != RoundFill) {
            fillTileSize.setWidth(fillTileSize.width() * positioningAreaSize.height() / (nrTiles * fillTileSize.height()));
        }

        setTileSize(fillTileSize);
        setPhaseY(tileSize().height() ? tileSize().height() - roundToInt(computedYPosition + top) % tileSize().height() : 0);
        setSpaceSize(IntSize());
    }

    if (backgroundRepeatX == RepeatFill) {
        int xOffset = fillLayer.backgroundXOrigin() == RightEdge ? availableWidth - computedXPosition : computedXPosition;
        setPhaseX(tileSize().width() ? tileSize().width() - roundToInt(xOffset + left) % tileSize().width() : 0);
        setSpaceSize(IntSize());
    } else if (backgroundRepeatX == SpaceFill && fillTileSize.width() > 0) {
        int space = getSpaceBetweenImageTiles(positioningAreaSize.width(), tileSize().width());
        int actualWidth = tileSize().width() + space;

        if (space >= 0) {
            computedXPosition = roundedMinimumValueForLength(Length(), availableWidth);
            setSpaceSize(IntSize(space, 0));
            setPhaseX(actualWidth ? actualWidth - roundToInt(computedXPosition + left) % actualWidth : 0);
        } else {
            backgroundRepeatX = NoRepeatFill;
        }
    }
    if (backgroundRepeatX == NoRepeatFill) {
        int xOffset = fillLayer.backgroundXOrigin() == RightEdge ? availableWidth - computedXPosition : computedXPosition;
        setNoRepeatX(left + xOffset);
        setSpaceSize(IntSize(0, spaceSize().height()));
    }

    if (backgroundRepeatY == RepeatFill) {
        int yOffset = fillLayer.backgroundYOrigin() == BottomEdge ? availableHeight - computedYPosition : computedYPosition;
        setPhaseY(tileSize().height() ? tileSize().height() - roundToInt(yOffset + top) % tileSize().height() : 0);
        setSpaceSize(IntSize(spaceSize().width(), 0));
    } else if (backgroundRepeatY == SpaceFill && fillTileSize.height() > 0) {
        int space = getSpaceBetweenImageTiles(positioningAreaSize.height(), tileSize().height());
        int actualHeight = tileSize().height() + space;

        if (space >= 0) {
            computedYPosition = roundedMinimumValueForLength(Length(), availableHeight);
            setSpaceSize(IntSize(spaceSize().width(), space));
            setPhaseY(actualHeight ? actualHeight - roundToInt(computedYPosition + top) % actualHeight : 0);
        } else {
            backgroundRepeatY = NoRepeatFill;
        }
    }
    if (backgroundRepeatY == NoRepeatFill) {
        int yOffset = fillLayer.backgroundYOrigin() == BottomEdge ? availableHeight - computedYPosition : computedYPosition;
        setNoRepeatY(top + yOffset);
        setSpaceSize(IntSize(spaceSize().width(), 0));
    }

    if (fixedAttachment)
        useFixedAttachment(snappedPaintRect.location());

    clip(snappedPaintRect);
}

} // namespace blink
