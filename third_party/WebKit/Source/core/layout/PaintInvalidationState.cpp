// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/layout/PaintInvalidationState.h"

#include "core/layout/Layer.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutView.h"
#include "core/layout/svg/LayoutSVGModelObject.h"
#include "core/layout/svg/LayoutSVGRoot.h"
#include "platform/Partitions.h"

namespace blink {

PaintInvalidationState::PaintInvalidationState(const LayoutView& layoutView)
    : m_clipped(false)
    , m_cachedOffsetsEnabled(true)
    , m_forceCheckForPaintInvalidation(false)
    , m_paintInvalidationContainer(*layoutView.containerForPaintInvalidation())
{
    bool establishesPaintInvalidationContainer = layoutView == m_paintInvalidationContainer;
    if (!establishesPaintInvalidationContainer) {
        if (!layoutView.supportsPaintInvalidationStateCachedOffsets()) {
            m_cachedOffsetsEnabled = false;
            return;
        }
        FloatPoint point = layoutView.localToContainerPoint(FloatPoint(), &m_paintInvalidationContainer, TraverseDocumentBoundaries);
        m_paintOffset = LayoutSize(point.x(), point.y());
    }
    m_clipRect = layoutView.viewRect();
    m_clipRect.move(m_paintOffset);
    m_clipped = true;
}

PaintInvalidationState::PaintInvalidationState(const PaintInvalidationState& next, LayoutBoxModelObject& renderer, const LayoutBoxModelObject& paintInvalidationContainer)
    : m_clipped(false)
    , m_cachedOffsetsEnabled(true)
    , m_forceCheckForPaintInvalidation(next.m_forceCheckForPaintInvalidation)
    , m_paintInvalidationContainer(paintInvalidationContainer)
{
    // FIXME: SVG could probably benefit from a stack-based optimization like html does. crbug.com/391054
    bool establishesPaintInvalidationContainer = renderer == m_paintInvalidationContainer;
    bool fixed = renderer.style()->position() == FixedPosition;

    if (!renderer.supportsPaintInvalidationStateCachedOffsets() || !next.m_cachedOffsetsEnabled)
        m_cachedOffsetsEnabled = false;
    if (establishesPaintInvalidationContainer) {
        // When we hit a new paint invalidation container, we don't need to
        // continue forcing a check for paint invalidation because movement
        // from our parents will just move the whole invalidation container.
        m_forceCheckForPaintInvalidation = false;
    } else {
        if (m_cachedOffsetsEnabled) {
            if (fixed) {
                FloatPoint fixedOffset = renderer.localToContainerPoint(FloatPoint(), &m_paintInvalidationContainer, TraverseDocumentBoundaries);
                m_paintOffset = LayoutSize(fixedOffset.x(), fixedOffset.y());
            } else {
                LayoutSize offset = renderer.isBox() && !renderer.isTableRow() ? toLayoutBox(renderer).locationOffset() : LayoutSize();
                m_paintOffset = next.m_paintOffset + offset;
            }

            if (renderer.isOutOfFlowPositioned() && !fixed) {
                if (LayoutObject* container = renderer.container()) {
                    if (container->style()->hasInFlowPosition() && container->isLayoutInline())
                        m_paintOffset += toLayoutInline(container)->offsetForInFlowPositionedInline(toLayoutBox(renderer));
                }
            }

            if (renderer.style()->hasInFlowPosition() && renderer.hasLayer())
                m_paintOffset += renderer.layer()->offsetForInFlowPosition();
        }

        m_clipped = !fixed && next.m_clipped;
        if (m_clipped)
            m_clipRect = next.m_clipRect;
    }

    if (m_cachedOffsetsEnabled && renderer.isSVGRoot()) {
        const LayoutSVGRoot& svgRoot = toLayoutSVGRoot(renderer);
        m_svgTransform = adoptPtr(new AffineTransform(svgRoot.localToBorderBoxTransform()));
        if (svgRoot.shouldApplyViewportClip())
            addClipRectRelativeToPaintOffset(LayoutSize(svgRoot.pixelSnappedSize()));
    }

    applyClipIfNeeded(renderer);

    // FIXME: <http://bugs.webkit.org/show_bug.cgi?id=13443> Apply control clip if present.
}

PaintInvalidationState::PaintInvalidationState(const PaintInvalidationState& next, const LayoutSVGModelObject& renderer)
    : m_clipped(next.m_clipped)
    , m_cachedOffsetsEnabled(next.m_cachedOffsetsEnabled)
    , m_forceCheckForPaintInvalidation(next.m_forceCheckForPaintInvalidation)
    , m_clipRect(next.m_clipRect)
    , m_paintOffset(next.m_paintOffset)
    , m_paintInvalidationContainer(next.m_paintInvalidationContainer)
{
    ASSERT(renderer != m_paintInvalidationContainer);

    if (m_cachedOffsetsEnabled)
        m_svgTransform = adoptPtr(new AffineTransform(next.svgTransform() * renderer.localToParentTransform()));
}

void PaintInvalidationState::addClipRectRelativeToPaintOffset(const LayoutSize& clipSize)
{
    LayoutRect clipRect(toPoint(m_paintOffset), clipSize);
    if (m_clipped) {
        m_clipRect.intersect(clipRect);
    } else {
        m_clipRect = clipRect;
        m_clipped = true;
    }
}

void PaintInvalidationState::applyClipIfNeeded(const LayoutObject& renderer)
{
    if (!renderer.hasOverflowClip())
        return;

    const LayoutBox& box = toLayoutBox(renderer);

    // Do not clip scroll layer contents because the compositor expects the whole layer
    // to be always invalidated in-time.
    if (box.usesCompositedScrolling())
        ASSERT(!m_clipped); // The box should establish paint invalidation container, so no m_clipped inherited.
    else
        addClipRectRelativeToPaintOffset(LayoutSize(box.layer()->size()));

    m_paintOffset -= box.scrolledContentOffset();
}

} // namespace blink
