// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/BoxPainter.h"

#include "core/paint/BoxDecorationData.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderBox.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderTable.h"
#include "core/rendering/RenderTheme.h"
#include "core/rendering/RenderView.h"
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
        if (isBaseColorVisible && m_renderBox.isDocumentElementWithOpaqueBackground()) {
            m_renderBox.paintRootBackgroundColor(paintInfo, rect, Color());
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
    m_renderBox.paintFillLayerExtended(paintInfo, c, fillLayer, rect, bleedAvoidance, 0, LayoutSize(), op, backgroundObject, skipBaseColor);
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

} // namespace blink
