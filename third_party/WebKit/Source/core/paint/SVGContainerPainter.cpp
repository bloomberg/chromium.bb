// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/SVGContainerPainter.h"

#include "core/frame/Settings.h"
#include "core/paint/ObjectPainter.h"
#include "core/rendering/GraphicsContextAnnotator.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/svg/RenderSVGContainer.h"
#include "core/rendering/svg/RenderSVGViewportContainer.h"
#include "core/rendering/svg/SVGRenderSupport.h"
#include "core/rendering/svg/SVGRenderingContext.h"
#include "core/svg/SVGSVGElement.h"
#include "platform/graphics/GraphicsContextCullSaver.h"
#include "platform/graphics/GraphicsContextStateSaver.h"

namespace blink {

void SVGContainerPainter::paint(PaintInfo& paintInfo)
{
    ANNOTATE_GRAPHICS_CONTEXT(paintInfo, &m_renderSVGContainer);

    // Spec: groups w/o children still may render filter content.
    if (!m_renderSVGContainer.firstChild() && !m_renderSVGContainer.selfWillPaint())
        return;

    FloatRect paintInvalidationRect = m_renderSVGContainer.paintInvalidationRectInLocalCoordinates();
    if (!SVGRenderSupport::paintInfoIntersectsPaintInvalidationRect(paintInvalidationRect, m_renderSVGContainer.localToParentTransform(), paintInfo))
        return;

    // Spec: An empty viewBox on the <svg> element disables rendering.
    ASSERT(m_renderSVGContainer.element());
    if (isSVGSVGElement(*m_renderSVGContainer.element()) && toSVGSVGElement(*m_renderSVGContainer.element()).hasEmptyViewBox())
        return;

    PaintInfo childPaintInfo(paintInfo);
    {
        GraphicsContextStateSaver stateSaver(*childPaintInfo.context);

        if (m_renderSVGContainer.isSVGViewportContainer() && SVGRenderSupport::isOverflowHidden(&m_renderSVGContainer))
            paintInfo.context->clip(toRenderSVGViewportContainer(m_renderSVGContainer).viewport());

        childPaintInfo.applyTransform(m_renderSVGContainer.localToParentTransform());

        SVGRenderingContext renderingContext;
        GraphicsContextCullSaver cullSaver(*childPaintInfo.context);
        bool continueRendering = true;
        if (childPaintInfo.phase == PaintPhaseForeground) {
            renderingContext.prepareToRenderSVGContent(&m_renderSVGContainer, childPaintInfo);
            continueRendering = renderingContext.isRenderingPrepared();

            if (continueRendering && m_renderSVGContainer.document().settings()->containerCullingEnabled())
                cullSaver.cull(paintInvalidationRect);
        }

        if (continueRendering) {
            childPaintInfo.updatePaintingRootForChildren(&m_renderSVGContainer);
            for (RenderObject* child = m_renderSVGContainer.firstChild(); child; child = child->nextSibling())
                child->paint(childPaintInfo, IntPoint());
        }
    }

    // FIXME: This really should be drawn from local coordinates, but currently we hack it
    // to avoid our clip killing our outline rect. Thus we translate our
    // outline rect into parent coords before drawing.
    // FIXME: This means our focus ring won't share our rotation like it should.
    // We should instead disable our clip during PaintPhaseOutline
    if (paintInfo.phase == PaintPhaseForeground && m_renderSVGContainer.style()->outlineWidth() && m_renderSVGContainer.style()->visibility() == VISIBLE) {
        IntRect paintRectInParent = enclosingIntRect(m_renderSVGContainer.localToParentTransform().mapRect(paintInvalidationRect));
        ObjectPainter(m_renderSVGContainer).paintOutline(paintInfo, paintRectInParent);
    }
}

} // namespace blink
