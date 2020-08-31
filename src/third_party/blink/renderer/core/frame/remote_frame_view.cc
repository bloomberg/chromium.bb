// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/remote_frame_view.h"

#include "components/paint_preview/common/paint_preview_tracker.h"
#include "third_party/blink/public/mojom/frame/frame_owner_element_type.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/frame/remote_frame_client.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"

namespace blink {

RemoteFrameView::RemoteFrameView(RemoteFrame* remote_frame)
    : FrameView(IntRect()), remote_frame_(remote_frame) {
  DCHECK(remote_frame);
  Show();
}

RemoteFrameView::~RemoteFrameView() = default;

LocalFrameView* RemoteFrameView::ParentFrameView() const {
  if (!IsAttached())
    return nullptr;

  HTMLFrameOwnerElement* owner = remote_frame_->DeprecatedLocalOwner();
  if (owner &&
      owner->OwnerType() == mojom::blink::FrameOwnerElementType::kPortal)
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
  if (owner &&
      owner->OwnerType() == mojom::blink::FrameOwnerElementType::kPortal)
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
  UpdateFrameVisibility(true);
  UpdateRenderThrottlingStatus(
      IsHiddenForThrottling(),
      ParentFrameView()->CanThrottleRenderingForPropagation());
  needs_frame_rect_propagation_ = true;
  ParentFrameView()->SetNeedsUpdateGeometries();
}

void RemoteFrameView::DetachFromLayout() {
  DCHECK(IsAttached());
  SetParentVisible(false);
  SetAttached(false);
}

bool RemoteFrameView::UpdateViewportIntersectionsForSubtree(
    unsigned parent_flags) {
  UpdateViewportIntersection(parent_flags, needs_occlusion_tracking_);
  return needs_occlusion_tracking_;
}

void RemoteFrameView::SetViewportIntersection(
    const ViewportIntersectionState& intersection_state) {
  ViewportIntersectionState new_state(intersection_state);
  new_state.compositor_visible_rect = WebRect(compositing_rect_);
  if (new_state != last_intersection_state_) {
    last_intersection_state_ = new_state;
    remote_frame_->Client()->UpdateRemoteViewportIntersection(new_state);
  }
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

void RemoteFrameView::UpdateCompositingRect() {
  compositing_rect_ = IntRect();
  LocalFrameView* local_root_view = ParentLocalRootFrameView();
  if (!local_root_view || !remote_frame_->OwnerLayoutObject())
    return;

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
  PhysicalRect viewport_rect =
      remote_frame_->OwnerLayoutObject()->AncestorToLocalRect(
          nullptr, PhysicalRect(PhysicalOffset(), PhysicalSize(viewport_size)),
          kApplyRemoteRootFrameOffset | kTraverseDocumentBoundaries);
  compositing_rect_ = EnclosingIntRect(viewport_rect);
  IntSize frame_size = Size();

  // Iframes that fit within the window viewport get fully rastered. For
  // iframes that are larger than the window viewport, add a 30% buffer to the
  // draw area to try to prevent guttering during scroll.
  // TODO(kenrb): The 30% value is arbitrary, it gives 15% overdraw in both
  // directions when the iframe extends beyond both edges of the viewport, and
  // it seems to make guttering rare with slow to medium speed wheel scrolling.
  // Can we collect UMA data to estimate how much extra rastering this causes,
  // and possibly how common guttering is?
  compositing_rect_.InflateX(ceilf(viewport_rect.Width() * 0.15f));
  compositing_rect_.InflateY(ceilf(viewport_rect.Height() * 0.15f));
  compositing_rect_.SetWidth(
      std::min(frame_size.Width(), compositing_rect_.Width()));
  compositing_rect_.SetHeight(
      std::min(frame_size.Height(), compositing_rect_.Height()));
  IntPoint compositing_rect_location = compositing_rect_.Location();
  compositing_rect_location.ClampNegativeToZero();
  compositing_rect_.SetLocation(compositing_rect_location);
}

void RemoteFrameView::Dispose() {
  HTMLFrameOwnerElement* owner_element = remote_frame_->DeprecatedLocalOwner();
  // ownerElement can be null during frame swaps, because the
  // RemoteFrameView is disconnected before detachment.
  if (owner_element && owner_element->OwnedEmbeddedContentView() == this)
    owner_element->SetEmbeddedContentView(nullptr);
  SetNeedsOcclusionTracking(false);
}

void RemoteFrameView::InvalidateRect(const IntRect& rect) {
  auto* object = remote_frame_->OwnerLayoutObject();
  if (!object)
    return;

  PhysicalRect repaint_rect(rect);
  repaint_rect.Move(PhysicalOffset(object->BorderLeft() + object->PaddingLeft(),
                                   object->BorderTop() + object->PaddingTop()));
  object->InvalidatePaintRectangle(repaint_rect);
}

void RemoteFrameView::SetFrameRect(const IntRect& rect) {
  EmbeddedContentView::SetFrameRect(rect);
  if (needs_frame_rect_propagation_)
    PropagateFrameRects();
}

void RemoteFrameView::PropagateFrameRects() {
  // Update the rect to reflect the position of the frame relative to the
  // containing local frame root. The position of the local root within
  // any remote frames, if any, is accounted for by the embedder.
  needs_frame_rect_propagation_ = false;
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
  if (!rect.Intersects(FrameRect()))
    return;

  if (context.IsPrintingOrPaintingPreview()) {
    DrawingRecorder recorder(context, *GetFrame().OwnerLayoutObject(),
                             DisplayItem::kDocumentBackground);
    context.Save();
    context.Translate(paint_offset.Width(), paint_offset.Height());
    DCHECK(context.Canvas());

    uint32_t content_id = 0;
    if (context.Printing()) {
      // Inform the remote frame to print.
      content_id = Print(FrameRect(), context.Canvas());
    } else if (context.IsPaintingPreview()) {
      // Inform the remote frame to capture a paint preview.
      content_id = CapturePaintPreview(FrameRect(), context.Canvas());
    }
    // Record the place holder id on canvas.
    context.Canvas()->recordCustomData(content_id);
    context.Restore();
  }

  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
      GetFrame().GetCcLayer()) {
    auto offset = GetLayoutEmbeddedContent()->ReplacedContentRect().offset;
    RecordForeignLayer(context, *GetFrame().OwnerLayoutObject(),
                       DisplayItem::kForeignLayerRemoteFrame,
                       GetFrame().GetCcLayer(), FloatPoint(offset));
  }
}

void RemoteFrameView::UpdateGeometry() {
  if (LayoutEmbeddedContent* layout = GetLayoutEmbeddedContent())
    layout->UpdateGeometry(*this);
}

void RemoteFrameView::Hide() {
  SetSelfVisible(false);
  UpdateFrameVisibility(
      !last_intersection_state_.viewport_intersection.IsEmpty());
}

void RemoteFrameView::Show() {
  SetSelfVisible(true);
  UpdateFrameVisibility(
      !last_intersection_state_.viewport_intersection.IsEmpty());
}

void RemoteFrameView::ParentVisibleChanged() {
  if (IsSelfVisible()) {
    UpdateFrameVisibility(
        !last_intersection_state_.viewport_intersection.IsEmpty());
  }
}

void RemoteFrameView::VisibilityForThrottlingChanged() {
  TRACE_EVENT0("blink", "RemoteFrameView::VisibilityForThrottlingChanged");
  remote_frame_->GetRemoteFrameHostRemote().UpdateRenderThrottlingStatus(
      IsHiddenForThrottling(), IsSubtreeThrottled());
}

void RemoteFrameView::VisibilityChanged(
    blink::mojom::FrameVisibility visibility) {
  remote_frame_->GetRemoteFrameHostRemote().VisibilityChanged(visibility);
}

bool RemoteFrameView::CanThrottleRendering() const {
  return IsSubtreeThrottled() || IsHiddenForThrottling();
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

uint32_t RemoteFrameView::CapturePaintPreview(const IntRect& rect,
                                              cc::PaintCanvas* canvas) const {
  auto* tracker = canvas->GetPaintPreviewTracker();
  DCHECK(tracker);  // |tracker| must exist or there is a bug upstream.

  // Create a placeholder ID that maps to an embedding token.
  HTMLFrameOwnerElement* owner = remote_frame_->DeprecatedLocalOwner();
  DCHECK(owner);

  // RACE: there is a possibility that the embedding token will be null and
  // still be in a valid state. This can occur is the frame has recently
  // navigated and the embedding token hasn't propagated from the FrameTreeNode
  // to this HTMLFrameOwnerElement yet (over IPC). If the token is null the
  // failure can be handled gracefully by simply ignoring the subframe in the
  // result.
  base::Optional<base::UnguessableToken> maybe_embedding_token =
      owner->GetEmbeddingToken();
  if (!maybe_embedding_token.has_value())
    return 0;
  uint32_t content_id =
      tracker->CreateContentForRemoteFrame(rect, maybe_embedding_token.value());

  // Send a request to the browser to trigger a capture of the remote frame.
  remote_frame_->GetRemoteFrameHostRemote()
      .CapturePaintPreviewOfCrossProcessSubframe(rect, tracker->Guid());
  return content_id;
}

void RemoteFrameView::Trace(Visitor* visitor) {
  visitor->Trace(remote_frame_);
}

}  // namespace blink
