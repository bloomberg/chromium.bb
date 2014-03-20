/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 *
 * Portions are Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Other contributors:
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   David Baron <dbaron@fas.harvard.edu>
 *   Christian Biesinger <cbiesinger@web.de>
 *   Randall Jesup <rjesup@wgate.com>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Josh Soref <timeless@mac.com>
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#include "config.h"
#include "core/rendering/RenderLayerRepainter.h"

#include "core/rendering/FilterEffectRenderer.h"
#include "core/rendering/LayoutRectRecorder.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/compositing/CompositedLayerMapping.h"

namespace WebCore {

RenderLayerRepainter::RenderLayerRepainter(RenderLayerModelObject* renderer)
    : m_renderer(renderer)
    , m_repaintStatus(NeedsNormalRepaint)
{
}

void RenderLayerRepainter::repaintAfterLayout(RenderGeometryMap* geometryMap, bool shouldCheckForRepaint)
{
    if (RuntimeEnabledFeatures::repaintAfterLayoutEnabled())
        return;

    // FIXME: really, we're in the repaint phase here, and the following queries are legal.
    // Until those states are fully fledged, I'll just disable the ASSERTS.
    DisableCompositingQueryAsserts disabler;
    if (m_renderer->layer()->hasVisibleContent()) {
        RenderView* view = m_renderer->view();
        ASSERT(view);
        // FIXME: LayoutState does not work with RenderLayers as there is not a 1-to-1
        // mapping between them and the RenderObjects. It would be neat to enable
        // LayoutState outside the layout() phase and use it here.
        ASSERT(!view->layoutStateEnabled());

        RenderLayerModelObject* repaintContainer = m_renderer->containerForRepaint();
        LayoutRect oldRepaintRect = m_repaintRect;
        LayoutRect oldOutlineBox = m_outlineBox;
        computeRepaintRects(repaintContainer, geometryMap);
        shouldCheckForRepaint &= shouldRepaintLayer();

        // FIXME: Should ASSERT that value calculated for m_outlineBox using the cached offset is the same
        // as the value not using the cached offset, but we can't due to https://bugs.webkit.org/show_bug.cgi?id=37048
        if (shouldCheckForRepaint) {
            if (view && !view->document().printing()) {
                if (m_repaintStatus & NeedsFullRepaint) {
                    m_renderer->repaintUsingContainer(repaintContainer, pixelSnappedIntRect(oldRepaintRect));
                    if (m_repaintRect != oldRepaintRect)
                        m_renderer->repaintUsingContainer(repaintContainer, pixelSnappedIntRect(m_repaintRect));
                } else {
                    m_renderer->repaintAfterLayoutIfNeeded(repaintContainer, m_renderer->selfNeedsLayout(), oldRepaintRect, oldOutlineBox, &m_repaintRect, &m_outlineBox);
                }
            }
        }
    } else {
        clearRepaintRects();
    }

    m_repaintStatus = NeedsNormalRepaint;

}

void RenderLayerRepainter::clearRepaintRects()
{
    ASSERT(!m_renderer->layer()->hasVisibleContent());

    m_repaintRect = IntRect();
    m_outlineBox = IntRect();
}

void RenderLayerRepainter::computeRepaintRects(const RenderLayerModelObject* repaintContainer, const RenderGeometryMap* geometryMap)
{
    if (RuntimeEnabledFeatures::repaintAfterLayoutEnabled())
        return;

    m_repaintRect = m_renderer->clippedOverflowRectForRepaint(repaintContainer);
    m_outlineBox = m_renderer->outlineBoundsForRepaint(repaintContainer, geometryMap);
}

void RenderLayerRepainter::computeRepaintRectsIncludingDescendants()
{
    if (RuntimeEnabledFeatures::repaintAfterLayoutEnabled()) {
        // FIXME: We want RenderLayerRepainter to go away when
        // repaint-after-layout is on by default so we need to figure out how to
        // handle this update.
        //
        // This is a little silly as we create and immediately destroy the RAII
        // object but it makes sure we correctly set all of the repaint flags.
        LayoutRectRecorder recorder(*m_renderer);

    } else {
        // FIXME: computeRepaintRects() has to walk up the parent chain for every layer to compute the rects.
        // We should make this more efficient.
        // FIXME: it's wrong to call this when layout is not up-to-date, which we do.
        computeRepaintRects(m_renderer->containerForRepaint());
    }

    for (RenderLayer* layer = m_renderer->layer()->firstChild(); layer; layer = layer->nextSibling())
        layer->repainter().computeRepaintRectsIncludingDescendants();
}

inline bool RenderLayerRepainter::shouldRepaintLayer() const
{
    if (RuntimeEnabledFeatures::repaintAfterLayoutEnabled())
        return false;

    if (m_repaintStatus != NeedsFullRepaintForPositionedMovementLayout)
        return true;

    // Composited layers that were moved during a positioned movement only
    // layout, don't need to be repainted. They just need to be recomposited.
    return m_renderer->compositingState() != PaintsIntoOwnBacking;
}

// Since we're only painting non-composited layers, we know that they all share the same repaintContainer.
void RenderLayerRepainter::repaintIncludingNonCompositingDescendants(RenderLayerModelObject* repaintContainer)
{
    m_renderer->repaintUsingContainer(repaintContainer, pixelSnappedIntRect(m_renderer->clippedOverflowRectForRepaint(repaintContainer)));

    for (RenderLayer* curr = m_renderer->layer()->firstChild(); curr; curr = curr->nextSibling()) {
        if (!curr->hasCompositedLayerMapping())
            curr->repainter().repaintIncludingNonCompositingDescendants(repaintContainer);
    }
}

LayoutRect RenderLayerRepainter::repaintRectIncludingNonCompositingDescendants() const
{
    LayoutRect repaintRect;
    if (RuntimeEnabledFeatures::repaintAfterLayoutEnabled())
        repaintRect = m_renderer->newRepaintRect();
    else
        repaintRect = m_repaintRect;

    for (RenderLayer* child = m_renderer->layer()->firstChild(); child; child = child->nextSibling()) {
        // Don't include repaint rects for composited child layers; they will paint themselves and have a different origin.
        if (child->hasCompositedLayerMapping())
            continue;

        repaintRect.unite(child->repainter().repaintRectIncludingNonCompositingDescendants());
    }
    return repaintRect;
}

void RenderLayerRepainter::setBackingNeedsRepaint()
{
    ASSERT(m_renderer->compositingState() != NotComposited);

    if (m_renderer->compositingState() == PaintsIntoGroupedBacking) {
        // FIXME: should probably setNeedsDisplayInRect for this layer's bounds only.
        m_renderer->groupedMapping()->squashingLayer()->setNeedsDisplay();
    } else {
        m_renderer->compositedLayerMapping()->setContentsNeedDisplay();
    }
}

void RenderLayerRepainter::setBackingNeedsRepaintInRect(const LayoutRect& r)
{
    // https://bugs.webkit.org/show_bug.cgi?id=61159 describes an unreproducible crash here,
    // so assert but check that the layer is composited.
    ASSERT(m_renderer->compositingState() != NotComposited);
    if (m_renderer->compositingState() == NotComposited) {
        // If we're trying to repaint the placeholder document layer, propagate the
        // repaint to the native view system.
        LayoutRect absRect(r);
        LayoutPoint delta;
        m_renderer->layer()->convertToLayerCoords(m_renderer->layer()->root(), delta);
        absRect.moveBy(delta);

        RenderView* view = m_renderer->view();
        if (view)
            view->repaintViewRectangle(absRect);
    } else {
        if (m_renderer->compositingState() == PaintsIntoGroupedBacking) {
            // FIXME: LayoutRect rounding to IntRect is probably not a good idea.
            IntRect offsetRect = pixelSnappedIntRect(r);
            if (m_renderer->hasTransform())
                offsetRect = m_renderer->layer()->transform()->mapRect(pixelSnappedIntRect(r));

            offsetRect.move(-m_renderer->layer()->offsetFromSquashingLayerOrigin());
            m_renderer->groupedMapping()->squashingLayer()->setNeedsDisplayInRect(offsetRect);
        } else {
            IntRect repaintRect = pixelSnappedIntRect(r.location() +  m_renderer->compositedLayerMapping()->subpixelAccumulation(), r.size());
            m_renderer->compositedLayerMapping()->setContentsNeedDisplayInRect(repaintRect);
        }
    }
}

void RenderLayerRepainter::repaintIncludingDescendants()
{
    m_renderer->repaint();
    for (RenderLayer* curr = m_renderer->layer()->firstChild(); curr; curr = curr->nextSibling())
        curr->repainter().repaintIncludingDescendants();
}

void RenderLayerRepainter::setFilterBackendNeedsRepaintingInRect(const LayoutRect& rect)
{
    if (rect.isEmpty())
        return;

    LayoutRect rectForRepaint = rect;
    m_renderer->style()->filterOutsets().expandRect(rectForRepaint);

    RenderLayerFilterInfo* filterInfo = m_renderer->layer()->filterInfo();
    ASSERT(filterInfo);
    filterInfo->expandDirtySourceRect(rectForRepaint);

    ASSERT(filterInfo->renderer());
    if (filterInfo->renderer()->hasCustomShaderFilter()) {
        // If we have at least one custom shader, we need to update the whole bounding box of the layer, because the
        // shader can address any ouput pixel.
        // Note: This is only for output rect, so there's no need to expand the dirty source rect.
        rectForRepaint.unite(m_renderer->layer()->calculateLayerBounds(m_renderer->layer()));
    }

    RenderLayer* parentLayer = enclosingFilterRepaintLayer();
    ASSERT(parentLayer);
    FloatQuad repaintQuad(rectForRepaint);
    LayoutRect parentLayerRect = m_renderer->localToContainerQuad(repaintQuad, parentLayer->renderer()).enclosingBoundingBox();

    if (parentLayer->hasCompositedLayerMapping()) {
        parentLayer->repainter().setBackingNeedsRepaintInRect(parentLayerRect);
        return;
    }

    if (parentLayer->paintsWithFilters()) {
        parentLayer->repainter().setFilterBackendNeedsRepaintingInRect(parentLayerRect);
        return;
    }

    if (parentLayer->isRootLayer()) {
        RenderView* view = toRenderView(parentLayer->renderer());
        view->repaintViewRectangle(parentLayerRect);
        return;
    }

    ASSERT_NOT_REACHED();
}

RenderLayer* RenderLayerRepainter::enclosingFilterRepaintLayer() const
{
    for (const RenderLayer* curr = m_renderer->layer(); curr; curr = curr->parent()) {
        if ((curr != m_renderer->layer() && curr->requiresFullLayerImageForFilters()) || curr->compositingState() == PaintsIntoOwnBacking || curr->isRootLayer())
            return const_cast<RenderLayer*>(curr);
    }
    return 0;
}

} // Namespace WebCore
