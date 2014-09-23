// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/ViewPainter.h"

#include "core/frame/FrameView.h"
#include "core/rendering/GraphicsContextAnnotator.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderBox.h"
#include "core/rendering/RenderView.h"

namespace blink {

void ViewPainter::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    // If we ever require layout but receive a paint anyway, something has gone horribly wrong.
    ASSERT(!m_renderView.needsLayout());
    // RenderViews should never be called to paint with an offset not on device pixels.
    ASSERT(LayoutPoint(IntPoint(paintOffset.x(), paintOffset.y())) == paintOffset);

    ANNOTATE_GRAPHICS_CONTEXT(paintInfo, &m_renderView);

    // This avoids painting garbage between columns if there is a column gap.
    if (m_renderView.frameView() && m_renderView.style()->isOverflowPaged())
        paintInfo.context->fillRect(paintInfo.rect, m_renderView.frameView()->baseBackgroundColor());

    m_renderView.paintObject(paintInfo, paintOffset);
}

static inline bool rendererObscuresBackground(RenderBox* rootBox)
{
    ASSERT(rootBox);
    RenderStyle* style = rootBox->style();
    if (style->visibility() != VISIBLE
        || style->opacity() != 1
        || style->hasFilter()
        || style->hasTransform())
        return false;

    if (rootBox->compositingState() == PaintsIntoOwnBacking)
        return false;

    const RenderObject* rootRenderer = rootBox->rendererForRootBackground();
    if (rootRenderer->style()->backgroundClip() == TextFillBox)
        return false;

    return true;
}

void ViewPainter::paintBoxDecorationBackground(PaintInfo& paintInfo)
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
        Color baseColor = m_renderView.frameView()->baseBackgroundColor();
        if (baseColor.alpha()) {
            CompositeOperator previousOperator = paintInfo.context->compositeOperation();
            paintInfo.context->setCompositeOperation(CompositeCopy);
            paintInfo.context->fillRect(paintInfo.rect, baseColor);
            paintInfo.context->setCompositeOperation(previousOperator);
        } else {
            paintInfo.context->clearRect(paintInfo.rect);
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
