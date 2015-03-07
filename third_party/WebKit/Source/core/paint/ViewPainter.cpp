// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/ViewPainter.h"

#include "core/frame/FrameView.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutView.h"
#include "core/layout/PaintInfo.h"
#include "core/paint/BlockPainter.h"
#include "core/paint/GraphicsContextAnnotator.h"
#include "core/paint/RenderDrawingRecorder.h"

namespace blink {

void ViewPainter::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    // If we ever require layout but receive a paint anyway, something has gone horribly wrong.
    ASSERT(!m_layoutView.needsLayout());
    // LayoutViews should never be called to paint with an offset not on device pixels.
    ASSERT(LayoutPoint(IntPoint(paintOffset.x(), paintOffset.y())) == paintOffset);

    ANNOTATE_GRAPHICS_CONTEXT(paintInfo, &m_layoutView);

    // This avoids painting garbage between columns if there is a column gap.
    if (m_layoutView.frameView() && m_layoutView.style()->isOverflowPaged()) {
        LayoutRect paintRect(paintInfo.rect);
        if (RuntimeEnabledFeatures::slimmingPaintEnabled())
            paintRect = m_layoutView.viewRect();

        RenderDrawingRecorder recorder(paintInfo.context, m_layoutView, DisplayItem::ViewBackground, paintRect);
        if (!recorder.canUseCachedDrawing())
            paintInfo.context->fillRect(paintRect, m_layoutView.frameView()->baseBackgroundColor());
    }

    m_layoutView.paintObject(paintInfo, paintOffset);
    BlockPainter(m_layoutView).paintOverflowControlsIfNeeded(paintInfo, paintOffset);
}

static inline bool rendererObscuresBackground(LayoutBox* rootBox)
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
    if (m_layoutView.document().ownerElement() || !m_layoutView.view())
        return;

    if (paintInfo.skipRootBackground())
        return;

    bool shouldPaintBackground = true;
    Node* documentElement = m_layoutView.document().documentElement();
    if (LayoutBox* rootBox = documentElement ? toLayoutBox(documentElement->layoutObject()) : 0)
        shouldPaintBackground = !rootFillsViewportBackground(rootBox) || !rendererObscuresBackground(rootBox);

    // If painting will entirely fill the view, no need to fill the background.
    if (!shouldPaintBackground)
        return;

    // This code typically only executes if the root element's visibility has been set to hidden,
    // if there is a transform on the <html>, or if there is a page scale factor less than 1.
    // Only fill with the base background color (typically white) if we're the root document,
    // since iframes/frames with no background in the child document should show the parent's background.
    if (!m_layoutView.frameView()->isTransparent()) {
        LayoutRect paintRect(paintInfo.rect);
        if (RuntimeEnabledFeatures::slimmingPaintEnabled())
            paintRect = m_layoutView.viewRect();

        RenderDrawingRecorder recorder(paintInfo.context, m_layoutView, DisplayItem::BoxDecorationBackground, m_layoutView.viewRect());
        if (!recorder.canUseCachedDrawing()) {
            Color baseColor = m_layoutView.frameView()->baseBackgroundColor();
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

bool ViewPainter::rootFillsViewportBackground(LayoutBox* rootBox) const
{
    ASSERT(rootBox);
    // CSS Boxes always fill the viewport background (see paintRootBoxFillLayers)
    if (!rootBox->isSVG())
        return true;

    return rootBox->frameRect().contains(m_layoutView.frameRect());
}

} // namespace blink
