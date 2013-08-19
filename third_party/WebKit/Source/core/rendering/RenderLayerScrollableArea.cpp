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
#include "core/rendering/RenderLayer.h"

#include "core/page/Frame.h"
#include "core/page/FrameView.h"
#include "core/page/Page.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/platform/ScrollAnimator.h"

namespace WebCore {

RenderLayerScrollableArea::RenderLayerScrollableArea(RenderLayer* layer)
    : m_layer(layer)
{
    ScrollableArea::setConstrainsScrollingToContentEdge(false);

    Node* node = m_layer->renderer()->node();
    if (node && node->isElementNode()) {
        // We save and restore only the scrollOffset as the other scroll values are recalculated.
        Element* element = toElement(node);
        m_layer->m_scrollOffset = element->savedLayerScrollOffset();
        if (!m_layer->m_scrollOffset.isZero())
            scrollAnimator()->setCurrentPosition(FloatPoint(m_layer->m_scrollOffset.width(), m_layer->m_scrollOffset.height()));
        element->setSavedLayerScrollOffset(IntSize());
    }

}

RenderLayerScrollableArea::~RenderLayerScrollableArea()
{
    if (Frame* frame = m_layer->renderer()->frame()) {
        if (FrameView* frameView = frame->view()) {
            frameView->removeScrollableArea(this);
        }
    }

    if (m_layer->renderer()->frame() && m_layer->renderer()->frame()->page()) {
        if (ScrollingCoordinator* scrollingCoordinator = m_layer->renderer()->frame()->page()->scrollingCoordinator())
            scrollingCoordinator->willDestroyScrollableArea(this);
    }
}

Scrollbar* RenderLayerScrollableArea::horizontalScrollbar() const
{
    return m_layer->horizontalScrollbar();
}

Scrollbar* RenderLayerScrollableArea::verticalScrollbar() const
{
    return m_layer->verticalScrollbar();
}

ScrollableArea* RenderLayerScrollableArea::enclosingScrollableArea() const
{
    return m_layer->enclosingScrollableArea();
}

void RenderLayerScrollableArea::updateNeedsCompositedScrolling()
{
    m_layer->updateNeedsCompositedScrolling();
}

GraphicsLayer* RenderLayerScrollableArea::layerForScrolling() const
{
    return m_layer->layerForScrolling();
}

GraphicsLayer* RenderLayerScrollableArea::layerForHorizontalScrollbar() const
{
    return m_layer->layerForHorizontalScrollbar();
}

GraphicsLayer* RenderLayerScrollableArea::layerForVerticalScrollbar() const
{
    return m_layer->layerForVerticalScrollbar();
}

GraphicsLayer* RenderLayerScrollableArea::layerForScrollCorner() const
{
    return m_layer->layerForScrollCorner();
}

bool RenderLayerScrollableArea::usesCompositedScrolling() const
{
    return m_layer->usesCompositedScrolling();
}

void RenderLayerScrollableArea::invalidateScrollbarRect(Scrollbar* scrollbar, const IntRect& rect)
{
    m_layer->invalidateScrollbarRect(scrollbar, rect);
}

void RenderLayerScrollableArea::invalidateScrollCornerRect(const IntRect& rect)
{
    m_layer->invalidateScrollCornerRect(rect);
}

bool RenderLayerScrollableArea::isActive() const
{
    return m_layer->isActive();
}

bool RenderLayerScrollableArea::isScrollCornerVisible() const
{
    return m_layer->isScrollCornerVisible();
}

IntRect RenderLayerScrollableArea::scrollCornerRect() const
{
    return m_layer->scrollCornerRect();
}

IntRect RenderLayerScrollableArea::convertFromScrollbarToContainingView(const Scrollbar* scrollbar, const IntRect& rect) const
{
    return m_layer->convertFromScrollbarToContainingView(scrollbar, rect);
}

IntRect RenderLayerScrollableArea::convertFromContainingViewToScrollbar(const Scrollbar* scrollbar, const IntRect& rect) const
{
    return m_layer->convertFromContainingViewToScrollbar(scrollbar, rect);
}

IntPoint RenderLayerScrollableArea::convertFromScrollbarToContainingView(const Scrollbar* scrollbar, const IntPoint& point) const
{
    return m_layer->convertFromScrollbarToContainingView(scrollbar, point);
}

IntPoint RenderLayerScrollableArea::convertFromContainingViewToScrollbar(const Scrollbar* scrollbar, const IntPoint& point) const
{
    return m_layer->convertFromContainingViewToScrollbar(scrollbar, point);
}

int RenderLayerScrollableArea::scrollSize(ScrollbarOrientation orientation) const
{
    return m_layer->scrollSize(orientation);
}

void RenderLayerScrollableArea::setScrollOffset(const IntPoint& offset)
{
    m_layer->setScrollOffset(offset);
}

IntPoint RenderLayerScrollableArea::scrollPosition() const
{
    return m_layer->scrollPosition();
}

IntPoint RenderLayerScrollableArea::minimumScrollPosition() const
{
    return m_layer->minimumScrollPosition();
}

IntPoint RenderLayerScrollableArea::maximumScrollPosition() const
{
    return m_layer->maximumScrollPosition();
}

IntRect RenderLayerScrollableArea::visibleContentRect(VisibleContentRectIncludesScrollbars scrollbarInclusion) const
{
    return m_layer->visibleContentRect(scrollbarInclusion);
}

int RenderLayerScrollableArea::visibleHeight() const
{
    return m_layer->visibleHeight();
}

int RenderLayerScrollableArea::visibleWidth() const
{
    return m_layer->visibleWidth();
}

IntSize RenderLayerScrollableArea::contentsSize() const
{
    return m_layer->contentsSize();
}

IntSize RenderLayerScrollableArea::overhangAmount() const
{
    return m_layer->overhangAmount();
}

IntPoint RenderLayerScrollableArea::lastKnownMousePosition() const
{
    return m_layer->lastKnownMousePosition();
}

bool RenderLayerScrollableArea::shouldSuspendScrollAnimations() const
{
    return m_layer->shouldSuspendScrollAnimations();
}

bool RenderLayerScrollableArea::scrollbarsCanBeActive() const
{
    return m_layer->scrollbarsCanBeActive();
}

IntRect RenderLayerScrollableArea::scrollableAreaBoundingBox() const
{
    return m_layer->scrollableAreaBoundingBox();
}

bool RenderLayerScrollableArea::userInputScrollable(ScrollbarOrientation orientation) const
{
    return m_layer->userInputScrollable(orientation);
}

int RenderLayerScrollableArea::pageStep(ScrollbarOrientation orientation) const
{
    return m_layer->pageStep(orientation);
}

} // Namespace WebCore
