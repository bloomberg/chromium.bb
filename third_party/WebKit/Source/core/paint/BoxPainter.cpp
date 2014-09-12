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
#include "core/rendering/RenderTable.h"
#include "core/rendering/RenderTheme.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/compositing/CompositedLayerMapping.h"
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
        m_renderBox.paintBoxShadow(paintInfo, paintRect, style, Normal);

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
            m_renderBox.paintBorder(paintInfo, paintRect, style, boxDecorationData.bleedAvoidance());

        paintBackground(paintInfo, paintRect, boxDecorationData.backgroundColor, boxDecorationData.bleedAvoidance());

        if (boxDecorationData.hasAppearance)
            RenderTheme::theme().paintDecorations(&m_renderBox, paintInfo, snappedPaintRect);
    }
    m_renderBox.paintBoxShadow(paintInfo, paintRect, style, Inset);

    // The theme will tell us whether or not we should also paint the CSS border.
    if (boxDecorationData.hasBorder && boxDecorationData.bleedAvoidance() != BackgroundBleedBackgroundOverBorder
        && (!boxDecorationData.hasAppearance || (!themePainted && RenderTheme::theme().paintBorderOnly(&m_renderBox, paintInfo, snappedPaintRect)))
        && !(m_renderBox.isTable() && toRenderTable(&m_renderBox)->collapseBorders()))
        m_renderBox.paintBorder(paintInfo, paintRect, style, boxDecorationData.bleedAvoidance());
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
        m_renderBox.paintNinePieceImage(paintInfo.context, paintRect, m_renderBox.style(), m_renderBox.style()->maskBoxImage(), compositeOp);
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

} // namespace blink
