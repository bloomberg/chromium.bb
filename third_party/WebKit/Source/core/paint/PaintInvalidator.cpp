// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintInvalidator.h"

#include "core/editing/FrameSelection.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutTable.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/paint/ObjectPaintProperties.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/PaintLayerScrollableArea.h"
#include "core/paint/PaintPropertyTreeBuilder.h"

namespace blink {

// TODO(wangxianzhu): Combine this into PaintInvalidator::mapLocalRectToPaintInvalidationBacking() when removing PaintInvalidationState.
static LayoutRect mapLocalRectToPaintInvalidationBacking(GeometryMapper& geometryMapper, const LayoutObject& object, const FloatRect& localRect, const PaintInvalidatorContext& context)
{
    // TODO(wkorman): The flip below is required because visual rects are
    // currently in "physical coordinates with flipped block-flow direction"
    // (see LayoutBoxModelObject.h) but we need them to be in physical
    // coordinates.
    FloatRect rect = localRect;
    if (object.isBox())
        toLayoutBox(object).flipForWritingMode(rect);

    LayoutRect result;
    if (object == context.paintInvalidationContainer) {
        result = LayoutRect(rect);
    } else {
        rect.moveBy(FloatPoint(context.treeBuilderContext.current.paintOffset));

        bool success = false;
        PropertyTreeState currentTreeState(context.treeBuilderContext.current.transform, context.treeBuilderContext.current.clip, context.treeBuilderContext.currentEffect);
        const PropertyTreeState& containerTreeState = context.paintInvalidationContainer->objectPaintProperties()->localBorderBoxProperties()->propertyTreeState;
        result = LayoutRect(geometryMapper.mapToVisualRectInDestinationSpace(rect, currentTreeState, containerTreeState, success));
        DCHECK(success);
    }

    if (context.paintInvalidationContainer->layer()->groupedMapping())
        PaintLayer::mapRectInPaintInvalidationContainerToBacking(*context.paintInvalidationContainer, result);
    return result;
}

void PaintInvalidatorContext::mapLocalRectToPaintInvalidationBacking(const LayoutObject& object, LayoutRect& rect) const
{
    GeometryMapper geometryMapper;
    rect = blink::mapLocalRectToPaintInvalidationBacking(geometryMapper, object, FloatRect(rect), *this);
}

LayoutRect PaintInvalidator::mapLocalRectToPaintInvalidationBacking(const LayoutObject& object, const FloatRect& localRect, const PaintInvalidatorContext& context)
{
    return blink::mapLocalRectToPaintInvalidationBacking(m_geometryMapper, object, localRect, context);
}

LayoutRect PaintInvalidator::computePaintInvalidationRectInBacking(const LayoutObject& object, const PaintInvalidatorContext& context)
{
    FloatRect localRect;
    if (object.isSVG() && !object.isSVGRoot())
        localRect = SVGLayoutSupport::localOverflowRectForPaintInvalidation(object);
    else
        localRect = FloatRect(object.localOverflowRectForPaintInvalidation());

    return mapLocalRectToPaintInvalidationBacking(object, localRect, context);
}

LayoutPoint PaintInvalidator::computeLocationFromPaintInvalidationBacking(const LayoutObject& object, const PaintInvalidatorContext& context)
{
    FloatPoint point;
    if (object != context.paintInvalidationContainer) {
        point.moveBy(FloatPoint(context.treeBuilderContext.current.paintOffset));

        bool success = false;
        PropertyTreeState currentTreeState(context.treeBuilderContext.current.transform, context.treeBuilderContext.current.clip, context.treeBuilderContext.currentEffect);
        const PropertyTreeState& containerTreeState = context.paintInvalidationContainer->objectPaintProperties()->localBorderBoxProperties()->propertyTreeState;
        point = m_geometryMapper.mapRectToDestinationSpace(FloatRect(point, FloatSize()), currentTreeState, containerTreeState, success).location();
        DCHECK(success);
    }

    if (context.paintInvalidationContainer->layer()->groupedMapping())
        PaintLayer::mapPointInPaintInvalidationContainerToBacking(*context.paintInvalidationContainer, point);

    return LayoutPoint(point);
}

void PaintInvalidator::updatePaintingLayer(const LayoutObject& object, PaintInvalidatorContext& context)
{
    if (object.hasLayer() && toLayoutBoxModelObject(object).hasSelfPaintingLayer())
        context.paintingLayer = toLayoutBoxModelObject(object).layer();

    if (object.isLayoutBlockFlow() && toLayoutBlockFlow(object).containsFloats())
        context.paintingLayer->setNeedsPaintPhaseFloat();

    if (object == context.paintingLayer->layoutObject())
        return;

    if (object.styleRef().hasOutline())
        context.paintingLayer->setNeedsPaintPhaseDescendantOutlines();

    if (object.hasBoxDecorationBackground()
        // We also paint overflow controls in background phase.
        || (object.hasOverflowClip() && toLayoutBox(object).getScrollableArea()->hasOverflowControls())) {
        context.paintingLayer->setNeedsPaintPhaseDescendantBlockBackgrounds();
    }

    if (object.isTable()) {
        const LayoutTable& table = toLayoutTable(object);
        if (table.collapseBorders() && !table.collapsedBorders().isEmpty())
            context.paintingLayer->setNeedsPaintPhaseDescendantBlockBackgrounds();
    }
}

void PaintInvalidator::updateContext(const LayoutObject& object, PaintInvalidatorContext& context)
{
    if (object.isPaintInvalidationContainer()) {
        context.paintInvalidationContainer = toLayoutBoxModelObject(&object);
        if (object.styleRef().isStackingContext())
            context.paintInvalidationContainerForStackedContents = toLayoutBoxModelObject(&object);
    } else if (object.isLayoutView()) {
        // paintInvalidationContainerForStackedContents is only for stacked descendants in its own frame,
        // because it doesn't establish stacking context for stacked contents in sub-frames.
        // Contents stacked in the root stacking context in this frame should use this frame's paintInvalidationContainer.
        context.paintInvalidationContainerForStackedContents = context.paintInvalidationContainer;
    } else if (object.styleRef().isStacked()
        // This is to exclude some objects (e.g. LayoutText) inheriting stacked style from parent but aren't actually stacked.
        && object.hasLayer()
        && context.paintInvalidationContainer != context.paintInvalidationContainerForStackedContents) {
        // The current object is stacked, so we should use m_paintInvalidationContainerForStackedContents as its
        // paint invalidation container on which the current object is painted.
        context.paintInvalidationContainer = context.paintInvalidationContainerForStackedContents;
        if (context.forcedSubtreeInvalidationFlags & PaintInvalidatorContext::ForcedSubtreeFullInvalidationForStackedContents)
            context.forcedSubtreeInvalidationFlags |= PaintInvalidatorContext::ForcedSubtreeFullInvalidation;
    }

    if (object == context.paintInvalidationContainer) {
        // When we hit a new paint invalidation container, we don't need to
        // continue forcing a check for paint invalidation, since we're
        // descending into a different invalidation container. (For instance if
        // our parents were moved, the entire container will just move.)
        if (object != context.paintInvalidationContainerForStackedContents) {
            // However, we need to keep the ForcedSubtreeFullInvalidationForStackedContents flag
            // if the current object isn't the paint invalidation container of stacked contents.
            context.forcedSubtreeInvalidationFlags &= PaintInvalidatorContext::ForcedSubtreeFullInvalidationForStackedContents;
        } else {
            context.forcedSubtreeInvalidationFlags = 0;
        }
    }

    DCHECK(context.paintInvalidationContainer == object.containerForPaintInvalidation());
    DCHECK(context.paintingLayer == object.paintingLayer());

    if (object.mayNeedPaintInvalidationSubtree())
        context.forcedSubtreeInvalidationFlags |= PaintInvalidatorContext::ForcedSubtreeInvalidationChecking;

    context.oldBounds = object.previousPaintInvalidationRect();
    context.oldLocation = object.previousPositionFromPaintInvalidationBacking();
    context.newBounds = computePaintInvalidationRectInBacking(object, context);
    context.newLocation = computeLocationFromPaintInvalidationBacking(object, context);

    IntSize adjustment = object.scrollAdjustmentForPaintInvalidation(*context.paintInvalidationContainer);
    context.newLocation.move(adjustment);
    context.newBounds.move(adjustment);

    object.getMutableForPainting().setPreviousPaintInvalidationRect(context.newBounds);
    object.getMutableForPainting().setPreviousPositionFromPaintInvalidationBacking(context.newLocation);
}

void PaintInvalidator::invalidatePaintIfNeeded(FrameView& frameView, PaintInvalidatorContext& context)
{
    LayoutView* layoutView = frameView.layoutView();
    CHECK(layoutView);

    context.paintInvalidationContainer = context.paintInvalidationContainerForStackedContents = &layoutView->containerForPaintInvalidation();
    context.paintingLayer = layoutView->layer();

    if (!frameView.frame().settings() || !frameView.frame().settings()->rootLayerScrolls())
        frameView.invalidatePaintOfScrollControlsIfNeeded(context);

    if (frameView.frame().selection().isCaretBoundsDirty())
        frameView.frame().selection().invalidateCaretRect();

    // Temporary callback for crbug.com/487345,402044
    // TODO(ojan): Make this more general to be used by PositionObserver
    // and rAF throttling.
    IntRect visibleRect = frameView.rootFrameToContents(frameView.computeVisibleArea());
    layoutView->sendMediaPositionChangeNotifications(visibleRect);
}

void PaintInvalidator::invalidatePaintIfNeeded(const LayoutObject& object, PaintInvalidatorContext& context)
{
    object.getMutableForPainting().ensureIsReadyForPaintInvalidation();

    if (!context.forcedSubtreeInvalidationFlags && !object.shouldCheckForPaintInvalidationRegardlessOfPaintInvalidationState())
        return;

    updatePaintingLayer(object, context);

    if (object.document().printing())
        return; // Don't invalidate paints if we're printing.

    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("blink.invalidation"), "PaintInvalidator::invalidatePaintIfNeeded()", "object", object.debugName().ascii());

    updateContext(object, context);

    if (!object.shouldCheckForPaintInvalidationRegardlessOfPaintInvalidationState() && context.forcedSubtreeInvalidationFlags == PaintInvalidatorContext::ForcedSubtreeInvalidationRectUpdate) {
        // We are done updating the paint invalidation rect. No other paint invalidation work to do for this object.
        return;
    }

    switch (object.invalidatePaintIfNeeded(context)) {
    case PaintInvalidationDelayedFull:
        m_pendingDelayedPaintInvalidations.append(&object);
        break;
    case PaintInvalidationSubtree:
        context.forcedSubtreeInvalidationFlags |= (PaintInvalidatorContext::ForcedSubtreeFullInvalidation | PaintInvalidatorContext::ForcedSubtreeFullInvalidationForStackedContents);
        break;
    case PaintInvalidationSVGResourceChange:
        context.forcedSubtreeInvalidationFlags |= PaintInvalidatorContext::ForcedSubtreeInvalidationChecking;
        break;
    default:
        break;
    }

    if (context.oldLocation != context.newLocation)
        context.forcedSubtreeInvalidationFlags |= PaintInvalidatorContext::ForcedSubtreeInvalidationChecking;

    object.getMutableForPainting().clearPaintInvalidationFlags();
}

void PaintInvalidator::processPendingDelayedPaintInvalidations()
{
    for (auto target : m_pendingDelayedPaintInvalidations)
        target->getMutableForPainting().setShouldDoFullPaintInvalidation(PaintInvalidationDelayedFull);
}

} // namespace blink
