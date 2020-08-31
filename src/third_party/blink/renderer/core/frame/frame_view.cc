// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/frame_view.h"

#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/frame/frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_geometry.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/page/page.h"

namespace blink {

Frame& FrameView::GetFrame() const {
  if (const LocalFrameView* lfv = DynamicTo<LocalFrameView>(this))
    return lfv->GetFrame();
  return DynamicTo<RemoteFrameView>(this)->GetFrame();
}

bool FrameView::CanThrottleRenderingForPropagation() const {
  if (CanThrottleRendering())
    return true;
  LocalFrame* parent_frame = DynamicTo<LocalFrame>(GetFrame().Tree().Parent());
  if (!parent_frame)
    return false;
  Frame& frame = GetFrame();
  LayoutEmbeddedContent* owner = frame.OwnerLayoutObject();
  return !owner && frame.IsCrossOriginToMainFrame();
}

bool FrameView::DisplayLockedInParentFrame() {
  Frame& frame = GetFrame();
  LayoutEmbeddedContent* owner = frame.OwnerLayoutObject();
  // We check the inclusive ancestor to determine whether the subtree is locked,
  // since the contents of the frame are in the subtree of the frame, so they
  // would be locked if the frame owner is itself locked.
  return owner && DisplayLockUtilities::NearestLockedInclusiveAncestor(*owner);
}

bool FrameView::UpdateViewportIntersection(unsigned flags,
                                           bool needs_occlusion_tracking) {
  bool can_skip_sticky_frame_tracking =
      flags & IntersectionObservation::kCanSkipStickyFrameTracking;

  if (!(flags & IntersectionObservation::kImplicitRootObserversNeedUpdate))
    return can_skip_sticky_frame_tracking;
  // This should only run in child frames.
  Frame& frame = GetFrame();
  HTMLFrameOwnerElement* owner_element = frame.DeprecatedLocalOwner();
  if (!owner_element)
    return can_skip_sticky_frame_tracking;
  Document& owner_document = owner_element->GetDocument();
  IntPoint viewport_offset;
  IntRect viewport_intersection, mainframe_document_intersection;
  DocumentLifecycle::LifecycleState parent_lifecycle_state =
      owner_document.Lifecycle().GetState();
  FrameOcclusionState occlusion_state =
      owner_document.GetFrame()->GetOcclusionState();
  bool should_compute_occlusion =
      needs_occlusion_tracking &&
      occlusion_state == FrameOcclusionState::kGuaranteedNotOccluded &&
      parent_lifecycle_state >= DocumentLifecycle::kPrePaintClean &&
      RuntimeEnabledFeatures::IntersectionObserverV2Enabled();

  LayoutEmbeddedContent* owner_layout_object =
      owner_element->GetLayoutEmbeddedContent();
  if (!owner_layout_object || owner_layout_object->ContentSize().IsEmpty()) {
    // The frame is detached from layout, not visible, or zero size; leave
    // viewport_intersection empty, and signal the frame as occluded if
    // necessary.
    occlusion_state = FrameOcclusionState::kPossiblyOccluded;
  } else if (parent_lifecycle_state >= DocumentLifecycle::kLayoutClean &&
             !owner_document.View()->NeedsLayout()) {
    unsigned geometry_flags =
        IntersectionGeometry::kShouldUseReplacedContentRect;
    if (should_compute_occlusion)
      geometry_flags |= IntersectionGeometry::kShouldComputeVisibility;

    IntersectionGeometry geometry(nullptr, *owner_element, {},
                                  {IntersectionObserver::kMinimumThreshold},
                                  geometry_flags);
    PhysicalRect new_rect_in_parent = geometry.IntersectionRect();
    if (new_rect_in_parent.size != rect_in_parent_.size ||
        ((new_rect_in_parent.X() - rect_in_parent_.X()).Abs() +
             (new_rect_in_parent.Y() - rect_in_parent_.Y()).Abs() >
         LayoutUnit(kMaxChildFrameScreenRectMovement))) {
      rect_in_parent_ = new_rect_in_parent;
      if (Page* page = GetFrame().GetPage()) {
        rect_in_parent_stable_since_ = page->Animator().Clock().CurrentTime();
      } else {
        rect_in_parent_stable_since_ = base::TimeTicks::Now();
      }
    }
    if (should_compute_occlusion && !geometry.IsVisible())
      occlusion_state = FrameOcclusionState::kPossiblyOccluded;

    // The coordinate system for the iframe's LayoutObject has its origin at the
    // top/left of the border box rect. The coordinate system of the child frame
    // is the same as the coordinate system of the iframe's content box rect.
    // The iframe's PhysicalContentBoxOffset() can be used to move between them.
    PhysicalOffset content_box_offset =
        owner_layout_object->PhysicalContentBoxOffset();

    if (NeedsViewportOffset() || !can_skip_sticky_frame_tracking) {
      viewport_offset = -RoundedIntPoint(
          owner_layout_object->AbsoluteToLocalPoint(
              PhysicalOffset(),
              kTraverseDocumentBoundaries | kApplyRemoteRootFrameOffset) -
          content_box_offset);
      if (!can_skip_sticky_frame_tracking) {
        // If the frame is small, skip tracking this frame and its subframes.
        if (frame.GetMainFrameViewportSize().IsEmpty() ||
            !StickyFrameTracker::IsLarge(
                frame.GetMainFrameViewportSize(),
                new_rect_in_parent.PixelSnappedSize())) {
          can_skip_sticky_frame_tracking = true;
        }
        // If the frame is a large sticky ad, record a use counter and skip
        // tracking its subframes; otherwise continue tracking its subframes.
        else if (frame.IsAdSubframe() &&
                 GetStickyFrameTracker()->UpdateStickyStatus(
                     frame.GetMainFrameScrollOffset(), viewport_offset)) {
          UseCounter::Count(owner_element->GetDocument(),
                            WebFeature::kLargeStickyAd);
          can_skip_sticky_frame_tracking = true;
        }
      }
    }

    // Generate matrix to transform from the space of the containing document
    // to the space of the iframe's contents.
    TransformState parent_frame_to_iframe_content_transform(
        TransformState::kUnapplyInverseTransformDirection);
    // First transform to box coordinates of the iframe element...
    owner_layout_object->MapAncestorToLocal(
        nullptr, parent_frame_to_iframe_content_transform, 0);
    // ... then apply content_box_offset to translate to the coordinate of the
    // child frame.
    parent_frame_to_iframe_content_transform.Move(
        owner_layout_object->PhysicalContentBoxOffset());
    TransformationMatrix matrix =
        parent_frame_to_iframe_content_transform.AccumulatedTransform()
            .Inverse();
    if (geometry.IsIntersecting()) {
      PhysicalRect intersection_rect = PhysicalRect::EnclosingRect(
          matrix.ProjectQuad(FloatRect(geometry.IntersectionRect()))
              .BoundingBox());

      // Don't let EnclosingRect turn an empty rect into a non-empty one.
      if (intersection_rect.IsEmpty()) {
        viewport_intersection =
            IntRect(FlooredIntPoint(intersection_rect.offset), IntSize());
      } else {
        viewport_intersection = EnclosingIntRect(intersection_rect);
      }
    }

    PhysicalRect mainframe_intersection_rect;
    if (!geometry.UnclippedIntersectionRect().IsEmpty()) {
      mainframe_intersection_rect = PhysicalRect::EnclosingRect(
          matrix.ProjectQuad(FloatRect(geometry.UnclippedIntersectionRect()))
              .BoundingBox());

      if (mainframe_intersection_rect.IsEmpty()) {
        mainframe_document_intersection = IntRect(
            FlooredIntPoint(mainframe_intersection_rect.offset), IntSize());
      } else {
        mainframe_document_intersection =
            EnclosingIntRect(mainframe_intersection_rect);
      }
    }
  } else if (occlusion_state == FrameOcclusionState::kGuaranteedNotOccluded) {
    // If the parent LocalFrameView is throttled and out-of-date, then we can't
    // get any useful information.
    occlusion_state = FrameOcclusionState::kUnknown;
  }

  SetViewportIntersection(
      {viewport_offset, viewport_intersection, mainframe_document_intersection,
       WebRect(), occlusion_state, frame.GetMainFrameViewportSize(),
       frame.GetMainFrameScrollOffset(), can_skip_sticky_frame_tracking});

  UpdateFrameVisibility(!viewport_intersection.IsEmpty());

  if (ShouldReportMainFrameIntersection()) {
    GetFrame().Client()->OnMainFrameDocumentIntersectionChanged(
        mainframe_document_intersection);
  }

  // We don't throttle 0x0 or display:none iframes, because in practice they are
  // sometimes used to drive UI logic.
  bool hidden_for_throttling = viewport_intersection.IsEmpty() &&
                               !FrameRect().IsEmpty() && owner_layout_object;
  bool subtree_throttled = false;
  Frame* parent_frame = GetFrame().Tree().Parent();
  if (parent_frame && parent_frame->View()) {
    subtree_throttled =
        parent_frame->View()->CanThrottleRenderingForPropagation();
  }
  UpdateRenderThrottlingStatus(hidden_for_throttling, subtree_throttled);
  return can_skip_sticky_frame_tracking;
}

void FrameView::UpdateFrameVisibility(bool intersects_viewport) {
  blink::mojom::FrameVisibility frame_visibility;
  if (LifecycleUpdatesThrottled())
    return;
  if (IsVisible()) {
    frame_visibility =
        intersects_viewport
            ? blink::mojom::FrameVisibility::kRenderedInViewport
            : blink::mojom::FrameVisibility::kRenderedOutOfViewport;
  } else {
    frame_visibility = blink::mojom::FrameVisibility::kNotRendered;
  }
  if (frame_visibility != frame_visibility_) {
    frame_visibility_ = frame_visibility;
    VisibilityChanged(frame_visibility);
  }
}

void FrameView::UpdateRenderThrottlingStatus(bool hidden_for_throttling,
                                             bool subtree_throttled,
                                             bool recurse) {
  bool visibility_changed = (hidden_for_throttling_ || subtree_throttled_) !=
                            (hidden_for_throttling || subtree_throttled ||
                             DisplayLockedInParentFrame());
  hidden_for_throttling_ = hidden_for_throttling;
  subtree_throttled_ = subtree_throttled || DisplayLockedInParentFrame();
  if (visibility_changed)
    VisibilityForThrottlingChanged();
  if (recurse) {
    for (Frame* child = GetFrame().Tree().FirstChild(); child;
         child = child->Tree().NextSibling()) {
      if (FrameView* child_view = child->View()) {
        child_view->UpdateRenderThrottlingStatus(
            child_view->IsHiddenForThrottling(),
            child_view->IsAttached() && CanThrottleRenderingForPropagation(),
            true);
      }
    }
  }
}

bool FrameView::RectInParentIsStable(
    const base::TimeTicks& event_timestamp) const {
  if (event_timestamp - rect_in_parent_stable_since_ <
      base::TimeDelta::FromMilliseconds(kMinScreenRectStableTimeMs)) {
    return false;
  }
  LocalFrameView* parent = ParentFrameView();
  if (!parent)
    return true;
  return parent->RectInParentIsStable(event_timestamp);
}

StickyFrameTracker* FrameView::GetStickyFrameTracker() {
  if (!sticky_frame_tracker_)
    sticky_frame_tracker_ = std::make_unique<StickyFrameTracker>();
  return sticky_frame_tracker_.get();
}

}  // namespace blink
