// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/SVGContainerPainter.h"

#include "core/layout/PaintInfo.h"
#include "core/layout/svg/LayoutSVGContainer.h"
#include "core/layout/svg/LayoutSVGViewportContainer.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/paint/FloatClipRecorder.h"
#include "core/paint/GraphicsContextAnnotator.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/SVGPaintContext.h"
#include "core/paint/TransformRecorder.h"
#include "core/svg/SVGSVGElement.h"

namespace blink {

void SVGContainerPainter::paint(const PaintInfo& paintInfo)
{
    ANNOTATE_GRAPHICS_CONTEXT(paintInfo, &m_renderSVGContainer);

    // Spec: groups w/o children still may render filter content.
    if (!m_renderSVGContainer.firstChild() && !m_renderSVGContainer.selfWillPaint())
        return;

    // Spec: An empty viewBox on the <svg> element disables rendering.
    ASSERT(m_renderSVGContainer.element());
    if (isSVGSVGElement(*m_renderSVGContainer.element()) && toSVGSVGElement(*m_renderSVGContainer.element()).hasEmptyViewBox())
        return;

    PaintInfo paintInfoBeforeFiltering(paintInfo);
    TransformRecorder transformRecorder(*paintInfoBeforeFiltering.context, m_renderSVGContainer.displayItemClient(), m_renderSVGContainer.localToParentTransform());
    {
        OwnPtr<FloatClipRecorder> clipRecorder;
        if (m_renderSVGContainer.isSVGViewportContainer() && SVGLayoutSupport::isOverflowHidden(&m_renderSVGContainer)) {
            FloatRect viewport = m_renderSVGContainer.localToParentTransform().inverse().mapRect(toLayoutSVGViewportContainer(m_renderSVGContainer).viewport());
            clipRecorder = adoptPtr(new FloatClipRecorder(*paintInfoBeforeFiltering.context, m_renderSVGContainer.displayItemClient(), paintInfoBeforeFiltering.phase, viewport));
        }

        SVGPaintContext paintContext(m_renderSVGContainer, paintInfoBeforeFiltering);
        bool continueRendering = true;
        if (paintContext.paintInfo().phase == PaintPhaseForeground)
            continueRendering = paintContext.applyClipMaskAndFilterIfNecessary();

        if (continueRendering) {
            paintContext.paintInfo().updatePaintingRootForChildren(&m_renderSVGContainer);
            for (LayoutObject* child = m_renderSVGContainer.firstChild(); child; child = child->nextSibling())
                child->paint(paintContext.paintInfo(), IntPoint());
        }
    }

    if (paintInfoBeforeFiltering.phase == PaintPhaseForeground && m_renderSVGContainer.style()->outlineWidth() && m_renderSVGContainer.style()->visibility() == VISIBLE) {
        LayoutRect layoutBoundingBox(m_renderSVGContainer.paintInvalidationRectInLocalCoordinates());
        ObjectPainter(m_renderSVGContainer).paintOutline(paintInfoBeforeFiltering, layoutBoundingBox, layoutBoundingBox);
    }
}

} // namespace blink
