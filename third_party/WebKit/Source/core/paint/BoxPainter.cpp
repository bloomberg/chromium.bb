// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
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
#include "core/layout/compositing/CompositedDeprecatedPaintLayerMapping.h"
#include "core/style/BorderEdge.h"
#include "core/style/ShadowList.h"
#include "core/paint/BackgroundImageGeometry.h"
#include "core/paint/BoxBorderPainter.h"
#include "core/paint/BoxDecorationData.h"
#include "core/paint/DeprecatedPaintLayer.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/PaintInfo.h"
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
    if (!RuntimeEnabledFeatures::slimmingPaintEnabled())
        return LayoutRect();

    // The document element is specified to paint its background infinitely.
    if (m_layoutBox.isDocumentElement())
        return rootBackgroundRect();

    // Use the visual overflow rect here, because it will include overflow introduced by the theme.
    LayoutRect bounds = m_layoutBox.visualOverflowRect();
    bounds.moveBy(paintOffset);
    return LayoutRect(pixelSnappedIntRect(bounds));
}

LayoutRect BoxPainter::rootBackgroundRect()
{
    LayoutView* layoutView = m_layoutBox.view();
    LayoutRect result = layoutView->backgroundRect(&m_layoutBox);
    // In root-layer-scrolls mode, root background is painted in coordinates of the
    // root scrolling contents layer, so don't need scroll offset adjustment.
    if (layoutView->hasOverflowClip() && !layoutView->frame()->settings()->rootLayerScrolls())
        result.move(-layoutView->scrolledContentOffset());
    return result;
}

namespace {

bool bleedAvoidanceIsClipping(BackgroundBleedAvoidance bleedAvoidance)
{
    return bleedAvoidance == BackgroundBleedClipOnly || bleedAvoidance == BackgroundBleedClipLayer;
}

} // anonymous namespace

void BoxPainter::paintBoxDecorationBackgroundWithRect(const PaintInfo& paintInfo, const LayoutPoint& paintOffset, const LayoutRect& paintRect)
{
    LayoutObjectDrawingRecorder recorder(*paintInfo.context, m_layoutBox, DisplayItem::BoxDecorationBackground, boundsForDrawingRecorder(paintOffset));
    if (recorder.canUseCachedDrawing())
        return;

    const ComputedStyle& style = m_layoutBox.styleRef();
    BoxDecorationData boxDecorationData(m_layoutBox);

#if ENABLE(ASSERT)
    // FIXME: For now we don't have notification on media buffered range change from media player
    // and miss paint invalidation on buffered range change. crbug.com/484288.
    if (style.appearance() == MediaSliderPart)
        recorder.setUnderInvalidationCheckingMode(DrawingDisplayItem::DontCheck);
#endif

    // FIXME: Should eventually give the theme control over whether the box shadow should paint, since controls could have
    // custom shadows of their own.
    if (!m_layoutBox.boxShadowShouldBeAppliedToBackground(boxDecorationData.bleedAvoidance))
        paintBoxShadow(paintInfo, paintRect, style, Normal);

    GraphicsContextStateSaver stateSaver(*paintInfo.context, false);
    if (bleedAvoidanceIsClipping(boxDecorationData.bleedAvoidance)) {

        stateSaver.save();
        FloatRoundedRect border = style.getRoundedBorderFor(paintRect);
        paintInfo.context->clipRoundedRect(border);

        if (boxDecorationData.bleedAvoidance == BackgroundBleedClipLayer)
            paintInfo.context->beginLayer();
    }

    // If we have a native theme appearance, paint that before painting our background.
    // The theme will tell us whether or not we should also paint the CSS background.
    IntRect snappedPaintRect(pixelSnappedIntRect(paintRect));
    ThemePainter& themePainter = LayoutTheme::theme().painter();
    bool themePainted = boxDecorationData.hasAppearance && !themePainter.paint(&m_layoutBox, paintInfo, snappedPaintRect);
    if (!themePainted) {
        if (boxDecorationData.bleedAvoidance == BackgroundBleedBackgroundOverBorder)
            paintBorder(m_layoutBox, paintInfo, paintRect, style, boxDecorationData.bleedAvoidance);

        paintBackground(paintInfo, paintRect, boxDecorationData.backgroundColor, boxDecorationData.bleedAvoidance);

        if (boxDecorationData.hasAppearance)
            themePainter.paintDecorations(&m_layoutBox, paintInfo, snappedPaintRect);
    }
    paintBoxShadow(paintInfo, paintRect, style, Inset);

    // The theme will tell us whether or not we should also paint the CSS border.
    if (boxDecorationData.hasBorder && boxDecorationData.bleedAvoidance != BackgroundBleedBackgroundOverBorder
        && (!boxDecorationData.hasAppearance || (!themePainted && LayoutTheme::theme().painter().paintBorderOnly(&m_layoutBox, paintInfo, snappedPaintRect)))
        && !(m_layoutBox.isTable() && toLayoutTable(&m_layoutBox)->collapseBorders()))
        paintBorder(m_layoutBox, paintInfo, paintRect, style, boxDecorationData.bleedAvoidance);

    if (boxDecorationData.bleedAvoidance == BackgroundBleedClipLayer)
        paintInfo.context->endLayer();
}

static bool skipBodyBackground(const LayoutBox* bodyElementLayoutObject)
{
    ASSERT(bodyElementLayoutObject->isBody());
    // The <body> only paints its background if the root element has defined a background independent of the body,
    // or if the <body>'s parent is not the document element's layoutObject (e.g. inside SVG foreignObject).
    LayoutObject* documentElementLayoutObject = bodyElementLayoutObject->document().documentElement()->layoutObject();
    return documentElementLayoutObject
        && !documentElementLayoutObject->hasBackground()
        && (documentElementLayoutObject == bodyElementLayoutObject->parent());
}

void BoxPainter::paintBackground(const PaintInfo& paintInfo, const LayoutRect& paintRect, const Color& backgroundColor, BackgroundBleedAvoidance bleedAvoidance)
{
    if (m_layoutBox.isDocumentElement()) {
        paintRootBoxFillLayers(paintInfo);
        return;
    }
    if (m_layoutBox.isBody() && skipBodyBackground(&m_layoutBox))
        return;
    if (m_layoutBox.boxDecorationBackgroundIsKnownToBeObscured())
        return;
    paintFillLayers(paintInfo, backgroundColor, m_layoutBox.style()->backgroundLayers(), paintRect, bleedAvoidance);
}

void BoxPainter::paintRootBoxFillLayers(const PaintInfo& paintInfo)
{
    if (paintInfo.skipRootBackground())
        return;

    LayoutObject* rootBackgroundLayoutObject = m_layoutBox.layoutObjectForRootBackground();

    const FillLayer& bgLayer = rootBackgroundLayoutObject->style()->backgroundLayers();
    Color bgColor = rootBackgroundLayoutObject->resolveColor(CSSPropertyBackgroundColor);

    paintFillLayers(paintInfo, bgColor, bgLayer, rootBackgroundRect(), BackgroundBleedNone, SkXfermode::kSrcOver_Mode, rootBackgroundLayoutObject);
}

void BoxPainter::paintFillLayers(const PaintInfo& paintInfo, const Color& c, const FillLayer& fillLayer, const LayoutRect& rect,
    BackgroundBleedAvoidance bleedAvoidance, SkXfermode::Mode op, LayoutObject* backgroundObject)
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
        // LayoutBoxModelObject::paintFillLayerExtended. A more efficient solution might be to move
        // the layer recursion into paintFillLayerExtended, or to compute the layer geometry here
        // and pass it down.

        if (!shouldDrawBackgroundInSeparateBuffer && curLayer->blendMode() != WebBlendModeNormal)
            shouldDrawBackgroundInSeparateBuffer = true;

        if (curLayer->clipOccludesNextLayers()
            && curLayer->hasOpaqueImage(&m_layoutBox)
            && curLayer->image()->canRender(m_layoutBox, m_layoutBox.style()->effectiveZoom())
            && curLayer->hasRepeatXY()
            && curLayer->blendMode() == WebBlendModeNormal
            && !m_layoutBox.boxShadowShouldBeAppliedToBackground(bleedAvoidance))
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
        if (isBaseColorVisible && isDocumentElementWithOpaqueBackground(m_layoutBox)) {
            paintRootBackgroundColor(m_layoutBox, paintInfo, rect, Color());
            skipBaseColor = true;
        }
        context->beginLayer();
    }

    Vector<const FillLayer*>::const_reverse_iterator topLayer = layers.rend();
    for (Vector<const FillLayer*>::const_reverse_iterator it = layers.rbegin(); it != topLayer; ++it)
        paintFillLayer(paintInfo, c, **it, rect, bleedAvoidance, op, backgroundObject, skipBaseColor);

    if (shouldDrawBackgroundInSeparateBuffer)
        context->endLayer();
}

void BoxPainter::paintFillLayer(const PaintInfo& paintInfo, const Color& c, const FillLayer& fillLayer, const LayoutRect& rect,
    BackgroundBleedAvoidance bleedAvoidance, SkXfermode::Mode op, LayoutObject* backgroundObject, bool skipBaseColor)
{
    BoxPainter::paintFillLayerExtended(m_layoutBox, paintInfo, c, fillLayer, rect, bleedAvoidance, 0, LayoutSize(), op, backgroundObject, skipBaseColor);
}

void BoxPainter::applyBoxShadowForBackground(GraphicsContext* context, LayoutObject& obj)
{
    const ShadowList* shadowList = obj.style()->boxShadow();
    ASSERT(shadowList);
    for (size_t i = shadowList->shadows().size(); i--; ) {
        const ShadowData& boxShadow = shadowList->shadows()[i];
        if (boxShadow.style() != Normal)
            continue;
        FloatSize shadowOffset(boxShadow.x(), boxShadow.y());
        context->setShadow(shadowOffset, boxShadow.blur(),
            boxShadow.color().resolve(obj.resolveColor(CSSPropertyColor)),
            DrawLooperBuilder::ShadowRespectsTransforms, DrawLooperBuilder::ShadowIgnoresAlpha);
        return;
    }
}

FloatRoundedRect BoxPainter::getBackgroundRoundedRect(LayoutObject& obj, const LayoutRect& borderRect,
    InlineFlowBox* box, LayoutUnit inlineBoxWidth, LayoutUnit inlineBoxHeight,
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

FloatRoundedRect BoxPainter::backgroundRoundedRectAdjustedForBleedAvoidance(LayoutObject& obj,
    const LayoutRect& borderRect, BackgroundBleedAvoidance bleedAvoidance, InlineFlowBox* box,
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
    if (bleedAvoidance == BackgroundBleedBackgroundOverBorder)
        return obj.style()->getRoundedInnerBorderFor(borderRect, includeLogicalLeftEdge, includeLogicalRightEdge);

    return getBackgroundRoundedRect(obj, borderRect, box, boxSize.width(), boxSize.height(), includeLogicalLeftEdge, includeLogicalRightEdge);
}

void BoxPainter::paintFillLayerExtended(LayoutBoxModelObject& obj, const PaintInfo& paintInfo, const Color& color, const FillLayer& bgLayer, const LayoutRect& rect,
    BackgroundBleedAvoidance bleedAvoidance, InlineFlowBox* box, const LayoutSize& boxSize, SkXfermode::Mode op, LayoutObject* backgroundObject, bool skipBaseColor)
{
    GraphicsContext* context = paintInfo.context;
    if (rect.isEmpty())
        return;

    bool includeLeftEdge = box ? box->includeLogicalLeftEdge() : true;
    bool includeRightEdge = box ? box->includeLogicalRightEdge() : true;

    bool hasRoundedBorder = obj.style()->hasBorderRadius() && (includeLeftEdge || includeRightEdge);
    bool clippedWithLocalScrolling = obj.hasOverflowClip() && bgLayer.attachment() == LocalBackgroundAttachment;
    bool isBorderFill = bgLayer.clip() == BorderFillBox;
    bool isDocumentElementLayoutObject = obj.isDocumentElement();
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
    // while layout.)
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
    if (!isDocumentElementLayoutObject && !clippedWithLocalScrolling && !shouldPaintBackgroundImage && isBorderFill && isBottomLayer) {
        if (!colorVisible)
            return;

        bool boxShadowShouldBeAppliedToBackground = obj.boxShadowShouldBeAppliedToBackground(bleedAvoidance, box);
        GraphicsContextStateSaver shadowStateSaver(*context, boxShadowShouldBeAppliedToBackground);
        if (boxShadowShouldBeAppliedToBackground)
            BoxPainter::applyBoxShadowForBackground(context, obj);

        if (hasRoundedBorder && !bleedAvoidanceIsClipping(bleedAvoidance)) {
            FloatRoundedRect border = backgroundRoundedRectAdjustedForBleedAvoidance(obj, rect,
                bleedAvoidance, box, boxSize, includeLeftEdge, includeRightEdge);

            if (border.isRenderable()) {
                context->fillRoundedRect(border, bgColor);
            } else {
                RoundedInnerRectClipper clipper(obj, paintInfo, rect, border, ApplyToContext);
                context->fillRect(border.rect(), bgColor);
            }
        } else {
            context->fillRect(pixelSnappedIntRect(rect), bgColor);
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

    GraphicsContextStateSaver clipWithScrollingStateSaver(*context, clippedWithLocalScrolling);
    LayoutRect scrolledPaintRect = rect;
    if (clippedWithLocalScrolling) {
        // Clip to the overflow area.
        LayoutBox* thisBox = toLayoutBox(&obj);
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
        if (!RuntimeEnabledFeatures::slimmingPaintEnabled())
            maskRect.intersect(paintInfo.rect);

        // We draw the background into a separate layer, to be later masked with yet another layer
        // holding the text content.
        backgroundClipStateSaver.save();
        context->clip(maskRect);
        context->beginLayer();

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
        bool isOpaqueRoot = (isDocumentElementLayoutObject && !bgColor.hasAlpha()) || isDocumentElementWithOpaqueBackground(obj);
        if (boxShadowShouldBeAppliedToBackground || !shouldPaintBackgroundImage || !bgLayer.hasOpaqueImage(&obj) || !bgLayer.hasRepeatXY() || (isOpaqueRoot && !toLayoutBox(&obj)->size().height()))  {
            if (!RuntimeEnabledFeatures::slimmingPaintEnabled() && !boxShadowShouldBeAppliedToBackground)
                backgroundRect.intersect(paintInfo.rect);

            GraphicsContextStateSaver shadowStateSaver(*context, boxShadowShouldBeAppliedToBackground);
            if (boxShadowShouldBeAppliedToBackground)
                BoxPainter::applyBoxShadowForBackground(context, obj);

            if (isOpaqueRoot && !skipBaseColor) {
                paintRootBackgroundColor(obj, paintInfo, rect, bgColor);
            } else if (bgColor.alpha()) {
                context->fillRect(backgroundRect, bgColor);
            }
        }
    }

    // no progressive loading of the background image
    if (shouldPaintBackgroundImage) {
        BackgroundImageGeometry geometry;
        calculateBackgroundImageGeometry(obj, paintInfo.paintContainer(), bgLayer, scrolledPaintRect, geometry, backgroundObject);
        if (!geometry.destRect().isEmpty()) {
            SkXfermode::Mode bgOp = WebCoreCompositeToSkiaComposite(bgLayer.composite(), bgLayer.blendMode());
            // if op != SkXfermode::kSrcOver_Mode, a mask is being painted.
            SkXfermode::Mode compositeOp = op == SkXfermode::kSrcOver_Mode ? bgOp : op;
            LayoutObject* clientForBackgroundImage = backgroundObject ? backgroundObject : &obj;
            RefPtr<Image> image = bgImage->image(clientForBackgroundImage, geometry.tileSize());
            InterpolationQuality interpolationQuality = chooseInterpolationQuality(*clientForBackgroundImage, context, image.get(), &bgLayer, LayoutSize(geometry.tileSize()));
            if (bgLayer.maskSourceType() == MaskLuminance)
                context->setColorFilter(ColorFilterLuminanceToAlpha);
            InterpolationQuality previousInterpolationQuality = context->imageInterpolationQuality();
            context->setImageInterpolationQuality(interpolationQuality);
            TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "PaintImage", "data", InspectorPaintImageEvent::data(obj, *bgImage));
            context->drawTiledImage(image.get(), geometry.destRect(), geometry.phase(), geometry.tileSize(),
                compositeOp, geometry.spaceSize());
            context->setImageInterpolationQuality(previousInterpolationQuality);
        }
    }

    if (bgLayer.clip() == TextFillBox) {
        // Create the text mask layer.
        context->beginLayer(1, SkXfermode::kDstIn_Mode);

        // FIXME: Workaround for https://code.google.com/p/skia/issues/detail?id=1291.
        context->clearRect(maskRect);

        // Now draw the text into the mask. We do this by painting using a special paint phase that signals to
        // InlineTextBoxes that they should just add their contents to the clip.
        PaintInfo info(context, maskRect, PaintPhaseTextClip, PaintBehaviorForceBlackText, 0);
        if (box) {
            RootInlineBox& root = box->root();
            box->paint(info, LayoutPoint(scrolledPaintRect.x() - box->x(), scrolledPaintRect.y() - box->y()), root.lineTop(), root.lineBottom());
        } else {
            // FIXME: this should only have an effect for the line box list within |obj|. Change this to create a LineBoxListPainter directly.
            LayoutSize localOffset = obj.isBox() ? toLayoutBox(&obj)->locationOffset() : LayoutSize();
            obj.paint(info, scrolledPaintRect.location() - localOffset);
        }

        context->endLayer();
        context->endLayer();
    }
}

void BoxPainter::paintMask(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!paintInfo.shouldPaintWithinRoot(&m_layoutBox) || m_layoutBox.style()->visibility() != VISIBLE || paintInfo.phase != PaintPhaseMask)
        return;

    LayoutRect paintRect = LayoutRect(paintOffset, m_layoutBox.size());
    paintMaskImages(paintInfo, paintRect);
}

void BoxPainter::paintMaskImages(const PaintInfo& paintInfo, const LayoutRect& paintRect)
{
    // Figure out if we need to push a transparency layer to render our mask.
    bool pushTransparencyLayer = false;
    bool compositedMask = m_layoutBox.hasLayer() && m_layoutBox.layer()->hasCompositedMask();
    bool flattenCompositingLayers = m_layoutBox.view()->frameView() && m_layoutBox.view()->frameView()->paintBehavior() & PaintBehaviorFlattenCompositingLayers;

    bool allMaskImagesLoaded = true;

    if (!compositedMask || flattenCompositingLayers) {
        pushTransparencyLayer = true;
        StyleImage* maskBoxImage = m_layoutBox.style()->maskBoxImage().image();
        const FillLayer& maskLayers = m_layoutBox.style()->maskLayers();

        // Don't render a masked element until all the mask images have loaded, to prevent a flash of unmasked content.
        if (maskBoxImage)
            allMaskImagesLoaded &= maskBoxImage->isLoaded();

        allMaskImagesLoaded &= maskLayers.imagesAreLoaded();

        paintInfo.context->beginLayer(1, SkXfermode::kDstIn_Mode);
    }

    if (allMaskImagesLoaded) {
        paintFillLayers(paintInfo, Color::transparent, m_layoutBox.style()->maskLayers(), paintRect);
        paintNinePieceImage(m_layoutBox, paintInfo.context, paintRect, m_layoutBox.styleRef(), m_layoutBox.style()->maskBoxImage());
    }

    if (pushTransparencyLayer)
        paintInfo.context->endLayer();
}

void BoxPainter::paintClippingMask(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT(paintInfo.phase == PaintPhaseClippingMask);

    if (!paintInfo.shouldPaintWithinRoot(&m_layoutBox) || m_layoutBox.style()->visibility() != VISIBLE)
        return;

    if (!m_layoutBox.layer() || m_layoutBox.layer()->compositingState() != PaintsIntoOwnBacking)
        return;

    IntRect paintRect = pixelSnappedIntRect(LayoutRect(paintOffset, m_layoutBox.size()));
    LayoutObjectDrawingRecorder drawingRecorder(*paintInfo.context, m_layoutBox, paintInfo.phase, paintRect);
    if (drawingRecorder.canUseCachedDrawing())
        return;

    paintInfo.context->fillRect(paintRect, Color::black);
}

void BoxPainter::paintRootBackgroundColor(LayoutObject& obj, const PaintInfo& paintInfo, const LayoutRect& rootBackgroundRect, const Color& bgColor)
{
    if (rootBackgroundRect.isEmpty())
        return;

    ASSERT(obj.isDocumentElement());

    IntRect backgroundRect(pixelSnappedIntRect(rootBackgroundRect));
    if (!RuntimeEnabledFeatures::slimmingPaintEnabled())
        backgroundRect.intersect(paintInfo.rect);

    Color baseColor = obj.view()->frameView()->baseBackgroundColor();
    bool shouldClearDocumentBackground = obj.document().settings() && obj.document().settings()->shouldClearDocumentBackground();
    SkXfermode::Mode operation = shouldClearDocumentBackground ?
        SkXfermode::kSrc_Mode : SkXfermode::kSrcOver_Mode;

    GraphicsContext* context = paintInfo.context;

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

bool BoxPainter::isDocumentElementWithOpaqueBackground(LayoutObject& obj)
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
            // to crawl around a layout tree with potential :before/:after content and
            // anonymous blocks created by inline <body> tags etc. We can locate the <body>
            // layout object very easily via the DOM.
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

// Return the amount of space to leave between image tiles for the background-repeat: space property.
static inline int getSpaceBetweenImageTiles(int areaSize, int tileSize)
{
    int numberOfTiles = areaSize / tileSize;
    int space = -1;

    if (numberOfTiles > 1) {
        // Spec doesn't specify rounding, so use the same method as for background-repeat: round.
        space = lroundf((areaSize - numberOfTiles * tileSize) / (float)(numberOfTiles - 1));
    }

    return space;
}

void BoxPainter::calculateBackgroundImageGeometry(LayoutBoxModelObject& obj, const LayoutBoxModelObject* paintContainer, const FillLayer& fillLayer, const LayoutRect& paintRect,
    BackgroundImageGeometry& geometry, LayoutObject* backgroundObject)
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
            positioningAreaSize = pixelSnappedIntSize(toLayoutBox(&obj)->size() - LayoutSize(left + right, top + bottom), toLayoutBox(&obj)->location());
            // The positioning area is right aligned in paintRect if RightToLeftWritingMode, or left aligned otherwise.
            if (obj.style()->writingMode() == RightToLeftWritingMode)
                left = paintRect.width() - positioningAreaSize.width() - right - obj.marginRight();
            else
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
            viewportRect.setLocation(IntPoint(frameView->scrollOffsetForViewportConstrainedObjects()));

        if (paintContainer) {
            IntPoint absoluteContainerOffset = roundedIntPoint(paintContainer->localToAbsolute(FloatPoint()));
            viewportRect.moveBy(-absoluteContainerOffset);
        }

        geometry.setDestRect(viewportRect);
        positioningAreaSize = geometry.destRect().size();
    }

    const LayoutObject* clientForBackgroundImage = backgroundObject ? backgroundObject : &obj;
    IntSize fillTileSize = calculateFillTileSize(obj, fillLayer, positioningAreaSize);
    fillLayer.image()->setContainerSizeForLayoutObject(clientForBackgroundImage, fillTileSize, obj.style()->effectiveZoom());
    geometry.setTileSize(fillTileSize);

    EFillRepeat backgroundRepeatX = fillLayer.repeatX();
    EFillRepeat backgroundRepeatY = fillLayer.repeatY();
    int availableWidth = positioningAreaSize.width() - geometry.tileSize().width();
    int availableHeight = positioningAreaSize.height() - geometry.tileSize().height();

    LayoutUnit computedXPosition = roundedMinimumValueForLength(fillLayer.xPosition(), availableWidth);
    if (backgroundRepeatX == RoundFill && positioningAreaSize.width() > 0 && fillTileSize.width() > 0) {
        long nrTiles = std::max(1l, lroundf((float)positioningAreaSize.width() / fillTileSize.width()));

        // Round tile size per css3-background spec.
        fillTileSize.setWidth(lroundf(positioningAreaSize.width() / (float)nrTiles));

        // Maintain aspect ratio if background-size: auto is set
        if (fillLayer.size().size.height().isAuto() && backgroundRepeatY != RoundFill) {
            fillTileSize.setHeight(fillTileSize.height() * positioningAreaSize.width() / (nrTiles * fillTileSize.width()));
        }

        geometry.setTileSize(fillTileSize);
        geometry.setPhaseX(geometry.tileSize().width() ? geometry.tileSize().width() - roundToInt(computedXPosition + left) % geometry.tileSize().width() : 0);
        geometry.setSpaceSize(IntSize());
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

        geometry.setTileSize(fillTileSize);
        geometry.setPhaseY(geometry.tileSize().height() ? geometry.tileSize().height() - roundToInt(computedYPosition + top) % geometry.tileSize().height() : 0);
        geometry.setSpaceSize(IntSize());
    }

    if (backgroundRepeatX == RepeatFill) {
        geometry.setPhaseX(geometry.tileSize().width() ? geometry.tileSize().width() - roundToInt(computedXPosition + left) % geometry.tileSize().width() : 0);
        geometry.setSpaceSize(IntSize());
    } else if (backgroundRepeatX == SpaceFill && fillTileSize.width() > 0) {
        int space = getSpaceBetweenImageTiles(positioningAreaSize.width(), geometry.tileSize().width());
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
        int space = getSpaceBetweenImageTiles(positioningAreaSize.height(), geometry.tileSize().height());
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
}

InterpolationQuality BoxPainter::chooseInterpolationQuality(LayoutObject& obj, GraphicsContext* context, Image* image, const void* layer, const LayoutSize& size)
{
    return ImageQualityController::imageQualityController()->chooseInterpolationQuality(context, &obj, image, layer, size);
}

bool BoxPainter::fixedBackgroundPaintsInLocalCoordinates(const LayoutObject& obj)
{
    if (!obj.isDocumentElement())
        return false;

    if (obj.view()->frameView() && obj.view()->frameView()->paintBehavior() & PaintBehaviorFlattenCompositingLayers)
        return false;

    DeprecatedPaintLayer* rootLayer = obj.view()->layer();
    if (!rootLayer || rootLayer->compositingState() == NotComposited)
        return false;

    return rootLayer->compositedDeprecatedPaintLayerMapping()->backgroundLayerPaintsFixedRootBackground();
}

static inline void applySubPixelHeuristicForTileSize(LayoutSize& tileSize, const IntSize& positioningAreaSize)
{
    tileSize.setWidth(positioningAreaSize.width() - tileSize.width() <= 1 ? tileSize.width().ceil() : tileSize.width().floor());
    tileSize.setHeight(positioningAreaSize.height() - tileSize.height() <= 1 ? tileSize.height().ceil() : tileSize.height().floor());
}

IntSize BoxPainter::calculateFillTileSize(const LayoutBoxModelObject& obj, const FillLayer& fillLayer, const IntSize& positioningAreaSize)
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

static LayoutUnit computeBorderImageSide(const BorderImageLength& borderSlice, LayoutUnit borderSide, LayoutUnit imageSide, LayoutUnit boxExtent)
{
    if (borderSlice.isNumber())
        return borderSlice.number() * borderSide;
    if (borderSlice.length().isAuto())
        return imageSide;
    return valueForLength(borderSlice.length(), boxExtent);
}

bool BoxPainter::paintNinePieceImage(LayoutBoxModelObject& obj, GraphicsContext* graphicsContext, const LayoutRect& rect, const ComputedStyle& style, const NinePieceImage& ninePieceImage, SkXfermode::Mode op)
{
    StyleImage* styleImage = ninePieceImage.image();
    if (!styleImage)
        return false;

    if (!styleImage->isLoaded())
        return true; // Never paint a nine-piece image incrementally, but don't paint the fallback borders either.

    if (!styleImage->canRender(obj, style.effectiveZoom()))
        return false;

    // FIXME: border-image is broken with full page zooming when tiling has to happen, since the tiling function
    // doesn't have any understanding of the zoom that is in effect on the tile.
    LayoutRect rectWithOutsets = rect;
    rectWithOutsets.expand(style.imageOutsets(ninePieceImage));
    IntRect borderImageRect = pixelSnappedIntRect(rectWithOutsets);

    IntSize imageSize = obj.calculateImageIntrinsicDimensions(styleImage, borderImageRect.size(), LayoutBoxModelObject::DoNotScaleByEffectiveZoom);

    // If both values are 'auto' then the intrinsic width and/or height of the image should be used, if any.
    styleImage->setContainerSizeForLayoutObject(&obj, imageSize, style.effectiveZoom());

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

    RefPtr<Image> image = styleImage->image(&obj, imageSize);

    float destinationWidth = borderImageRect.width() - leftWidth - rightWidth;
    float destinationHeight = borderImageRect.height() - topWidth - bottomWidth;

    float sourceWidth = imageWidth - leftSlice - rightSlice;
    float sourceHeight = imageHeight - topSlice - bottomSlice;

    float leftSideScale = drawLeft ? (float)leftWidth / leftSlice : 1;
    float rightSideScale = drawRight ? (float)rightWidth / rightSlice : 1;
    float topSideScale = drawTop ? (float)topWidth / topSlice : 1;
    float bottomSideScale = drawBottom ? (float)bottomWidth / bottomSlice : 1;

    InterpolationQuality interpolationQuality = chooseInterpolationQuality(obj, graphicsContext, image.get(), 0, rectWithOutsets.size());
    InterpolationQuality previousInterpolationQuality = graphicsContext->imageInterpolationQuality();
    graphicsContext->setImageInterpolationQuality(interpolationQuality);

    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "PaintImage", "data", InspectorPaintImageEvent::data(obj, *styleImage));
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

bool BoxPainter::shouldAntialiasLines(GraphicsContext* context)
{
    // FIXME: We may want to not antialias when scaled by an integral value,
    // and we may want to antialias when translated by a non-integral value.
    // FIXME: See crbug.com/382491. getCTM does not include scale factors applied at raster time, such
    // as device zoom.
    return !context->getCTM().isIdentityOrTranslationOrFlipped();
}

bool BoxPainter::allCornersClippedOut(const FloatRoundedRect& border, const IntRect& intClipRect)
{
    LayoutRect boundingRect(border.rect());
    LayoutRect clipRect(intClipRect);
    if (clipRect.contains(boundingRect))
        return false;

    FloatRoundedRect::Radii radii = border.radii();

    LayoutRect topLeftRect(boundingRect.location(), LayoutSize(radii.topLeft()));
    if (clipRect.intersects(topLeftRect))
        return false;

    LayoutRect topRightRect(boundingRect.location(), LayoutSize(radii.topRight()));
    topRightRect.setX(boundingRect.maxX() - topRightRect.width());
    if (clipRect.intersects(topRightRect))
        return false;

    LayoutRect bottomLeftRect(boundingRect.location(), LayoutSize(radii.bottomLeft()));
    bottomLeftRect.setY(boundingRect.maxY() - bottomLeftRect.height());
    if (clipRect.intersects(bottomLeftRect))
        return false;

    LayoutRect bottomRightRect(boundingRect.location(), LayoutSize(radii.bottomRight()));
    bottomRightRect.setX(boundingRect.maxX() - bottomRightRect.width());
    bottomRightRect.setY(boundingRect.maxY() - bottomRightRect.height());
    if (clipRect.intersects(bottomRightRect))
        return false;

    return true;
}

void BoxPainter::paintBorder(LayoutBoxModelObject& obj, const PaintInfo& info, const LayoutRect& rect, const ComputedStyle& style, BackgroundBleedAvoidance bleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
{
    BoxBorderPainter().paintBorder(obj, info, rect, style, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge);
}

void BoxPainter::paintBoxShadow(const PaintInfo& info, const LayoutRect& paintRect, const ComputedStyle& style, ShadowStyle shadowStyle, bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
{
    // FIXME: Deal with border-image. Would be great to use border-image as a mask.
    GraphicsContext* context = info.context;
    if (!style.boxShadow())
        return;
    FloatRoundedRect border = (shadowStyle == Inset) ? style.getRoundedInnerBorderFor(paintRect, includeLogicalLeftEdge, includeLogicalRightEdge)
        : style.getRoundedBorderFor(paintRect, includeLogicalLeftEdge, includeLogicalRightEdge);

    bool hasBorderRadius = style.hasBorderRadius();
    bool isHorizontal = style.isHorizontalWritingMode();
    bool hasOpaqueBackground = style.visitedDependentColor(CSSPropertyBackgroundColor).alpha() == 255;

    GraphicsContextStateSaver stateSaver(*context, false);

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
                        context->clipOutRoundedRect(rectToClipOut);
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
                        context->clipOut(rectToClipOut);
                }
            }

            // Draw only the shadow.
            context->setShadow(shadowOffset, shadowBlur, shadowColor, DrawLooperBuilder::ShadowRespectsTransforms, DrawLooperBuilder::ShadowIgnoresAlpha, DrawShadowOnly);

            if (hasBorderRadius) {
                FloatRoundedRect influenceRect(pixelSnappedIntRect(LayoutRect(shadowRect)), border.radii());
                float changeAmount = 2 * shadowBlur + shadowSpread;
                if (changeAmount >= 0)
                    influenceRect.expandRadii(changeAmount);
                else
                    influenceRect.shrinkRadii(-changeAmount);

                if (allCornersClippedOut(influenceRect, info.rect)) {
                    context->fillRect(fillRect, Color::black);
                } else {
                    // TODO: support non-integer shadows - crbug.com/334829
                    FloatRoundedRect roundedFillRect = border;
                    roundedFillRect.inflate(shadowSpread);

                    if (shadowSpread >= 0)
                        roundedFillRect.expandRadii(shadowSpread);
                    else
                        roundedFillRect.shrinkRadii(-shadowSpread);
                    if (!roundedFillRect.isRenderable())
                        roundedFillRect.adjustRadii();
                    roundedFillRect.constrainRadii();
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

} // namespace blink
