// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/BoxPainter.h"

#include "core/HTMLNames.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/layout/ImageQualityController.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutTable.h"
#include "core/layout/LayoutTheme.h"
#include "core/layout/LayoutView.h"
#include "core/layout/compositing/CompositedLayerMapping.h"
#include "core/style/BorderEdge.h"
#include "core/style/ShadowList.h"
#include "core/paint/BackgroundImageGeometry.h"
#include "core/paint/BoxBorderPainter.h"
#include "core/paint/BoxDecorationData.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/NinePieceImagePainter.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/RoundedInnerRectClipper.h"
#include "core/paint/ThemePainter.h"
#include "platform/LengthFunctions.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/geometry/LayoutRectOutsets.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/graphics/paint/CompositingDisplayItem.h"
#include "wtf/Optional.h"

namespace blink {

void BoxPainter::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    LayoutPoint adjustedPaintOffset = paintOffset + m_layoutBox.location();
    // default implementation. Just pass paint through to the children
    PaintInfo childInfo(paintInfo);
    childInfo.updatePaintingRootForChildren(&m_layoutBox);
    for (LayoutObject* child = m_layoutBox.slowFirstChild(); child; child = child->nextSibling())
        child->paint(childInfo, adjustedPaintOffset);
}

void BoxPainter::paintBoxDecorationBackground(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!paintInfo.shouldPaintWithinRoot(&m_layoutBox))
        return;

    LayoutRect paintRect = m_layoutBox.borderBoxRect();
    paintRect.moveBy(paintOffset);
    paintBoxDecorationBackgroundWithRect(paintInfo, paintOffset, paintRect);
}

LayoutRect BoxPainter::boundsForDrawingRecorder(const LayoutPoint& paintOffset)
{
    // Use the visual overflow rect here, because it will include overflow introduced by the theme.
    LayoutRect bounds = m_layoutBox.visualOverflowRect();
    bounds.moveBy(paintOffset);
    return bounds;
}

namespace {

bool bleedAvoidanceIsClipping(BackgroundBleedAvoidance bleedAvoidance)
{
    return bleedAvoidance == BackgroundBleedClipOnly || bleedAvoidance == BackgroundBleedClipLayer;
}

} // anonymous namespace

void BoxPainter::paintBoxDecorationBackgroundWithRect(const PaintInfo& paintInfo, const LayoutPoint& paintOffset, const LayoutRect& paintRect)
{
    const ComputedStyle& style = m_layoutBox.styleRef();

    // FIXME: For now we don't have notification on media buffered range change from media player
    // and miss paint invalidation on buffered range change. crbug.com/484288.
    Optional<DisplayItemCacheSkipper> cacheSkipper;
    if (style.appearance() == MediaSliderPart)
        cacheSkipper.emplace(paintInfo.context);

    if (LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(paintInfo.context, m_layoutBox, DisplayItem::BoxDecorationBackground, paintOffset))
        return;

    LayoutObjectDrawingRecorder recorder(paintInfo.context, m_layoutBox, DisplayItem::BoxDecorationBackground, boundsForDrawingRecorder(paintOffset), paintOffset);

    BoxDecorationData boxDecorationData(m_layoutBox);

    // FIXME: Should eventually give the theme control over whether the box shadow should paint, since controls could have
    // custom shadows of their own.
    if (!m_layoutBox.boxShadowShouldBeAppliedToBackground(boxDecorationData.bleedAvoidance))
        paintBoxShadow(paintInfo, paintRect, style, Normal);

    GraphicsContextStateSaver stateSaver(paintInfo.context, false);
    if (bleedAvoidanceIsClipping(boxDecorationData.bleedAvoidance)) {

        stateSaver.save();
        FloatRoundedRect border = style.getRoundedBorderFor(paintRect);
        paintInfo.context.clipRoundedRect(border);

        if (boxDecorationData.bleedAvoidance == BackgroundBleedClipLayer)
            paintInfo.context.beginLayer();
    }

    // If we have a native theme appearance, paint that before painting our background.
    // The theme will tell us whether or not we should also paint the CSS background.
    IntRect snappedPaintRect(pixelSnappedIntRect(paintRect));
    ThemePainter& themePainter = LayoutTheme::theme().painter();
    bool themePainted = boxDecorationData.hasAppearance && !themePainter.paint(m_layoutBox, paintInfo, snappedPaintRect);
    if (!themePainted) {
        paintBackground(paintInfo, paintRect, boxDecorationData.backgroundColor, boxDecorationData.bleedAvoidance);

        if (boxDecorationData.hasAppearance)
            themePainter.paintDecorations(m_layoutBox, paintInfo, snappedPaintRect);
    }
    paintBoxShadow(paintInfo, paintRect, style, Inset);

    // The theme will tell us whether or not we should also paint the CSS border.
    if (boxDecorationData.hasBorderDecoration
        && (!boxDecorationData.hasAppearance || (!themePainted && LayoutTheme::theme().painter().paintBorderOnly(m_layoutBox, paintInfo, snappedPaintRect)))
        && !(m_layoutBox.isTable() && toLayoutTable(&m_layoutBox)->collapseBorders()))
        paintBorder(m_layoutBox, paintInfo, paintRect, style, boxDecorationData.bleedAvoidance);

    if (boxDecorationData.bleedAvoidance == BackgroundBleedClipLayer)
        paintInfo.context.endLayer();
}

void BoxPainter::paintBackground(const PaintInfo& paintInfo, const LayoutRect& paintRect, const Color& backgroundColor, BackgroundBleedAvoidance bleedAvoidance)
{
    if (m_layoutBox.isDocumentElement())
        return;
    if (m_layoutBox.backgroundStolenForBeingBody())
        return;
    if (m_layoutBox.boxDecorationBackgroundIsKnownToBeObscured())
        return;
    paintFillLayers(paintInfo, backgroundColor, m_layoutBox.style()->backgroundLayers(), paintRect, bleedAvoidance);
}

static bool isFillLayerOpaque(const FillLayer& layer, const LayoutObject& imageClient)
{
    return layer.hasOpaqueImage(&imageClient)
        && layer.image()->canRender()
        && !layer.image()->imageSize(&imageClient, imageClient.style()->effectiveZoom()).isEmpty()
        && layer.hasRepeatXY();
}

bool BoxPainter::calculateFillLayerOcclusionCulling(FillLayerOcclusionOutputList &reversedPaintList, const FillLayer& fillLayer)
{
    bool isNonAssociative = false;
    for (auto currentLayer = &fillLayer; currentLayer; currentLayer = currentLayer->next()) {
        reversedPaintList.append(currentLayer);
        // Stop traversal when an opaque layer is encountered.
        // FIXME : It would be possible for the following occlusion culling test to be more aggressive
        // on layers with no repeat by testing whether the image covers the layout rect.
        // Testing that here would imply duplicating a lot of calculations that are currently done in
        // LayoutBoxModelObject::paintFillLayer. A more efficient solution might be to move the layer
        // recursion into paintFillLayer, or to compute the layer geometry here and pass it down.

        // TODO(trchen): Need to check compositing mode as well.
        if (currentLayer->blendMode() != WebBlendModeNormal)
            isNonAssociative = true;

        // TODO(trchen): A fill layer cannot paint if the calculated tile size is empty.
        // This occlusion check can be wrong.
        if (currentLayer->clipOccludesNextLayers()
            && isFillLayerOpaque(*currentLayer, m_layoutBox)) {
            if (currentLayer->clip() == BorderFillBox)
                isNonAssociative = false;
            break;
        }
    }
    return isNonAssociative;
}

void BoxPainter::paintFillLayers(const PaintInfo& paintInfo, const Color& c, const FillLayer& fillLayer, const LayoutRect& rect, BackgroundBleedAvoidance bleedAvoidance, SkXfermode::Mode op, const LayoutObject* backgroundObject)
{
    // TODO(trchen): Box shadow optimization and background color are concepts that only
    // apply to background layers. Ideally we should refactor those out of paintFillLayer.
    FillLayerOcclusionOutputList reversedPaintList;
    bool shouldDrawBackgroundInSeparateBuffer = false;
    if (!m_layoutBox.boxShadowShouldBeAppliedToBackground(bleedAvoidance)) {
        shouldDrawBackgroundInSeparateBuffer = calculateFillLayerOcclusionCulling(reversedPaintList, fillLayer);
    } else {
        // If we are responsible for painting box shadow, don't perform fill layer culling.
        // TODO(trchen): In theory we only need to make sure the last layer has border box clipping
        // and make it paint the box shadow. Investigate optimization opportunity later.
        for (auto currentLayer = &fillLayer; currentLayer; currentLayer = currentLayer->next()) {
            reversedPaintList.append(currentLayer);
            if (currentLayer->composite() != CompositeSourceOver || currentLayer->blendMode() != WebBlendModeNormal)
                shouldDrawBackgroundInSeparateBuffer = true;
        }
    }

    // TODO(trchen): We can optimize out isolation group if we have a non-transparent
    // background color and the bottom layer encloses all other layers.

    GraphicsContext& context = paintInfo.context;

    if (shouldDrawBackgroundInSeparateBuffer)
        context.beginLayer();

    for (auto it = reversedPaintList.rbegin(); it != reversedPaintList.rend(); ++it)
        paintFillLayer(m_layoutBox, paintInfo, c, **it, rect, bleedAvoidance, 0, LayoutSize(), op, backgroundObject);

    if (shouldDrawBackgroundInSeparateBuffer)
        context.endLayer();
}

void BoxPainter::applyBoxShadowForBackground(GraphicsContext& context, const LayoutObject& obj)
{
    const ShadowList* shadowList = obj.style()->boxShadow();
    ASSERT(shadowList);
    for (size_t i = shadowList->shadows().size(); i--; ) {
        const ShadowData& boxShadow = shadowList->shadows()[i];
        if (boxShadow.style() != Normal)
            continue;
        FloatSize shadowOffset(boxShadow.x(), boxShadow.y());
        context.setShadow(shadowOffset, boxShadow.blur(),
            boxShadow.color().resolve(obj.resolveColor(CSSPropertyColor)),
            DrawLooperBuilder::ShadowRespectsTransforms, DrawLooperBuilder::ShadowIgnoresAlpha);
        return;
    }
}

FloatRoundedRect BoxPainter::getBackgroundRoundedRect(const LayoutObject& obj, const LayoutRect& borderRect,
    const InlineFlowBox* box, LayoutUnit inlineBoxWidth, LayoutUnit inlineBoxHeight,
    bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
{
    FloatRoundedRect border = obj.style()->getRoundedBorderFor(borderRect, includeLogicalLeftEdge, includeLogicalRightEdge);
    if (box && (box->nextLineBox() || box->prevLineBox())) {
        FloatRoundedRect segmentBorder = obj.style()->getRoundedBorderFor(LayoutRect(0, 0, inlineBoxWidth, inlineBoxHeight),
            includeLogicalLeftEdge, includeLogicalRightEdge);
        border.setRadii(segmentBorder.radii());
    }
    return border;
}

FloatRoundedRect BoxPainter::backgroundRoundedRectAdjustedForBleedAvoidance(const LayoutObject& obj,
    const LayoutRect& borderRect, BackgroundBleedAvoidance bleedAvoidance, const InlineFlowBox* box,
    const LayoutSize& boxSize, bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
{
    if (bleedAvoidance == BackgroundBleedShrinkBackground) {
        // Inset the background rect by a "safe" amount: 1/2 border-width for opaque border styles,
        // 1/6 border-width for double borders.

        // TODO(fmalita): we should be able to fold these parameters into BoxBorderInfo or
        // BoxDecorationData and avoid calling getBorderEdgeInfo redundantly here.
        BorderEdge edges[4];
        obj.style()->getBorderEdgeInfo(edges, includeLogicalLeftEdge, includeLogicalRightEdge);

        // Use the most conservative inset to avoid mixed-style corner issues.
        float fractionalInset = 1.0f / 2;
        for (auto& edge : edges) {
            if (edge.borderStyle() == DOUBLE) {
                fractionalInset = 1.0f / 6;
                break;
            }
        }

        FloatRectOutsets insets(
            -fractionalInset * edges[BSTop].width,
            -fractionalInset * edges[BSRight].width,
            -fractionalInset * edges[BSBottom].width,
            -fractionalInset * edges[BSLeft].width);

        FloatRoundedRect backgroundRoundedRect = getBackgroundRoundedRect(obj, borderRect, box, boxSize.width(), boxSize.height(),
            includeLogicalLeftEdge, includeLogicalRightEdge);
        FloatRect insetRect(backgroundRoundedRect.rect());
        insetRect.expand(insets);
        FloatRoundedRect::Radii insetRadii(backgroundRoundedRect.radii());
        insetRadii.shrink(-insets.top(), -insets.bottom(), -insets.left(), -insets.right());
        return FloatRoundedRect(insetRect, insetRadii);
    }

    return getBackgroundRoundedRect(obj, borderRect, box, boxSize.width(), boxSize.height(), includeLogicalLeftEdge, includeLogicalRightEdge);
}

void BoxPainter::paintFillLayer(const LayoutBoxModelObject& obj, const PaintInfo& paintInfo, const Color& color, const FillLayer& bgLayer, const LayoutRect& rect, BackgroundBleedAvoidance bleedAvoidance, const InlineFlowBox* box, const LayoutSize& boxSize, SkXfermode::Mode op, const LayoutObject* backgroundObject)
{
    GraphicsContext& context = paintInfo.context;
    if (rect.isEmpty())
        return;

    bool includeLeftEdge = box ? box->includeLogicalLeftEdge() : true;
    bool includeRightEdge = box ? box->includeLogicalRightEdge() : true;

    bool hasRoundedBorder = obj.style()->hasBorderRadius() && (includeLeftEdge || includeRightEdge);
    bool clippedWithLocalScrolling = obj.hasOverflowClip() && bgLayer.attachment() == LocalBackgroundAttachment;
    bool isBorderFill = bgLayer.clip() == BorderFillBox;
    bool isBottomLayer = !bgLayer.next();

    Color bgColor = color;
    StyleImage* bgImage = bgLayer.image();

    bool forceBackgroundToWhite = shouldForceWhiteBackgroundForPrintEconomy(obj.styleRef(), obj.document());

    // When printing backgrounds is disabled or using economy mode,
    // change existing background colors and images to a solid white background.
    // If there's no bg color or image, leave it untouched to avoid affecting transparency.
    // We don't try to avoid loading the background images, because this style flag is only set
    // when printing, and at that point we've already loaded the background images anyway. (To avoid
    // loading the background images we'd have to do this check when applying styles rather than
    // while layout.)
    if (forceBackgroundToWhite) {
        // Note that we can't reuse this variable below because the bgColor might be changed
        bool shouldPaintBackgroundColor = isBottomLayer && bgColor.alpha();
        if (bgImage || shouldPaintBackgroundColor) {
            bgColor = Color::white;
            bgImage = nullptr;
        }
    }

    // Fast path for drawing simple color backgrounds.
    if (!clippedWithLocalScrolling && !bgImage && isBorderFill && isBottomLayer) {
        if (!bgColor.alpha())
            return;

        bool boxShadowShouldBeAppliedToBackground = obj.boxShadowShouldBeAppliedToBackground(bleedAvoidance, box);
        GraphicsContextStateSaver shadowStateSaver(context, boxShadowShouldBeAppliedToBackground);
        if (boxShadowShouldBeAppliedToBackground)
            BoxPainter::applyBoxShadowForBackground(context, obj);

        if (hasRoundedBorder && !bleedAvoidanceIsClipping(bleedAvoidance)) {
            FloatRoundedRect border = backgroundRoundedRectAdjustedForBleedAvoidance(obj, rect,
                bleedAvoidance, box, boxSize, includeLeftEdge, includeRightEdge);

            if (border.isRenderable()) {
                context.fillRoundedRect(border, bgColor);
            } else {
                RoundedInnerRectClipper clipper(obj, paintInfo, rect, border, ApplyToContext);
                context.fillRect(border.rect(), bgColor);
            }
        } else {
            context.fillRect(pixelSnappedIntRect(rect), bgColor);
        }

        return;
    }

    // BorderFillBox radius clipping is taken care of by BackgroundBleedClip{Only,Layer}
    bool clipToBorderRadius = hasRoundedBorder && !(isBorderFill && bleedAvoidanceIsClipping(bleedAvoidance));
    Optional<RoundedInnerRectClipper> clipToBorder;
    if (clipToBorderRadius) {
        FloatRoundedRect border = isBorderFill
            ? backgroundRoundedRectAdjustedForBleedAvoidance(obj, rect, bleedAvoidance, box, boxSize, includeLeftEdge, includeRightEdge)
            : getBackgroundRoundedRect(obj, rect, box, boxSize.width(), boxSize.height(), includeLeftEdge, includeRightEdge);

        // Clip to the padding or content boxes as necessary.
        if (bgLayer.clip() == ContentFillBox) {
            border = obj.style()->getRoundedInnerBorderFor(LayoutRect(border.rect()),
                LayoutRectOutsets(
                    -(obj.paddingTop() + obj.borderTop()),
                    -(obj.paddingRight() + obj.borderRight()),
                    -(obj.paddingBottom() + obj.borderBottom()),
                    -(obj.paddingLeft() + obj.borderLeft())),
                includeLeftEdge, includeRightEdge);
        } else if (bgLayer.clip() == PaddingFillBox) {
            border = obj.style()->getRoundedInnerBorderFor(LayoutRect(border.rect()), includeLeftEdge, includeRightEdge);
        }

        clipToBorder.emplace(obj, paintInfo, rect, border, ApplyToContext);
    }

    int bLeft = includeLeftEdge ? obj.borderLeft() : 0;
    int bRight = includeRightEdge ? obj.borderRight() : 0;
    LayoutUnit pLeft = includeLeftEdge ? obj.paddingLeft() : LayoutUnit();
    LayoutUnit pRight = includeRightEdge ? obj.paddingRight() : LayoutUnit();

    GraphicsContextStateSaver clipWithScrollingStateSaver(context, clippedWithLocalScrolling);
    LayoutRect scrolledPaintRect = rect;
    if (clippedWithLocalScrolling) {
        // Clip to the overflow area.
        const LayoutBox& thisBox = toLayoutBox(obj);
        // TODO(chrishtr): this should be pixel-snapped.
        context.clip(FloatRect(thisBox.overflowClipRect(rect.location())));

        // Adjust the paint rect to reflect a scrolled content box with borders at the ends.
        IntSize offset = thisBox.scrolledContentOffset();
        scrolledPaintRect.move(-offset);
        scrolledPaintRect.setWidth(bLeft + thisBox.scrollWidth() + bRight);
        scrolledPaintRect.setHeight(thisBox.borderTop() + thisBox.scrollHeight() + thisBox.borderBottom());
    }

    GraphicsContextStateSaver backgroundClipStateSaver(context, false);
    IntRect maskRect;

    switch (bgLayer.clip()) {
    case PaddingFillBox:
    case ContentFillBox: {
        if (clipToBorderRadius)
            break;

        // Clip to the padding or content boxes as necessary.
        bool includePadding = bgLayer.clip() == ContentFillBox;
        LayoutRect clipRect(scrolledPaintRect.x() + bLeft + (includePadding ? pLeft : LayoutUnit()),
            scrolledPaintRect.y() + obj.borderTop() + (includePadding ? obj.paddingTop() : LayoutUnit()),
            scrolledPaintRect.width() - bLeft - bRight - (includePadding ? pLeft + pRight : LayoutUnit()),
            scrolledPaintRect.height() - obj.borderTop() - obj.borderBottom() - (includePadding ? obj.paddingTop() + obj.paddingBottom() : LayoutUnit()));
        backgroundClipStateSaver.save();
        // TODO(chrishtr): this should be pixel-snapped.
        context.clip(FloatRect(clipRect));

        break;
    }
    case TextFillBox: {
        // First figure out how big the mask has to be. It should be no bigger than what we need
        // to actually render, so we should intersect the dirty rect with the border box of the background.
        maskRect = pixelSnappedIntRect(rect);

        // We draw the background into a separate layer, to be later masked with yet another layer
        // holding the text content.
        backgroundClipStateSaver.save();
        context.clip(maskRect);
        context.beginLayer();

        break;
    }
    case BorderFillBox:
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    BackgroundImageGeometry geometry;
    if (bgImage)
        geometry.calculate(obj, paintInfo.paintContainer(), paintInfo.globalPaintFlags(), bgLayer, scrolledPaintRect);
    bool shouldPaintBackgroundImage = bgImage && bgImage->canRender();

    // Paint the color first underneath all images, culled if background image occludes it.
    // TODO(trchen): In the !bgLayer.hasRepeatXY() case, we could improve the culling test
    // by verifying whether the background image covers the entire painting area.
    if (isBottomLayer) {
        IntRect backgroundRect(pixelSnappedIntRect(scrolledPaintRect));
        bool boxShadowShouldBeAppliedToBackground = obj.boxShadowShouldBeAppliedToBackground(bleedAvoidance, box);
        bool backgroundImageOccludesBackgroundColor = shouldPaintBackgroundImage && isFillLayerOpaque(bgLayer, obj);
        if (boxShadowShouldBeAppliedToBackground || !backgroundImageOccludesBackgroundColor)  {
            GraphicsContextStateSaver shadowStateSaver(context, boxShadowShouldBeAppliedToBackground);
            if (boxShadowShouldBeAppliedToBackground)
                BoxPainter::applyBoxShadowForBackground(context, obj);

            if (bgColor.alpha())
                context.fillRect(backgroundRect, bgColor);
        }
    }

    // no progressive loading of the background image
    if (shouldPaintBackgroundImage) {
        if (!geometry.destRect().isEmpty()) {
            SkXfermode::Mode bgOp = WebCoreCompositeToSkiaComposite(bgLayer.composite(), bgLayer.blendMode());
            // if op != SkXfermode::kSrcOver_Mode, a mask is being painted.
            SkXfermode::Mode compositeOp = op == SkXfermode::kSrcOver_Mode ? bgOp : op;
            const LayoutObject* clientForBackgroundImage = backgroundObject ? backgroundObject : &obj;
            RefPtr<Image> image = bgImage->image(clientForBackgroundImage, flooredIntSize(geometry.imageContainerSize()), obj.style()->effectiveZoom());
            InterpolationQuality interpolationQuality = chooseInterpolationQuality(*clientForBackgroundImage, image.get(), &bgLayer, LayoutSize(geometry.tileSize()));
            if (bgLayer.maskSourceType() == MaskLuminance)
                context.setColorFilter(ColorFilterLuminanceToAlpha);
            InterpolationQuality previousInterpolationQuality = context.imageInterpolationQuality();
            context.setImageInterpolationQuality(interpolationQuality);
            TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "PaintImage", "data", InspectorPaintImageEvent::data(obj, *bgImage));
            context.drawTiledImage(image.get(), FloatRect(geometry.destRect()), FloatPoint(geometry.phase()), FloatSize(geometry.tileSize()),
                compositeOp, FloatSize(geometry.spaceSize()));
            context.setImageInterpolationQuality(previousInterpolationQuality);
        }
    }

    if (bgLayer.clip() == TextFillBox) {
        // Create the text mask layer.
        context.beginLayer(1, SkXfermode::kDstIn_Mode);

        // Now draw the text into the mask. We do this by painting using a special paint phase that signals to
        // InlineTextBoxes that they should just add their contents to the clip.
        PaintInfo info(context, maskRect, PaintPhaseTextClip, GlobalPaintNormalPhase, 0);
        if (box) {
            const RootInlineBox& root = box->root();
            box->paint(info, LayoutPoint(scrolledPaintRect.x() - box->x(), scrolledPaintRect.y() - box->y()), root.lineTop(), root.lineBottom());
        } else {
            // FIXME: this should only have an effect for the line box list within |obj|. Change this to create a LineBoxListPainter directly.
            LayoutSize localOffset = obj.isBox() ? toLayoutBox(&obj)->locationOffset() : LayoutSize();
            obj.paint(info, scrolledPaintRect.location() - localOffset);
        }

        context.endLayer();
        context.endLayer();
    }
}

void BoxPainter::paintMask(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!paintInfo.shouldPaintWithinRoot(&m_layoutBox) || m_layoutBox.style()->visibility() != VISIBLE || paintInfo.phase != PaintPhaseMask)
        return;

    if (LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(paintInfo.context, m_layoutBox, paintInfo.phase, paintOffset))
        return;

    LayoutRect visualOverflowRect(m_layoutBox.visualOverflowRect());
    visualOverflowRect.moveBy(paintOffset);

    LayoutObjectDrawingRecorder recorder(paintInfo.context, m_layoutBox, paintInfo.phase, visualOverflowRect, paintOffset);
    LayoutRect paintRect = LayoutRect(paintOffset, m_layoutBox.size());
    paintMaskImages(paintInfo, paintRect);
}

void BoxPainter::paintMaskImages(const PaintInfo& paintInfo, const LayoutRect& paintRect)
{
    // Figure out if we need to push a transparency layer to render our mask.
    bool pushTransparencyLayer = false;
    bool compositedMask = m_layoutBox.hasLayer() && m_layoutBox.layer()->hasCompositedMask();
    bool flattenCompositingLayers = paintInfo.globalPaintFlags() & GlobalPaintFlattenCompositingLayers;

    bool allMaskImagesLoaded = true;

    if (!compositedMask || flattenCompositingLayers) {
        pushTransparencyLayer = true;
        StyleImage* maskBoxImage = m_layoutBox.style()->maskBoxImage().image();
        const FillLayer& maskLayers = m_layoutBox.style()->maskLayers();

        // Don't render a masked element until all the mask images have loaded, to prevent a flash of unmasked content.
        if (maskBoxImage)
            allMaskImagesLoaded &= maskBoxImage->isLoaded();

        allMaskImagesLoaded &= maskLayers.imagesAreLoaded();

        paintInfo.context.beginLayer(1, SkXfermode::kDstIn_Mode);
    }

    if (allMaskImagesLoaded) {
        paintFillLayers(paintInfo, Color::transparent, m_layoutBox.style()->maskLayers(), paintRect);
        paintNinePieceImage(m_layoutBox, paintInfo.context, paintRect, m_layoutBox.styleRef(), m_layoutBox.style()->maskBoxImage());
    }

    if (pushTransparencyLayer)
        paintInfo.context.endLayer();
}

void BoxPainter::paintClippingMask(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT(paintInfo.phase == PaintPhaseClippingMask);

    if (!paintInfo.shouldPaintWithinRoot(&m_layoutBox) || m_layoutBox.style()->visibility() != VISIBLE)
        return;

    if (!m_layoutBox.layer() || m_layoutBox.layer()->compositingState() != PaintsIntoOwnBacking)
        return;

    if (LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(paintInfo.context, m_layoutBox, paintInfo.phase, paintOffset))
        return;

    IntRect paintRect = pixelSnappedIntRect(LayoutRect(paintOffset, m_layoutBox.size()));
    LayoutObjectDrawingRecorder drawingRecorder(paintInfo.context, m_layoutBox, paintInfo.phase, paintRect, paintOffset);
    paintInfo.context.fillRect(paintRect, Color::black);
}

InterpolationQuality BoxPainter::chooseInterpolationQuality(const LayoutObject& obj, Image* image, const void* layer, const LayoutSize& size)
{
    return ImageQualityController::imageQualityController()->chooseInterpolationQuality(obj, image, layer, size);
}

bool BoxPainter::paintNinePieceImage(const LayoutBoxModelObject& obj, GraphicsContext& graphicsContext, const LayoutRect& rect, const ComputedStyle& style, const NinePieceImage& ninePieceImage, SkXfermode::Mode op)
{
    NinePieceImagePainter ninePieceImagePainter(obj);
    return ninePieceImagePainter.paint(graphicsContext, rect, style, ninePieceImage, op);
}

void BoxPainter::paintBorder(const LayoutBoxModelObject& obj, const PaintInfo& info,
    const LayoutRect& rect, const ComputedStyle& style, BackgroundBleedAvoidance bleedAvoidance,
    bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
{
    // border-image is not affected by border-radius.
    if (paintNinePieceImage(obj, info.context, rect, style, style.borderImage()))
        return;

    const BoxBorderPainter borderPainter(rect, style, bleedAvoidance,
        includeLogicalLeftEdge, includeLogicalRightEdge);
    borderPainter.paintBorder(info, rect);
}

void BoxPainter::paintBoxShadow(const PaintInfo& info, const LayoutRect& paintRect, const ComputedStyle& style, ShadowStyle shadowStyle, bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
{
    // FIXME: Deal with border-image. Would be great to use border-image as a mask.
    GraphicsContext& context = info.context;
    if (!style.boxShadow())
        return;
    FloatRoundedRect border = (shadowStyle == Inset) ? style.getRoundedInnerBorderFor(paintRect, includeLogicalLeftEdge, includeLogicalRightEdge)
        : style.getRoundedBorderFor(paintRect, includeLogicalLeftEdge, includeLogicalRightEdge);

    bool hasBorderRadius = style.hasBorderRadius();
    bool isHorizontal = style.isHorizontalWritingMode();
    bool hasOpaqueBackground = style.visitedDependentColor(CSSPropertyBackgroundColor).alpha() == 255;

    GraphicsContextStateSaver stateSaver(context, false);

    const ShadowList* shadowList = style.boxShadow();
    for (size_t i = shadowList->shadows().size(); i--; ) {
        const ShadowData& shadow = shadowList->shadows()[i];
        if (shadow.style() != shadowStyle)
            continue;

        FloatSize shadowOffset(shadow.x(), shadow.y());
        float shadowBlur = shadow.blur();
        float shadowSpread = shadow.spread();

        if (shadowOffset.isZero() && !shadowBlur && !shadowSpread)
            continue;

        const Color& shadowColor = shadow.color().resolve(style.visitedDependentColor(CSSPropertyColor));

        if (shadow.style() == Normal) {
            FloatRect fillRect = border.rect();
            fillRect.inflate(shadowSpread);
            if (fillRect.isEmpty())
                continue;

            FloatRect shadowRect(border.rect());
            shadowRect.inflate(shadowBlur + shadowSpread);
            shadowRect.move(shadowOffset);

            // Save the state and clip, if not already done.
            // The clip does not depend on any shadow-specific properties.
            if (!stateSaver.saved()) {
                stateSaver.save();
                if (hasBorderRadius) {
                    FloatRoundedRect rectToClipOut = border;

                    // If the box is opaque, it is unnecessary to clip it out. However, doing so saves time
                    // when painting the shadow. On the other hand, it introduces subpixel gaps along the
                    // corners. Those are avoided by insetting the clipping path by one CSS pixel.
                    if (hasOpaqueBackground)
                        rectToClipOut.inflateWithRadii(-1);

                    if (!rectToClipOut.isEmpty())
                        context.clipOutRoundedRect(rectToClipOut);
                } else {
                    // This IntRect is correct even with fractional shadows, because it is used for the rectangle
                    // of the box itself, which is always pixel-aligned.
                    FloatRect rectToClipOut = border.rect();

                    // If the box is opaque, it is unnecessary to clip it out. However, doing so saves time
                    // when painting the shadow. On the other hand, it introduces subpixel gaps along the
                    // edges if they are not pixel-aligned. Those are avoided by insetting the clipping path
                    // by one CSS pixel.
                    if (hasOpaqueBackground)
                        rectToClipOut.inflate(-1);

                    if (!rectToClipOut.isEmpty())
                        context.clipOut(rectToClipOut);
                }
            }

            // Draw only the shadow.
            context.setShadow(shadowOffset, shadowBlur, shadowColor, DrawLooperBuilder::ShadowRespectsTransforms, DrawLooperBuilder::ShadowIgnoresAlpha, DrawShadowOnly);

            if (hasBorderRadius) {
                FloatRoundedRect influenceRect(pixelSnappedIntRect(LayoutRect(shadowRect)), border.radii());
                float changeAmount = 2 * shadowBlur + shadowSpread;
                if (changeAmount >= 0)
                    influenceRect.expandRadii(changeAmount);
                else
                    influenceRect.shrinkRadii(-changeAmount);

                FloatRoundedRect roundedFillRect = border;
                roundedFillRect.inflate(shadowSpread);

                if (shadowSpread >= 0)
                    roundedFillRect.expandRadii(shadowSpread);
                else
                    roundedFillRect.shrinkRadii(-shadowSpread);
                if (!roundedFillRect.isRenderable())
                    roundedFillRect.adjustRadii();
                roundedFillRect.constrainRadii();
                context.fillRoundedRect(roundedFillRect, Color::black);
            } else {
                context.fillRect(fillRect, Color::black);
            }
        } else {
            // The inset shadow case.
            GraphicsContext::Edges clippedEdges = GraphicsContext::NoEdge;
            if (!includeLogicalLeftEdge) {
                if (isHorizontal)
                    clippedEdges |= GraphicsContext::LeftEdge;
                else
                    clippedEdges |= GraphicsContext::TopEdge;
            }
            if (!includeLogicalRightEdge) {
                if (isHorizontal)
                    clippedEdges |= GraphicsContext::RightEdge;
                else
                    clippedEdges |= GraphicsContext::BottomEdge;
            }
            context.drawInnerShadow(border, shadowColor, shadowOffset, shadowBlur, shadowSpread, clippedEdges);
        }
    }
}

bool BoxPainter::shouldForceWhiteBackgroundForPrintEconomy(const ComputedStyle& style, const Document& document)
{
    return document.printing() && style.printColorAdjust() == PrintColorAdjustEconomy
        && (!document.settings() || !document.settings()->shouldPrintBackgrounds());
}

} // namespace blink
