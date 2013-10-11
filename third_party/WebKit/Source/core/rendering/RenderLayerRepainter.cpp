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

#include "core/rendering/CompositedLayerMapping.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderView.h"

namespace WebCore {

RenderLayerRepainter::RenderLayerRepainter(RenderLayerModelObject* renderer)
    : m_renderer(renderer)
    , m_repaintStatus(NeedsNormalRepaint)
{
}

void RenderLayerRepainter::repaintAfterLayout(RenderGeometryMap* geometryMap, bool shouldCheckForRepaint)
{
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

        // FIXME: Should ASSERT that value calculated for m_outlineBox using the cached offset is the same
        // as the value not using the cached offset, but we can't due to https://bugs.webkit.org/show_bug.cgi?id=37048
        if (shouldCheckForRepaint) {
            if (view && !view->document().printing()) {
                if (m_repaintStatus & NeedsFullRepaint) {
                    m_renderer->repaintUsingContainer(repaintContainer, pixelSnappedIntRect(oldRepaintRect));
                    if (m_repaintRect != oldRepaintRect)
                        m_renderer->repaintUsingContainer(repaintContainer, pixelSnappedIntRect(m_repaintRect));
                } else if (shouldRepaintAfterLayout()) {
                    m_renderer->repaintAfterLayoutIfNeeded(repaintContainer, oldRepaintRect, oldOutlineBox, &m_repaintRect, &m_outlineBox);
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
    m_repaintRect = m_renderer->clippedOverflowRectForRepaint(repaintContainer);
    m_outlineBox = m_renderer->outlineBoundsForRepaint(repaintContainer, geometryMap);
}

void RenderLayerRepainter::computeRepaintRectsIncludingDescendants()
{
    // FIXME: computeRepaintRects() has to walk up the parent chain for every layer to compute the rects.
    // We should make this more efficient.
    // FIXME: it's wrong to call this when layout is not up-to-date, which we do.
    computeRepaintRects(m_renderer->containerForRepaint());

    for (RenderLayer* layer = m_renderer->layer()->firstChild(); layer; layer = layer->nextSibling())
        layer->repainter().computeRepaintRectsIncludingDescendants();
}

inline bool RenderLayerRepainter::shouldRepaintAfterLayout() const
{
    if (m_repaintStatus == NeedsNormalRepaint)
        return true;

    // Composited layers that were moved during a positioned movement only
    // layout, don't need to be repainted. They just need to be recomposited.
    ASSERT(m_repaintStatus == NeedsFullRepaintForPositionedMovementLayout);
    return !m_renderer->isComposited() || (m_renderer->isComposited() && m_renderer->layer()->compositedLayerMapping()->paintsIntoCompositedAncestor());
}

// Since we're only painting non-composited layers, we know that they all share the same repaintContainer.
void RenderLayerRepainter::repaintIncludingNonCompositingDescendants(RenderLayerModelObject* repaintContainer)
{
    m_renderer->repaintUsingContainer(repaintContainer, pixelSnappedIntRect(m_renderer->clippedOverflowRectForRepaint(repaintContainer)));

    for (RenderLayer* curr = m_renderer->layer()->firstChild(); curr; curr = curr->nextSibling()) {
        if (!curr->isComposited())
            curr->repainter().repaintIncludingNonCompositingDescendants(repaintContainer);
    }
}

LayoutRect RenderLayerRepainter::repaintRectIncludingNonCompositingDescendants() const
{
    LayoutRect repaintRect = m_repaintRect;
    for (RenderLayer* child = m_renderer->layer()->firstChild(); child; child = child->nextSibling()) {
        // Don't include repaint rects for composited child layers; they will paint themselves and have a different origin.
        if (child->isComposited())
            continue;

        repaintRect.unite(child->repainter().repaintRectIncludingNonCompositingDescendants());
    }
    return repaintRect;
}

} // Namespace WebCore
