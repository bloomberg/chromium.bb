// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/ViewPainter.h"

#include "core/frame/FrameView.h"
#include "core/layout/PaintInfo.h"
#include "core/paint/BlockPainter.h"
#include "core/paint/GraphicsContextAnnotator.h"
#include "core/paint/RenderDrawingRecorder.h"
#include "core/rendering/RenderBox.h"
#include "core/rendering/RenderView.h"

namespace blink {

void ViewPainter::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    // If we ever require layout but receive a paint anyway, something has gone horribly wrong.
    ASSERT(!m_renderView.needsLayout());
    // RenderViews should never be called to paint with an offset not on device pixels.
    ASSERT(LayoutPoint(IntPoint(paintOffset.x(), paintOffset.y())) == paintOffset);

    ANNOTATE_GRAPHICS_CONTEXT(paintInfo, &m_renderView);

    // This avoids painting garbage between columns if there is a column gap.
    if (m_renderView.frameView() && m_renderView.style()->isOverflowPaged()) {
        LayoutRect paintRect = paintInfo.rect;
        if (RuntimeEnabledFeatures::slimmingPaintEnabled())
            paintRect = m_renderView.viewRect();

        RenderDrawingRecorder recorder(paintInfo.context, m_renderView, DisplayItem::ViewBackground, paintRect);
        if (!recorder.canUseCachedDrawing())
            paintInfo.context->fillRect(paintRect, m_renderView.frameView()->baseBackgroundColor());
    }

    m_renderView.paintObject(paintInfo, paintOffset);
    BlockPainter(m_renderView).paintOverflowControlsIfNeeded(paintInfo, paintOffset);
}

static inline bool rendererObscuresBackground(RenderBox* rootBox)
{
    ASSERT(rootBox);
    const LayoutStyle& style = rootBox->styleRef();
    if (style.visibility() != VISIBLE
        || style.opacity() != 1
        || style.hasFilter()
        || style.hasTransform())
        return false;

    if (rootBox->compositingState() == PaintsIntoOwnBacking)
        return false;

    const LayoutObject* rootRenderer = rootBox->rendererForRootBackground();
    if (rootRenderer->style()->backgroundClip() == TextFillBox)
        return false;

    return true;
}

void ViewPainter::paintBoxDecorationBackground(const PaintInfo& paintInfo)
{
    if (m_renderView.document().ownerElement() || !m_renderView.view())
        return;

    if (paintInfo.skipRootBackground())
        return;

    bool shouldPaintBackground = true;
    Node* documentElement = m_renderView.document().documentElement();
    if (RenderBox* rootBox = documentElement ? toRenderBox(documentElement->renderer()) : 0)
        shouldPaintBackground = !rootFillsViewportBackground(rootBox) || !rendererObscuresBackground(rootBox);

    // If painting will entirely fill the view, no need to fill the background.
    if (!shouldPaintBackground)
        return;

    // This code typically only executes if the root element's visibility has been set to hidden,
    // if there is a transform on the <html>, or if there is a page scale factor less than 1.
    // Only fill with the base background color (typically white) if we're the root document,
    // since iframes/frames with no background in the child document should show the parent's background.
    if (!m_renderView.frameView()->isTransparent()) {
        LayoutRect paintRect = paintInfo.rect;
        if (RuntimeEnabledFeatures::slimmingPaintEnabled())
            paintRect = m_renderView.viewRect();

        RenderDrawingRecorder recorder(paintInfo.context, m_renderView, DisplayItem::BoxDecorationBackground, m_renderView.viewRect());
        if (!recorder.canUseCachedDrawing()) {
            Color baseColor = m_renderView.frameView()->baseBackgroundColor();
            if (baseColor.alpha()) {
                SkXfermode::Mode previousOperation = paintInfo.context->compositeOperation();
                paintInfo.context->setCompositeOperation(SkXfermode::kSrc_Mode);
                paintInfo.context->fillRect(paintRect, baseColor);
                paintInfo.context->setCompositeOperation(previousOperation);
            } else {
                paintInfo.context->clearRect(paintRect);
            }
        }
    }
}

bool ViewPainter::rootFillsViewportBackground(RenderBox* rootBox) const
{
    ASSERT(rootBox);
    // CSS Boxes always fill the viewport background (see paintRootBoxFillLayers)
    if (!rootBox->isSVG())
        return true;

    return rootBox->frameRect().contains(m_renderView.frameRect());
}

} // namespace blink
