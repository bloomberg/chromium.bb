// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/remote_frame_view.h"

#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/frame/remote_frame_client.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

namespace blink {

RemoteFrameView::RemoteFrameView(RemoteFrame* remote_frame)
    : FrameView(IntRect()), remote_frame_(remote_frame) {
  DCHECK(remote_frame);
}

RemoteFrameView::~RemoteFrameView() = default;

LocalFrameView* RemoteFrameView::ParentFrameView() const {
  if (!IsAttached())
    return nullptr;

  HTMLFrameOwnerElement* owner = remote_frame_->DeprecatedLocalOwner();
  if (owner && owner->OwnerType() == FrameOwnerElementType::kPortal)
    return owner->GetDocument().GetFrame()->View();

  // |is_attached_| is only set from AttachToLayout(), which ensures that the
  // parent is a local frame.
  return To<LocalFrame>(remote_frame_->Tree().Parent())->View();
}

LayoutEmbeddedContent* RemoteFrameView::GetLayoutEmbeddedContent() const {
  return remote_frame_->OwnerLayoutObject();
}

LocalFrameView* RemoteFrameView::ParentLocalRootFrameView() const {
  if (!IsAttached())
    return nullptr;

  HTMLFrameOwnerElement* owner = remote_frame_->DeprecatedLocalOwner();
  if (owner && owner->OwnerType() == FrameOwnerElementType::kPortal)
    return owner->GetDocument().GetFrame()->LocalFrameRoot().View();

  // |is_attached_| is only set from AttachToLayout(), which ensures that the
  // parent is a local frame.
  return To<LocalFrame>(remote_frame_->Tree().Parent())
      ->LocalFrameRoot()
      .View();
}

void RemoteFrameView::AttachToLayout() {
  DCHECK(!IsAttached());
  SetAttached(true);
  if (ParentFrameView()->IsVisible())
    SetParentVisible(true);
  UpdateVisibility(true);

  // For RemoteFrame's, the work of the visibility observer is done in
  // RemoteFrameView::UpdateViewportIntersectionsForSubtree. Disable the
  // visibility observer to prevent redundant work.
  HTMLFrameOwnerElement* owner = remote_frame_->DeprecatedLocalOwner();
  if (owner)
    owner->StopVisibilityObserver();

  subtree_throttled_ = ParentFrameView()->CanThrottleRendering();

  FrameRectsChanged(FrameRect());
}

void RemoteFrameView::DetachFromLayout() {
  DCHECK(IsAttached());
  SetParentVisible(false);
  SetAttached(false);
}

RemoteFrameView* RemoteFrameView::Create(RemoteFrame* remote_frame) {
  RemoteFrameView* view = MakeGarbageCollected<RemoteFrameView>(remote_frame);
  view->Show();
  return view;
}

bool RemoteFrameView::UpdateViewportIntersectionsForSubtree(
    unsigned parent_flags) {
  if (!(parent_flags &
        IntersectionObservation::kImplicitRootObserversNeedUpdate)) {
    return needs_occlusion_tracking_;
  }

  // This should only run in child frames.
  HTMLFrameOwnerElement* owner_element = remote_frame_->DeprecatedLocalOwner();
  DCHECK(owner_element);
  LayoutEmbeddedContent* owner = GetLayoutEmbeddedContent();
  IntRect viewport_intersection;
  DocumentLifecycle::LifecycleState parent_lifecycle_state =
      owner_element->GetDocument().Lifecycle().GetState();
  FrameOcclusionState occlusion_state =
      owner_element->GetDocument().GetFrame()->GetOcclusionState();
  bool should_compute_occlusion =
      needs_occlusion_tracking_ &&
      occlusion_state == FrameOcclusionState::kGuaranteedNotOccluded &&
      parent_lifecycle_state >= DocumentLifecycle::kPrePaintClean &&
      RuntimeEnabledFeatures::IntersectionObserverV2Enabled();

  if (!owner || !IsSelfVisible()) {
    // The frame is detached from layout or not visible; leave
    // viewport_intersection empty, and signal the frame as occluded if
    // necessary.
    occlusion_state = FrameOcclusionState::kPossiblyOccluded;
  } else if (parent_lifecycle_state >= DocumentLifecycle::kLayoutClean) {
    unsigned geometry_flags =
        IntersectionGeometry::kShouldUseReplacedContentRect;
    if (should_compute_occlusion)
      geometry_flags |= IntersectionGeometry::kShouldComputeVisibility;

    IntersectionGeometry geometry(nullptr, *owner_element, {},
                                  {IntersectionObserver::kMinimumThreshold},
                                  geometry_flags);
    // geometry.IntersectionRect() is in absolute coordinates of the owning
    // document. Map it down to absolute coordinates in the child document.
    LayoutRect intersection_rect = LayoutRect(
        owner
            ->AncestorToLocalQuad(
                nullptr, FloatQuad(FloatRect(geometry.IntersectionRect())),
                kUseTransforms)
            .BoundingBox());
    // Map from the box coordinates of the owner to the inner frame.
    intersection_rect.Move(-owner->PhysicalContentBoxOffset());
    // Don't let EnclosingIntRect turn an empty rect into a non-empty one.
    if (intersection_rect.IsEmpty()) {
      viewport_intersection =
          IntRect(FlooredIntPoint(intersection_rect.Location()), IntSize());
    } else {
      viewport_intersection = EnclosingIntRect(intersection_rect);
    }
    if (should_compute_occlusion && !geometry.IsVisible())
      occlusion_state = FrameOcclusionState::kPossiblyOccluded;
  } else if (occlusion_state == FrameOcclusionState::kGuaranteedNotOccluded) {
    // If the parent LocalFrameView is throttled and out-of-date, then we can't
    // get any useful information.
    occlusion_state = FrameOcclusionState::kUnknown;
  }

  if (viewport_intersection != last_viewport_intersection_ ||
      occlusion_state != last_occlusion_state_) {
    last_viewport_intersection_ = viewport_intersection;
    last_occlusion_state_ = occlusion_state;
    remote_frame_->Client()->UpdateRemoteViewportIntersection(
        viewport_intersection, occlusion_state);
  }

  // TODO(szager): There is redundant functionality here; clean it up.
  // UpdateVisibility controls RenderFrameHostImpl::visibility_ for the iframe,
  // and RenderWidget::is_hidden_ in the iframe process. When an OOPIF is marked
  // "hidden", it stops running lifecycle updates altogether.
  // UpdateRenderThrottlingStatus sets the hidden_for_throttling_ and
  // subtree_throttled_ flags on LocalFrameView in the iframe process. They
  // control whether the iframe skips doing rendering work during lifecycle
  // updates.
  bool is_visible_for_throttling = !viewport_intersection.IsEmpty();
  UpdateVisibility(is_visible_for_throttling);
  UpdateRenderThrottlingStatus(
      !is_visible_for_throttling,
      IsAttached() && ParentFrameView()->CanThrottleRendering());

  return needs_occlusion_tracking_;
}

void RemoteFrameView::SetNeedsOcclusionTracking(bool needs_tracking) {
  if (needs_occlusion_tracking_ == needs_tracking)
    return;
  needs_occlusion_tracking_ = needs_tracking;
  if (needs_tracking) {
    if (LocalFrameView* parent_view = ParentLocalRootFrameView())
      parent_view->ScheduleAnimation();
  }
}

IntRect RemoteFrameView::GetCompositingRect() {
  LocalFrameView* local_root_view = ParentLocalRootFrameView();
  if (!local_root_view || !remote_frame_->OwnerLayoutObject())
    return IntRect();

  // For main frames we constrain the rect that gets painted to the viewport.
  // If the local frame root is an OOPIF itself, then we use the root's
  // intersection rect. This represents a conservative maximum for the area
  // that needs to be rastered by the OOPIF compositor.
  IntSize viewport_size = local_root_view->Size();
  if (local_root_view->GetPage()->MainFrame() != local_root_view->GetFrame()) {
    viewport_size =
        local_root_view->GetFrame().RemoteViewportIntersection().Size();
  }

  // The viewport size needs to account for intermediate CSS transforms before
  // being compared to the frame size.
  FloatQuad viewport_quad =
      remote_frame_->OwnerLayoutObject()->AncestorToLocalQuad(
          local_root_view->GetLayoutView(),
          FloatRect(FloatPoint(), FloatSize(viewport_size)),
          kTraverseDocumentBoundaries | kUseTransforms);
  IntSize converted_viewport_size =
      EnclosingIntRect(viewport_quad.BoundingBox()).Size();

  IntSize frame_size = Size();

  // Iframes that fit within the window viewport get fully rastered. For
  // iframes that are larger than the window viewport, add a 30% buffer to the
  // draw area to try to prevent guttering during scroll.
  // TODO(kenrb): The 30% value is arbitrary, it gives 15% overdraw in both
  // directions when the iframe extends beyond both edges of the viewport, and
  // it seems to make guttering rare with slow to medium speed wheel scrolling.
  // Can we collect UMA data to estimate how much extra rastering this causes,
  // and possibly how common guttering is?
  converted_viewport_size.Scale(1.3f);
  converted_viewport_size.SetWidth(
      std::min(frame_size.Width(), converted_viewport_size.Width()));
  converted_viewport_size.SetHeight(
      std::min(frame_size.Height(), converted_viewport_size.Height()));
  IntPoint expanded_origin;
  if (!last_viewport_intersection_.IsEmpty()) {
    IntSize expanded_size =
        last_viewport_intersection_.Size().ExpandedTo(converted_viewport_size);
    expanded_size -= last_viewport_intersection_.Size();
    expanded_size.Scale(0.5f, 0.5f);
    expanded_origin = last_viewport_intersection_.Location() - expanded_size;
    expanded_origin.ClampNegativeToZero();
  }
  return IntRect(expanded_origin, converted_viewport_size);
}

void RemoteFrameView::Dispose() {
  HTMLFrameOwnerElement* owner_element = remote_frame_->DeprecatedLocalOwner();
  // ownerElement can be null during frame swaps, because the
  // RemoteFrameView is disconnected before detachment.
  if (owner_element && owner_element->OwnedEmbeddedContentView() == this)
    owner_element->SetEmbeddedContentView(nullptr);
}

void RemoteFrameView::InvalidateRect(const IntRect& rect) {
  auto* object = remote_frame_->OwnerLayoutObject();
  if (!object)
    return;

  LayoutRect repaint_rect(rect);
  repaint_rect.Move(object->BorderLeft() + object->PaddingLeft(),
                    object->BorderTop() + object->PaddingTop());
  object->InvalidatePaintRectangle(repaint_rect);
}

void RemoteFrameView::PropagateFrameRects() {
  // Update the rect to reflect the position of the frame relative to the
  // containing local frame root. The position of the local root within
  // any remote frames, if any, is accounted for by the embedder.
  IntRect frame_rect(FrameRect());
  IntRect screen_space_rect = frame_rect;

  if (LocalFrameView* parent = ParentFrameView()) {
    screen_space_rect = parent->ConvertToRootFrame(screen_space_rect);
  }
  remote_frame_->Client()->FrameRectsChanged(frame_rect, screen_space_rect);
}

void RemoteFrameView::Paint(GraphicsContext& context,
                            const GlobalPaintFlags flags,
                            const CullRect& rect,
                            const IntSize& paint_offset) const {
  // Painting remote frames is only for printing.
  if (!context.Printing())
    return;

  if (!rect.Intersects(FrameRect()))
    return;

  DrawingRecorder recorder(context, *GetFrame().OwnerLayoutObject(),
                           DisplayItem::kDocumentBackground);
  context.Save();
  context.Translate(paint_offset.Width(), paint_offset.Height());

  DCHECK(context.Canvas());
  // Inform the remote frame to print.
  uint32_t content_id = Print(FrameRect(), context.Canvas());

  // Record the place holder id on canvas.
  context.Canvas()->recordCustomData(content_id);
  context.Restore();
}

void RemoteFrameView::UpdateGeometry() {
  if (LayoutEmbeddedContent* layout = GetLayoutEmbeddedContent())
    layout->UpdateGeometry(*this);
}

void RemoteFrameView::Hide() {
  SetSelfVisible(false);
  UpdateVisibility(scroll_visible_);
}

void RemoteFrameView::Show() {
  SetSelfVisible(true);
  UpdateVisibility(scroll_visible_);
}

void RemoteFrameView::ParentVisibleChanged() {
  if (IsSelfVisible())
    UpdateVisibility(scroll_visible_);
}

void RemoteFrameView::UpdateVisibility(bool scroll_visible) {
  blink::mojom::FrameVisibility visibility;
  scroll_visible_ = scroll_visible;
  if (IsVisible()) {
    visibility = scroll_visible
                     ? blink::mojom::FrameVisibility::kRenderedInViewport
                     : blink::mojom::FrameVisibility::kRenderedOutOfViewport;
  } else {
    visibility = blink::mojom::FrameVisibility::kNotRendered;
  }

  if (visibility == visibility_)
    return;
  visibility_ = visibility;
  remote_frame_->Client()->VisibilityChanged(visibility);
}

void RemoteFrameView::UpdateRenderThrottlingStatus(bool hidden,
                                                   bool subtree_throttled) {
  TRACE_EVENT0("blink", "RemoteFrameView::UpdateRenderThrottlingStatus");
  if (!remote_frame_->Client())
    return;

  bool was_throttled = CanThrottleRendering();

  // Note that we disallow throttling of 0x0 and display:none frames because
  // some sites use them to drive UI logic.
  HTMLFrameOwnerElement* owner_element = remote_frame_->DeprecatedLocalOwner();
  hidden_for_throttling_ = hidden && !Size().IsEmpty() &&
                           (owner_element && owner_element->GetLayoutObject());
  subtree_throttled_ = subtree_throttled;

  bool is_throttled = CanThrottleRendering();
  if (was_throttled != is_throttled) {
    remote_frame_->Client()->UpdateRenderThrottlingStatus(is_throttled,
                                                          subtree_throttled_);
  }
}

bool RemoteFrameView::CanThrottleRendering() const {
  if (!RuntimeEnabledFeatures::RenderingPipelineThrottlingEnabled())
    return false;
  if (subtree_throttled_)
    return true;
  return hidden_for_throttling_;
}

void RemoteFrameView::SetIntrinsicSizeInfo(
    const IntrinsicSizingInfo& size_info) {
  intrinsic_sizing_info_ = size_info;
  has_intrinsic_sizing_info_ = true;
}

bool RemoteFrameView::GetIntrinsicSizingInfo(
    IntrinsicSizingInfo& sizing_info) const {
  if (!has_intrinsic_sizing_info_)
    return false;

  sizing_info = intrinsic_sizing_info_;
  return true;
}

bool RemoteFrameView::HasIntrinsicSizingInfo() const {
  return has_intrinsic_sizing_info_;
}

uint32_t RemoteFrameView::Print(const IntRect& rect,
                                cc::PaintCanvas* canvas) const {
  return remote_frame_->Client()->Print(rect, canvas);
}

void RemoteFrameView::Trace(blink::Visitor* visitor) {
  visitor->Trace(remote_frame_);
  visitor->Trace(visibility_observer_);
}

}  // namespace blink
