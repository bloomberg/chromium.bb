// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/BoxPainter.h"

#include "core/HTMLNames.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/paint/BackgroundImageGeometry.h"
#include "core/paint/BoxDecorationData.h"
#include "core/rendering/ImageQualityController.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderBox.h"
#include "core/rendering/RenderBoxModelObject.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderObject.h"
#include "core/rendering/RenderTable.h"
#include "core/rendering/RenderTheme.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/compositing/CompositedLayerMapping.h"
#include "core/rendering/style/BorderEdge.h"
#include "core/rendering/style/ShadowList.h"
#include "platform/LengthFunctions.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/graphics/GraphicsContextStateSaver.h"

namespace blink {

void BoxPainter::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    LayoutPoint adjustedPaintOffset = paintOffset + m_renderBox.location();
    // default implementation. Just pass paint through to the children
    PaintInfo childInfo(paintInfo);
    childInfo.updatePaintingRootForChildren(&m_renderBox);
    for (RenderObject* child = m_renderBox.slowFirstChild(); child; child = child->nextSibling())
        child->paint(childInfo, adjustedPaintOffset);
}

void BoxPainter::paintBoxDecorationBackground(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!paintInfo.shouldPaintWithinRoot(&m_renderBox))
        return;

    LayoutRect paintRect = m_renderBox.borderBoxRect();
    paintRect.moveBy(paintOffset);
    paintBoxDecorationBackgroundWithRect(paintInfo, paintOffset, paintRect);
}

void BoxPainter::paintBoxDecorationBackgroundWithRect(PaintInfo& paintInfo, const LayoutPoint& paintOffset, const LayoutRect& paintRect)
{
    RenderStyle* style = m_renderBox.style();
    BoxDecorationData boxDecorationData(*style, m_renderBox.canRenderBorderImage(), m_renderBox.backgroundHasOpaqueTopLayer(), paintInfo.context);

    // FIXME: Should eventually give the theme control over whether the box shadow should paint, since controls could have
    // custom shadows of their own.
    if (!m_renderBox.boxShadowShouldBeAppliedToBackground(boxDecorationData.bleedAvoidance()))
        paintBoxShadow(paintInfo, paintRect, style, Normal);

    GraphicsContextStateSaver stateSaver(*paintInfo.context, false);
    if (boxDecorationData.bleedAvoidance() == BackgroundBleedClipBackground) {
        stateSaver.save();
        RoundedRect border = style->getRoundedBorderFor(paintRect);
        paintInfo.context->clipRoundedRect(border);
    }

    // If we have a native theme appearance, paint that before painting our background.
    // The theme will tell us whether or not we should also paint the CSS background.
    IntRect snappedPaintRect(pixelSnappedIntRect(paintRect));
    bool themePainted = boxDecorationData.hasAppearance && !RenderTheme::theme().paint(&m_renderBox, paintInfo, snappedPaintRect);
    if (!themePainted) {
        if (boxDecorationData.bleedAvoidance() == BackgroundBleedBackgroundOverBorder)
            paintBorder(m_renderBox, paintInfo, paintRect, style, boxDecorationData.bleedAvoidance());

        paintBackground(paintInfo, paintRect, boxDecorationData.backgroundColor, boxDecorationData.bleedAvoidance());

        if (boxDecorationData.hasAppearance)
            RenderTheme::theme().paintDecorations(&m_renderBox, paintInfo, snappedPaintRect);
    }
    paintBoxShadow(paintInfo, paintRect, style, Inset);

    // The theme will tell us whether or not we should also paint the CSS border.
    if (boxDecorationData.hasBorder && boxDecorationData.bleedAvoidance() != BackgroundBleedBackgroundOverBorder
        && (!boxDecorationData.hasAppearance || (!themePainted && RenderTheme::theme().paintBorderOnly(&m_renderBox, paintInfo, snappedPaintRect)))
        && !(m_renderBox.isTable() && toRenderTable(&m_renderBox)->collapseBorders()))
        paintBorder(m_renderBox, paintInfo, paintRect, style, boxDecorationData.bleedAvoidance());
}

static bool skipBodyBackground(const RenderBox* bodyElementRenderer)
{
    ASSERT(bodyElementRenderer->isBody());
    // The <body> only paints its background if the root element has defined a background independent of the body,
    // or if the <body>'s parent is not the document element's renderer (e.g. inside SVG foreignObject).
    RenderObject* documentElementRenderer = bodyElementRenderer->document().documentElement()->renderer();
    return documentElementRenderer
        && !documentElementRenderer->hasBackground()
        && (documentElementRenderer == bodyElementRenderer->parent());
}

void BoxPainter::paintBackground(const PaintInfo& paintInfo, const LayoutRect& paintRect, const Color& backgroundColor, BackgroundBleedAvoidance bleedAvoidance)
{
    if (m_renderBox.isDocumentElement()) {
        paintRootBoxFillLayers(paintInfo);
        return;
    }
    if (m_renderBox.isBody() && skipBodyBackground(&m_renderBox))
        return;
    if (m_renderBox.boxDecorationBackgroundIsKnownToBeObscured())
        return;
    paintFillLayers(paintInfo, backgroundColor, m_renderBox.style()->backgroundLayers(), paintRect, bleedAvoidance);
}

void BoxPainter::paintRootBoxFillLayers(const PaintInfo& paintInfo)
{
    if (paintInfo.skipRootBackground())
        return;

    RenderObject* rootBackgroundRenderer = m_renderBox.rendererForRootBackground();

    const FillLayer& bgLayer = rootBackgroundRenderer->style()->backgroundLayers();
    Color bgColor = rootBackgroundRenderer->resolveColor(CSSPropertyBackgroundColor);

    paintFillLayers(paintInfo, bgColor, bgLayer, m_renderBox.view()->backgroundRect(&m_renderBox), BackgroundBleedNone, CompositeSourceOver, rootBackgroundRenderer);
}

void BoxPainter::paintFillLayers(const PaintInfo& paintInfo, const Color& c, const FillLayer& fillLayer, const LayoutRect& rect,
    BackgroundBleedAvoidance bleedAvoidance, CompositeOperator op, RenderObject* backgroundObject)
{
    Vector<const FillLayer*, 8> layers;
    const FillLayer* curLayer = &fillLayer;
    bool shouldDrawBackgroundInSeparateBuffer = false;
    bool isBottomLayerOccluded = false;
    while (curLayer) {
        layers.append(curLayer);
        // Stop traversal when an opaque layer is encountered.
        // FIXME : It would be possible for the following occlusion culling test to be more aggressive
        // on layers with no repeat by testing whether the image covers the layout rect.
        // Testing that here would imply duplicating a lot of calculations that are currently done in
        // RenderBoxModelObject::paintFillLayerExtended. A more efficient solution might be to move
        // the layer recursion into paintFillLayerExtended, or to compute the layer geometry here
        // and pass it down.

        if (!shouldDrawBackgroundInSeparateBuffer && curLayer->blendMode() != WebBlendModeNormal)
            shouldDrawBackgroundInSeparateBuffer = true;

        // The clipOccludesNextLayers condition must be evaluated first to avoid short-circuiting.
        if (curLayer->clipOccludesNextLayers(curLayer == &fillLayer)
            && curLayer->hasOpaqueImage(&m_renderBox)
            && curLayer->image()->canRender(m_renderBox, m_renderBox.style()->effectiveZoom())
            && curLayer->hasRepeatXY()
            && curLayer->blendMode() == WebBlendModeNormal
            && !m_renderBox.boxShadowShouldBeAppliedToBackground(bleedAvoidance))
            break;
        curLayer = curLayer->next();
    }

    if (layers.size() > 0  && (**layers.rbegin()).next())
        isBottomLayerOccluded = true;

    GraphicsContext* context = paintInfo.context;
    if (!context)
        shouldDrawBackgroundInSeparateBuffer = false;

    bool skipBaseColor = false;
    if (shouldDrawBackgroundInSeparateBuffer) {
        bool isBaseColorVisible = !isBottomLayerOccluded && c.hasAlpha();

        // Paint the document's base background color outside the transparency layer,
        // so that the background images don't blend with this color: http://crbug.com/389039.
        if (isBaseColorVisible && isDocumentElementWithOpaqueBackground(m_renderBox)) {
            paintRootBackgroundColor(m_renderBox, paintInfo, rect, Color());
            skipBaseColor = true;
        }
        context->beginTransparencyLayer(1);
    }

    Vector<const FillLayer*>::const_reverse_iterator topLayer = layers.rend();
    for (Vector<const FillLayer*>::const_reverse_iterator it = layers.rbegin(); it != topLayer; ++it)
        paintFillLayer(paintInfo, c, **it, rect, bleedAvoidance, op, backgroundObject, skipBaseColor);

    if (shouldDrawBackgroundInSeparateBuffer)
        context->endLayer();
}

void BoxPainter::paintFillLayer(const PaintInfo& paintInfo, const Color& c, const FillLayer& fillLayer, const LayoutRect& rect,
    BackgroundBleedAvoidance bleedAvoidance, CompositeOperator op, RenderObject* backgroundObject, bool skipBaseColor)
{
    BoxPainter::paintFillLayerExtended(m_renderBox, paintInfo, c, fillLayer, rect, bleedAvoidance, 0, LayoutSize(), op, backgroundObject, skipBaseColor);
}

void BoxPainter::applyBoxShadowForBackground(GraphicsContext* context, RenderObject& obj)
{
    const ShadowList* shadowList = obj.style()->boxShadow();
    ASSERT(shadowList);
    for (size_t i = shadowList->shadows().size(); i--; ) {
        const ShadowData& boxShadow = shadowList->shadows()[i];
        if (boxShadow.style() != Normal)
            continue;
        FloatSize shadowOffset(boxShadow.x(), boxShadow.y());
        context->setShadow(shadowOffset, boxShadow.blur(), boxShadow.color(),
            DrawLooperBuilder::ShadowRespectsTransforms, DrawLooperBuilder::ShadowIgnoresAlpha);
        return;
    }
}

// FIXME: See crbug.com/382491. The use of getCTM in this context is incorrect because the matrix returned does not
// include scales applied at raster time, such as the device zoom.
static LayoutRect shrinkRectByOnePixel(GraphicsContext* context, const LayoutRect& rect)
{
    LayoutRect shrunkRect = rect;
    AffineTransform transform = context->getCTM();
    shrunkRect.inflateX(-static_cast<LayoutUnit>(ceil(1 / transform.xScale())));
    shrunkRect.inflateY(-static_cast<LayoutUnit>(ceil(1 / transform.yScale())));
    return shrunkRect;
}

RoundedRect BoxPainter::getBackgroundRoundedRect(RenderObject& obj, const LayoutRect& borderRect, InlineFlowBox* box, LayoutUnit inlineBoxWidth, LayoutUnit inlineBoxHeight,
    bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
{
    RoundedRect border = obj.style()->getRoundedBorderFor(borderRect, includeLogicalLeftEdge, includeLogicalRightEdge);
    if (box && (box->nextLineBox() || box->prevLineBox())) {
        RoundedRect segmentBorder = obj.style()->getRoundedBorderFor(LayoutRect(0, 0, inlineBoxWidth, inlineBoxHeight), includeLogicalLeftEdge, includeLogicalRightEdge);
        border.setRadii(segmentBorder.radii());
    }

    return border;
}

RoundedRect BoxPainter::backgroundRoundedRectAdjustedForBleedAvoidance(RenderObject& obj, GraphicsContext* context, const LayoutRect& borderRect, BackgroundBleedAvoidance bleedAvoidance, InlineFlowBox* box, const LayoutSize& boxSize, bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
{
    if (bleedAvoidance == BackgroundBleedShrinkBackground) {
        // We shrink the rectangle by one pixel on each side because the bleed is one pixel maximum.
        return BoxPainter::getBackgroundRoundedRect(obj, shrinkRectByOnePixel(context, borderRect), box, boxSize.width(), boxSize.height(), includeLogicalLeftEdge, includeLogicalRightEdge);
    }
    if (bleedAvoidance == BackgroundBleedBackgroundOverBorder)
        return obj.style()->getRoundedInnerBorderFor(borderRect, includeLogicalLeftEdge, includeLogicalRightEdge);

    return BoxPainter::getBackgroundRoundedRect(obj, borderRect, box, boxSize.width(), boxSize.height(), includeLogicalLeftEdge, includeLogicalRightEdge);
}

void BoxPainter::clipRoundedInnerRect(GraphicsContext * context, const LayoutRect& rect, const RoundedRect& clipRect)
{
    if (clipRect.isRenderable()) {
        context->clipRoundedRect(clipRect);
    } else {
        // We create a rounded rect for each of the corners and clip it, while making sure we clip opposing corners together.
        if (!clipRect.radii().topLeft().isEmpty() || !clipRect.radii().bottomRight().isEmpty()) {
            IntRect topCorner(clipRect.rect().x(), clipRect.rect().y(), rect.maxX() - clipRect.rect().x(), rect.maxY() - clipRect.rect().y());
            RoundedRect::Radii topCornerRadii;
            topCornerRadii.setTopLeft(clipRect.radii().topLeft());
            context->clipRoundedRect(RoundedRect(topCorner, topCornerRadii));

            IntRect bottomCorner(rect.x(), rect.y(), clipRect.rect().maxX() - rect.x(), clipRect.rect().maxY() - rect.y());
            RoundedRect::Radii bottomCornerRadii;
            bottomCornerRadii.setBottomRight(clipRect.radii().bottomRight());
            context->clipRoundedRect(RoundedRect(bottomCorner, bottomCornerRadii));
        }

        if (!clipRect.radii().topRight().isEmpty() || !clipRect.radii().bottomLeft().isEmpty()) {
            IntRect topCorner(rect.x(), clipRect.rect().y(), clipRect.rect().maxX() - rect.x(), rect.maxY() - clipRect.rect().y());
            RoundedRect::Radii topCornerRadii;
            topCornerRadii.setTopRight(clipRect.radii().topRight());
            context->clipRoundedRect(RoundedRect(topCorner, topCornerRadii));

            IntRect bottomCorner(clipRect.rect().x(), rect.y(), rect.maxX() - clipRect.rect().x(), clipRect.rect().maxY() - rect.y());
            RoundedRect::Radii bottomCornerRadii;
            bottomCornerRadii.setBottomLeft(clipRect.radii().bottomLeft());
            context->clipRoundedRect(RoundedRect(bottomCorner, bottomCornerRadii));
        }
    }
}

void BoxPainter::paintFillLayerExtended(RenderBoxModelObject& obj, const PaintInfo& paintInfo, const Color& color, const FillLayer& bgLayer, const LayoutRect& rect,
    BackgroundBleedAvoidance bleedAvoidance, InlineFlowBox* box, const LayoutSize& boxSize, CompositeOperator op, RenderObject* backgroundObject, bool skipBaseColor)
{
    GraphicsContext* context = paintInfo.context;
    if (rect.isEmpty())
        return;

    bool includeLeftEdge = box ? box->includeLogicalLeftEdge() : true;
    bool includeRightEdge = box ? box->includeLogicalRightEdge() : true;

    bool hasRoundedBorder = obj.style()->hasBorderRadius() && (includeLeftEdge || includeRightEdge);
    bool clippedWithLocalScrolling = obj.hasOverflowClip() && bgLayer.attachment() == LocalBackgroundAttachment;
    bool isBorderFill = bgLayer.clip() == BorderFillBox;
    bool isDocumentElementRenderer = obj.isDocumentElement();
    bool isBottomLayer = !bgLayer.next();

    Color bgColor = color;
    StyleImage* bgImage = bgLayer.image();
    bool shouldPaintBackgroundImage = bgImage && bgImage->canRender(obj, obj.style()->effectiveZoom());

    bool forceBackgroundToWhite = false;
    if (obj.document().printing()) {
        if (obj.style()->printColorAdjust() == PrintColorAdjustEconomy)
            forceBackgroundToWhite = true;
        if (obj.document().settings() && obj.document().settings()->shouldPrintBackgrounds())
            forceBackgroundToWhite = false;
    }

    // When printing backgrounds is disabled or using economy mode,
    // change existing background colors and images to a solid white background.
    // If there's no bg color or image, leave it untouched to avoid affecting transparency.
    // We don't try to avoid loading the background images, because this style flag is only set
    // when printing, and at that point we've already loaded the background images anyway. (To avoid
    // loading the background images we'd have to do this check when applying styles rather than
    // while rendering.)
    if (forceBackgroundToWhite) {
        // Note that we can't reuse this variable below because the bgColor might be changed
        bool shouldPaintBackgroundColor = isBottomLayer && bgColor.alpha();
        if (shouldPaintBackgroundImage || shouldPaintBackgroundColor) {
            bgColor = Color::white;
            shouldPaintBackgroundImage = false;
        }
    }

    bool colorVisible = bgColor.alpha();

    // Fast path for drawing simple color backgrounds.
    if (!isDocumentElementRenderer && !clippedWithLocalScrolling && !shouldPaintBackgroundImage && isBorderFill && isBottomLayer) {
        if (!colorVisible)
            return;

        bool boxShadowShouldBeAppliedToBackground = obj.boxShadowShouldBeAppliedToBackground(bleedAvoidance, box);
        GraphicsContextStateSaver shadowStateSaver(*context, boxShadowShouldBeAppliedToBackground);
        if (boxShadowShouldBeAppliedToBackground)
            BoxPainter::applyBoxShadowForBackground(context, obj);

        if (hasRoundedBorder && bleedAvoidance != BackgroundBleedClipBackground) {
            RoundedRect border = backgroundRoundedRectAdjustedForBleedAvoidance(obj, context, rect, bleedAvoidance, box, boxSize, includeLeftEdge, includeRightEdge);
            if (border.isRenderable()) {
                context->fillRoundedRect(border, bgColor);
            } else {
                context->save();
                clipRoundedInnerRect(context, rect, border);
                context->fillRect(border.rect(), bgColor);
                context->restore();
            }
        } else {
            context->fillRect(pixelSnappedIntRect(rect), bgColor);
        }

        return;
    }

    // BorderFillBox radius clipping is taken care of by BackgroundBleedClipBackground
    bool clipToBorderRadius = hasRoundedBorder && !(isBorderFill && bleedAvoidance == BackgroundBleedClipBackground);
    GraphicsContextStateSaver clipToBorderStateSaver(*context, clipToBorderRadius);
    if (clipToBorderRadius) {
        RoundedRect border = isBorderFill ? backgroundRoundedRectAdjustedForBleedAvoidance(obj, context, rect, bleedAvoidance, box, boxSize, includeLeftEdge, includeRightEdge) : getBackgroundRoundedRect(obj, rect, box, boxSize.width(), boxSize.height(), includeLeftEdge, includeRightEdge);

        // Clip to the padding or content boxes as necessary.
        if (bgLayer.clip() == ContentFillBox) {
            border = obj.style()->getRoundedInnerBorderFor(border.rect(),
                obj.paddingTop() + obj.borderTop(), obj.paddingBottom() + obj.borderBottom(),
                obj.paddingLeft() + obj.borderLeft(), obj.paddingRight() + obj.borderRight(), includeLeftEdge, includeRightEdge);
        } else if (bgLayer.clip() == PaddingFillBox) {
            border = obj.style()->getRoundedInnerBorderFor(border.rect(), includeLeftEdge, includeRightEdge);
        }

        clipRoundedInnerRect(context, rect, border);
    }

    int bLeft = includeLeftEdge ? obj.borderLeft() : 0;
    int bRight = includeRightEdge ? obj.borderRight() : 0;
    LayoutUnit pLeft = includeLeftEdge ? obj.paddingLeft() : LayoutUnit();
    LayoutUnit pRight = includeRightEdge ? obj.paddingRight() : LayoutUnit();

    GraphicsContextStateSaver clipWithScrollingStateSaver(*context, clippedWithLocalScrolling);
    LayoutRect scrolledPaintRect = rect;
    if (clippedWithLocalScrolling) {
        // Clip to the overflow area.
        RenderBox* thisBox = toRenderBox(&obj);
        context->clip(thisBox->overflowClipRect(rect.location()));

        // Adjust the paint rect to reflect a scrolled content box with borders at the ends.
        IntSize offset = thisBox->scrolledContentOffset();
        scrolledPaintRect.move(-offset);
        scrolledPaintRect.setWidth(bLeft + thisBox->scrollWidth() + bRight);
        scrolledPaintRect.setHeight(thisBox->borderTop() + thisBox->scrollHeight() + thisBox->borderBottom());
    }

    GraphicsContextStateSaver backgroundClipStateSaver(*context, false);
    IntRect maskRect;

    switch (bgLayer.clip()) {
    case PaddingFillBox:
    case ContentFillBox: {
        if (clipToBorderRadius)
            break;

        // Clip to the padding or content boxes as necessary.
        bool includePadding = bgLayer.clip() == ContentFillBox;
        LayoutRect clipRect = LayoutRect(scrolledPaintRect.x() + bLeft + (includePadding ? pLeft : LayoutUnit()),
            scrolledPaintRect.y() + obj.borderTop() + (includePadding ? obj.paddingTop() : LayoutUnit()),
            scrolledPaintRect.width() - bLeft - bRight - (includePadding ? pLeft + pRight : LayoutUnit()),
            scrolledPaintRect.height() - obj.borderTop() - obj.borderBottom() - (includePadding ? obj.paddingTop() + obj.paddingBottom() : LayoutUnit()));
        backgroundClipStateSaver.save();
        context->clip(clipRect);

        break;
    }
    case TextFillBox: {
        // First figure out how big the mask has to be. It should be no bigger than what we need
        // to actually render, so we should intersect the dirty rect with the border box of the background.
        maskRect = pixelSnappedIntRect(rect);
        maskRect.intersect(paintInfo.rect);

        // We draw the background into a separate layer, to be later masked with yet another layer
        // holding the text content.
        backgroundClipStateSaver.save();
        context->clip(maskRect);
        context->beginTransparencyLayer(1);

        break;
    }
    case BorderFillBox:
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    // Paint the color first underneath all images, culled if background image occludes it.
    // FIXME: In the bgLayer->hasFiniteBounds() case, we could improve the culling test
    // by verifying whether the background image covers the entire layout rect.
    if (isBottomLayer) {
        IntRect backgroundRect(pixelSnappedIntRect(scrolledPaintRect));
        bool boxShadowShouldBeAppliedToBackground = obj.boxShadowShouldBeAppliedToBackground(bleedAvoidance, box);
        bool isOpaqueRoot = (isDocumentElementRenderer && !bgColor.hasAlpha()) || isDocumentElementWithOpaqueBackground(obj);
        if (boxShadowShouldBeAppliedToBackground || !shouldPaintBackgroundImage || !bgLayer.hasOpaqueImage(&obj) || !bgLayer.hasRepeatXY() || (isOpaqueRoot && !toRenderBox(&obj)->height()))  {
            if (!boxShadowShouldBeAppliedToBackground)
                backgroundRect.intersect(paintInfo.rect);

            GraphicsContextStateSaver shadowStateSaver(*context, boxShadowShouldBeAppliedToBackground);
            if (boxShadowShouldBeAppliedToBackground)
                BoxPainter::applyBoxShadowForBackground(context, obj);

            if (isOpaqueRoot && !skipBaseColor) {
                paintRootBackgroundColor(obj, paintInfo, rect, bgColor);
            } else if (bgColor.alpha()) {
                context->fillRect(backgroundRect, bgColor, context->compositeOperation());
            }
        }
    }

    // no progressive loading of the background image
    if (shouldPaintBackgroundImage) {
        BackgroundImageGeometry geometry;
        calculateBackgroundImageGeometry(obj, paintInfo.paintContainer(), bgLayer, scrolledPaintRect, geometry, backgroundObject);
        geometry.clip(paintInfo.rect);
        if (!geometry.destRect().isEmpty()) {
            CompositeOperator compositeOp = op == CompositeSourceOver ? bgLayer.composite() : op;
            RenderObject* clientForBackgroundImage = backgroundObject ? backgroundObject : &obj;
            RefPtr<Image> image = bgImage->image(clientForBackgroundImage, geometry.tileSize());
            InterpolationQuality interpolationQuality = chooseInterpolationQuality(obj, context, image.get(), &bgLayer, geometry.tileSize());
            if (bgLayer.maskSourceType() == MaskLuminance)
                context->setColorFilter(ColorFilterLuminanceToAlpha);
            InterpolationQuality previousInterpolationQuality = context->imageInterpolationQuality();
            context->setImageInterpolationQuality(interpolationQuality);
            context->drawTiledImage(image.get(), geometry.destRect(), geometry.relativePhase(), geometry.tileSize(),
                compositeOp, bgLayer.blendMode(), geometry.spaceSize());
            context->setImageInterpolationQuality(previousInterpolationQuality);
        }
    }

    if (bgLayer.clip() == TextFillBox) {
        // Create the text mask layer.
        context->setCompositeOperation(CompositeDestinationIn);
        context->beginTransparencyLayer(1);

        // FIXME: Workaround for https://code.google.com/p/skia/issues/detail?id=1291.
        context->clearRect(maskRect);

        // Now draw the text into the mask. We do this by painting using a special paint phase that signals to
        // InlineTextBoxes that they should just add their contents to the clip.
        PaintInfo info(context, maskRect, PaintPhaseTextClip, PaintBehaviorForceBlackText, 0);
        context->setCompositeOperation(CompositeSourceOver);
        if (box) {
            RootInlineBox& root = box->root();
            box->paint(info, LayoutPoint(scrolledPaintRect.x() - box->x(), scrolledPaintRect.y() - box->y()), root.lineTop(), root.lineBottom());
        } else {
            LayoutSize localOffset = obj.isBox() ? toRenderBox(&obj)->locationOffset() : LayoutSize();
            obj.paint(info, scrolledPaintRect.location() - localOffset);
        }

        context->endLayer();
        context->endLayer();
    }
}

void BoxPainter::paintMask(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!paintInfo.shouldPaintWithinRoot(&m_renderBox) || m_renderBox.style()->visibility() != VISIBLE || paintInfo.phase != PaintPhaseMask)
        return;

    LayoutRect paintRect = LayoutRect(paintOffset, m_renderBox.size());
    paintMaskImages(paintInfo, paintRect);
}

void BoxPainter::paintMaskImages(const PaintInfo& paintInfo, const LayoutRect& paintRect)
{
    // Figure out if we need to push a transparency layer to render our mask.
    bool pushTransparencyLayer = false;
    bool compositedMask = m_renderBox.hasLayer() && m_renderBox.layer()->hasCompositedMask();
    bool flattenCompositingLayers = m_renderBox.view()->frameView() && m_renderBox.view()->frameView()->paintBehavior() & PaintBehaviorFlattenCompositingLayers;
    CompositeOperator compositeOp = CompositeSourceOver;

    bool allMaskImagesLoaded = true;

    if (!compositedMask || flattenCompositingLayers) {
        pushTransparencyLayer = true;
        StyleImage* maskBoxImage = m_renderBox.style()->maskBoxImage().image();
        const FillLayer& maskLayers = m_renderBox.style()->maskLayers();

        // Don't render a masked element until all the mask images have loaded, to prevent a flash of unmasked content.
        if (maskBoxImage)
            allMaskImagesLoaded &= maskBoxImage->isLoaded();

        allMaskImagesLoaded &= maskLayers.imagesAreLoaded();

        paintInfo.context->setCompositeOperation(CompositeDestinationIn);
        paintInfo.context->beginTransparencyLayer(1);
        compositeOp = CompositeSourceOver;
    }

    if (allMaskImagesLoaded) {
        paintFillLayers(paintInfo, Color::transparent, m_renderBox.style()->maskLayers(), paintRect, BackgroundBleedNone, compositeOp);
        paintNinePieceImage(m_renderBox, paintInfo.context, paintRect, m_renderBox.style(), m_renderBox.style()->maskBoxImage(), compositeOp);
    }

    if (pushTransparencyLayer)
        paintInfo.context->endLayer();
}

void BoxPainter::paintClippingMask(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!paintInfo.shouldPaintWithinRoot(&m_renderBox) || m_renderBox.style()->visibility() != VISIBLE || paintInfo.phase != PaintPhaseClippingMask)
        return;

    if (!m_renderBox.layer() || m_renderBox.layer()->compositingState() != PaintsIntoOwnBacking)
        return;

    // We should never have this state in this function. A layer with a mask
    // should have always created its own backing if it became composited.
    ASSERT(m_renderBox.layer()->compositingState() != HasOwnBackingButPaintsIntoAncestor);

    LayoutRect paintRect = LayoutRect(paintOffset, m_renderBox.size());
    paintInfo.context->fillRect(pixelSnappedIntRect(paintRect), Color::black);
}

void BoxPainter::paintRootBackgroundColor(RenderObject& obj, const PaintInfo& paintInfo, const LayoutRect& rect, const Color& bgColor)
{
    GraphicsContext* context = paintInfo.context;
    if (rect.isEmpty())
        return;

    ASSERT(obj.isDocumentElement());

    IntRect backgroundRect(pixelSnappedIntRect(rect));
    backgroundRect.intersect(paintInfo.rect);

    Color baseColor = obj.view()->frameView()->baseBackgroundColor();
    bool shouldClearDocumentBackground = obj.document().settings() && obj.document().settings()->shouldClearDocumentBackground();
    CompositeOperator operation = shouldClearDocumentBackground ? CompositeCopy : context->compositeOperation();

    // If we have an alpha go ahead and blend with the base background color.
    if (baseColor.alpha()) {
        if (bgColor.alpha())
            baseColor = baseColor.blend(bgColor);
        context->fillRect(backgroundRect, baseColor, operation);
    } else if (bgColor.alpha()) {
        context->fillRect(backgroundRect, bgColor, operation);
    } else if (shouldClearDocumentBackground) {
        context->clearRect(backgroundRect);
    }
}

bool BoxPainter::isDocumentElementWithOpaqueBackground(RenderObject& obj)
{
    if (!obj.isDocumentElement())
        return false;

    // The background is opaque only if we're the root document, since iframes with
    // no background in the child document should show the parent's background.
    bool isOpaque = true;
    Element* ownerElement = obj.document().ownerElement();
    if (ownerElement) {
        if (!isHTMLFrameElement(*ownerElement)) {
            // Locate the <body> element using the DOM. This is easier than trying
            // to crawl around a render tree with potential :before/:after content and
            // anonymous blocks created by inline <body> tags etc. We can locate the <body>
            // render object very easily via the DOM.
            HTMLElement* body = obj.document().body();
            if (body) {
                // Can't scroll a frameset document anyway.
                isOpaque = body->hasTagName(HTMLNames::framesetTag);
            } else {
                // FIXME: SVG specific behavior should be in the SVG code.
                // SVG documents and XML documents with SVG root nodes are transparent.
                isOpaque = !obj.document().hasSVGRootNode();
            }
        }
    } else if (obj.view()->frameView()) {
        isOpaque = !obj.view()->frameView()->isTransparent();
    }

    return isOpaque;
}

static inline int getSpace(int areaSize, int tileSize)
{
    int numberOfTiles = areaSize / tileSize;
    int space = -1;

    if (numberOfTiles > 1)
        space = lroundf((float)(areaSize - numberOfTiles * tileSize) / (numberOfTiles - 1));

    return space;
}

void BoxPainter::calculateBackgroundImageGeometry(RenderBoxModelObject& obj, const RenderLayerModelObject* paintContainer, const FillLayer& fillLayer, const LayoutRect& paintRect,
    BackgroundImageGeometry& geometry, RenderObject* backgroundObject)
{
    LayoutUnit left = 0;
    LayoutUnit top = 0;
    IntSize positioningAreaSize;
    IntRect snappedPaintRect = pixelSnappedIntRect(paintRect);

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
        geometry.setDestRect(snappedPaintRect);

        LayoutUnit right = 0;
        LayoutUnit bottom = 0;
        // Scroll and Local.
        if (fillLayer.origin() != BorderFillBox) {
            left = obj.borderLeft();
            right = obj.borderRight();
            top = obj.borderTop();
            bottom = obj.borderBottom();
            if (fillLayer.origin() == ContentFillBox) {
                left += obj.paddingLeft();
                right += obj.paddingRight();
                top += obj.paddingTop();
                bottom += obj.paddingBottom();
            }
        }

        // The background of the box generated by the root element covers the entire canvas including
        // its margins. Since those were added in already, we have to factor them out when computing
        // the background positioning area.
        if (obj.isDocumentElement()) {
            positioningAreaSize = pixelSnappedIntSize(toRenderBox(&obj)->size() - LayoutSize(left + right, top + bottom), toRenderBox(&obj)->location());
            left += obj.marginLeft();
            top += obj.marginTop();
        } else {
            positioningAreaSize = pixelSnappedIntSize(paintRect.size() - LayoutSize(left + right, top + bottom), paintRect.location());
        }
    } else {
        geometry.setHasNonLocalGeometry();

        IntRect viewportRect = pixelSnappedIntRect(obj.viewRect());
        if (fixedBackgroundPaintsInLocalCoordinates(obj))
            viewportRect.setLocation(IntPoint());
        else if (FrameView* frameView = obj.view()->frameView())
            viewportRect.setLocation(IntPoint(frameView->scrollOffsetForFixedPosition()));

        if (paintContainer) {
            IntPoint absoluteContainerOffset = roundedIntPoint(paintContainer->localToAbsolute(FloatPoint()));
            viewportRect.moveBy(-absoluteContainerOffset);
        }

        geometry.setDestRect(pixelSnappedIntRect(viewportRect));
        positioningAreaSize = geometry.destRect().size();
    }

    const RenderObject* clientForBackgroundImage = backgroundObject ? backgroundObject : &obj;
    IntSize fillTileSize = calculateFillTileSize(obj, fillLayer, positioningAreaSize);
    fillLayer.image()->setContainerSizeForRenderer(clientForBackgroundImage, fillTileSize, obj.style()->effectiveZoom());
    geometry.setTileSize(fillTileSize);

    EFillRepeat backgroundRepeatX = fillLayer.repeatX();
    EFillRepeat backgroundRepeatY = fillLayer.repeatY();
    int availableWidth = positioningAreaSize.width() - geometry.tileSize().width();
    int availableHeight = positioningAreaSize.height() - geometry.tileSize().height();

    LayoutUnit computedXPosition = roundedMinimumValueForLength(fillLayer.xPosition(), availableWidth);
    if (backgroundRepeatX == RoundFill && positioningAreaSize.width() > 0 && fillTileSize.width() > 0) {
        long nrTiles = std::max(1l, lroundf((float)positioningAreaSize.width() / fillTileSize.width()));

        if (fillLayer.size().size.height().isAuto() && backgroundRepeatY != RoundFill) {
            fillTileSize.setHeight(fillTileSize.height() * positioningAreaSize.width() / (nrTiles * fillTileSize.width()));
        }

        fillTileSize.setWidth(positioningAreaSize.width() / nrTiles);
        geometry.setTileSize(fillTileSize);
        geometry.setPhaseX(geometry.tileSize().width() ? geometry.tileSize().width() - roundToInt(computedXPosition + left) % geometry.tileSize().width() : 0);
        geometry.setSpaceSize(IntSize());
    }

    LayoutUnit computedYPosition = roundedMinimumValueForLength(fillLayer.yPosition(), availableHeight);
    if (backgroundRepeatY == RoundFill && positioningAreaSize.height() > 0 && fillTileSize.height() > 0) {
        long nrTiles = std::max(1l, lroundf((float)positioningAreaSize.height() / fillTileSize.height()));

        if (fillLayer.size().size.width().isAuto() && backgroundRepeatX != RoundFill) {
            fillTileSize.setWidth(fillTileSize.width() * positioningAreaSize.height() / (nrTiles * fillTileSize.height()));
        }

        fillTileSize.setHeight(positioningAreaSize.height() / nrTiles);
        geometry.setTileSize(fillTileSize);
        geometry.setPhaseY(geometry.tileSize().height() ? geometry.tileSize().height() - roundToInt(computedYPosition + top) % geometry.tileSize().height() : 0);
        geometry.setSpaceSize(IntSize());
    }

    if (backgroundRepeatX == RepeatFill) {
        geometry.setPhaseX(geometry.tileSize().width() ? geometry.tileSize().width() - roundToInt(computedXPosition + left) % geometry.tileSize().width() : 0);
        geometry.setSpaceSize(IntSize());
    } else if (backgroundRepeatX == SpaceFill && fillTileSize.width() > 0) {
        int space = getSpace(positioningAreaSize.width(), geometry.tileSize().width());
        int actualWidth = geometry.tileSize().width() + space;

        if (space >= 0) {
            computedXPosition = roundedMinimumValueForLength(Length(), availableWidth);
            geometry.setSpaceSize(IntSize(space, 0));
            geometry.setPhaseX(actualWidth ? actualWidth - roundToInt(computedXPosition + left) % actualWidth : 0);
        } else {
            backgroundRepeatX = NoRepeatFill;
        }
    }
    if (backgroundRepeatX == NoRepeatFill) {
        int xOffset = fillLayer.backgroundXOrigin() == RightEdge ? availableWidth - computedXPosition : computedXPosition;
        geometry.setNoRepeatX(left + xOffset);
        geometry.setSpaceSize(IntSize(0, geometry.spaceSize().height()));
    }

    if (backgroundRepeatY == RepeatFill) {
        geometry.setPhaseY(geometry.tileSize().height() ? geometry.tileSize().height() - roundToInt(computedYPosition + top) % geometry.tileSize().height() : 0);
        geometry.setSpaceSize(IntSize(geometry.spaceSize().width(), 0));
    } else if (backgroundRepeatY == SpaceFill && fillTileSize.height() > 0) {
        int space = getSpace(positioningAreaSize.height(), geometry.tileSize().height());
        int actualHeight = geometry.tileSize().height() + space;

        if (space >= 0) {
            computedYPosition = roundedMinimumValueForLength(Length(), availableHeight);
            geometry.setSpaceSize(IntSize(geometry.spaceSize().width(), space));
            geometry.setPhaseY(actualHeight ? actualHeight - roundToInt(computedYPosition + top) % actualHeight : 0);
        } else {
            backgroundRepeatY = NoRepeatFill;
        }
    }
    if (backgroundRepeatY == NoRepeatFill) {
        int yOffset = fillLayer.backgroundYOrigin() == BottomEdge ? availableHeight - computedYPosition : computedYPosition;
        geometry.setNoRepeatY(top + yOffset);
        geometry.setSpaceSize(IntSize(geometry.spaceSize().width(), 0));
    }

    if (fixedAttachment)
        geometry.useFixedAttachment(snappedPaintRect.location());

    geometry.clip(snappedPaintRect);
    geometry.setDestOrigin(geometry.destRect().location());
}

InterpolationQuality BoxPainter::chooseInterpolationQuality(RenderBoxModelObject& obj, GraphicsContext* context, Image* image, const void* layer, const LayoutSize& size)
{
    return ImageQualityController::imageQualityController()->chooseInterpolationQuality(context, &obj, image, layer, size);
}

bool BoxPainter::fixedBackgroundPaintsInLocalCoordinates(const RenderObject& obj)
{
    if (!obj.isDocumentElement())
        return false;

    if (obj.view()->frameView() && obj.view()->frameView()->paintBehavior() & PaintBehaviorFlattenCompositingLayers)
        return false;

    RenderLayer* rootLayer = obj.view()->layer();
    if (!rootLayer || rootLayer->compositingState() == NotComposited)
        return false;

    return rootLayer->compositedLayerMapping()->backgroundLayerPaintsFixedRootBackground();
}

static inline void applySubPixelHeuristicForTileSize(LayoutSize& tileSize, const IntSize& positioningAreaSize)
{
    tileSize.setWidth(positioningAreaSize.width() - tileSize.width() <= 1 ? tileSize.width().ceil() : tileSize.width().floor());
    tileSize.setHeight(positioningAreaSize.height() - tileSize.height() <= 1 ? tileSize.height().ceil() : tileSize.height().floor());
}

IntSize BoxPainter::calculateFillTileSize(const RenderBoxModelObject& obj, const FillLayer& fillLayer, const IntSize& positioningAreaSize)
{
    StyleImage* image = fillLayer.image();
    EFillSizeType type = fillLayer.size().type;

    IntSize imageIntrinsicSize = obj.calculateImageIntrinsicDimensions(image, positioningAreaSize, RenderBoxModelObject::ScaleByEffectiveZoom);
    imageIntrinsicSize.scale(1 / image->imageScaleFactor(), 1 / image->imageScaleFactor());
    switch (type) {
    case SizeLength: {
        LayoutSize tileSize = positioningAreaSize;

        Length layerWidth = fillLayer.size().size.width();
        Length layerHeight = fillLayer.size().size.height();

        if (layerWidth.isFixed())
            tileSize.setWidth(layerWidth.value());
        else if (layerWidth.isPercent())
            tileSize.setWidth(valueForLength(layerWidth, positioningAreaSize.width()));

        if (layerHeight.isFixed())
            tileSize.setHeight(layerHeight.value());
        else if (layerHeight.isPercent())
            tileSize.setHeight(valueForLength(layerHeight, positioningAreaSize.height()));

        applySubPixelHeuristicForTileSize(tileSize, positioningAreaSize);

        // If one of the values is auto we have to use the appropriate
        // scale to maintain our aspect ratio.
        if (layerWidth.isAuto() && !layerHeight.isAuto()) {
            if (imageIntrinsicSize.height())
                tileSize.setWidth(imageIntrinsicSize.width() * tileSize.height() / imageIntrinsicSize.height());
        } else if (!layerWidth.isAuto() && layerHeight.isAuto()) {
            if (imageIntrinsicSize.width())
                tileSize.setHeight(imageIntrinsicSize.height() * tileSize.width() / imageIntrinsicSize.width());
        } else if (layerWidth.isAuto() && layerHeight.isAuto()) {
            // If both width and height are auto, use the image's intrinsic size.
            tileSize = imageIntrinsicSize;
        }

        tileSize.clampNegativeToZero();
        return flooredIntSize(tileSize);
    }
    case SizeNone: {
        // If both values are ‘auto’ then the intrinsic width and/or height of the image should be used, if any.
        if (!imageIntrinsicSize.isEmpty())
            return imageIntrinsicSize;

        // If the image has neither an intrinsic width nor an intrinsic height, its size is determined as for ‘contain’.
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

static LayoutUnit computeBorderImageSide(const BorderImageLength& borderSlice, LayoutUnit borderSide, LayoutUnit imageSide, LayoutUnit boxExtent)
{
    if (borderSlice.isNumber())
        return borderSlice.number() * borderSide;
    if (borderSlice.length().isAuto())
        return imageSide;
    return valueForLength(borderSlice.length(), boxExtent);
}

bool BoxPainter::paintNinePieceImage(RenderBoxModelObject& obj, GraphicsContext* graphicsContext, const LayoutRect& rect, const RenderStyle* style, const NinePieceImage& ninePieceImage, CompositeOperator op)
{
    StyleImage* styleImage = ninePieceImage.image();
    if (!styleImage)
        return false;

    if (!styleImage->isLoaded())
        return true; // Never paint a nine-piece image incrementally, but don't paint the fallback borders either.

    if (!styleImage->canRender(obj, style->effectiveZoom()))
        return false;

    // FIXME: border-image is broken with full page zooming when tiling has to happen, since the tiling function
    // doesn't have any understanding of the zoom that is in effect on the tile.
    LayoutRect rectWithOutsets = rect;
    rectWithOutsets.expand(style->imageOutsets(ninePieceImage));
    IntRect borderImageRect = pixelSnappedIntRect(rectWithOutsets);

    IntSize imageSize = obj.calculateImageIntrinsicDimensions(styleImage, borderImageRect.size(), RenderBoxModelObject::DoNotScaleByEffectiveZoom);

    // If both values are ‘auto’ then the intrinsic width and/or height of the image should be used, if any.
    styleImage->setContainerSizeForRenderer(&obj, imageSize, style->effectiveZoom());

    int imageWidth = imageSize.width();
    int imageHeight = imageSize.height();

    float imageScaleFactor = styleImage->imageScaleFactor();
    int topSlice = std::min<int>(imageHeight, valueForLength(ninePieceImage.imageSlices().top(), imageHeight)) * imageScaleFactor;
    int rightSlice = std::min<int>(imageWidth, valueForLength(ninePieceImage.imageSlices().right(), imageWidth)) * imageScaleFactor;
    int bottomSlice = std::min<int>(imageHeight, valueForLength(ninePieceImage.imageSlices().bottom(), imageHeight)) * imageScaleFactor;
    int leftSlice = std::min<int>(imageWidth, valueForLength(ninePieceImage.imageSlices().left(), imageWidth)) * imageScaleFactor;

    ENinePieceImageRule hRule = ninePieceImage.horizontalRule();
    ENinePieceImageRule vRule = ninePieceImage.verticalRule();

    int topWidth = computeBorderImageSide(ninePieceImage.borderSlices().top(), style->borderTopWidth(), topSlice, borderImageRect.height());
    int rightWidth = computeBorderImageSide(ninePieceImage.borderSlices().right(), style->borderRightWidth(), rightSlice, borderImageRect.width());
    int bottomWidth = computeBorderImageSide(ninePieceImage.borderSlices().bottom(), style->borderBottomWidth(), bottomSlice, borderImageRect.height());
    int leftWidth = computeBorderImageSide(ninePieceImage.borderSlices().left(), style->borderLeftWidth(), leftSlice, borderImageRect.width());

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

    RefPtr<Image> image = styleImage->image(&obj, imageSize);

    float destinationWidth = borderImageRect.width() - leftWidth - rightWidth;
    float destinationHeight = borderImageRect.height() - topWidth - bottomWidth;

    float sourceWidth = imageWidth - leftSlice - rightSlice;
    float sourceHeight = imageHeight - topSlice - bottomSlice;

    float leftSideScale = drawLeft ? (float)leftWidth / leftSlice : 1;
    float rightSideScale = drawRight ? (float)rightWidth / rightSlice : 1;
    float topSideScale = drawTop ? (float)topWidth / topSlice : 1;
    float bottomSideScale = drawBottom ? (float)bottomWidth / bottomSlice : 1;

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

    return true;
}

static IntRect calculateSideRect(const RoundedRect& outerBorder, const BorderEdge edges[], int side)
{
    IntRect sideRect = outerBorder.rect();
    int width = edges[side].width;

    if (side == BSTop)
        sideRect.setHeight(width);
    else if (side == BSBottom)
        sideRect.shiftYEdgeTo(sideRect.maxY() - width);
    else if (side == BSLeft)
        sideRect.setWidth(width);
    else
        sideRect.shiftXEdgeTo(sideRect.maxX() - width);

    return sideRect;
}

enum BorderEdgeFlag {
    TopBorderEdge = 1 << BSTop,
    RightBorderEdge = 1 << BSRight,
    BottomBorderEdge = 1 << BSBottom,
    LeftBorderEdge = 1 << BSLeft,
    AllBorderEdges = TopBorderEdge | BottomBorderEdge | LeftBorderEdge | RightBorderEdge
};

static inline BorderEdgeFlag edgeFlagForSide(BoxSide side)
{
    return static_cast<BorderEdgeFlag>(1 << side);
}

static inline bool includesEdge(BorderEdgeFlags flags, BoxSide side)
{
    return flags & edgeFlagForSide(side);
}

inline bool styleRequiresClipPolygon(EBorderStyle style)
{
    return style == DOTTED || style == DASHED; // These are drawn with a stroke, so we have to clip to get corner miters.
}

static bool borderStyleFillsBorderArea(EBorderStyle style)
{
    return !(style == DOTTED || style == DASHED || style == DOUBLE);
}

static bool borderStyleHasInnerDetail(EBorderStyle style)
{
    return style == GROOVE || style == RIDGE || style == DOUBLE;
}

static bool borderStyleIsDottedOrDashed(EBorderStyle style)
{
    return style == DOTTED || style == DASHED;
}

// OUTSET darkens the bottom and right (and maybe lightens the top and left)
// INSET darkens the top and left (and maybe lightens the bottom and right)
static inline bool borderStyleHasUnmatchedColorsAtCorner(EBorderStyle style, BoxSide side, BoxSide adjacentSide)
{
    // These styles match at the top/left and bottom/right.
    if (style == INSET || style == GROOVE || style == RIDGE || style == OUTSET) {
        const BorderEdgeFlags topRightFlags = edgeFlagForSide(BSTop) | edgeFlagForSide(BSRight);
        const BorderEdgeFlags bottomLeftFlags = edgeFlagForSide(BSBottom) | edgeFlagForSide(BSLeft);

        BorderEdgeFlags flags = edgeFlagForSide(side) | edgeFlagForSide(adjacentSide);
        return flags == topRightFlags || flags == bottomLeftFlags;
    }
    return false;
}

static inline bool colorsMatchAtCorner(BoxSide side, BoxSide adjacentSide, const BorderEdge edges[])
{
    if (edges[side].shouldRender() != edges[adjacentSide].shouldRender())
        return false;

    if (!edges[side].sharesColorWith(edges[adjacentSide]))
        return false;

    return !borderStyleHasUnmatchedColorsAtCorner(edges[side].borderStyle(), side, adjacentSide);
}

static inline bool colorNeedsAntiAliasAtCorner(BoxSide side, BoxSide adjacentSide, const BorderEdge edges[])
{
    if (!edges[side].color.hasAlpha())
        return false;

    if (edges[side].shouldRender() != edges[adjacentSide].shouldRender())
        return false;

    if (!edges[side].sharesColorWith(edges[adjacentSide]))
        return true;

    return borderStyleHasUnmatchedColorsAtCorner(edges[side].borderStyle(), side, adjacentSide);
}

bool BoxPainter::shouldAntialiasLines(GraphicsContext* context)
{
    // FIXME: We may want to not antialias when scaled by an integral value,
    // and we may want to antialias when translated by a non-integral value.
    // FIXME: See crbug.com/382491. getCTM does not include scale factors applied at raster time, such
    // as device zoom.
    return !context->getCTM().isIdentityOrTranslationOrFlipped();
}

static bool borderWillArcInnerEdge(const LayoutSize& firstRadius, const FloatSize& secondRadius)
{
    return !firstRadius.isZero() || !secondRadius.isZero();
}

// This assumes that we draw in order: top, bottom, left, right.
static inline bool willBeOverdrawn(BoxSide side, BoxSide adjacentSide, const BorderEdge edges[])
{
    switch (side) {
    case BSTop:
    case BSBottom:
        if (edges[adjacentSide].presentButInvisible())
            return false;

        if (!edges[side].sharesColorWith(edges[adjacentSide]) && edges[adjacentSide].color.hasAlpha())
            return false;

        if (!borderStyleFillsBorderArea(edges[adjacentSide].borderStyle()))
            return false;

        return true;

    case BSLeft:
    case BSRight:
        // These draw last, so are never overdrawn.
        return false;
    }
    return false;
}

static inline bool borderStylesRequireMitre(BoxSide side, BoxSide adjacentSide, EBorderStyle style, EBorderStyle adjacentStyle)
{
    if (style == DOUBLE || adjacentStyle == DOUBLE || adjacentStyle == GROOVE || adjacentStyle == RIDGE)
        return true;

    if (borderStyleIsDottedOrDashed(style) != borderStyleIsDottedOrDashed(adjacentStyle))
        return true;

    if (style != adjacentStyle)
        return true;

    return borderStyleHasUnmatchedColorsAtCorner(style, side, adjacentSide);
}

static bool joinRequiresMitre(BoxSide side, BoxSide adjacentSide, const BorderEdge edges[], bool allowOverdraw)
{
    if ((edges[side].isTransparent && edges[adjacentSide].isTransparent) || !edges[adjacentSide].isPresent)
        return false;

    if (allowOverdraw && willBeOverdrawn(side, adjacentSide, edges))
        return false;

    if (!edges[side].sharesColorWith(edges[adjacentSide]))
        return true;

    if (borderStylesRequireMitre(side, adjacentSide, edges[side].borderStyle(), edges[adjacentSide].borderStyle()))
        return true;

    return false;
}

static IntRect calculateSideRectIncludingInner(const RoundedRect& outerBorder, const BorderEdge edges[], BoxSide side)
{
    IntRect sideRect = outerBorder.rect();
    int width;

    switch (side) {
    case BSTop:
        width = sideRect.height() - edges[BSBottom].width;
        sideRect.setHeight(width);
        break;
    case BSBottom:
        width = sideRect.height() - edges[BSTop].width;
        sideRect.shiftYEdgeTo(sideRect.maxY() - width);
        break;
    case BSLeft:
        width = sideRect.width() - edges[BSRight].width;
        sideRect.setWidth(width);
        break;
    case BSRight:
        width = sideRect.width() - edges[BSLeft].width;
        sideRect.shiftXEdgeTo(sideRect.maxX() - width);
        break;
    }

    return sideRect;
}

static RoundedRect calculateAdjustedInnerBorder(const RoundedRect&innerBorder, BoxSide side)
{
    // Expand the inner border as necessary to make it a rounded rect (i.e. radii contained within each edge).
    // This function relies on the fact we only get radii not contained within each edge if one of the radii
    // for an edge is zero, so we can shift the arc towards the zero radius corner.
    RoundedRect::Radii newRadii = innerBorder.radii();
    IntRect newRect = innerBorder.rect();

    float overshoot;
    float maxRadii;

    switch (side) {
    case BSTop:
        overshoot = newRadii.topLeft().width() + newRadii.topRight().width() - newRect.width();
        if (overshoot > 0) {
            ASSERT(!(newRadii.topLeft().width() && newRadii.topRight().width()));
            newRect.setWidth(newRect.width() + overshoot);
            if (!newRadii.topLeft().width())
                newRect.move(-overshoot, 0);
        }
        newRadii.setBottomLeft(IntSize(0, 0));
        newRadii.setBottomRight(IntSize(0, 0));
        maxRadii = std::max(newRadii.topLeft().height(), newRadii.topRight().height());
        if (maxRadii > newRect.height())
            newRect.setHeight(maxRadii);
        break;

    case BSBottom:
        overshoot = newRadii.bottomLeft().width() + newRadii.bottomRight().width() - newRect.width();
        if (overshoot > 0) {
            ASSERT(!(newRadii.bottomLeft().width() && newRadii.bottomRight().width()));
            newRect.setWidth(newRect.width() + overshoot);
            if (!newRadii.bottomLeft().width())
                newRect.move(-overshoot, 0);
        }
        newRadii.setTopLeft(IntSize(0, 0));
        newRadii.setTopRight(IntSize(0, 0));
        maxRadii = std::max(newRadii.bottomLeft().height(), newRadii.bottomRight().height());
        if (maxRadii > newRect.height()) {
            newRect.move(0, newRect.height() - maxRadii);
            newRect.setHeight(maxRadii);
        }
        break;

    case BSLeft:
        overshoot = newRadii.topLeft().height() + newRadii.bottomLeft().height() - newRect.height();
        if (overshoot > 0) {
            ASSERT(!(newRadii.topLeft().height() && newRadii.bottomLeft().height()));
            newRect.setHeight(newRect.height() + overshoot);
            if (!newRadii.topLeft().height())
                newRect.move(0, -overshoot);
        }
        newRadii.setTopRight(IntSize(0, 0));
        newRadii.setBottomRight(IntSize(0, 0));
        maxRadii = std::max(newRadii.topLeft().width(), newRadii.bottomLeft().width());
        if (maxRadii > newRect.width())
            newRect.setWidth(maxRadii);
        break;

    case BSRight:
        overshoot = newRadii.topRight().height() + newRadii.bottomRight().height() - newRect.height();
        if (overshoot > 0) {
            ASSERT(!(newRadii.topRight().height() && newRadii.bottomRight().height()));
            newRect.setHeight(newRect.height() + overshoot);
            if (!newRadii.topRight().height())
                newRect.move(0, -overshoot);
        }
        newRadii.setTopLeft(IntSize(0, 0));
        newRadii.setBottomLeft(IntSize(0, 0));
        maxRadii = std::max(newRadii.topRight().width(), newRadii.bottomRight().width());
        if (maxRadii > newRect.width()) {
            newRect.move(newRect.width() - maxRadii, 0);
            newRect.setWidth(maxRadii);
        }
        break;
    }

    return RoundedRect(newRect, newRadii);
}

void BoxPainter::clipBorderSideForComplexInnerPath(GraphicsContext* graphicsContext, const RoundedRect& outerBorder, const RoundedRect& innerBorder,
    BoxSide side, const BorderEdge edges[])
{
    graphicsContext->clip(calculateSideRectIncludingInner(outerBorder, edges, side));
    RoundedRect adjustedInnerRect = calculateAdjustedInnerBorder(innerBorder, side);
    if (!adjustedInnerRect.isEmpty())
        graphicsContext->clipOutRoundedRect(adjustedInnerRect);
}

static bool allCornersClippedOut(const RoundedRect& border, const LayoutRect& clipRect)
{
    LayoutRect boundingRect = border.rect();
    if (clipRect.contains(boundingRect))
        return false;

    RoundedRect::Radii radii = border.radii();

    LayoutRect topLeftRect(boundingRect.location(), radii.topLeft());
    if (clipRect.intersects(topLeftRect))
        return false;

    LayoutRect topRightRect(boundingRect.location(), radii.topRight());
    topRightRect.setX(boundingRect.maxX() - topRightRect.width());
    if (clipRect.intersects(topRightRect))
        return false;

    LayoutRect bottomLeftRect(boundingRect.location(), radii.bottomLeft());
    bottomLeftRect.setY(boundingRect.maxY() - bottomLeftRect.height());
    if (clipRect.intersects(bottomLeftRect))
        return false;

    LayoutRect bottomRightRect(boundingRect.location(), radii.bottomRight());
    bottomRightRect.setX(boundingRect.maxX() - bottomRightRect.width());
    bottomRightRect.setY(boundingRect.maxY() - bottomRightRect.height());
    if (clipRect.intersects(bottomRightRect))
        return false;

    return true;
}

void BoxPainter::paintBorder(RenderBoxModelObject& obj, const PaintInfo& info, const LayoutRect& rect, const RenderStyle* style, BackgroundBleedAvoidance bleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
{
    GraphicsContext* graphicsContext = info.context;
    // border-image is not affected by border-radius.
    if (paintNinePieceImage(obj, graphicsContext, rect, style, style->borderImage()))
        return;

    BorderEdge edges[4];
    style->getBorderEdgeInfo(edges, includeLogicalLeftEdge, includeLogicalRightEdge);
    RoundedRect outerBorder = style->getRoundedBorderFor(rect, includeLogicalLeftEdge, includeLogicalRightEdge);
    RoundedRect innerBorder = style->getRoundedInnerBorderFor(borderInnerRectAdjustedForBleedAvoidance(graphicsContext, rect, bleedAvoidance), includeLogicalLeftEdge, includeLogicalRightEdge);

    if (outerBorder.rect().isEmpty())
        return;

    bool haveAlphaColor = false;
    bool haveAllSolidEdges = true;
    bool haveAllDoubleEdges = true;
    int numEdgesVisible = 4;
    bool allEdgesShareColor = true;
    bool allEdgesShareWidth = true;
    int firstVisibleEdge = -1;
    BorderEdgeFlags edgesToDraw = 0;

    for (int i = BSTop; i <= BSLeft; ++i) {
        const BorderEdge& currEdge = edges[i];

        if (edges[i].shouldRender())
            edgesToDraw |= edgeFlagForSide(static_cast<BoxSide>(i));

        if (currEdge.presentButInvisible()) {
            --numEdgesVisible;
            allEdgesShareColor = false;
            allEdgesShareWidth = false;
            continue;
        }

        if (!currEdge.shouldRender()) {
            --numEdgesVisible;
            continue;
        }

        if (firstVisibleEdge == -1) {
            firstVisibleEdge = i;
        } else {
            if (currEdge.color != edges[firstVisibleEdge].color)
                allEdgesShareColor = false;
            if (currEdge.width != edges[firstVisibleEdge].width)
                allEdgesShareWidth = false;
        }

        if (currEdge.color.hasAlpha())
            haveAlphaColor = true;

        if (currEdge.borderStyle() != SOLID)
            haveAllSolidEdges = false;

        if (currEdge.borderStyle() != DOUBLE)
            haveAllDoubleEdges = false;
    }

    // If no corner intersects the clip region, we can pretend outerBorder is
    // rectangular to improve performance.
    if (haveAllSolidEdges && outerBorder.isRounded() && allCornersClippedOut(outerBorder, info.rect))
        outerBorder.setRadii(RoundedRect::Radii());

    // isRenderable() check avoids issue described in https://bugs.webkit.org/show_bug.cgi?id=38787
    if ((haveAllSolidEdges || haveAllDoubleEdges) && allEdgesShareColor && innerBorder.isRenderable()) {
        // Fast path for drawing all solid edges and all unrounded double edges

        if (numEdgesVisible == 4 && (outerBorder.isRounded() || haveAlphaColor)
            && (haveAllSolidEdges || (!outerBorder.isRounded() && !innerBorder.isRounded()))) {
            Path path;

            if (outerBorder.isRounded() && allEdgesShareWidth) {

                // Very fast path for single stroked round rect with circular corners

                graphicsContext->fillBetweenRoundedRects(outerBorder, innerBorder, edges[firstVisibleEdge].color);
                return;
            }
            if (outerBorder.isRounded() && bleedAvoidance != BackgroundBleedClipBackground)
                path.addRoundedRect(outerBorder);
            else
                path.addRect(outerBorder.rect());

            if (haveAllDoubleEdges) {
                IntRect innerThirdRect = outerBorder.rect();
                IntRect outerThirdRect = outerBorder.rect();
                for (int side = BSTop; side <= BSLeft; ++side) {
                    int outerWidth;
                    int innerWidth;
                    edges[side].getDoubleBorderStripeWidths(outerWidth, innerWidth);

                    if (side == BSTop) {
                        innerThirdRect.shiftYEdgeTo(innerThirdRect.y() + innerWidth);
                        outerThirdRect.shiftYEdgeTo(outerThirdRect.y() + outerWidth);
                    } else if (side == BSBottom) {
                        innerThirdRect.setHeight(innerThirdRect.height() - innerWidth);
                        outerThirdRect.setHeight(outerThirdRect.height() - outerWidth);
                    } else if (side == BSLeft) {
                        innerThirdRect.shiftXEdgeTo(innerThirdRect.x() + innerWidth);
                        outerThirdRect.shiftXEdgeTo(outerThirdRect.x() + outerWidth);
                    } else {
                        innerThirdRect.setWidth(innerThirdRect.width() - innerWidth);
                        outerThirdRect.setWidth(outerThirdRect.width() - outerWidth);
                    }
                }

                RoundedRect outerThird = outerBorder;
                RoundedRect innerThird = innerBorder;
                innerThird.setRect(innerThirdRect);
                outerThird.setRect(outerThirdRect);

                if (outerThird.isRounded() && bleedAvoidance != BackgroundBleedClipBackground)
                    path.addRoundedRect(outerThird);
                else
                    path.addRect(outerThird.rect());

                if (innerThird.isRounded() && bleedAvoidance != BackgroundBleedClipBackground)
                    path.addRoundedRect(innerThird);
                else
                    path.addRect(innerThird.rect());
            }

            if (innerBorder.isRounded())
                path.addRoundedRect(innerBorder);
            else
                path.addRect(innerBorder.rect());

            graphicsContext->setFillRule(RULE_EVENODD);
            graphicsContext->setFillColor(edges[firstVisibleEdge].color);
            graphicsContext->fillPath(path);
            return;
        }
        // Avoid creating transparent layers
        if (haveAllSolidEdges && numEdgesVisible != 4 && !outerBorder.isRounded() && haveAlphaColor) {
            Path path;

            for (int i = BSTop; i <= BSLeft; ++i) {
                const BorderEdge& currEdge = edges[i];
                if (currEdge.shouldRender()) {
                    IntRect sideRect = calculateSideRect(outerBorder, edges, i);
                    path.addRect(sideRect);
                }
            }

            graphicsContext->setFillRule(RULE_NONZERO);
            graphicsContext->setFillColor(edges[firstVisibleEdge].color);
            graphicsContext->fillPath(path);
            return;
        }
    }

    bool clipToOuterBorder = outerBorder.isRounded();
    GraphicsContextStateSaver stateSaver(*graphicsContext, clipToOuterBorder);
    if (clipToOuterBorder) {
        // Clip to the inner and outer radii rects.
        if (bleedAvoidance != BackgroundBleedClipBackground)
            graphicsContext->clipRoundedRect(outerBorder);
        // isRenderable() check avoids issue described in https://bugs.webkit.org/show_bug.cgi?id=38787
        // The inside will be clipped out later (in clipBorderSideForComplexInnerPath)
        if (innerBorder.isRenderable() && !innerBorder.isEmpty())
            graphicsContext->clipOutRoundedRect(innerBorder);
    }

    // If only one edge visible antialiasing doesn't create seams
    bool antialias = shouldAntialiasLines(graphicsContext) || numEdgesVisible == 1;
    RoundedRect unadjustedInnerBorder = (bleedAvoidance == BackgroundBleedBackgroundOverBorder) ? style->getRoundedInnerBorderFor(rect, includeLogicalLeftEdge, includeLogicalRightEdge) : innerBorder;
    IntPoint innerBorderAdjustment(innerBorder.rect().x() - unadjustedInnerBorder.rect().x(), innerBorder.rect().y() - unadjustedInnerBorder.rect().y());
    if (haveAlphaColor)
        paintTranslucentBorderSides(obj, graphicsContext, style, outerBorder, unadjustedInnerBorder, innerBorderAdjustment, edges, edgesToDraw, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge, antialias);
    else
        paintBorderSides(obj, graphicsContext, style, outerBorder, unadjustedInnerBorder, innerBorderAdjustment, edges, edgesToDraw, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge, antialias);
}

static inline bool includesAdjacentEdges(BorderEdgeFlags flags)
{
    return (flags & (TopBorderEdge | RightBorderEdge)) == (TopBorderEdge | RightBorderEdge)
        || (flags & (RightBorderEdge | BottomBorderEdge)) == (RightBorderEdge | BottomBorderEdge)
        || (flags & (BottomBorderEdge | LeftBorderEdge)) == (BottomBorderEdge | LeftBorderEdge)
        || (flags & (LeftBorderEdge | TopBorderEdge)) == (LeftBorderEdge | TopBorderEdge);
}

void BoxPainter::paintTranslucentBorderSides(RenderObject& obj, GraphicsContext* graphicsContext, const RenderStyle* style, const RoundedRect& outerBorder, const RoundedRect& innerBorder, const IntPoint& innerBorderAdjustment,
    const BorderEdge edges[], BorderEdgeFlags edgesToDraw, BackgroundBleedAvoidance bleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge, bool antialias)
{
    // willBeOverdrawn assumes that we draw in order: top, bottom, left, right.
    // This is different from BoxSide enum order.
    static const BoxSide paintOrder[] = { BSTop, BSBottom, BSLeft, BSRight };

    while (edgesToDraw) {
        // Find undrawn edges sharing a color.
        Color commonColor;

        BorderEdgeFlags commonColorEdgeSet = 0;
        for (size_t i = 0; i < sizeof(paintOrder) / sizeof(paintOrder[0]); ++i) {
            BoxSide currSide = paintOrder[i];
            if (!includesEdge(edgesToDraw, currSide))
                continue;

            bool includeEdge;
            if (!commonColorEdgeSet) {
                commonColor = edges[currSide].color;
                includeEdge = true;
            } else {
                includeEdge = edges[currSide].color == commonColor;
            }

            if (includeEdge)
                commonColorEdgeSet |= edgeFlagForSide(currSide);
        }

        bool useTransparencyLayer = includesAdjacentEdges(commonColorEdgeSet) && commonColor.hasAlpha();
        if (useTransparencyLayer) {
            graphicsContext->beginTransparencyLayer(static_cast<float>(commonColor.alpha()) / 255);
            commonColor = Color(commonColor.red(), commonColor.green(), commonColor.blue());
        }

        paintBorderSides(obj, graphicsContext, style, outerBorder, innerBorder, innerBorderAdjustment, edges, commonColorEdgeSet, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge, antialias, &commonColor);

        if (useTransparencyLayer)
            graphicsContext->endLayer();

        edgesToDraw &= ~commonColorEdgeSet;
    }
}

LayoutRect BoxPainter::borderInnerRectAdjustedForBleedAvoidance(GraphicsContext* context, const LayoutRect& rect, BackgroundBleedAvoidance bleedAvoidance)
{
    // We shrink the rectangle by one pixel on each side to make it fully overlap the anti-aliased background border
    return (bleedAvoidance == BackgroundBleedBackgroundOverBorder) ? shrinkRectByOnePixel(context, rect) : rect;
}

void BoxPainter::paintOneBorderSide(RenderObject& obj, GraphicsContext* graphicsContext, const RenderStyle* style, const RoundedRect& outerBorder, const RoundedRect& innerBorder,
    const IntRect& sideRect, BoxSide side, BoxSide adjacentSide1, BoxSide adjacentSide2, const BorderEdge edges[], const Path* path,
    BackgroundBleedAvoidance bleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge, bool antialias, const Color* overrideColor)
{
    const BorderEdge& edgeToRender = edges[side];
    ASSERT(edgeToRender.width);
    const BorderEdge& adjacentEdge1 = edges[adjacentSide1];
    const BorderEdge& adjacentEdge2 = edges[adjacentSide2];

    bool mitreAdjacentSide1 = joinRequiresMitre(side, adjacentSide1, edges, !antialias);
    bool mitreAdjacentSide2 = joinRequiresMitre(side, adjacentSide2, edges, !antialias);

    bool adjacentSide1StylesMatch = colorsMatchAtCorner(side, adjacentSide1, edges);
    bool adjacentSide2StylesMatch = colorsMatchAtCorner(side, adjacentSide2, edges);

    const Color& colorToPaint = overrideColor ? *overrideColor : edgeToRender.color;

    if (path) {
        GraphicsContextStateSaver stateSaver(*graphicsContext);
        if (innerBorder.isRenderable())
            clipBorderSidePolygon(graphicsContext, outerBorder, innerBorder, side, adjacentSide1StylesMatch, adjacentSide2StylesMatch);
        else
            clipBorderSideForComplexInnerPath(graphicsContext, outerBorder, innerBorder, side, edges);
        float thickness = std::max(std::max(edgeToRender.width, adjacentEdge1.width), adjacentEdge2.width);
        drawBoxSideFromPath(graphicsContext, outerBorder.rect(), *path, edges, edgeToRender.width, thickness, side, style,
            colorToPaint, edgeToRender.borderStyle(), bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge);
    } else {
        bool clipForStyle = styleRequiresClipPolygon(edgeToRender.borderStyle()) && (mitreAdjacentSide1 || mitreAdjacentSide2);
        bool clipAdjacentSide1 = colorNeedsAntiAliasAtCorner(side, adjacentSide1, edges) && mitreAdjacentSide1;
        bool clipAdjacentSide2 = colorNeedsAntiAliasAtCorner(side, adjacentSide2, edges) && mitreAdjacentSide2;
        bool shouldClip = clipForStyle || clipAdjacentSide1 || clipAdjacentSide2;

        GraphicsContextStateSaver clipStateSaver(*graphicsContext, shouldClip);
        if (shouldClip) {
            bool aliasAdjacentSide1 = clipAdjacentSide1 || (clipForStyle && mitreAdjacentSide1);
            bool aliasAdjacentSide2 = clipAdjacentSide2 || (clipForStyle && mitreAdjacentSide2);
            clipBorderSidePolygon(graphicsContext, outerBorder, innerBorder, side, !aliasAdjacentSide1, !aliasAdjacentSide2);
            // Since we clipped, no need to draw with a mitre.
            mitreAdjacentSide1 = false;
            mitreAdjacentSide2 = false;
        }

        ObjectPainter::drawLineForBoxSide(graphicsContext, sideRect.x(), sideRect.y(), sideRect.maxX(), sideRect.maxY(), side, colorToPaint, edgeToRender.borderStyle(),
            mitreAdjacentSide1 ? adjacentEdge1.width : 0, mitreAdjacentSide2 ? adjacentEdge2.width : 0, antialias);
    }
}

void BoxPainter::paintBorderSides(RenderObject& obj, GraphicsContext* graphicsContext, const RenderStyle* style, const RoundedRect& outerBorder, const RoundedRect& innerBorder,
    const IntPoint& innerBorderAdjustment, const BorderEdge edges[], BorderEdgeFlags edgeSet, BackgroundBleedAvoidance bleedAvoidance,
    bool includeLogicalLeftEdge, bool includeLogicalRightEdge, bool antialias, const Color* overrideColor)
{
    bool renderRadii = outerBorder.isRounded();

    Path roundedPath;
    if (renderRadii)
        roundedPath.addRoundedRect(outerBorder);

    // The inner border adjustment for bleed avoidance mode BackgroundBleedBackgroundOverBorder
    // is only applied to sideRect, which is okay since BackgroundBleedBackgroundOverBorder
    // is only to be used for solid borders and the shape of the border painted by drawBoxSideFromPath
    // only depends on sideRect when painting solid borders.

    if (edges[BSTop].shouldRender() && includesEdge(edgeSet, BSTop)) {
        IntRect sideRect = outerBorder.rect();
        sideRect.setHeight(edges[BSTop].width + innerBorderAdjustment.y());

        bool usePath = renderRadii && (borderStyleHasInnerDetail(edges[BSTop].borderStyle()) || borderWillArcInnerEdge(innerBorder.radii().topLeft(), innerBorder.radii().topRight()));
        paintOneBorderSide(obj, graphicsContext, style, outerBorder, innerBorder, sideRect, BSTop, BSLeft, BSRight, edges, usePath ? &roundedPath : 0, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge, antialias, overrideColor);
    }

    if (edges[BSBottom].shouldRender() && includesEdge(edgeSet, BSBottom)) {
        IntRect sideRect = outerBorder.rect();
        sideRect.shiftYEdgeTo(sideRect.maxY() - edges[BSBottom].width - innerBorderAdjustment.y());

        bool usePath = renderRadii && (borderStyleHasInnerDetail(edges[BSBottom].borderStyle()) || borderWillArcInnerEdge(innerBorder.radii().bottomLeft(), innerBorder.radii().bottomRight()));
        paintOneBorderSide(obj, graphicsContext, style, outerBorder, innerBorder, sideRect, BSBottom, BSLeft, BSRight, edges, usePath ? &roundedPath : 0, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge, antialias, overrideColor);
    }

    if (edges[BSLeft].shouldRender() && includesEdge(edgeSet, BSLeft)) {
        IntRect sideRect = outerBorder.rect();
        sideRect.setWidth(edges[BSLeft].width + innerBorderAdjustment.x());

        bool usePath = renderRadii && (borderStyleHasInnerDetail(edges[BSLeft].borderStyle()) || borderWillArcInnerEdge(innerBorder.radii().bottomLeft(), innerBorder.radii().topLeft()));
        paintOneBorderSide(obj, graphicsContext, style, outerBorder, innerBorder, sideRect, BSLeft, BSTop, BSBottom, edges, usePath ? &roundedPath : 0, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge, antialias, overrideColor);
    }

    if (edges[BSRight].shouldRender() && includesEdge(edgeSet, BSRight)) {
        IntRect sideRect = outerBorder.rect();
        sideRect.shiftXEdgeTo(sideRect.maxX() - edges[BSRight].width - innerBorderAdjustment.x());

        bool usePath = renderRadii && (borderStyleHasInnerDetail(edges[BSRight].borderStyle()) || borderWillArcInnerEdge(innerBorder.radii().bottomRight(), innerBorder.radii().topRight()));
        paintOneBorderSide(obj, graphicsContext, style, outerBorder, innerBorder, sideRect, BSRight, BSTop, BSBottom, edges, usePath ? &roundedPath : 0, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge, antialias, overrideColor);
    }
}

void BoxPainter::drawBoxSideFromPath(GraphicsContext* graphicsContext, const LayoutRect& borderRect, const Path& borderPath, const BorderEdge edges[],
    float thickness, float drawThickness, BoxSide side, const RenderStyle* style, Color color, EBorderStyle borderStyle, BackgroundBleedAvoidance bleedAvoidance,
    bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
{
    if (thickness <= 0)
        return;

    if (borderStyle == DOUBLE && thickness < 3)
        borderStyle = SOLID;

    switch (borderStyle) {
    case BNONE:
    case BHIDDEN:
        return;
    case DOTTED:
    case DASHED: {
        graphicsContext->setStrokeColor(color);

        // The stroke is doubled here because the provided path is the
        // outside edge of the border so half the stroke is clipped off.
        // The extra multiplier is so that the clipping mask can antialias
        // the edges to prevent jaggies.
        graphicsContext->setStrokeThickness(drawThickness * 2 * 1.1f);
        graphicsContext->setStrokeStyle(borderStyle == DASHED ? DashedStroke : DottedStroke);

        // If the number of dashes that fit in the path is odd and non-integral then we
        // will have an awkwardly-sized dash at the end of the path. To try to avoid that
        // here, we simply make the whitespace dashes ever so slightly bigger.
        // FIXME: This could be even better if we tried to manipulate the dash offset
        // and possibly the gapLength to get the corners dash-symmetrical.
        float dashLength = thickness * ((borderStyle == DASHED) ? 3.0f : 1.0f);
        float gapLength = dashLength;
        float numberOfDashes = borderPath.length() / dashLength;
        // Don't try to show dashes if we have less than 2 dashes + 2 gaps.
        // FIXME: should do this test per side.
        if (numberOfDashes >= 4) {
            bool evenNumberOfFullDashes = !((int)numberOfDashes % 2);
            bool integralNumberOfDashes = !(numberOfDashes - (int)numberOfDashes);
            if (!evenNumberOfFullDashes && !integralNumberOfDashes) {
                float numberOfGaps = numberOfDashes / 2;
                gapLength += (dashLength  / numberOfGaps);
            }

            DashArray lineDash;
            lineDash.append(dashLength);
            lineDash.append(gapLength);
            graphicsContext->setLineDash(lineDash, dashLength);
        }

        // FIXME: stroking the border path causes issues with tight corners:
        // https://bugs.webkit.org/show_bug.cgi?id=58711
        // Also, to get the best appearance we should stroke a path between the two borders.
        graphicsContext->strokePath(borderPath);
        return;
    }
    case DOUBLE: {
        // Get the inner border rects for both the outer border line and the inner border line
        int outerBorderTopWidth;
        int innerBorderTopWidth;
        edges[BSTop].getDoubleBorderStripeWidths(outerBorderTopWidth, innerBorderTopWidth);

        int outerBorderRightWidth;
        int innerBorderRightWidth;
        edges[BSRight].getDoubleBorderStripeWidths(outerBorderRightWidth, innerBorderRightWidth);

        int outerBorderBottomWidth;
        int innerBorderBottomWidth;
        edges[BSBottom].getDoubleBorderStripeWidths(outerBorderBottomWidth, innerBorderBottomWidth);

        int outerBorderLeftWidth;
        int innerBorderLeftWidth;
        edges[BSLeft].getDoubleBorderStripeWidths(outerBorderLeftWidth, innerBorderLeftWidth);

        // Draw inner border line
        {
            GraphicsContextStateSaver stateSaver(*graphicsContext);
            RoundedRect innerClip = style->getRoundedInnerBorderFor(borderRect,
                innerBorderTopWidth, innerBorderBottomWidth, innerBorderLeftWidth, innerBorderRightWidth,
                includeLogicalLeftEdge, includeLogicalRightEdge);

            graphicsContext->clipRoundedRect(innerClip);
            drawBoxSideFromPath(graphicsContext, borderRect, borderPath, edges, thickness, drawThickness, side, style, color, SOLID, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge);
        }

        // Draw outer border line
        {
            GraphicsContextStateSaver stateSaver(*graphicsContext);
            LayoutRect outerRect = borderRect;
            if (bleedAvoidance == BackgroundBleedClipBackground) {
                outerRect.inflate(1);
                ++outerBorderTopWidth;
                ++outerBorderBottomWidth;
                ++outerBorderLeftWidth;
                ++outerBorderRightWidth;
            }

            RoundedRect outerClip = style->getRoundedInnerBorderFor(outerRect,
                outerBorderTopWidth, outerBorderBottomWidth, outerBorderLeftWidth, outerBorderRightWidth,
                includeLogicalLeftEdge, includeLogicalRightEdge);
            graphicsContext->clipOutRoundedRect(outerClip);
            drawBoxSideFromPath(graphicsContext, borderRect, borderPath, edges, thickness, drawThickness, side, style, color, SOLID, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge);
        }
        return;
    }
    case RIDGE:
    case GROOVE:
    {
        EBorderStyle s1;
        EBorderStyle s2;
        if (borderStyle == GROOVE) {
            s1 = INSET;
            s2 = OUTSET;
        } else {
            s1 = OUTSET;
            s2 = INSET;
        }

        // Paint full border
        drawBoxSideFromPath(graphicsContext, borderRect, borderPath, edges, thickness, drawThickness, side, style, color, s1, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge);

        // Paint inner only
        GraphicsContextStateSaver stateSaver(*graphicsContext);
        LayoutUnit topWidth = edges[BSTop].usedWidth() / 2;
        LayoutUnit bottomWidth = edges[BSBottom].usedWidth() / 2;
        LayoutUnit leftWidth = edges[BSLeft].usedWidth() / 2;
        LayoutUnit rightWidth = edges[BSRight].usedWidth() / 2;

        RoundedRect clipRect = style->getRoundedInnerBorderFor(borderRect,
            topWidth, bottomWidth, leftWidth, rightWidth,
            includeLogicalLeftEdge, includeLogicalRightEdge);

        graphicsContext->clipRoundedRect(clipRect);
        drawBoxSideFromPath(graphicsContext, borderRect, borderPath, edges, thickness, drawThickness, side, style, color, s2, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge);
        return;
    }
    case INSET:
        if (side == BSTop || side == BSLeft)
            color = color.dark();
        break;
    case OUTSET:
        if (side == BSBottom || side == BSRight)
            color = color.dark();
        break;
    default:
        break;
    }

    graphicsContext->setStrokeStyle(NoStroke);
    graphicsContext->setFillColor(color);
    graphicsContext->drawRect(pixelSnappedIntRect(borderRect));
}

void BoxPainter::paintBoxShadow(const PaintInfo& info, const LayoutRect& paintRect, const RenderStyle* s, ShadowStyle shadowStyle, bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
{
    // FIXME: Deal with border-image. Would be great to use border-image as a mask.
    GraphicsContext* context = info.context;
    if (!s->boxShadow())
        return;

    RoundedRect border = (shadowStyle == Inset) ? s->getRoundedInnerBorderFor(paintRect, includeLogicalLeftEdge, includeLogicalRightEdge)
        : s->getRoundedBorderFor(paintRect, includeLogicalLeftEdge, includeLogicalRightEdge);

    bool hasBorderRadius = s->hasBorderRadius();
    bool isHorizontal = s->isHorizontalWritingMode();
    bool hasOpaqueBackground = s->visitedDependentColor(CSSPropertyBackgroundColor).alpha() == 255;

    GraphicsContextStateSaver stateSaver(*context, false);

    const ShadowList* shadowList = s->boxShadow();
    for (size_t i = shadowList->shadows().size(); i--; ) {
        const ShadowData& shadow = shadowList->shadows()[i];
        if (shadow.style() != shadowStyle)
            continue;

        FloatSize shadowOffset(shadow.x(), shadow.y());
        float shadowBlur = shadow.blur();
        float shadowSpread = shadow.spread();

        if (shadowOffset.isZero() && !shadowBlur && !shadowSpread)
            continue;

        const Color& shadowColor = shadow.color();

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
                    RoundedRect rectToClipOut = border;

                    // If the box is opaque, it is unnecessary to clip it out. However, doing so saves time
                    // when painting the shadow. On the other hand, it introduces subpixel gaps along the
                    // corners. Those are avoided by insetting the clipping path by one pixel.
                    if (hasOpaqueBackground)
                        rectToClipOut.inflateWithRadii(-1);

                    if (!rectToClipOut.isEmpty()) {
                        context->clipOutRoundedRect(rectToClipOut);
                    }
                } else {
                    // This IntRect is correct even with fractional shadows, because it is used for the rectangle
                    // of the box itself, which is always pixel-aligned.
                    IntRect rectToClipOut = border.rect();

                    // If the box is opaque, it is unnecessary to clip it out. However, doing so saves time
                    // when painting the shadow. On the other hand, it introduces subpixel gaps along the
                    // edges if they are not pixel-aligned. Those are avoided by insetting the clipping path
                    // by one pixel.
                    if (hasOpaqueBackground) {
                        // FIXME: The function to decide on the policy based on the transform should be a named function.
                        // FIXME: It's not clear if this check is right. What about integral scale factors?
                        // FIXME: See crbug.com/382491. The use of getCTM may also be wrong because it does not include
                        // device zoom applied at raster time.
                        AffineTransform transform = context->getCTM();
                        if (transform.a() != 1 || (transform.d() != 1 && transform.d() != -1) || transform.b() || transform.c())
                            rectToClipOut.inflate(-1);
                    }

                    if (!rectToClipOut.isEmpty()) {
                        context->clipOut(rectToClipOut);
                    }
                }
            }

            // Draw only the shadow.
            OwnPtr<DrawLooperBuilder> drawLooperBuilder = DrawLooperBuilder::create();
            drawLooperBuilder->addShadow(shadowOffset, shadowBlur, shadowColor,
                DrawLooperBuilder::ShadowRespectsTransforms, DrawLooperBuilder::ShadowIgnoresAlpha);
            context->setDrawLooper(drawLooperBuilder.release());

            if (hasBorderRadius) {
                RoundedRect influenceRect(pixelSnappedIntRect(LayoutRect(shadowRect)), border.radii());
                influenceRect.expandRadii(2 * shadowBlur + shadowSpread);
                if (allCornersClippedOut(influenceRect, info.rect)) {
                    context->fillRect(fillRect, Color::black);
                } else {
                    // TODO: support non-integer shadows - crbug.com/334829
                    RoundedRect roundedFillRect = border;
                    roundedFillRect.inflate(shadowSpread);

                    roundedFillRect.expandRadii(shadowSpread);
                    if (!roundedFillRect.isRenderable())
                        roundedFillRect.adjustRadii();
                    context->fillRoundedRect(roundedFillRect, Color::black);
                }
            } else {
                context->fillRect(fillRect, Color::black);
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
            // TODO: support non-integer shadows - crbug.com/334828
            context->drawInnerShadow(border, shadowColor, flooredIntSize(shadowOffset), shadowBlur, shadowSpread, clippedEdges);
        }
    }
}

void BoxPainter::clipBorderSidePolygon(GraphicsContext* graphicsContext, const RoundedRect& outerBorder, const RoundedRect& innerBorder, BoxSide side, bool firstEdgeMatches, bool secondEdgeMatches)
{
    FloatPoint quad[4];

    const LayoutRect& outerRect = outerBorder.rect();
    const LayoutRect& innerRect = innerBorder.rect();

    FloatPoint centerPoint(innerRect.location().x().toFloat() + innerRect.width().toFloat() / 2, innerRect.location().y().toFloat() + innerRect.height().toFloat() / 2);

    // For each side, create a quad that encompasses all parts of that side that may draw,
    // including areas inside the innerBorder.
    //
    //         0----------------3
    //       0  \              /  0
    //       |\  1----------- 2  /|
    //       | 1                1 |
    //       | |                | |
    //       | |                | |
    //       | 2                2 |
    //       |/  1------------2  \|
    //       3  /              \  3
    //         0----------------3
    //
    switch (side) {
    case BSTop:
        quad[0] = outerRect.minXMinYCorner();
        quad[1] = innerRect.minXMinYCorner();
        quad[2] = innerRect.maxXMinYCorner();
        quad[3] = outerRect.maxXMinYCorner();

        if (!innerBorder.radii().topLeft().isZero()) {
            findIntersection(quad[0], quad[1],
                FloatPoint(
                    quad[1].x() + innerBorder.radii().topLeft().width(),
                    quad[1].y()),
                FloatPoint(
                    quad[1].x(),
                    quad[1].y() + innerBorder.radii().topLeft().height()),
                quad[1]);
        }

        if (!innerBorder.radii().topRight().isZero()) {
            findIntersection(quad[3], quad[2],
                FloatPoint(
                    quad[2].x() - innerBorder.radii().topRight().width(),
                    quad[2].y()),
                FloatPoint(
                    quad[2].x(),
                    quad[2].y() + innerBorder.radii().topRight().height()),
                quad[2]);
        }
        break;

    case BSLeft:
        quad[0] = outerRect.minXMinYCorner();
        quad[1] = innerRect.minXMinYCorner();
        quad[2] = innerRect.minXMaxYCorner();
        quad[3] = outerRect.minXMaxYCorner();

        if (!innerBorder.radii().topLeft().isZero()) {
            findIntersection(quad[0], quad[1],
                FloatPoint(
                    quad[1].x() + innerBorder.radii().topLeft().width(),
                    quad[1].y()),
                FloatPoint(
                    quad[1].x(),
                    quad[1].y() + innerBorder.radii().topLeft().height()),
                quad[1]);
        }

        if (!innerBorder.radii().bottomLeft().isZero()) {
            findIntersection(quad[3], quad[2],
                FloatPoint(
                    quad[2].x() + innerBorder.radii().bottomLeft().width(),
                    quad[2].y()),
                FloatPoint(
                    quad[2].x(),
                    quad[2].y() - innerBorder.radii().bottomLeft().height()),
                quad[2]);
        }
        break;

    case BSBottom:
        quad[0] = outerRect.minXMaxYCorner();
        quad[1] = innerRect.minXMaxYCorner();
        quad[2] = innerRect.maxXMaxYCorner();
        quad[3] = outerRect.maxXMaxYCorner();

        if (!innerBorder.radii().bottomLeft().isZero()) {
            findIntersection(quad[0], quad[1],
                FloatPoint(
                    quad[1].x() + innerBorder.radii().bottomLeft().width(),
                    quad[1].y()),
                FloatPoint(
                    quad[1].x(),
                    quad[1].y() - innerBorder.radii().bottomLeft().height()),
                quad[1]);
        }

        if (!innerBorder.radii().bottomRight().isZero()) {
            findIntersection(quad[3], quad[2],
                FloatPoint(
                    quad[2].x() - innerBorder.radii().bottomRight().width(),
                    quad[2].y()),
                FloatPoint(
                    quad[2].x(),
                    quad[2].y() - innerBorder.radii().bottomRight().height()),
                quad[2]);
        }
        break;

    case BSRight:
        quad[0] = outerRect.maxXMinYCorner();
        quad[1] = innerRect.maxXMinYCorner();
        quad[2] = innerRect.maxXMaxYCorner();
        quad[3] = outerRect.maxXMaxYCorner();

        if (!innerBorder.radii().topRight().isZero()) {
            findIntersection(quad[0], quad[1],
                FloatPoint(
                    quad[1].x() - innerBorder.radii().topRight().width(),
                    quad[1].y()),
                FloatPoint(
                    quad[1].x(),
                    quad[1].y() + innerBorder.radii().topRight().height()),
                quad[1]);
        }

        if (!innerBorder.radii().bottomRight().isZero()) {
            findIntersection(quad[3], quad[2],
                FloatPoint(
                    quad[2].x() - innerBorder.radii().bottomRight().width(),
                    quad[2].y()),
                FloatPoint(
                    quad[2].x(),
                    quad[2].y() - innerBorder.radii().bottomRight().height()),
                quad[2]);
        }
        break;
    }

    // If the border matches both of its adjacent sides, don't anti-alias the clip, and
    // if neither side matches, anti-alias the clip.
    if (firstEdgeMatches == secondEdgeMatches) {
        graphicsContext->clipConvexPolygon(4, quad, !firstEdgeMatches);
        return;
    }

    // If antialiasing settings for the first edge and second edge is different,
    // they have to be addressed separately. We do this by breaking the quad into
    // two parallelograms, made by moving quad[1] and quad[2].
    float ax = quad[1].x() - quad[0].x();
    float ay = quad[1].y() - quad[0].y();
    float bx = quad[2].x() - quad[1].x();
    float by = quad[2].y() - quad[1].y();
    float cx = quad[3].x() - quad[2].x();
    float cy = quad[3].y() - quad[2].y();

    const static float kEpsilon = 1e-2f;
    float r1, r2;
    if (fabsf(bx) < kEpsilon && fabsf(by) < kEpsilon) {
        // The quad was actually a triangle.
        r1 = r2 = 1.0f;
    } else {
        // Extend parallelogram a bit to hide calculation error
        const static float kExtendFill = 1e-2f;

        r1 = (-ax * by + ay * bx) / (cx * by - cy * bx) + kExtendFill;
        r2 = (-cx * by + cy * bx) / (ax * by - ay * bx) + kExtendFill;
    }

    FloatPoint firstQuad[4];
    firstQuad[0] = quad[0];
    firstQuad[1] = quad[1];
    firstQuad[2] = FloatPoint(quad[3].x() + r2 * ax, quad[3].y() + r2 * ay);
    firstQuad[3] = quad[3];
    graphicsContext->clipConvexPolygon(4, firstQuad, !firstEdgeMatches);

    FloatPoint secondQuad[4];
    secondQuad[0] = quad[0];
    secondQuad[1] = FloatPoint(quad[0].x() - r1 * cx, quad[0].y() - r1 * cy);
    secondQuad[2] = quad[2];
    secondQuad[3] = quad[3];
    graphicsContext->clipConvexPolygon(4, secondQuad, !secondEdgeMatches);
}

} // namespace blink
