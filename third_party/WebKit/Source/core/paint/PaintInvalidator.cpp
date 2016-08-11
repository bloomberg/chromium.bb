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
#include "core/paint/PaintLayer.h"
#include "core/paint/PaintLayerScrollableArea.h"

namespace blink {

void PaintInvalidatorContext::mapLocalRectToPaintInvalidationBacking(const LayoutObject& object, LayoutRect& rect) const
{
    // TODO(wangxianzhu): For now this is the same as the slow path in PaintInvalidationState.cpp
    // (slowMapToVisualRectInAncestorSpace()). Should implement this with GeometryMapper.
    if (object.isBox())
        toLayoutBox(object).flipForWritingMode(rect);

    if (object.isLayoutView())
        toLayoutView(object).mapToVisualRectInAncestorSpace(paintInvalidationContainer, rect, InputIsInFrameCoordinates, DefaultVisualRectFlags);
    else
        object.mapToVisualRectInAncestorSpace(paintInvalidationContainer, rect);
}

static LayoutRect computePaintInvalidationRectInBacking(const LayoutObject& object, const PaintInvalidatorContext& context)
{
    if (object.isSVG() && !object.isSVGRoot()) {
        // TODO(wangxianzhu): For now this is the same as the slow path in PaintInvalidationState.cpp
        // (PaintInvalidationState::computePaintInvalidationRectInBackingForSVG()). Should implement this with GeometryMapper.
        LayoutRect rect = SVGLayoutSupport::clippedOverflowRectForPaintInvalidation(object, *context.paintInvalidationContainer);
        if (context.paintInvalidationContainer->layer()->groupedMapping())
            PaintLayer::mapRectInPaintInvalidationContainerToBacking(*context.paintInvalidationContainer, rect);
        return rect;
    }

    LayoutRect rect = object.localOverflowRectForPaintInvalidation();
    context.mapLocalRectToPaintInvalidationBacking(object, rect);
    return rect;
}

static LayoutPoint computeLocationFromPaintInvalidationBacking(const LayoutObject& object, const PaintInvalidatorContext& context)
{
    // TODO(wangxianzhu): For now this is the same as the slow path in PaintInvalidationState.cpp
    // (slowLocalToAncestorPoint()). Should implement this with GeometryMapper.
    FloatPoint point;
    if (object != context.paintInvalidationContainer) {
        if (object.isLayoutView()) {
            point = toLayoutView(object).localToAncestorPoint(point, context.paintInvalidationContainer, TraverseDocumentBoundaries | InputIsInFrameCoordinates);
        } else {
            point = object.localToAncestorPoint(point, context.paintInvalidationContainer, TraverseDocumentBoundaries);
            // Paint invalidation does not include scroll of paintInvalidationContainer.
            if (context.paintInvalidationContainer->isBox()) {
                const LayoutBox* box = toLayoutBox(context.paintInvalidationContainer);
                if (box->hasOverflowClip())
                    point.move(box->scrolledContentOffset());
            }
        }
    }


    if (context.paintInvalidationContainer->layer()->groupedMapping())
        PaintLayer::mapPointInPaintInvalidationContainerToBacking(*context.paintInvalidationContainer, point);

    return LayoutPoint(point);
}

static void updatePaintingLayer(const LayoutObject& object, PaintInvalidatorContext& context)
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

static void updateContext(const LayoutObject& object, PaintInvalidatorContext& context)
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
