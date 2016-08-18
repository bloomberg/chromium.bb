// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ObjectPaintInvalidator.h"

#include "core/layout/LayoutBlockFlow.h"
#include "core/paint/PaintInvalidator.h"
#include "core/paint/PaintLayer.h"

namespace blink {

typedef HashMap<const LayoutObject*, LayoutRect> SelectionPaintInvalidationMap;
static SelectionPaintInvalidationMap& selectionPaintInvalidationMap()
{
    DEFINE_STATIC_LOCAL(SelectionPaintInvalidationMap, map, ());
    return map;
}

static void setPreviousSelectionPaintInvalidationRect(const LayoutObject& object, const LayoutRect& rect)
{
    if (rect.isEmpty())
        selectionPaintInvalidationMap().remove(&object);
    else
        selectionPaintInvalidationMap().set(&object, rect);
}

void ObjectPaintInvalidator::objectWillBeDestroyed(const LayoutObject& object)
{
    selectionPaintInvalidationMap().remove(&object);
}

bool ObjectPaintInvalidator::incrementallyInvalidatePaint()
{
    const LayoutRect& oldBounds = m_context.oldBounds;
    const LayoutRect& newBounds = m_context.newBounds;

    DCHECK(oldBounds.location() == newBounds.location());

    LayoutUnit deltaRight = newBounds.maxX() - oldBounds.maxX();
    LayoutUnit deltaBottom = newBounds.maxY() - oldBounds.maxY();
    if (!deltaRight && !deltaBottom)
        return false;

    if (deltaRight > 0) {
        LayoutRect invalidationRect(oldBounds.maxX(), newBounds.y(), deltaRight, newBounds.height());
        m_object.invalidatePaintUsingContainer(*m_context.paintInvalidationContainer, invalidationRect, PaintInvalidationIncremental);
    } else if (deltaRight < 0) {
        LayoutRect invalidationRect(newBounds.maxX(), oldBounds.y(), -deltaRight, oldBounds.height());
        m_object.invalidatePaintUsingContainer(*m_context.paintInvalidationContainer, invalidationRect, PaintInvalidationIncremental);
    }

    if (deltaBottom > 0) {
        LayoutRect invalidationRect(newBounds.x(), oldBounds.maxY(), newBounds.width(), deltaBottom);
        m_object.invalidatePaintUsingContainer(*m_context.paintInvalidationContainer, invalidationRect, PaintInvalidationIncremental);
    } else if (deltaBottom < 0) {
        LayoutRect invalidationRect(oldBounds.x(), newBounds.maxY(), oldBounds.width(), -deltaBottom);
        m_object.invalidatePaintUsingContainer(*m_context.paintInvalidationContainer, invalidationRect, PaintInvalidationIncremental);
    }

    return true;
}

void ObjectPaintInvalidator::fullyInvalidatePaint(PaintInvalidationReason reason, const LayoutRect& oldBounds, const LayoutRect& newBounds)
{
    // The following logic avoids invalidating twice if one set of bounds contains the other.
    if (!newBounds.contains(oldBounds)) {
        LayoutRect invalidationRect = oldBounds;
        m_object.invalidatePaintUsingContainer(*m_context.paintInvalidationContainer, invalidationRect, reason);

        if (invalidationRect.contains(newBounds))
            return;
    }

    m_object.invalidatePaintUsingContainer(*m_context.paintInvalidationContainer, newBounds, reason);
}

PaintInvalidationReason ObjectPaintInvalidator::computePaintInvalidationReason()
{
    // This is before any early return to ensure the background obscuration status is saved.
    bool backgroundObscurationChanged = false;
    bool backgroundObscured = m_object.backgroundIsKnownToBeObscured();
    if (backgroundObscured != m_object.previousBackgroundObscured()) {
        m_object.getMutableForPainting().setPreviousBackgroundObscured(backgroundObscured);
        backgroundObscurationChanged = true;
    }

    if (m_context.forcedSubtreeInvalidationFlags & PaintInvalidatorContext::ForcedSubtreeFullInvalidation)
        return PaintInvalidationSubtree;

    if (m_object.shouldDoFullPaintInvalidation())
        return m_object.fullPaintInvalidationReason();

    if (m_context.oldBounds.isEmpty() && m_context.newBounds.isEmpty())
        return PaintInvalidationNone;

    if (backgroundObscurationChanged)
        return PaintInvalidationBackgroundObscurationChange;

    if (m_object.paintedOutputOfObjectHasNoEffectRegardlessOfSize())
        return PaintInvalidationNone;

    const ComputedStyle& style = m_object.styleRef();

    // The outline may change shape because of position change of descendants. For simplicity,
    // just force full paint invalidation if this object is marked for checking paint invalidation
    // for any reason.
    // TODO(wangxianzhu): Optimize this.
    if (style.hasOutline())
        return PaintInvalidationOutline;

    bool locationChanged = m_context.newLocation != m_context.oldLocation;

    // If the bounds are the same then we know that none of the statements below
    // can match, so we can early out. However, we can't return PaintInvalidationNone even if
    // !locationChagned, but conservatively return PaintInvalidationIncremental because we are
    // not sure whether paint invalidation is actually needed just based on information known
    // to LayoutObject. For example, a LayoutBox may need paint invalidation if border box changes.
    if (m_context.oldBounds == m_context.newBounds)
        return locationChanged ? PaintInvalidationLocationChange : PaintInvalidationIncremental;

    // If the size is zero on one of our bounds then we know we're going to have
    // to do a full invalidation of either old bounds or new bounds.
    if (m_context.oldBounds.isEmpty())
        return PaintInvalidationBecameVisible;
    if (m_context.newBounds.isEmpty())
        return PaintInvalidationBecameInvisible;

    // If we shifted, we don't know the exact reason so we are conservative and trigger a full invalidation. Shifting could
    // be caused by some layout property (left / top) or some in-flow layoutObject inserted / removed before us in the tree.
    if (m_context.newBounds.location() != m_context.oldBounds.location())
        return PaintInvalidationBoundsChange;

    if (locationChanged)
        return PaintInvalidationLocationChange;

    return PaintInvalidationIncremental;
}

void ObjectPaintInvalidator::invalidateSelectionIfNeeded(PaintInvalidationReason reason)
{
    // Update selection rect when we are doing full invalidation (in case that the object is moved,
    // composite status changed, etc.) or shouldInvalidationSelection is set (in case that the
    // selection itself changed).
    bool fullInvalidation = isImmediateFullPaintInvalidationReason(reason);
    if (!fullInvalidation && !m_object.shouldInvalidateSelection())
        return;

    LayoutRect oldSelectionRect = selectionPaintInvalidationMap().get(&m_object);
    LayoutRect newSelectionRect = m_object.localSelectionRect();
    if (!newSelectionRect.isEmpty())
        m_context.mapLocalRectToPaintInvalidationBacking(m_object, newSelectionRect);

    newSelectionRect.move(m_object.scrollAdjustmentForPaintInvalidation(*m_context.paintInvalidationContainer));

    setPreviousSelectionPaintInvalidationRect(m_object, newSelectionRect);

    if (!fullInvalidation) {
        fullyInvalidatePaint(PaintInvalidationSelection, oldSelectionRect, newSelectionRect);
        m_context.paintingLayer->setNeedsRepaint();
        m_object.invalidateDisplayItemClients(PaintInvalidationSelection);
    }
}

PaintInvalidationReason ObjectPaintInvalidator::invalidatePaintIfNeededWithComputedReason(PaintInvalidationReason reason)
{
    // We need to invalidate the selection before checking for whether we are doing a full invalidation.
    // This is because we need to update the previous selection rect regardless.
    invalidateSelectionIfNeeded(reason);

    if (reason == PaintInvalidationIncremental && !incrementallyInvalidatePaint())
        reason = PaintInvalidationNone;

    switch (reason) {
    case PaintInvalidationNone:
        // TODO(trchen): Currently we don't keep track of paint offset of layout objects.
        // There are corner cases that the display items need to be invalidated for paint offset
        // mutation, but incurs no pixel difference (i.e. bounds stay the same) so no rect-based
        // invalidation is issued. See crbug.com/508383 and crbug.com/515977.
        // This is a workaround to force display items to update paint offset.
        if (m_context.forcedSubtreeInvalidationFlags & PaintInvalidatorContext::ForcedSubtreeInvalidationChecking) {
            reason = PaintInvalidationLocationChange;
            break;
        }
        return PaintInvalidationNone;
    case PaintInvalidationIncremental:
        break;
    case PaintInvalidationDelayedFull:
        return PaintInvalidationDelayedFull;
    default:
        DCHECK(isImmediateFullPaintInvalidationReason(reason));
        fullyInvalidatePaint(reason, m_context.oldBounds, m_context.newBounds);
    }

    m_context.paintingLayer->setNeedsRepaint();
    m_object.invalidateDisplayItemClients(reason);
    return reason;
}

} // namespace blink
