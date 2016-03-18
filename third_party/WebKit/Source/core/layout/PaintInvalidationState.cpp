// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/PaintInvalidationState.h"

#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutView.h"
#include "core/layout/svg/LayoutSVGModelObject.h"
#include "core/layout/svg/LayoutSVGRoot.h"
#include "core/paint/PaintLayer.h"

namespace blink {

PaintInvalidationState::PaintInvalidationState(const LayoutView& layoutView, Vector<LayoutObject*>& pendingDelayedPaintInvalidations, const PaintInvalidationState* ownerPaintInvalidationState)
    : m_currentObject(layoutView)
    , m_clipped(false)
    , m_cachedOffsetsEnabled(true)
    , m_forcedSubtreeInvalidationWithinContainer(false)
    , m_forcedSubtreeInvalidationRectUpdateWithinContainer(false)
    , m_viewClippingAndScrollOffsetDisabled(false)
    , m_paintInvalidationContainer(layoutView.containerForPaintInvalidation())
    , m_pendingDelayedPaintInvalidations(pendingDelayedPaintInvalidations)
    , m_enclosingSelfPaintingLayer(*layoutView.layer())
#if ENABLE(ASSERT)
    , m_didUpdatePaintOffsetAndClipForChildren(true)
#endif
{
    ASSERT(!ownerPaintInvalidationState || ownerPaintInvalidationState->m_didUpdatePaintOffsetAndClipForChildren);

    bool establishesPaintInvalidationContainer = layoutView == m_paintInvalidationContainer;
    if (!establishesPaintInvalidationContainer) {
        if ((ownerPaintInvalidationState && !ownerPaintInvalidationState->m_cachedOffsetsEnabled)
            || !layoutView.supportsPaintInvalidationStateCachedOffsets()) {
            m_cachedOffsetsEnabled = false;
            return;
        }
        if (ownerPaintInvalidationState && ownerPaintInvalidationState->m_forcedSubtreeInvalidationWithinContainer)
            m_forcedSubtreeInvalidationWithinContainer = true;
        FloatPoint point = layoutView.localToAncestorPoint(FloatPoint(), &m_paintInvalidationContainer, TraverseDocumentBoundaries);
        m_paintOffset = LayoutSize(point.x(), point.y());
    }
    m_clipRect = layoutView.viewRect();
    m_clipRect.move(m_paintOffset);
    m_clipped = true;
}

// TODO(wangxianzhu): This is temporary for positioned object whose paintInvalidationContainer is different from
// the one we find during tree walk. Remove this after we fix the issue with tree walk in DOM-order.
PaintInvalidationState::PaintInvalidationState(const PaintInvalidationState& parentState, const LayoutBoxModelObject& currentObject, const LayoutBoxModelObject& paintInvalidationContainer)
    : m_currentObject(currentObject)
    , m_clipped(parentState.m_clipped)
    , m_cachedOffsetsEnabled(parentState.m_cachedOffsetsEnabled)
    , m_forcedSubtreeInvalidationWithinContainer(parentState.m_forcedSubtreeInvalidationWithinContainer)
    , m_forcedSubtreeInvalidationRectUpdateWithinContainer(parentState.m_forcedSubtreeInvalidationRectUpdateWithinContainer)
    , m_viewClippingAndScrollOffsetDisabled(false)
    , m_clipRect(parentState.m_clipRect)
    , m_paintOffset(parentState.m_paintOffset)
    , m_paintInvalidationContainer(paintInvalidationContainer)
    , m_svgTransform(parentState.m_svgTransform)
    , m_pendingDelayedPaintInvalidations(parentState.pendingDelayedPaintInvalidationTargets())
    , m_enclosingSelfPaintingLayer(parentState.enclosingSelfPaintingLayer(currentObject))
#if ENABLE(ASSERT)
    , m_didUpdatePaintOffsetAndClipForChildren(true)
#endif
{
    ASSERT(parentState.m_didUpdatePaintOffsetAndClipForChildren);
    ASSERT(!m_cachedOffsetsEnabled);
}

PaintInvalidationState::PaintInvalidationState(const PaintInvalidationState& parentState, const LayoutObject& currentObject)
    : m_currentObject(currentObject)
    , m_clipped(parentState.m_clipped)
    , m_cachedOffsetsEnabled(parentState.m_cachedOffsetsEnabled)
    , m_forcedSubtreeInvalidationWithinContainer(parentState.m_forcedSubtreeInvalidationWithinContainer)
    , m_forcedSubtreeInvalidationRectUpdateWithinContainer(parentState.m_forcedSubtreeInvalidationRectUpdateWithinContainer)
    , m_viewClippingAndScrollOffsetDisabled(false)
    , m_clipRect(parentState.m_clipRect)
    , m_paintOffset(parentState.m_paintOffset)
    , m_paintInvalidationContainer(currentObject.isPaintInvalidationContainer() ? toLayoutBoxModelObject(currentObject) : parentState.m_paintInvalidationContainer)
    , m_svgTransform(parentState.m_svgTransform)
    , m_pendingDelayedPaintInvalidations(parentState.pendingDelayedPaintInvalidationTargets())
    , m_enclosingSelfPaintingLayer(parentState.enclosingSelfPaintingLayer(currentObject))
#if ENABLE(ASSERT)
    , m_didUpdatePaintOffsetAndClipForChildren(false)
#endif
{
    ASSERT(parentState.m_didUpdatePaintOffsetAndClipForChildren);
}

void PaintInvalidationState::updatePaintOffsetAndClipForChildren()
{
#if ENABLE(ASSERT)
    ASSERT(!m_didUpdatePaintOffsetAndClipForChildren);
    m_didUpdatePaintOffsetAndClipForChildren = true;
#endif

    bool establishesPaintInvalidationContainer = m_currentObject == m_paintInvalidationContainer;

    if (!m_currentObject.isBoxModelObject()) {
        // TODO(wangxianzhu): SVG could probably benefit from a stack-based optimization like html does. crbug.com/391054.
        ASSERT(m_currentObject.isSVG());
        ASSERT(!establishesPaintInvalidationContainer);
        if (m_cachedOffsetsEnabled)
            m_svgTransform = AffineTransform(m_svgTransform * m_currentObject.localToParentTransform());
        return;
    }

    bool fixed = m_currentObject.style()->position() == FixedPosition;

    if (!m_currentObject.supportsPaintInvalidationStateCachedOffsets())
        m_cachedOffsetsEnabled = false;
    if (establishesPaintInvalidationContainer) {
        // When we hit a new paint invalidation container, we don't need to
        // continue forcing a check for paint invalidation, since we're
        // descending into a different invalidation container. (For instance if
        // our parents were moved, the entire container will just move.)
        m_forcedSubtreeInvalidationWithinContainer = false;
        m_forcedSubtreeInvalidationRectUpdateWithinContainer = false;

        m_clipped = false; // Will be updated in applyClipIfNeeded().
        m_paintOffset = LayoutSize();
    } else {
        if (m_cachedOffsetsEnabled) {
            if (fixed) {
                FloatPoint fixedOffset = m_currentObject.localToAncestorPoint(FloatPoint(), &m_paintInvalidationContainer, TraverseDocumentBoundaries);
                m_paintOffset = LayoutSize(fixedOffset.x(), fixedOffset.y());
            } else if (m_currentObject.isBox() && !m_currentObject.isTableRow()) {
                // We don't add locationOffset of table row because the child cells' location offsets include the row's location offset.
                m_paintOffset += toLayoutBox(m_currentObject).locationOffset();
            }

            if (m_currentObject.isOutOfFlowPositioned() && !fixed) {
                if (LayoutObject* container = m_currentObject.container()) {
                    if (container->isInFlowPositioned() && container->isLayoutInline())
                        m_paintOffset += toLayoutInline(container)->offsetForInFlowPositionedInline(toLayoutBox(m_currentObject));
                }
            }

            if (m_currentObject.isInFlowPositioned() && m_currentObject.hasLayer())
                m_paintOffset += toLayoutBoxModelObject(m_currentObject).layer()->offsetForInFlowPosition();
        }

        m_clipped = !fixed && m_clipped;
    }

    if (m_cachedOffsetsEnabled && m_currentObject.isSVGRoot()) {
        const LayoutSVGRoot& svgRoot = toLayoutSVGRoot(m_currentObject);
        m_svgTransform = AffineTransform(svgRoot.localToBorderBoxTransform());
        if (svgRoot.shouldApplyViewportClip())
            addClipRectRelativeToPaintOffset(LayoutSize(svgRoot.pixelSnappedSize()));
    }

    applyClipIfNeeded();

    // FIXME: <http://bugs.webkit.org/show_bug.cgi?id=13443> Apply control clip if present.
}

void PaintInvalidationState::mapObjectRectToAncestor(const LayoutObject& object, const LayoutBoxModelObject* ancestor, LayoutRect& rect) const
{
    ASSERT(canMapToAncestor(ancestor));

    if (ancestor == &object) {
        if (object.isBox() && object.styleRef().isFlippedBlocksWritingMode())
            toLayoutBox(object).flipForWritingMode(rect);
        return;
    }

    if (object.hasLayer()) {
        if (const TransformationMatrix* transform = toLayoutBoxModelObject(object).layer()->transform())
            rect = LayoutRect(transform->mapRect(pixelSnappedIntRect(rect)));

        if (object.isInFlowPositioned())
            rect.move(toLayoutBoxModelObject(object).layer()->offsetForInFlowPosition());
    }

    if (object.isBox())
        rect.moveBy(toLayoutBox(object).location());

    rect.move(m_paintOffset);

    if (m_clipped)
        rect.intersect(m_clipRect);
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

void PaintInvalidationState::applyClipIfNeeded()
{
    if (!m_currentObject.hasOverflowClip())
        return;

    const LayoutBox& box = toLayoutBox(m_currentObject);

    // Do not clip scroll layer contents because the compositor expects the whole layer
    // to be always invalidated in-time.
    if (box.usesCompositedScrolling())
        ASSERT(!m_clipped); // The box should establish paint invalidation container, so no m_clipped inherited.
    else
        addClipRectRelativeToPaintOffset(LayoutSize(box.layer()->size()));

    m_paintOffset -= box.scrolledContentOffset();
}

PaintLayer& PaintInvalidationState::enclosingSelfPaintingLayer(const LayoutObject& layoutObject) const
{
    if (layoutObject.hasLayer() && toLayoutBoxModelObject(layoutObject).hasSelfPaintingLayer())
        return *toLayoutBoxModelObject(layoutObject).layer();

    return m_enclosingSelfPaintingLayer;
}

} // namespace blink
