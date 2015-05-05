// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/SVGContainerPainter.h"

#include "core/layout/svg/LayoutSVGContainer.h"
#include "core/layout/svg/LayoutSVGViewportContainer.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/paint/FloatClipRecorder.h"
#include "core/paint/GraphicsContextAnnotator.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/SVGPaintContext.h"
#include "core/paint/TransformRecorder.h"
#include "core/svg/SVGSVGElement.h"

namespace blink {

void SVGContainerPainter::paint(const PaintInfo& paintInfo)
{
    ANNOTATE_GRAPHICS_CONTEXT(paintInfo, &m_layoutSVGContainer);

    // Spec: groups w/o children still may render filter content.
    if (!m_layoutSVGContainer.firstChild() && !m_layoutSVGContainer.selfWillPaint())
        return;

    // Spec: An empty viewBox on the <svg> element disables rendering.
    ASSERT(m_layoutSVGContainer.element());
    if (isSVGSVGElement(*m_layoutSVGContainer.element()) && toSVGSVGElement(*m_layoutSVGContainer.element()).hasEmptyViewBox())
        return;

    PaintInfo paintInfoBeforeFiltering(paintInfo);
    TransformRecorder transformRecorder(*paintInfoBeforeFiltering.context, m_layoutSVGContainer, m_layoutSVGContainer.localToParentTransform());
    {
        OwnPtr<FloatClipRecorder> clipRecorder;
        if (m_layoutSVGContainer.isSVGViewportContainer() && SVGLayoutSupport::isOverflowHidden(&m_layoutSVGContainer)) {
            FloatRect viewport = m_layoutSVGContainer.localToParentTransform().inverse().mapRect(toLayoutSVGViewportContainer(m_layoutSVGContainer).viewport());
            clipRecorder = adoptPtr(new FloatClipRecorder(*paintInfoBeforeFiltering.context, m_layoutSVGContainer, paintInfoBeforeFiltering.phase, viewport));
        }

        SVGPaintContext paintContext(m_layoutSVGContainer, paintInfoBeforeFiltering);
        bool continueRendering = true;
        if (paintContext.paintInfo().phase == PaintPhaseForeground)
            continueRendering = paintContext.applyClipMaskAndFilterIfNecessary();

        if (continueRendering) {
            paintContext.paintInfo().updatePaintingRootForChildren(&m_layoutSVGContainer);
            for (LayoutObject* child = m_layoutSVGContainer.firstChild(); child; child = child->nextSibling())
                child->paint(paintContext.paintInfo(), IntPoint());
        }
    }

    if (paintInfoBeforeFiltering.phase == PaintPhaseForeground && m_layoutSVGContainer.style()->outlineWidth() && m_layoutSVGContainer.style()->visibility() == VISIBLE) {
        LayoutRect layoutBoundingBox(m_layoutSVGContainer.paintInvalidationRectInLocalCoordinates());
        ObjectPainter(m_layoutSVGContainer).paintOutline(paintInfoBeforeFiltering, layoutBoundingBox, layoutBoundingBox);
    }
}

} // namespace blink
