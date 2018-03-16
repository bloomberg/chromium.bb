// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/RemoteFrameView.h"

#include "core/dom/ElementVisibilityObserver.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/RemoteFrame.h"
#include "core/frame/RemoteFrameClient.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/intersection_observer/IntersectionObserverEntry.h"
#include "core/layout/LayoutEmbeddedContent.h"
#include "core/layout/LayoutView.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/CullRect.h"
#include "platform/graphics/paint/DrawingRecorder.h"

namespace blink {

RemoteFrameView::RemoteFrameView(RemoteFrame* remote_frame)
    : remote_frame_(remote_frame), is_attached_(false) {
  DCHECK(remote_frame);
}

RemoteFrameView::~RemoteFrameView() = default;

LocalFrameView* RemoteFrameView::ParentFrameView() const {
  if (!is_attached_)
    return nullptr;

  Frame* parent_frame = remote_frame_->Tree().Parent();
  if (parent_frame && parent_frame->IsLocalFrame())
    return ToLocalFrame(parent_frame)->View();

  return nullptr;
}

void RemoteFrameView::AttachToLayout() {
  DCHECK(!is_attached_);
  is_attached_ = true;
  if (ParentFrameView()->IsVisible())
    SetParentVisible(true);

  SetupRenderThrottling();
  subtree_throttled_ = ParentFrameView()->CanThrottleRendering();

  FrameRectsChanged();
}

void RemoteFrameView::DetachFromLayout() {
  DCHECK(is_attached_);
  SetParentVisible(false);
  is_attached_ = false;
}

RemoteFrameView* RemoteFrameView::Create(RemoteFrame* remote_frame) {
  RemoteFrameView* view = new RemoteFrameView(remote_frame);
  view->Show();
  return view;
}

void RemoteFrameView::UpdateViewportIntersectionsForSubtree(
    DocumentLifecycle::LifecycleState target_state) {
  if (!remote_frame_->OwnerLayoutObject())
    return;

  LocalFrameView* local_root_view =
      ToLocalFrame(remote_frame_->Tree().Parent())->LocalFrameRoot().View();
  if (!local_root_view)
    return;

  // Start with rect in remote frame's coordinate space. Then
  // mapToVisualRectInAncestorSpace will move it to the local root's coordinate
  // space and account for any clip from containing elements such as a
  // scrollable div. Passing nullptr as an argument to
  // mapToVisualRectInAncestorSpace causes it to be clipped to the viewport,
  // even if there are RemoteFrame ancestors in the frame tree.
  LayoutRect rect(0, 0, frame_rect_.Width(), frame_rect_.Height());
  rect.Move(remote_frame_->OwnerLayoutObject()->ContentBoxOffset());
  IntRect viewport_intersection;
  if (remote_frame_->OwnerLayoutObject()->MapToVisualRectInAncestorSpace(
          nullptr, rect)) {
    IntRect root_visible_rect = local_root_view->VisibleContentRect();
    IntRect intersected_rect = EnclosingIntRect(rect);
    intersected_rect.Intersect(root_visible_rect);
    intersected_rect.Move(-local_root_view->ScrollOffsetInt());

    // Translate the intersection rect from the root frame's coordinate space
    // to the remote frame's coordinate space.
    viewport_intersection = ConvertFromRootFrame(intersected_rect);
  }

  if (viewport_intersection != last_viewport_intersection_) {
    remote_frame_->Client()->UpdateRemoteViewportIntersection(
        viewport_intersection);
  }

  last_viewport_intersection_ = viewport_intersection;
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

void RemoteFrameView::SetFrameRect(const IntRect& frame_rect) {
  if (frame_rect == frame_rect_)
    return;

  frame_rect_ = frame_rect;
  FrameRectsChanged();
}

IntRect RemoteFrameView::FrameRect() const {
  IntPoint location(frame_rect_.Location());

  // As an optimization, we don't include the root layer's scroll offset in the
  // frame rect.  As a result, we don't need to recalculate the frame rect every
  // time the root layer scrolls, but we need to add it in here.
  LayoutEmbeddedContent* owner = remote_frame_->OwnerLayoutObject();
  if (owner) {
    LayoutView* owner_layout_view = owner->View();
    DCHECK(owner_layout_view);
    if (owner_layout_view->HasOverflowClip())
      location.Move(-owner_layout_view->ScrolledContentOffset());
  }

  return IntRect(location, frame_rect_.Size());
}

void RemoteFrameView::FrameRectsChanged() {
  // Update the rect to reflect the position of the frame relative to the
  // containing local frame root. The position of the local root within
  // any remote frames, if any, is accounted for by the embedder.
  IntRect frame_rect(FrameRect());
  IntRect screen_space_rect = frame_rect;

  if (LocalFrameView* parent = ParentFrameView()) {
    screen_space_rect =
        parent->ConvertToRootFrame(parent->ContentsToFrame(screen_space_rect));
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

  if (!rect.IntersectsCullRect(FrameRect()))
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
  if (LayoutEmbeddedContent* layout = remote_frame_->OwnerLayoutObject())
    layout->UpdateGeometry(*this);
}

void RemoteFrameView::Hide() {
  self_visible_ = false;
  remote_frame_->Client()->VisibilityChanged(false);
}

void RemoteFrameView::Show() {
  self_visible_ = true;
  remote_frame_->Client()->VisibilityChanged(true);
}

void RemoteFrameView::SetParentVisible(bool visible) {
  if (parent_visible_ == visible)
    return;

  parent_visible_ = visible;
  if (!self_visible_)
    return;

  remote_frame_->Client()->VisibilityChanged(self_visible_ && parent_visible_);
}

IntRect RemoteFrameView::ConvertFromRootFrame(
    const IntRect& rect_in_root_frame) const {
  if (LocalFrameView* parent = ParentFrameView()) {
    IntRect parent_rect = parent->ConvertFromRootFrame(rect_in_root_frame);
    parent_rect.SetLocation(
        parent->ConvertSelfToChild(*this, parent_rect.Location()));
    return parent_rect;
  }
  return rect_in_root_frame;
}

void RemoteFrameView::SetupRenderThrottling() {
  if (visibility_observer_)
    return;

  Element* target_element = GetFrame().DeprecatedLocalOwner();
  if (!target_element)
    return;

  visibility_observer_ = new ElementVisibilityObserver(
      target_element, WTF::BindRepeating(
                          [](RemoteFrameView* remote_view, bool is_visible) {
                            remote_view->UpdateRenderThrottlingStatus(
                                !is_visible, remote_view->subtree_throttled_);
                          },
                          WrapWeakPersistent(this)));
  visibility_observer_->Start();
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
  hidden_for_throttling_ = hidden && !frame_rect_.IsEmpty() &&
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

uint32_t RemoteFrameView::Print(const IntRect& rect, WebCanvas* canvas) const {
  return remote_frame_->Client()->Print(rect, canvas);
}

void RemoteFrameView::Trace(blink::Visitor* visitor) {
  visitor->Trace(remote_frame_);
  visitor->Trace(visibility_observer_);
}

}  // namespace blink
