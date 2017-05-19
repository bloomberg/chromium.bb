// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ObjectPaintInvalidator.h"

#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/layout/LayoutView.h"
#include "core/layout/api/LayoutPartItem.h"
#include "core/layout/compositing/CompositedLayerMapping.h"
#include "core/paint/FindPaintOffsetAndVisualRectNeedingUpdate.h"
#include "core/paint/PaintInvalidator.h"
#include "core/paint/PaintLayer.h"
#include "platform/PlatformChromeClient.h"
#include "platform/graphics/GraphicsLayer.h"

namespace blink {

static bool g_disable_paint_invalidation_state_asserts = false;

typedef HashMap<const LayoutObject*, LayoutRect> SelectionVisualRectMap;
static SelectionVisualRectMap& GetSelectionVisualRectMap() {
  DEFINE_STATIC_LOCAL(SelectionVisualRectMap, map, ());
  return map;
}

static void SetSelectionVisualRect(const LayoutObject& object,
                                   const LayoutRect& rect) {
  DCHECK(object.HasSelectionVisualRect() ==
         GetSelectionVisualRectMap().Contains(&object));
  if (rect.IsEmpty()) {
    if (object.HasSelectionVisualRect()) {
      GetSelectionVisualRectMap().erase(&object);
      object.GetMutableForPainting().SetHasPreviousSelectionVisualRect(false);
    }
  } else {
    GetSelectionVisualRectMap().Set(&object, rect);
    object.GetMutableForPainting().SetHasPreviousSelectionVisualRect(true);
  }
}

typedef HashMap<const LayoutObject*, LayoutPoint> LocationInBackingMap;
static LocationInBackingMap& GetLocationInBackingMap() {
  DEFINE_STATIC_LOCAL(LocationInBackingMap, map, ());
  return map;
}

void ObjectPaintInvalidator::ObjectWillBeDestroyed(const LayoutObject& object) {
  DCHECK(object.HasSelectionVisualRect() ==
         GetSelectionVisualRectMap().Contains(&object));
  if (object.HasSelectionVisualRect())
    GetSelectionVisualRectMap().erase(&object);

  DCHECK(object.HasLocationInBacking() ==
         GetLocationInBackingMap().Contains(&object));
  if (object.HasLocationInBacking())
    GetLocationInBackingMap().erase(&object);
}

using LayoutObjectTraversalFunctor = std::function<void(const LayoutObject&)>;

static void TraverseNonCompositingDescendantsInPaintOrder(
    const LayoutObject&,
    const LayoutObjectTraversalFunctor&);

static void
TraverseNonCompositingDescendantsBelongingToAncestorPaintInvalidationContainer(
    const LayoutObject& object,
    const LayoutObjectTraversalFunctor& functor) {
  // |object| is a paint invalidation container, but is not a stacking context
  // or is a non-block, so the paint invalidation container of stacked
  // descendants may not belong to |object| but belong to an ancestor. This
  // function traverses all such descendants. See Case 1a and Case 2 below for
  // details.
  DCHECK(object.IsPaintInvalidationContainer() &&
         (!object.StyleRef().IsStackingContext() || !object.IsLayoutBlock()));

  LayoutObject* descendant = object.NextInPreOrder(&object);
  while (descendant) {
    if (!descendant->HasLayer() || !descendant->StyleRef().IsStacked()) {
      // Case 1: The descendant is not stacked (or is stacked but has not been
      // allocated a layer yet during style change), so either it's a paint
      // invalidation container in the same situation as |object|, or its paint
      // invalidation container is in such situation. Keep searching until a
      // stacked layer is found.
      if (!object.IsLayoutBlock() && descendant->IsFloating()) {
        // Case 1a (rare): However, if the descendant is a floating object below
        // a composited non-block object, the subtree may belong to an ancestor
        // in paint order, thus recur into the subtree. Note that for
        // performance, we don't check whether the floating object's container
        // is above or under |object|, so we may traverse more than expected.
        // Example:
        // <span id="object" class="position: relative; will-change: transform">
        //   <div id="descendant" class="float: left"></div>"
        // </span>
        TraverseNonCompositingDescendantsInPaintOrder(*descendant, functor);
        descendant = descendant->NextInPreOrderAfterChildren(&object);
      } else {
        descendant = descendant->NextInPreOrder(&object);
      }
    } else if (!descendant->IsPaintInvalidationContainer()) {
      // Case 2: The descendant is stacked and is not composited.
      // The invalidation container of its subtree is our ancestor,
      // thus recur into the subtree.
      TraverseNonCompositingDescendantsInPaintOrder(*descendant, functor);
      descendant = descendant->NextInPreOrderAfterChildren(&object);
    } else if (descendant->StyleRef().IsStackingContext() &&
               descendant->IsLayoutBlock()) {
      // Case 3: The descendant is an invalidation container and is a stacking
      // context.  No objects in the subtree can have invalidation container
      // outside of it, thus skip the whole subtree.
      // This excludes non-block because there might be floating objects under
      // the descendant belonging to some ancestor in paint order (Case 1a).
      descendant = descendant->NextInPreOrderAfterChildren(&object);
    } else {
      // Case 4: The descendant is an invalidation container but not a stacking
      // context, or the descendant is a non-block stacking context.
      // This is the same situation as |object|, thus keep searching.
      descendant = descendant->NextInPreOrder(&object);
    }
  }
}

static void TraverseNonCompositingDescendantsInPaintOrder(
    const LayoutObject& object,
    const LayoutObjectTraversalFunctor& functor) {
  functor(object);
  LayoutObject* descendant = object.NextInPreOrder(&object);
  while (descendant) {
    if (!descendant->IsPaintInvalidationContainer()) {
      functor(*descendant);
      descendant = descendant->NextInPreOrder(&object);
    } else if (descendant->StyleRef().IsStackingContext() &&
               descendant->IsLayoutBlock()) {
      // The descendant is an invalidation container and is a stacking context.
      // No objects in the subtree can have invalidation container outside of
      // it, thus skip the whole subtree.
      // This excludes non-blocks because there might be floating objects under
      // the descendant belonging to some ancestor in paint order (Case 1a).
      descendant = descendant->NextInPreOrderAfterChildren(&object);
    } else {
      // If a paint invalidation container is not a stacking context, or the
      // descendant is a non-block stacking context, some of its descendants may
      // belong to the parent container.
      TraverseNonCompositingDescendantsBelongingToAncestorPaintInvalidationContainer(
          *descendant, functor);
      descendant = descendant->NextInPreOrderAfterChildren(&object);
    }
  }
}

static void SetPaintingLayerNeedsRepaintDuringTraverse(
    const LayoutObject& object) {
  if (object.HasLayer() &&
      ToLayoutBoxModelObject(object).HasSelfPaintingLayer()) {
    ToLayoutBoxModelObject(object).Layer()->SetNeedsRepaint();
  } else if (object.IsFloating() && object.Parent() &&
             !object.Parent()->IsLayoutBlock()) {
    object.PaintingLayer()->SetNeedsRepaint();
  }
}

void ObjectPaintInvalidator::
    InvalidateDisplayItemClientsIncludingNonCompositingDescendants(
        PaintInvalidationReason reason) {
  // This is valid because we want to invalidate the client in the display item
  // list of the current backing.
  DisableCompositingQueryAsserts disabler;

  SlowSetPaintingLayerNeedsRepaint();
  TraverseNonCompositingDescendantsInPaintOrder(
      object_, [reason](const LayoutObject& object) {
        SetPaintingLayerNeedsRepaintDuringTraverse(object);
        object.InvalidateDisplayItemClients(reason);
      });
}

DISABLE_CFI_PERF
void ObjectPaintInvalidator::InvalidatePaintOfPreviousVisualRect(
    const LayoutBoxModelObject& paint_invalidation_container,
    PaintInvalidationReason reason) {
  // It's caller's responsibility to ensure enclosingSelfPaintingLayer's
  // needsRepaint is set.  Don't set the flag here because getting
  // enclosingSelfPaintLayer has cost and the caller can use various ways (e.g.
  // PaintInvalidatinState::enclosingSelfPaintingLayer()) to reduce the cost.
  DCHECK(!object_.PaintingLayer() || object_.PaintingLayer()->NeedsRepaint());

  // These disablers are valid because we want to use the current
  // compositing/invalidation status.
  DisablePaintInvalidationStateAsserts invalidation_disabler;
  DisableCompositingQueryAsserts compositing_disabler;

  LayoutRect invalidation_rect = object_.VisualRect();
  InvalidatePaintUsingContainer(paint_invalidation_container, invalidation_rect,
                                reason);
  object_.InvalidateDisplayItemClients(reason);

  // This method may be used to invalidate paint of an object changing paint
  // invalidation container.  Clear previous visual rect on the original paint
  // invalidation container to avoid under-invalidation if the visual rect on
  // the new paint invalidation container happens to be the same as the old one.
  object_.GetMutableForPainting().ClearPreviousVisualRects();
}

void ObjectPaintInvalidator::
    InvalidatePaintIncludingNonCompositingDescendants() {
  // Since we're only painting non-composited layers, we know that they all
  // share the same paintInvalidationContainer.
  const LayoutBoxModelObject& paint_invalidation_container =
      object_.ContainerForPaintInvalidation();
  SlowSetPaintingLayerNeedsRepaint();
  TraverseNonCompositingDescendantsInPaintOrder(
      object_, [&paint_invalidation_container](const LayoutObject& object) {
        SetPaintingLayerNeedsRepaintDuringTraverse(object);
        ObjectPaintInvalidator(object).InvalidatePaintOfPreviousVisualRect(
            paint_invalidation_container, PaintInvalidationReason::kSubtree);
      });
}

void ObjectPaintInvalidator::
    InvalidatePaintIncludingNonSelfPaintingLayerDescendantsInternal(
        const LayoutBoxModelObject& paint_invalidation_container) {
  InvalidatePaintOfPreviousVisualRect(paint_invalidation_container,
                                      PaintInvalidationReason::kSubtree);
  for (LayoutObject* child = object_.SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (!child->HasLayer() ||
        !ToLayoutBoxModelObject(child)->Layer()->IsSelfPaintingLayer())
      ObjectPaintInvalidator(*child)
          .InvalidatePaintIncludingNonSelfPaintingLayerDescendantsInternal(
              paint_invalidation_container);
  }
}

void ObjectPaintInvalidator::
    InvalidatePaintIncludingNonSelfPaintingLayerDescendants(
        const LayoutBoxModelObject& paint_invalidation_container) {
  SlowSetPaintingLayerNeedsRepaint();
  InvalidatePaintIncludingNonSelfPaintingLayerDescendantsInternal(
      paint_invalidation_container);
}

void ObjectPaintInvalidator::InvalidateDisplayItemClient(
    const DisplayItemClient& client,
    PaintInvalidationReason reason) {
  // It's caller's responsibility to ensure enclosingSelfPaintingLayer's
  // needsRepaint is set.  Don't set the flag here because getting
  // enclosingSelfPaintLayer has cost and the caller can use various ways (e.g.
  // PaintInvalidatinState::enclosingSelfPaintingLayer()) to reduce the cost.
  DCHECK(!object_.PaintingLayer() || object_.PaintingLayer()->NeedsRepaint());

  client.SetDisplayItemsUncached(reason);

  if (FrameView* frame_view = object_.GetFrameView())
    frame_view->TrackObjectPaintInvalidation(client, reason);
}

template <typename T>
void AddJsonObjectForRect(TracedValue* value, const char* name, const T& rect) {
  value->BeginDictionary(name);
  value->SetDouble("x", rect.X());
  value->SetDouble("y", rect.Y());
  value->SetDouble("width", rect.Width());
  value->SetDouble("height", rect.Height());
  value->EndDictionary();
}

static std::unique_ptr<TracedValue> JsonObjectForPaintInvalidationInfo(
    const LayoutRect& rect,
    const String& invalidation_reason) {
  std::unique_ptr<TracedValue> value = TracedValue::Create();
  AddJsonObjectForRect(value.get(), "rect", rect);
  value->SetString("invalidation_reason", invalidation_reason);
  return value;
}

static void InvalidatePaintRectangleOnWindow(
    const LayoutBoxModelObject& paint_invalidation_container,
    const IntRect& dirty_rect) {
  FrameView* frame_view = paint_invalidation_container.GetFrameView();
  DCHECK(paint_invalidation_container.IsLayoutView() &&
         paint_invalidation_container.Layer()->GetCompositingState() ==
             kNotComposited);

  if (!frame_view)
    return;

  if (paint_invalidation_container.GetDocument().Printing() &&
      !RuntimeEnabledFeatures::printBrowserEnabled())
    return;

  DCHECK(frame_view->GetFrame().OwnerLayoutItem().IsNull());

  IntRect paint_rect = dirty_rect;
  paint_rect.Intersect(frame_view->VisibleContentRect());
  if (paint_rect.IsEmpty())
    return;

  if (PlatformChromeClient* client = frame_view->GetChromeClient())
    client->InvalidateRect(frame_view->ContentsToRootFrame(paint_rect));
}

void ObjectPaintInvalidator::SetBackingNeedsPaintInvalidationInRect(
    const LayoutBoxModelObject& paint_invalidation_container,
    const LayoutRect& rect,
    PaintInvalidationReason reason) {
  // https://bugs.webkit.org/show_bug.cgi?id=61159 describes an unreproducible
  // crash here, so assert but check that the layer is composited.
  DCHECK(paint_invalidation_container.GetCompositingState() != kNotComposited);

  PaintLayer& layer = *paint_invalidation_container.Layer();
  if (layer.GroupedMapping()) {
    if (GraphicsLayer* squashing_layer =
            layer.GroupedMapping()->SquashingLayer()) {
      // Note: the subpixel accumulation of layer() does not need to be added
      // here. It is already taken into account.
      squashing_layer->SetNeedsDisplayInRect(EnclosingIntRect(rect), reason,
                                             object_);
    }
  } else if (object_.CompositedScrollsWithRespectTo(
                 paint_invalidation_container)) {
    layer.GetCompositedLayerMapping()->SetScrollingContentsNeedDisplayInRect(
        rect, reason, object_);
  } else if (paint_invalidation_container.UsesCompositedScrolling()) {
    DCHECK(object_ == paint_invalidation_container);
    if (reason ==
            PaintInvalidationReason::kBackgroundOnScrollingContentsLayer ||
        reason == PaintInvalidationReason::kCaret) {
      layer.GetCompositedLayerMapping()->SetScrollingContentsNeedDisplayInRect(
          rect, reason, object_);
    } else {
      layer.GetCompositedLayerMapping()
          ->SetNonScrollingContentsNeedDisplayInRect(rect, reason, object_);
    }
  } else {
    // Otherwise invalidate everything.
    layer.GetCompositedLayerMapping()->SetContentsNeedDisplayInRect(
        rect, reason, object_);
  }
}

void ObjectPaintInvalidator::InvalidatePaintUsingContainer(
    const LayoutBoxModelObject& paint_invalidation_container,
    const LayoutRect& dirty_rect,
    PaintInvalidationReason invalidation_reason) {
  // TODO(wangxianzhu): Enable the following assert after paint invalidation for
  // spv2 is ready.
  // DCHECK(!RuntimeEnabledFeatures::slimmingPaintV2Enabled());

  if (paint_invalidation_container.GetFrameView()->ShouldThrottleRendering())
    return;

  DCHECK(g_disable_paint_invalidation_state_asserts ||
         object_.GetDocument().Lifecycle().GetState() ==
             (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled()
                  ? DocumentLifecycle::kInPrePaint
                  : DocumentLifecycle::kInPaintInvalidation));

  if (dirty_rect.IsEmpty())
    return;

  CHECK(object_.IsRooted());

  // FIXME: Unify "devtools.timeline.invalidationTracking" and
  // "blink.invalidation". crbug.com/413527.
  TRACE_EVENT_INSTANT1(
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"),
      "PaintInvalidationTracking", TRACE_EVENT_SCOPE_THREAD, "data",
      InspectorPaintInvalidationTrackingEvent::Data(
          &object_, paint_invalidation_container));
  TRACE_EVENT2(
      TRACE_DISABLED_BY_DEFAULT("blink.invalidation"),
      "LayoutObject::invalidatePaintUsingContainer()", "object",
      object_.DebugName().Ascii(), "info",
      JsonObjectForPaintInvalidationInfo(
          dirty_rect, PaintInvalidationReasonToString(invalidation_reason)));

  // This conditional handles situations where non-rooted (and hence
  // non-composited) frames are painted, such as SVG images.
  if (!paint_invalidation_container.IsPaintInvalidationContainer()) {
    InvalidatePaintRectangleOnWindow(paint_invalidation_container,
                                     EnclosingIntRect(dirty_rect));
  }

  auto* view = paint_invalidation_container.View();
  if (view && view->UsesCompositing()) {
    if (paint_invalidation_container.IsPaintInvalidationContainer()) {
      SetBackingNeedsPaintInvalidationInRect(paint_invalidation_container,
                                             dirty_rect, invalidation_reason);
    }
  }
}

LayoutRect ObjectPaintInvalidator::InvalidatePaintRectangle(
    const LayoutRect& dirty_rect,
    DisplayItemClient* display_item_client) {
  CHECK(object_.IsRooted());

  if (dirty_rect.IsEmpty())
    return LayoutRect();

  if (object_.View()->GetDocument().Printing() &&
      !RuntimeEnabledFeatures::printBrowserEnabled())
    return LayoutRect();  // Don't invalidate paints if we're printing.

  const LayoutBoxModelObject& paint_invalidation_container =
      object_.ContainerForPaintInvalidation();
  LayoutRect dirty_rect_on_backing = dirty_rect;
  PaintLayer::MapRectToPaintInvalidationBacking(
      object_, paint_invalidation_container, dirty_rect_on_backing);
  dirty_rect_on_backing.Move(object_.ScrollAdjustmentForPaintInvalidation(
      paint_invalidation_container));
  InvalidatePaintUsingContainer(paint_invalidation_container,
                                dirty_rect_on_backing,
                                PaintInvalidationReason::kRectangle);

  SlowSetPaintingLayerNeedsRepaint();
  if (display_item_client) {
    InvalidateDisplayItemClient(*display_item_client,
                                PaintInvalidationReason::kRectangle);
  } else {
    object_.InvalidateDisplayItemClients(PaintInvalidationReason::kRectangle);
  }

  return dirty_rect_on_backing;
}

void ObjectPaintInvalidator::SlowSetPaintingLayerNeedsRepaint() {
  if (PaintLayer* painting_layer = object_.PaintingLayer())
    painting_layer->SetNeedsRepaint();
}

LayoutPoint ObjectPaintInvalidator::LocationInBacking() const {
  DCHECK(object_.HasLocationInBacking() ==
         GetLocationInBackingMap().Contains(&object_));
  return object_.HasLocationInBacking() ? GetLocationInBackingMap().at(&object_)
                                        : object_.VisualRect().Location();
}

void ObjectPaintInvalidator::SetLocationInBacking(const LayoutPoint& location) {
  DCHECK(object_.HasLocationInBacking() ==
         GetLocationInBackingMap().Contains(&object_));
  if (location == object_.VisualRect().Location()) {
    if (object_.HasLocationInBacking()) {
      GetLocationInBackingMap().erase(&object_);
      object_.GetMutableForPainting().SetHasPreviousLocationInBacking(false);
    }
  } else {
    GetLocationInBackingMap().Set(&object_, location);
    object_.GetMutableForPainting().SetHasPreviousLocationInBacking(true);
  }
}

void ObjectPaintInvalidatorWithContext::FullyInvalidatePaint(
    PaintInvalidationReason reason,
    const LayoutRect& old_visual_rect,
    const LayoutRect& new_visual_rect) {
  // The following logic avoids invalidating twice if one set of bounds contains
  // the other.
  if (!new_visual_rect.Contains(old_visual_rect)) {
    LayoutRect invalidation_rect = old_visual_rect;
    InvalidatePaintRectangleWithContext(invalidation_rect, reason);

    if (invalidation_rect.Contains(new_visual_rect))
      return;
  }

  InvalidatePaintRectangleWithContext(new_visual_rect, reason);
}

bool ObjectPaintInvalidatorWithContext::ParentFullyInvalidatedOnSameBacking() {
  if (!object_.Parent() || !context_.parent_context)
    return false;

  if (!IsImmediateFullPaintInvalidationReason(
          object_.Parent()->FullPaintInvalidationReason()))
    return false;

  // Parent and child should have the same paint invalidation container.
  if (context_.parent_context->paint_invalidation_container !=
      context_.paint_invalidation_container)
    return false;

  // Both parent and child are contents of the paint invalidation container,
  // so they are on the same backing.
  if (object_.Parent() != context_.paint_invalidation_container)
    return true;

  // If the paint invalidation container (i.e. parent) uses composited
  // scrolling, parent and child might be on different backing (scrolling
  // container vs scrolling contents).
  return !context_.paint_invalidation_container->UsesCompositedScrolling();
}

void ObjectPaintInvalidatorWithContext::InvalidatePaintRectangleWithContext(
    const LayoutRect& rect,
    PaintInvalidationReason reason) {
  if (rect.IsEmpty())
    return;

  // If the parent has fully invalidated and its visual rect covers this object
  // on the same backing, skip the invalidation.
  if (ParentFullyInvalidatedOnSameBacking() &&
      (context_.parent_context->old_visual_rect.Contains(rect) ||
       object_.Parent()->VisualRect().Contains(rect)))
    return;

  InvalidatePaintUsingContainer(*context_.paint_invalidation_container, rect,
                                reason);
}

DISABLE_CFI_PERF
PaintInvalidationReason
ObjectPaintInvalidatorWithContext::ComputePaintInvalidationReason() {
  // This is before any early return to ensure the background obscuration status
  // is saved.
  bool background_obscuration_changed = false;
  bool background_obscured = object_.BackgroundIsKnownToBeObscured();
  if (background_obscured != object_.PreviousBackgroundObscured()) {
    object_.GetMutableForPainting().SetPreviousBackgroundObscured(
        background_obscured);
    background_obscuration_changed = true;
  }

  if (context_.subtree_flags &
      PaintInvalidatorContext::kSubtreeFullInvalidation)
    return PaintInvalidationReason::kSubtree;

  if (object_.ShouldDoFullPaintInvalidation())
    return object_.FullPaintInvalidationReason();

  if (context_.old_visual_rect.IsEmpty() && object_.VisualRect().IsEmpty())
    return PaintInvalidationReason::kNone;

  if (background_obscuration_changed)
    return PaintInvalidationReason::kBackground;

  if (object_.PaintedOutputOfObjectHasNoEffectRegardlessOfSize())
    return PaintInvalidationReason::kNone;

  // Force full paint invalidation if the outline may be affected by descendants
  // and this object is marked for checking paint invalidation for any reason.
  if (object_.OutlineMayBeAffectedByDescendants() ||
      object_.PreviousOutlineMayBeAffectedByDescendants()) {
    object_.GetMutableForPainting()
        .UpdatePreviousOutlineMayBeAffectedByDescendants();
    return PaintInvalidationReason::kOutline;
  }

  // If the size is zero on one of our bounds then we know we're going to have
  // to do a full invalidation of either old bounds or new bounds.
  if (context_.old_visual_rect.IsEmpty())
    return PaintInvalidationReason::kAppeared;
  if (object_.VisualRect().IsEmpty())
    return PaintInvalidationReason::kDisappeared;

  // If we shifted, we don't know the exact reason so we are conservative and
  // trigger a full invalidation. Shifting could be caused by some layout
  // property (left / top) or some in-flow layoutObject inserted / removed
  // before us in the tree.
  if (object_.VisualRect().Location() != context_.old_visual_rect.Location())
    return PaintInvalidationReason::kGeometry;

  if (context_.new_location != context_.old_location)
    return PaintInvalidationReason::kGeometry;

  // Incremental invalidation is only applicable to LayoutBoxes. Return
  // PaintInvalidationIncremental no matter if oldVisualRect and newVisualRect
  // are equal because a LayoutBox may need paint invalidation if its border box
  // changes. BoxPaintInvalidator may also override this reason with a full
  // paint invalidation reason if needed.
  if (object_.IsBox())
    return PaintInvalidationReason::kIncremental;

  if (context_.old_visual_rect != object_.VisualRect())
    return PaintInvalidationReason::kGeometry;

  return PaintInvalidationReason::kNone;
}

DISABLE_CFI_PERF
void ObjectPaintInvalidatorWithContext::InvalidateSelectionIfNeeded(
    PaintInvalidationReason reason) {
  // Update selection rect when we are doing full invalidation with geometry
  // change (in case that the object is moved, composite status changed, etc.)
  // or shouldInvalidationSelection is set (in case that the selection itself
  // changed).
  bool full_invalidation = IsImmediateFullPaintInvalidationReason(reason);
  if (!full_invalidation && !object_.ShouldInvalidateSelection())
    return;

  DCHECK(object_.HasSelectionVisualRect() ==
         GetSelectionVisualRectMap().Contains(&object_));
  LayoutRect old_selection_rect;
  if (object_.HasSelectionVisualRect())
    old_selection_rect = GetSelectionVisualRectMap().at(&object_);

  LayoutRect new_selection_rect;
#if DCHECK_IS_ON()
  FindVisualRectNeedingUpdateScope finder(object_, context_, old_selection_rect,
                                          new_selection_rect);
#endif
  if (context_.NeedsVisualRectUpdate(object_)) {
    new_selection_rect = object_.LocalSelectionRect();
    context_.MapLocalRectToVisualRectInBacking(object_, new_selection_rect);
  } else {
    new_selection_rect = old_selection_rect;
  }

  SetSelectionVisualRect(object_, new_selection_rect);

  if (!full_invalidation) {
    FullyInvalidatePaint(PaintInvalidationReason::kSelection,
                         old_selection_rect, new_selection_rect);
    context_.painting_layer->SetNeedsRepaint();
    object_.InvalidateDisplayItemClients(PaintInvalidationReason::kSelection);
  }
}

DISABLE_CFI_PERF
PaintInvalidationReason
ObjectPaintInvalidatorWithContext::InvalidatePaintWithComputedReason(
    PaintInvalidationReason reason) {
  DCHECK(!(context_.subtree_flags &
           PaintInvalidatorContext::kSubtreeNoInvalidation));

  // We need to invalidate the selection before checking for whether we are
  // doing a full invalidation.  This is because we need to update the previous
  // selection rect regardless.
  InvalidateSelectionIfNeeded(reason);

  switch (reason) {
    case PaintInvalidationReason::kNone:
      // There are corner cases that the display items need to be invalidated
      // for paint offset mutation, but incurs no pixel difference (i.e. bounds
      // stay the same) so no rect-based invalidation is issued. See
      // crbug.com/508383 and crbug.com/515977.
      if (!RuntimeEnabledFeatures::slimmingPaintV2Enabled() &&
          (context_.subtree_flags &
           PaintInvalidatorContext::kSubtreeInvalidationChecking) &&
          !object_.IsSVGChild()) {
        // For SPv1, we conservatively assume the object changed paint offset
        // except for non-root SVG whose paint offset is always zero.
        reason = PaintInvalidationReason::kGeometry;
        break;
      }

      if (object_.IsSVG() &&
          (context_.subtree_flags &
           PaintInvalidatorContext::kSubtreeSVGResourceChange)) {
        reason = PaintInvalidationReason::kSVGResource;
        break;
      }
      return PaintInvalidationReason::kNone;
    case PaintInvalidationReason::kDelayedFull:
      return PaintInvalidationReason::kDelayedFull;
    default:
      DCHECK(IsImmediateFullPaintInvalidationReason(reason));
      // This allows descendants to know the computed reason if it's different
      // from the original reason before paint invalidation.
      object_.GetMutableForPainting()
          .SetShouldDoFullPaintInvalidationWithoutGeometryChange(reason);
      FullyInvalidatePaint(reason, context_.old_visual_rect,
                           object_.VisualRect());
  }

  context_.painting_layer->SetNeedsRepaint();
  object_.InvalidateDisplayItemClients(reason);
  return reason;
}

DisablePaintInvalidationStateAsserts::DisablePaintInvalidationStateAsserts()
    : disabler_(&g_disable_paint_invalidation_state_asserts, true) {}

}  // namespace blink
