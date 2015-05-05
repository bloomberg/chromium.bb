// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/ViewPainter.h"

#include "core/frame/FrameView.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutView.h"
#include "core/paint/BlockPainter.h"
#include "core/paint/GraphicsContextAnnotator.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/PaintInfo.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

void ViewPainter::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    // If we ever require layout but receive a paint anyway, something has gone horribly wrong.
    ASSERT(!m_layoutView.needsLayout());
    // LayoutViews should never be called to paint with an offset not on device pixels.
    ASSERT(LayoutPoint(IntPoint(paintOffset.x(), paintOffset.y())) == paintOffset);

    ANNOTATE_GRAPHICS_CONTEXT(paintInfo, &m_layoutView);

    // This avoids painting garbage between columns if there is a column gap.
    // This is legacy WebKit behavior and doesn't work with slimmingpaint. We can remove it once region-based columns are launched.
    if (!RuntimeEnabledFeatures::regionBasedColumnsEnabled() && m_layoutView.frameView() && m_layoutView.style()->isOverflowPaged()) {
        ASSERT(!RuntimeEnabledFeatures::slimmingPaintEnabled());
        LayoutRect paintRect(paintInfo.rect);
        paintInfo.context->fillRect(paintRect, m_layoutView.frameView()->baseBackgroundColor());
    }

    m_layoutView.paintObject(paintInfo, paintOffset);
    BlockPainter(m_layoutView).paintOverflowControlsIfNeeded(paintInfo, paintOffset);
}

static inline bool layoutObjectObscuresBackground(LayoutBox* rootBox)
{
    ASSERT(rootBox);
    const ComputedStyle& style = rootBox->styleRef();
    if (style.visibility() != VISIBLE
        || style.opacity() != 1
        || style.hasFilter()
        || style.hasTransform())
        return false;

    if (rootBox->compositingState() == PaintsIntoOwnBacking)
        return false;

    const LayoutObject* rootLayoutObject = rootBox->layoutObjectForRootBackground();
    if (rootLayoutObject->style()->backgroundClip() == TextFillBox)
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
        shouldPaintBackground = !rootFillsViewportBackground(rootBox) || !layoutObjectObscuresBackground(rootBox);

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

        LayoutObjectDrawingRecorder recorder(*paintInfo.context, m_layoutView, DisplayItem::BoxDecorationBackground, m_layoutView.viewRect());
        if (!recorder.canUseCachedDrawing()) {
            Color baseColor = m_layoutView.frameView()->baseBackgroundColor();
            paintInfo.context->fillRect(paintRect, baseColor, baseColor.alpha() ?
                SkXfermode::kSrc_Mode : SkXfermode::kClear_Mode);
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
