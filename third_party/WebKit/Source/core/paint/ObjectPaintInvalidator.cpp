// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ObjectPaintInvalidator.h"

#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutView.h"
#include "core/layout/api/LayoutPartItem.h"
#include "core/layout/compositing/CompositedLayerMapping.h"
#include "core/paint/PaintInvalidator.h"
#include "core/paint/PaintLayer.h"
#include "platform/HostWindow.h"
#include "platform/graphics/GraphicsLayer.h"

namespace blink {

static bool gDisablePaintInvalidationStateAsserts = false;

typedef HashMap<const LayoutObject*, LayoutRect> SelectionPaintInvalidationMap;
static SelectionPaintInvalidationMap& selectionPaintInvalidationMap() {
  DEFINE_STATIC_LOCAL(SelectionPaintInvalidationMap, map, ());
  return map;
}

static void setPreviousSelectionPaintInvalidationRect(
    const LayoutObject& object,
    const LayoutRect& rect) {
  if (rect.isEmpty())
    selectionPaintInvalidationMap().remove(&object);
  else
    selectionPaintInvalidationMap().set(&object, rect);
}

void ObjectPaintInvalidator::objectWillBeDestroyed(const LayoutObject& object) {
  selectionPaintInvalidationMap().remove(&object);
}

// TODO(trchen): Use std::function<void, LayoutObject&> when available.
template <typename LayoutObjectTraversalFunctor>
void traverseNonCompositingDescendantsInPaintOrder(
    const LayoutObject&,
    const LayoutObjectTraversalFunctor&);

template <typename LayoutObjectTraversalFunctor>
void traverseNonCompositingDescendantsBelongingToAncestorPaintInvalidationContainer(
    const LayoutObject& object,
    const LayoutObjectTraversalFunctor& functor) {
  // |object| is a paint invalidation container but is not a stacking context,
  // so the paint invalidation container of stacked descendants don't belong to
  // |object| but belong to an ancestor. This function traverses all such
  // descendants.
  DCHECK(object.isPaintInvalidationContainer() &&
         !object.styleRef().isStackingContext());

  LayoutObject* descendant = object.nextInPreOrder(&object);
  while (descendant) {
    if (!descendant->hasLayer() || !descendant->styleRef().isStacked()) {
      // Case 1: The descendant is not stacked (or is stacked but has not been
      // allocated a layer yet during style change), so either it's a paint
      // invalidation container in the same situation as |object|, or its paint
      // invalidation container is in such situation. Keep searching until a
      // stacked layer is found.
      descendant = descendant->nextInPreOrder(&object);
    } else if (!descendant->isPaintInvalidationContainer()) {
      // Case 2: The descendant is stacked and is not composited.
      // The invalidation container of its subtree is our ancestor,
      // thus recur into the subtree.
      traverseNonCompositingDescendantsInPaintOrder(*descendant, functor);
      descendant = descendant->nextInPreOrderAfterChildren(&object);
    } else if (descendant->styleRef().isStackingContext()) {
      // Case 3: The descendant is an invalidation container and is a stacking
      // context.  No objects in the subtree can have invalidation container
      // outside of it, thus skip the whole subtree.
      descendant = descendant->nextInPreOrderAfterChildren(&object);
    } else {
      // Case 4: The descendant is an invalidation container but not a stacking
      // context.  This is the same situation as |object|, thus keep searching.
      descendant = descendant->nextInPreOrder(&object);
    }
  }
}

template <typename LayoutObjectTraversalFunctor>
void traverseNonCompositingDescendantsInPaintOrder(
    const LayoutObject& object,
    const LayoutObjectTraversalFunctor& functor) {
  functor(object);
  LayoutObject* descendant = object.nextInPreOrder(&object);
  while (descendant) {
    if (!descendant->isPaintInvalidationContainer()) {
      functor(*descendant);
      descendant = descendant->nextInPreOrder(&object);
    } else if (descendant->styleRef().isStackingContext()) {
      // The descendant is an invalidation container and is a stacking context.
      // No objects in the subtree can have invalidation container outside of
      // it, thus skip the whole subtree.
      descendant = descendant->nextInPreOrderAfterChildren(&object);
    } else {
      // If a paint invalidation container is not a stacking context,
      // some of its descendants may belong to the parent container.
      traverseNonCompositingDescendantsBelongingToAncestorPaintInvalidationContainer(
          *descendant, functor);
      descendant = descendant->nextInPreOrderAfterChildren(&object);
    }
  }
}

void ObjectPaintInvalidator::
    invalidateDisplayItemClientsIncludingNonCompositingDescendants(
        PaintInvalidationReason reason) {
  // This is valid because we want to invalidate the client in the display item
  // list of the current backing.
  DisableCompositingQueryAsserts disabler;

  slowSetPaintingLayerNeedsRepaint();
  traverseNonCompositingDescendantsInPaintOrder(
      m_object, [reason](const LayoutObject& object) {
        if (object.hasLayer() &&
            toLayoutBoxModelObject(object).hasSelfPaintingLayer())
          toLayoutBoxModelObject(object).layer()->setNeedsRepaint();
        object.invalidateDisplayItemClients(reason);
      });
}

DISABLE_CFI_PERF
void ObjectPaintInvalidator::invalidatePaintOfPreviousPaintInvalidationRect(
    const LayoutBoxModelObject& paintInvalidationContainer,
    PaintInvalidationReason reason) {
  // It's caller's responsibility to ensure enclosingSelfPaintingLayer's
  // needsRepaint is set.  Don't set the flag here because getting
  // enclosingSelfPaintLayer has cost and the caller can use various ways (e.g.
  // PaintInvalidatinState::enclosingSelfPaintingLayer()) to reduce the cost.
  DCHECK(!m_object.paintingLayer() || m_object.paintingLayer()->needsRepaint());

  // These disablers are valid because we want to use the current
  // compositing/invalidation status.
  DisablePaintInvalidationStateAsserts invalidationDisabler;
  DisableCompositingQueryAsserts compositingDisabler;

  LayoutRect invalidationRect = m_object.previousPaintInvalidationRect();
  invalidatePaintUsingContainer(paintInvalidationContainer, invalidationRect,
                                reason);
  m_object.invalidateDisplayItemClients(reason);

  // This method may be used to invalidate paint of an object changing paint
  // invalidation container.  Clear previous paint invalidation rect on the
  // original paint invalidation container to avoid under-invalidation if the
  // new paint invalidation rect on the new paint invalidation container happens
  // to be the same as the old one.
  m_object.getMutableForPainting().clearPreviousPaintInvalidationRects();
}

void ObjectPaintInvalidator::
    invalidatePaintIncludingNonCompositingDescendants() {
  // Since we're only painting non-composited layers, we know that they all
  // share the same paintInvalidationContainer.
  const LayoutBoxModelObject& paintInvalidationContainer =
      m_object.containerForPaintInvalidation();
  traverseNonCompositingDescendantsInPaintOrder(
      m_object, [&paintInvalidationContainer](const LayoutObject& object) {
        if (object.hasLayer())
          toLayoutBoxModelObject(object).layer()->setNeedsRepaint();
        ObjectPaintInvalidator(object)
            .invalidatePaintOfPreviousPaintInvalidationRect(
                paintInvalidationContainer, PaintInvalidationSubtree);
      });
}

void ObjectPaintInvalidator::
    invalidatePaintIncludingNonSelfPaintingLayerDescendantsInternal(
        const LayoutBoxModelObject& paintInvalidationContainer) {
  invalidatePaintOfPreviousPaintInvalidationRect(paintInvalidationContainer,
                                                 PaintInvalidationSubtree);
  for (LayoutObject* child = m_object.slowFirstChild(); child;
       child = child->nextSibling()) {
    if (!child->hasLayer() ||
        !toLayoutBoxModelObject(child)->layer()->isSelfPaintingLayer())
      ObjectPaintInvalidator(*child)
          .invalidatePaintIncludingNonSelfPaintingLayerDescendantsInternal(
              paintInvalidationContainer);
  }
}

void ObjectPaintInvalidator::
    invalidatePaintIncludingNonSelfPaintingLayerDescendants(
        const LayoutBoxModelObject& paintInvalidationContainer) {
  slowSetPaintingLayerNeedsRepaint();
  invalidatePaintIncludingNonSelfPaintingLayerDescendantsInternal(
      paintInvalidationContainer);
}

void ObjectPaintInvalidator::invalidateDisplayItemClient(
    const DisplayItemClient& client,
    PaintInvalidationReason reason) {
  // It's caller's responsibility to ensure enclosingSelfPaintingLayer's
  // needsRepaint is set.  Don't set the flag here because getting
  // enclosingSelfPaintLayer has cost and the caller can use various ways (e.g.
  // PaintInvalidatinState::enclosingSelfPaintingLayer()) to reduce the cost.
  DCHECK(!m_object.paintingLayer() || m_object.paintingLayer()->needsRepaint());

  client.setDisplayItemsUncached(reason);

  if (FrameView* frameView = m_object.frameView())
    frameView->trackObjectPaintInvalidation(client, reason);
}

template <typename T>
void addJsonObjectForRect(TracedValue* value, const char* name, const T& rect) {
  value->beginDictionary(name);
  value->setDouble("x", rect.x());
  value->setDouble("y", rect.y());
  value->setDouble("width", rect.width());
  value->setDouble("height", rect.height());
  value->endDictionary();
}

static std::unique_ptr<TracedValue> jsonObjectForPaintInvalidationInfo(
    const LayoutRect& rect,
    const String& invalidationReason) {
  std::unique_ptr<TracedValue> value = TracedValue::create();
  addJsonObjectForRect(value.get(), "rect", rect);
  value->setString("invalidation_reason", invalidationReason);
  return value;
}

static void invalidatePaintRectangleOnWindow(
    const LayoutBoxModelObject& paintInvalidationContainer,
    const IntRect& dirtyRect) {
  FrameView* frameView = paintInvalidationContainer.frameView();
  DCHECK(paintInvalidationContainer.isLayoutView() &&
         paintInvalidationContainer.layer()->compositingState() ==
             NotComposited);
  if (!frameView || paintInvalidationContainer.document().printing())
    return;

  DCHECK(frameView->frame().ownerLayoutItem().isNull());

  IntRect paintRect = dirtyRect;
  paintRect.intersect(frameView->visibleContentRect());
  if (paintRect.isEmpty())
    return;

  if (HostWindow* window = frameView->getHostWindow())
    window->invalidateRect(frameView->contentsToRootFrame(paintRect));
}

void ObjectPaintInvalidator::setBackingNeedsPaintInvalidationInRect(
    const LayoutBoxModelObject& paintInvalidationContainer,
    const LayoutRect& rect,
    PaintInvalidationReason reason) {
  // https://bugs.webkit.org/show_bug.cgi?id=61159 describes an unreproducible
  // crash here, so assert but check that the layer is composited.
  DCHECK(paintInvalidationContainer.compositingState() != NotComposited);

  PaintLayer& layer = *paintInvalidationContainer.layer();
  if (layer.groupedMapping()) {
    if (GraphicsLayer* squashingLayer =
            layer.groupedMapping()->squashingLayer()) {
      // Note: the subpixel accumulation of layer() does not need to be added
      // here. It is already taken into account.
      squashingLayer->setNeedsDisplayInRect(enclosingIntRect(rect), reason,
                                            m_object);
    }
  } else if (m_object.compositedScrollsWithRespectTo(
                 paintInvalidationContainer)) {
    layer.compositedLayerMapping()->setScrollingContentsNeedDisplayInRect(
        rect, reason, m_object);
  } else if (paintInvalidationContainer.usesCompositedScrolling()) {
    if (layer.compositedLayerMapping()
            ->backgroundPaintsOntoScrollingContentsLayer()) {
      // TODO(flackr): Get a correct rect in the context of the scrolling
      // contents layer to update rather than updating the entire rect.
      const LayoutRect& scrollingContentsRect =
          toLayoutBox(m_object).layoutOverflowRect();
      layer.compositedLayerMapping()->setScrollingContentsNeedDisplayInRect(
          scrollingContentsRect, reason, m_object);
      layer.setNeedsRepaint();
      invalidateDisplayItemClient(
          *layer.compositedLayerMapping()->scrollingContentsLayer(), reason);
    }
    layer.compositedLayerMapping()->setNonScrollingContentsNeedDisplayInRect(
        rect, reason, m_object);
  } else {
    // Otherwise invalidate everything.
    layer.compositedLayerMapping()->setContentsNeedDisplayInRect(rect, reason,
                                                                 m_object);
  }
}

void ObjectPaintInvalidator::invalidatePaintUsingContainer(
    const LayoutBoxModelObject& paintInvalidationContainer,
    const LayoutRect& dirtyRect,
    PaintInvalidationReason invalidationReason) {
  // TODO(wangxianzhu): Enable the following assert after paint invalidation for
  // spv2 is ready.
  // DCHECK(!RuntimeEnabledFeatures::slimmingPaintV2Enabled());

  if (paintInvalidationContainer.frameView()->shouldThrottleRendering())
    return;

  DCHECK(gDisablePaintInvalidationStateAsserts ||
         m_object.document().lifecycle().state() ==
             (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled()
                  ? DocumentLifecycle::InPrePaint
                  : DocumentLifecycle::InPaintInvalidation));

  if (dirtyRect.isEmpty())
    return;

  CHECK(m_object.isRooted());

  // FIXME: Unify "devtools.timeline.invalidationTracking" and
  // "blink.invalidation". crbug.com/413527.
  TRACE_EVENT_INSTANT1(
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"),
      "PaintInvalidationTracking", TRACE_EVENT_SCOPE_THREAD, "data",
      InspectorPaintInvalidationTrackingEvent::data(
          &m_object, paintInvalidationContainer));
  TRACE_EVENT2(
      TRACE_DISABLED_BY_DEFAULT("blink.invalidation"),
      "LayoutObject::invalidatePaintUsingContainer()", "object",
      m_object.debugName().ascii(), "info",
      jsonObjectForPaintInvalidationInfo(
          dirtyRect, paintInvalidationReasonToString(invalidationReason)));

  // This conditional handles situations where non-rooted (and hence
  // non-composited) frames are painted, such as SVG images.
  if (!paintInvalidationContainer.isPaintInvalidationContainer())
    invalidatePaintRectangleOnWindow(paintInvalidationContainer,
                                     enclosingIntRect(dirtyRect));

  if (paintInvalidationContainer.view()->usesCompositing() &&
      paintInvalidationContainer.isPaintInvalidationContainer())
    setBackingNeedsPaintInvalidationInRect(paintInvalidationContainer,
                                           dirtyRect, invalidationReason);
}

LayoutRect ObjectPaintInvalidator::invalidatePaintRectangle(
    const LayoutRect& dirtyRect,
    DisplayItemClient* displayItemClient) {
  CHECK(m_object.isRooted());

  if (dirtyRect.isEmpty())
    return LayoutRect();

  if (m_object.view()->document().printing())
    return LayoutRect();  // Don't invalidate paints if we're printing.

  const LayoutBoxModelObject& paintInvalidationContainer =
      m_object.containerForPaintInvalidation();
  LayoutRect dirtyRectOnBacking = dirtyRect;
  PaintLayer::mapRectToPaintInvalidationBacking(
      m_object, paintInvalidationContainer, dirtyRectOnBacking);
  dirtyRectOnBacking.move(m_object.scrollAdjustmentForPaintInvalidation(
      paintInvalidationContainer));
  invalidatePaintUsingContainer(paintInvalidationContainer, dirtyRectOnBacking,
                                PaintInvalidationRectangle);

  slowSetPaintingLayerNeedsRepaint();
  if (displayItemClient)
    invalidateDisplayItemClient(*displayItemClient, PaintInvalidationRectangle);
  else
    m_object.invalidateDisplayItemClients(PaintInvalidationRectangle);

  return dirtyRectOnBacking;
}

void ObjectPaintInvalidator::slowSetPaintingLayerNeedsRepaint() {
  if (PaintLayer* paintingLayer = m_object.paintingLayer())
    paintingLayer->setNeedsRepaint();
}

void ObjectPaintInvalidatorWithContext::fullyInvalidatePaint(
    PaintInvalidationReason reason,
    const LayoutRect& oldBounds,
    const LayoutRect& newBounds) {
  // The following logic avoids invalidating twice if one set of bounds contains
  // the other.
  if (!newBounds.contains(oldBounds)) {
    LayoutRect invalidationRect = oldBounds;
    invalidatePaintUsingContainer(*m_context.paintInvalidationContainer,
                                  invalidationRect, reason);

    if (invalidationRect.contains(newBounds))
      return;
  }

  invalidatePaintUsingContainer(*m_context.paintInvalidationContainer,
                                newBounds, reason);
}

PaintInvalidationReason
ObjectPaintInvalidatorWithContext::computePaintInvalidationReason() {
  // This is before any early return to ensure the background obscuration status
  // is saved.
  bool backgroundObscurationChanged = false;
  bool backgroundObscured = m_object.backgroundIsKnownToBeObscured();
  if (backgroundObscured != m_object.previousBackgroundObscured()) {
    m_object.getMutableForPainting().setPreviousBackgroundObscured(
        backgroundObscured);
    backgroundObscurationChanged = true;
  }

  if (m_context.forcedSubtreeInvalidationFlags &
      PaintInvalidatorContext::ForcedSubtreeFullInvalidation)
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

  // The outline may change shape because of position change of descendants. For
  // simplicity, just force full paint invalidation if this object is marked for
  // checking paint invalidation for any reason.
  // TODO(wangxianzhu): Optimize this.
  if (style.hasOutline())
    return PaintInvalidationOutline;

  // If the size is zero on one of our bounds then we know we're going to have
  // to do a full invalidation of either old bounds or new bounds.
  if (m_context.oldBounds.isEmpty())
    return PaintInvalidationBecameVisible;
  if (m_context.newBounds.isEmpty())
    return PaintInvalidationBecameInvisible;

  // If we shifted, we don't know the exact reason so we are conservative and
  // trigger a full invalidation. Shifting could be caused by some layout
  // property (left / top) or some in-flow layoutObject inserted / removed
  // before us in the tree.
  if (m_context.newBounds.location() != m_context.oldBounds.location())
    return PaintInvalidationBoundsChange;

  if (m_context.newLocation != m_context.oldLocation)
    return PaintInvalidationLocationChange;

  // Incremental invalidation is only applicable to LayoutBoxes. Return
  // PaintInvalidationIncremental no matter if oldBounds and newBounds are equal
  // because a LayoutBox may need paint invalidation if its border box changes.
  if (m_object.isBox())
    return PaintInvalidationIncremental;

  if (m_context.oldBounds != m_context.newBounds)
    return PaintInvalidationBoundsChange;

  return PaintInvalidationNone;
}

void ObjectPaintInvalidatorWithContext::invalidateSelectionIfNeeded(
    PaintInvalidationReason reason) {
  // Update selection rect when we are doing full invalidation (in case that the
  // object is moved, composite status changed, etc.) or
  // shouldInvalidationSelection is set (in case that the selection itself
  // changed).
  bool fullInvalidation = isImmediateFullPaintInvalidationReason(reason);
  if (!fullInvalidation && !m_object.shouldInvalidateSelection())
    return;

  LayoutRect oldSelectionRect = selectionPaintInvalidationMap().get(&m_object);
  LayoutRect newSelectionRect = m_object.localSelectionRect();
  if (!newSelectionRect.isEmpty()) {
    m_context.mapLocalRectToPaintInvalidationBacking(m_object,
                                                     newSelectionRect);
    newSelectionRect.move(m_object.scrollAdjustmentForPaintInvalidation(
        *m_context.paintInvalidationContainer));
  }

  setPreviousSelectionPaintInvalidationRect(m_object, newSelectionRect);

  if (!fullInvalidation) {
    fullyInvalidatePaint(PaintInvalidationSelection, oldSelectionRect,
                         newSelectionRect);
    m_context.paintingLayer->setNeedsRepaint();
    m_object.invalidateDisplayItemClients(PaintInvalidationSelection);
  }
}

PaintInvalidationReason
ObjectPaintInvalidatorWithContext::invalidatePaintIfNeededWithComputedReason(
    PaintInvalidationReason reason) {
  // We need to invalidate the selection before checking for whether we are
  // doing a full invalidation.  This is because we need to update the previous
  // selection rect regardless.
  invalidateSelectionIfNeeded(reason);

  switch (reason) {
    case PaintInvalidationNone:
      // TODO(trchen): Currently we don't keep track of paint offset of layout
      // objects.  There are corner cases that the display items need to be
      // invalidated for paint offset mutation, but incurs no pixel difference
      // (i.e. bounds stay the same) so no rect-based invalidation is issued.
      // See crbug.com/508383 and crbug.com/515977.  This is a workaround to
      // force display items to update paint offset.
      if (m_context.forcedSubtreeInvalidationFlags &
          PaintInvalidatorContext::ForcedSubtreeInvalidationChecking) {
        reason = PaintInvalidationLocationChange;
        break;
      }
      return PaintInvalidationNone;
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

DisablePaintInvalidationStateAsserts::DisablePaintInvalidationStateAsserts()
    : m_disabler(&gDisablePaintInvalidationStateAsserts, true) {}

}  // namespace blink
