// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/PaintInvalidationState.h"

#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/LayoutView.h"
#include "core/layout/svg/LayoutSVGModelObject.h"
#include "core/layout/svg/LayoutSVGRoot.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/paint/PaintLayer.h"

// We can't enable this by default because saturated operations of LayoutUnit
// don't conform commutative law for overflowing results, preventing us from
// making fast-path and slow-path always return the same result.
#define ASSERT_SAME_RESULT_SLOW_AND_FAST_PATH (0 && ENABLE(ASSERT))

namespace blink {

static bool isAbsolutePositionUnderRelativePositionInline(const LayoutObject& object)
{
    if (object.styleRef().position() != AbsolutePosition)
        return false;
    if (LayoutObject* container = object.container())
        return container->isAnonymousBlock() && container->styleRef().position() == RelativePosition;
    return false;
}

static bool supportsCachedOffsets(const LayoutObject& object)
{
    // TODO(wangxianzhu): Move some conditions to fast path if possible.
    return !object.hasTransformRelatedProperty()
        && !object.hasReflection()
        && !object.hasFilter()
        && !object.isLayoutFlowThread()
        && !object.isLayoutMultiColumnSpannerPlaceholder()
        && object.styleRef().position() != FixedPosition
        && !object.styleRef().isFlippedBlocksWritingMode()
        // TODO(crbug.com/598094): Handle this in fast path.
        && !isAbsolutePositionUnderRelativePositionInline(object);
}

PaintInvalidationState::PaintInvalidationState(const LayoutView& layoutView, Vector<LayoutObject*>& pendingDelayedPaintInvalidations)
    : m_currentObject(layoutView)
    , m_clipped(false)
    , m_cachedOffsetsEnabled(true)
    , m_forcedSubtreeInvalidationWithinContainer(false)
    , m_forcedSubtreeInvalidationRectUpdateWithinContainer(false)
    , m_paintInvalidationContainer(layoutView.containerForPaintInvalidation())
    , m_pendingDelayedPaintInvalidations(pendingDelayedPaintInvalidations)
    , m_enclosingSelfPaintingLayer(*layoutView.layer())
#if ENABLE(ASSERT)
    , m_didUpdateForChildren(false)
#endif
{
    if (!supportsCachedOffsets(layoutView)) {
        m_cachedOffsetsEnabled = false;
        return;
    }

    FloatPoint point = layoutView.localToAncestorPoint(FloatPoint(), &m_paintInvalidationContainer, TraverseDocumentBoundaries | InputIsInFrameCoordinates);
    m_paintOffset = LayoutSize(point.x(), point.y());
}

// TODO(wangxianzhu): This is temporary for positioned object whose paintInvalidationContainer is different from
// the one we find during tree walk. Remove this after we fix the issue with tree walk in DOM-order.
PaintInvalidationState::PaintInvalidationState(const PaintInvalidationState& parentState, const LayoutBoxModelObject& currentObject, const LayoutBoxModelObject& paintInvalidationContainer)
    : m_currentObject(currentObject)
    , m_clipped(parentState.m_clipped)
    , m_cachedOffsetsEnabled(parentState.m_cachedOffsetsEnabled)
    , m_forcedSubtreeInvalidationWithinContainer(parentState.m_forcedSubtreeInvalidationWithinContainer)
    , m_forcedSubtreeInvalidationRectUpdateWithinContainer(parentState.m_forcedSubtreeInvalidationRectUpdateWithinContainer)
    , m_clipRect(parentState.m_clipRect)
    , m_paintOffset(parentState.m_paintOffset)
    , m_paintInvalidationContainer(paintInvalidationContainer)
    , m_svgTransform(parentState.m_svgTransform)
    , m_pendingDelayedPaintInvalidations(parentState.pendingDelayedPaintInvalidationTargets())
    , m_enclosingSelfPaintingLayer(parentState.enclosingSelfPaintingLayer(currentObject))
#if ENABLE(ASSERT)
    , m_didUpdateForChildren(true)
#endif
{
    ASSERT(parentState.m_didUpdateForChildren);
    ASSERT(!m_cachedOffsetsEnabled);
}

PaintInvalidationState::PaintInvalidationState(const PaintInvalidationState& parentState, const LayoutObject& currentObject)
    : m_currentObject(currentObject)
    , m_clipped(parentState.m_clipped)
    , m_cachedOffsetsEnabled(parentState.m_cachedOffsetsEnabled)
    , m_forcedSubtreeInvalidationWithinContainer(parentState.m_forcedSubtreeInvalidationWithinContainer)
    , m_forcedSubtreeInvalidationRectUpdateWithinContainer(parentState.m_forcedSubtreeInvalidationRectUpdateWithinContainer)
    , m_clipRect(parentState.m_clipRect)
    , m_paintOffset(parentState.m_paintOffset)
    , m_paintInvalidationContainer(currentObject.isPaintInvalidationContainer() ? toLayoutBoxModelObject(currentObject) : parentState.m_paintInvalidationContainer)
    , m_svgTransform(parentState.m_svgTransform)
    , m_pendingDelayedPaintInvalidations(parentState.pendingDelayedPaintInvalidationTargets())
    , m_enclosingSelfPaintingLayer(parentState.enclosingSelfPaintingLayer(currentObject))
#if ENABLE(ASSERT)
    , m_didUpdateForChildren(false)
#endif
{
    if (currentObject == parentState.m_currentObject) {
        // Sometimes we create a new PaintInvalidationState from parentState on the same object
        // (e.g. LayoutView, and the HorriblySlowRectMapping cases in LayoutBlock::invalidatePaintOfSubtreesIfNeeded()).
        // TODO(wangxianzhu): Avoid this for RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled().
#if ENABLE(ASSERT)
        m_didUpdateForChildren = parentState.m_didUpdateForChildren;
#endif
        return;
    }

    ASSERT(parentState.m_didUpdateForChildren);

    if (!currentObject.isBoxModelObject() && !currentObject.isSVG())
        return;

    if (m_cachedOffsetsEnabled && !supportsCachedOffsets(currentObject))
        m_cachedOffsetsEnabled = false;

    if (currentObject.isSVG()) {
        if (currentObject.isSVGRoot()) {
            m_svgTransform = toLayoutSVGRoot(currentObject).localToBorderBoxTransform();
            // Don't early return here, because the SVGRoot object needs to execute the later code
            // as a normal LayoutBox.
        } else {
            ASSERT(currentObject != m_paintInvalidationContainer);
            m_svgTransform *= currentObject.localToSVGParentTransform();
            return;
        }
    }

    if (currentObject == m_paintInvalidationContainer) {
        // When we hit a new paint invalidation container, we don't need to
        // continue forcing a check for paint invalidation, since we're
        // descending into a different invalidation container. (For instance if
        // our parents were moved, the entire container will just move.)
        m_forcedSubtreeInvalidationWithinContainer = false;
        m_forcedSubtreeInvalidationRectUpdateWithinContainer = false;

        m_clipped = false; // Will be updated in updateForChildren().
        m_paintOffset = LayoutSize();
        return;
    }

    if (!m_cachedOffsetsEnabled)
        return;

    if (currentObject.isLayoutView()) {
        ASSERT(&parentState.m_currentObject == toLayoutView(currentObject).frame()->ownerLayoutObject());
        m_paintOffset += toLayoutBox(parentState.m_currentObject).contentBoxOffset();
        // a LayoutView paints with a defined size but a pixel-rounded offset.
        m_paintOffset = LayoutSize(roundedIntSize(m_paintOffset));
        return;
    }

    if (currentObject.isBox())
        m_paintOffset += toLayoutBox(currentObject).locationOffset();

    if (currentObject.isInFlowPositioned() && currentObject.hasLayer())
        m_paintOffset += toLayoutBoxModelObject(currentObject).layer()->offsetForInFlowPosition();
}

void PaintInvalidationState::updateForChildren()
{
#if ENABLE(ASSERT)
    ASSERT(!m_didUpdateForChildren);
    m_didUpdateForChildren = true;
#endif

    if (!m_cachedOffsetsEnabled)
        return;

    if (!m_currentObject.isBoxModelObject() && !m_currentObject.isSVG())
        return;

    if (m_currentObject.isLayoutView()) {
        if (!m_currentObject.document().settings() || !m_currentObject.document().settings()->rootLayerScrolls()) {
            if (m_currentObject != m_paintInvalidationContainer) {
                m_paintOffset -= toLayoutView(m_currentObject).frameView()->scrollOffset();
                addClipRectRelativeToPaintOffset(toLayoutView(m_currentObject).viewRect());
            }
            return;
        }
    } else if (m_currentObject.isSVGRoot()) {
        const LayoutSVGRoot& svgRoot = toLayoutSVGRoot(m_currentObject);
        if (svgRoot.shouldApplyViewportClip())
            addClipRectRelativeToPaintOffset(LayoutRect(LayoutPoint(), LayoutSize(svgRoot.pixelSnappedSize())));
    } else if (m_currentObject.isTableRow()) {
        // Child table cell's locationOffset() includes its row's locationOffset().
        m_paintOffset -= toLayoutBox(m_currentObject).locationOffset();
    }

    if (!m_currentObject.hasOverflowClip())
        return;

    const LayoutBox& box = toLayoutBox(m_currentObject);

    // Do not clip scroll layer contents because the compositor expects the whole layer
    // to be always invalidated in-time.
    if (box.usesCompositedScrolling())
        ASSERT(!m_clipped); // The box should establish paint invalidation container, so no m_clipped inherited.
    else
        addClipRectRelativeToPaintOffset(LayoutRect(LayoutPoint(), LayoutSize(box.layer()->size())));

    m_paintOffset -= box.scrolledContentOffset();

    // FIXME: <http://bugs.webkit.org/show_bug.cgi?id=13443> Apply control clip if present.
}

static FloatPoint slowLocalToAncestorPoint(const LayoutObject& object, const LayoutBoxModelObject& ancestor, const FloatPoint& point)
{
    if (object.isLayoutView())
        return toLayoutView(object).localToAncestorPoint(point, &ancestor, TraverseDocumentBoundaries | InputIsInFrameCoordinates);
    return object.localToAncestorPoint(point, &ancestor, TraverseDocumentBoundaries);
}

LayoutPoint PaintInvalidationState::computePositionFromPaintInvalidationBacking() const
{
    ASSERT(!m_didUpdateForChildren);

    FloatPoint point;
    if (m_paintInvalidationContainer != m_currentObject) {
        if (m_cachedOffsetsEnabled) {
            if (m_currentObject.isSVG() && !m_currentObject.isSVGRoot())
                point = m_svgTransform.mapPoint(point);
            point += FloatPoint(m_paintOffset);
#if ASSERT_SAME_RESULT_SLOW_AND_FAST_PATH
            // TODO(wangxianzhu): We can't enable this ASSERT for now because of crbug.com/597745.
            // ASSERT(point == slowLocalOriginToAncestorPoint(m_currentObject, m_paintInvalidationContainer, FloatPoint());
#endif
        } else {
            point = slowLocalToAncestorPoint(m_currentObject, m_paintInvalidationContainer, FloatPoint());
        }
    }

    if (m_paintInvalidationContainer.layer()->groupedMapping())
        PaintLayer::mapPointInPaintInvalidationContainerToBacking(m_paintInvalidationContainer, point);

    return LayoutPoint(point);
}

LayoutRect PaintInvalidationState::computePaintInvalidationRectInBacking() const
{
    ASSERT(!m_didUpdateForChildren);

    if (m_currentObject.isSVG() && !m_currentObject.isSVGRoot())
        return computePaintInvalidationRectInBackingForSVG();

    LayoutRect rect = m_currentObject.localOverflowRectForPaintInvalidation();
    mapLocalRectToPaintInvalidationBacking(rect);
    return rect;
}

LayoutRect PaintInvalidationState::computePaintInvalidationRectInBackingForSVG() const
{
    LayoutRect rect;
    if (m_cachedOffsetsEnabled) {
        FloatRect svgRect = SVGLayoutSupport::localOverflowRectForPaintInvalidation(m_currentObject);
        rect = SVGLayoutSupport::transformPaintInvalidationRect(m_currentObject, m_svgTransform, svgRect);
        rect.move(m_paintOffset);
        if (m_clipped)
            rect.intersect(m_clipRect);
#if ASSERT_SAME_RESULT_SLOW_AND_FAST_PATH
        LayoutRect slowPathRect = SVGLayoutSupport::clippedOverflowRectForPaintInvalidation(m_currentObject, m_paintInvalidationContainer);
        // TODO(crbug.com/597902): Slow path misses clipping of paintInvalidationContainer.
        if (m_clipped)
            slowPathRect.intersect(m_clipRect);
        // TODO(crbug.com/597903): Fast path and slow path should generate equal empty rects.
        ASSERT((rect.isEmpty() && slowPathRect.isEmpty()) || rect == slowPathRect);
#endif
    } else {
        // TODO(wangxianzhu): Sometimes m_cachedOffsetsEnabled==false doesn't mean we can't use cached
        // m_svgTransform. We can use hybrid fast-path (for SVG) and slow-path (for things above the SVGRoot).
        rect = SVGLayoutSupport::clippedOverflowRectForPaintInvalidation(m_currentObject, m_paintInvalidationContainer);
    }

    if (m_paintInvalidationContainer.layer()->groupedMapping())
        PaintLayer::mapRectInPaintInvalidationContainerToBacking(m_paintInvalidationContainer, rect);
    return rect;
}

static void slowMapToVisualRectInAncestorSpace(const LayoutObject& object, const LayoutBoxModelObject& ancestor, LayoutRect& rect)
{
    // TODO(crbug.com/597965): LayoutBox::mapToVisualRectInAncestorSpace() incorrectly flips a rect
    // in its own space for writing mode. Here flip to workaround the flip.
    if (object.isBox() && (toLayoutBox(object).isWritingModeRoot() || (ancestor == object && object.styleRef().isFlippedBlocksWritingMode())))
        toLayoutBox(object).flipForWritingMode(rect);

    if (object.isLayoutView()) {
        toLayoutView(object).mapToVisualRectInAncestorSpace(&ancestor, rect, InputIsInFrameCoordinates, DefaultVisualRectFlags);
    } else if (object.isSVGRoot()) {
        // TODO(crbug.com/597813): This is to avoid the extra clip applied in LayoutSVGRoot::mapVisibleRectInAncestorSpace().
        toLayoutSVGRoot(object).LayoutReplaced::mapToVisualRectInAncestorSpace(&ancestor, rect);
    } else {
        object.mapToVisualRectInAncestorSpace(&ancestor, rect);
    }
}

void PaintInvalidationState::mapLocalRectToPaintInvalidationBacking(LayoutRect& rect) const
{
    ASSERT(!m_didUpdateForChildren);

    if (m_cachedOffsetsEnabled) {
#if ASSERT_SAME_RESULT_SLOW_AND_FAST_PATH
        LayoutRect slowPathRect(rect);
        slowMapToVisualRectInAncestorSpace(m_currentObject, m_paintInvalidationContainer, slowPathRect);
#endif
        rect.move(m_paintOffset);
        if (m_clipped)
            rect.intersect(m_clipRect);
#if ASSERT_SAME_RESULT_SLOW_AND_FAST_PATH
        // Make sure that the fast path and the slow path generate the same rect.
        // TODO(crbug.com/597902): Slow path misses clipping of paintInvalidationContainer.
        if (m_clipped)
            slowPathRect.intersect(m_clipRect);
        // TODO(wangxianzhu): The isLayoutView() condition is for cases that a sub-frame creates a
        // root PaintInvalidationState which doesn't inherit clip from ancestor frames.
        // Remove the condition when we eliminate the latter case of PaintInvalidationState(const LayoutView&, ...).
        // TODO(crbug.com/597903): Fast path and slow path should generate equal empty rects.
        ASSERT(m_currentObject.isLayoutView() || (rect.isEmpty() && slowPathRect.isEmpty()) || rect == slowPathRect);
#endif
    } else {
        slowMapToVisualRectInAncestorSpace(m_currentObject, m_paintInvalidationContainer, rect);
    }

    if (m_paintInvalidationContainer.layer()->groupedMapping())
        PaintLayer::mapRectInPaintInvalidationContainerToBacking(m_paintInvalidationContainer, rect);
}

void PaintInvalidationState::addClipRectRelativeToPaintOffset(const LayoutRect& localClipRect)
{
    LayoutRect clipRect = localClipRect;
    clipRect.move(m_paintOffset);
    if (m_clipped) {
        m_clipRect.intersect(clipRect);
    } else {
        m_clipRect = clipRect;
        m_clipped = true;
    }
}

PaintLayer& PaintInvalidationState::enclosingSelfPaintingLayer(const LayoutObject& layoutObject) const
{
    if (layoutObject.hasLayer() && toLayoutBoxModelObject(layoutObject).hasSelfPaintingLayer())
        return *toLayoutBoxModelObject(layoutObject).layer();

    return m_enclosingSelfPaintingLayer;
}

} // namespace blink
