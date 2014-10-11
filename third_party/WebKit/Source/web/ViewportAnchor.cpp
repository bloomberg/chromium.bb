/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "web/ViewportAnchor.h"

#include "core/dom/ContainerNode.h"
#include "core/dom/Node.h"
#include "core/frame/FrameView.h"
#include "core/page/EventHandler.h"
#include "core/rendering/HitTestResult.h"

namespace blink {

namespace {

static const float viewportAnchorRelativeEpsilon = 0.1f;
static const int viewportToNodeMaxRelativeArea = 2;

template <typename RectType>
int area(const RectType& rect) {
    return rect.width() * rect.height();
}

Node* findNonEmptyAnchorNode(const IntPoint& point, const IntRect& viewRect, EventHandler* eventHandler)
{
    Node* node = eventHandler->hitTestResultAtPoint(point, HitTestRequest::ReadOnly | HitTestRequest::Active).innerNode();

    // If the node bounding box is sufficiently large, make a single attempt to
    // find a smaller node; the larger the node bounds, the greater the
    // variability under resize.
    const int maxNodeArea = area(viewRect) * viewportToNodeMaxRelativeArea;
    if (node && area(node->boundingBox()) > maxNodeArea) {
        IntSize pointOffset = viewRect.size();
        pointOffset.scale(viewportAnchorRelativeEpsilon);
        node = eventHandler->hitTestResultAtPoint(point + pointOffset, HitTestRequest::ReadOnly | HitTestRequest::Active).innerNode();
    }

    while (node && node->boundingBox().isEmpty())
        node = node->parentNode();

    return node;
}

void moveToEncloseRect(IntRect& outer, const FloatRect& inner)
{
    IntPoint minimumPosition = ceiledIntPoint(inner.location() + inner.size() - FloatSize(outer.size()));
    IntPoint maximumPosition = flooredIntPoint(inner.location());

    IntPoint outerOrigin = outer.location();
    outerOrigin = outerOrigin.expandedTo(minimumPosition);
    outerOrigin = outerOrigin.shrunkTo(maximumPosition);

    outer.setLocation(outerOrigin);
}

void moveIntoRect(FloatRect& inner, const IntRect& outer)
{
    FloatPoint minimumPosition = FloatPoint(outer.location());
    FloatPoint maximumPosition = minimumPosition + outer.size() - inner.size();

    // Adjust maximumPosition to the nearest lower integer because
    // PinchViewport::maximumScrollPosition() does the same.
    // The value of minumumPosition is already adjusted since it is
    // constructed from an integer point.
    maximumPosition = flooredIntPoint(maximumPosition);

    FloatPoint innerOrigin = inner.location();
    innerOrigin = innerOrigin.expandedTo(minimumPosition);
    innerOrigin = innerOrigin.shrunkTo(maximumPosition);

    inner.setLocation(innerOrigin);
}

} // namespace

ViewportAnchor::ViewportAnchor(EventHandler* eventHandler)
    : m_eventHandler(eventHandler) { }

void ViewportAnchor::setAnchor(const IntRect& outerViewRect, const IntRect& innerViewRect,
    const FloatSize& anchorInInnerViewCoords)
{
    // Preserve the inner viewport position in document in case we won't find the anchor
    m_pinchViewportInDocument = innerViewRect.location();

    m_anchorNode.clear();
    m_anchorNodeBounds = LayoutRect();
    m_anchorInNodeCoords = FloatSize();
    m_anchorInInnerViewCoords = anchorInInnerViewCoords;
    m_normalizedPinchViewportOffset = FloatSize();

    if (innerViewRect.isEmpty())
        return;

    // Preserve origins at the absolute screen origin
    if (innerViewRect.location() == IntPoint::zero())
        return;

    // Inner rectangle should be within the outer one.
    ASSERT(outerViewRect.contains(innerViewRect));

    // Outer rectangle is used as a scale, we need positive width and height.
    ASSERT(!outerViewRect.isEmpty());

    m_normalizedPinchViewportOffset = innerViewRect.location() - outerViewRect.location();

    // Normalize by the size of the outer rect
    m_normalizedPinchViewportOffset.scale(1.0 / outerViewRect.width(), 1.0 / outerViewRect.height());

    FloatSize anchorOffset = innerViewRect.size();
    anchorOffset.scale(anchorInInnerViewCoords.width(), anchorInInnerViewCoords.height());
    const FloatPoint anchorPoint = FloatPoint(innerViewRect.location()) + anchorOffset;

    Node* node = findNonEmptyAnchorNode(flooredIntPoint(anchorPoint), innerViewRect, m_eventHandler);
    if (!node)
        return;

    m_anchorNode = node;
    m_anchorNodeBounds = node->boundingBox();
    m_anchorInNodeCoords = anchorPoint - m_anchorNodeBounds.location();
    m_anchorInNodeCoords.scale(1.f / m_anchorNodeBounds.width(), 1.f / m_anchorNodeBounds.height());
}

void ViewportAnchor::computeOrigins(const FrameView& frameView, const FloatSize& innerSize,
    IntPoint& mainFrameOffset, FloatPoint& pinchViewportOffset) const
{
    IntSize outerSize = frameView.visibleContentRect().size();

    // Compute the viewport origins in CSS pixels relative to the document.
    FloatSize absPinchViewportOffset = m_normalizedPinchViewportOffset;
    absPinchViewportOffset.scale(outerSize.width(), outerSize.height());

    FloatPoint innerOrigin = getInnerOrigin(innerSize);
    FloatPoint outerOrigin = innerOrigin - absPinchViewportOffset;

    IntRect outerRect = IntRect(flooredIntPoint(outerOrigin), outerSize);
    FloatRect innerRect = FloatRect(innerOrigin, innerSize);

    moveToEncloseRect(outerRect, innerRect);

    outerRect.setLocation(frameView.adjustScrollPositionWithinRange(outerRect.location()));

    moveIntoRect(innerRect, outerRect);

    mainFrameOffset = outerRect.location();
    pinchViewportOffset = FloatPoint(innerRect.location() - outerRect.location());
}

FloatPoint ViewportAnchor::getInnerOrigin(const FloatSize& innerSize) const
{
    if (!m_anchorNode || !m_anchorNode->inDocument())
        return m_pinchViewportInDocument;

    const LayoutRect currentNodeBounds = m_anchorNode->boundingBox();
    if (m_anchorNodeBounds == currentNodeBounds)
        return m_pinchViewportInDocument;

    // Compute the new anchor point relative to the node position
    FloatSize anchorOffsetFromNode = currentNodeBounds.size();
    anchorOffsetFromNode.scale(m_anchorInNodeCoords.width(), m_anchorInNodeCoords.height());
    FloatPoint anchorPoint = currentNodeBounds.location() + anchorOffsetFromNode;

    // Compute the new origin point relative to the new anchor point
    FloatSize anchorOffsetFromOrigin = innerSize;
    anchorOffsetFromOrigin.scale(m_anchorInInnerViewCoords.width(), m_anchorInInnerViewCoords.height());
    return anchorPoint - anchorOffsetFromOrigin;
}

} // namespace blink
