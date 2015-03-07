/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "web/FindInPageCoordinates.h"

#include "core/dom/Node.h"
#include "core/dom/Range.h"
#include "core/frame/LocalFrame.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/LayoutView.h"
#include "core/layout/style/LayoutStyle.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/geometry/IntPoint.h"

namespace blink {

static const LayoutBlock* enclosingScrollableAncestor(const LayoutObject* renderer)
{
    ASSERT(!renderer->isLayoutView());

    // Trace up the containingBlocks until we reach either the render view or a scrollable object.
    const LayoutBlock* container = renderer->containingBlock();
    while (!container->hasOverflowClip() && !container->isLayoutView())
        container = container->containingBlock();
    return container;
}

static FloatRect toNormalizedRect(const FloatRect& absoluteRect, const LayoutObject* renderer, const LayoutBlock* container)
{
    ASSERT(renderer);

    ASSERT(container || renderer->isLayoutView());
    if (!container)
        return FloatRect();

    // We want to normalize by the max layout overflow size instead of only the visible bounding box.
    // Quads and their enclosing bounding boxes need to be used in order to keep results transform-friendly.
    FloatPoint scrolledOrigin;

    // For overflow:scroll we need to get where the actual origin is independently of the scroll.
    if (container->hasOverflowClip())
        scrolledOrigin = -IntPoint(container->scrolledContentOffset());

    FloatRect overflowRect(scrolledOrigin, FloatSize(container->maxLayoutOverflow()));
    FloatRect containerRect = container->localToAbsoluteQuad(FloatQuad(overflowRect)).enclosingBoundingBox();

    if (containerRect.isEmpty())
        return FloatRect();

    // Make the coordinates relative to the container enclosing bounding box.
    // Since we work with rects enclosing quad unions this is still transform-friendly.
    FloatRect normalizedRect = absoluteRect;
    normalizedRect.moveBy(-containerRect.location());

    // Fixed positions do not make sense in this coordinate system, but need to leave consistent tickmarks.
    // So, use their position when the view is not scrolled, like an absolute position.
    if (renderer->style()->position() == FixedPosition && container->isLayoutView())
        normalizedRect.move(-toLayoutView(container)->frameView()->scrollOffsetForViewportConstrainedObjects());

    normalizedRect.scale(1 / containerRect.width(), 1 / containerRect.height());
    return normalizedRect;
}

FloatRect findInPageRectFromAbsoluteRect(const FloatRect& inputRect, const LayoutObject* baseRenderer)
{
    if (!baseRenderer || inputRect.isEmpty())
        return FloatRect();

    // Normalize the input rect to its container block.
    const LayoutBlock* baseContainer = enclosingScrollableAncestor(baseRenderer);
    FloatRect normalizedRect = toNormalizedRect(inputRect, baseRenderer, baseContainer);

    // Go up across frames.
    for (const LayoutBox* renderer = baseContainer; renderer; ) {

        // Go up the layout tree until we reach the root of the current frame (the LayoutView).
        while (!renderer->isLayoutView()) {
            const LayoutBlock* container = enclosingScrollableAncestor(renderer);

            // Compose the normalized rects.
            FloatRect normalizedBoxRect = toNormalizedRect(renderer->absoluteBoundingBoxRect(), renderer, container);
            normalizedRect.scale(normalizedBoxRect.width(), normalizedBoxRect.height());
            normalizedRect.moveBy(normalizedBoxRect.location());

            renderer = container;
        }

        ASSERT(renderer->isLayoutView());

        // Jump to the renderer owning the frame, if any.
        renderer = renderer->frame() ? renderer->frame()->ownerLayoutObject() : 0;
    }

    return normalizedRect;
}

FloatRect findInPageRectFromRange(Range* range)
{
    if (!range || !range->firstNode())
        return FloatRect();

    return findInPageRectFromAbsoluteRect(LayoutObject::absoluteBoundingBoxRectForRange(range), range->firstNode()->layoutObject());
}

} // namespace blink
