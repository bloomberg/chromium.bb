// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/RotationViewportAnchor.h"

#include "core/dom/ContainerNode.h"
#include "core/dom/Node.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/PageScaleConstraintsSet.h"
#include "core/frame/RootFrameViewport.h"
#include "core/frame/VisualViewport.h"
#include "core/input/EventHandler.h"
#include "core/layout/HitTestResult.h"
#include "platform/geometry/DoubleRect.h"

namespace blink {

namespace {

static const float kViewportAnchorRelativeEpsilon = 0.1f;
static const int kViewportToNodeMaxRelativeArea = 2;

Node* FindNonEmptyAnchorNode(const IntPoint& point,
                             const IntRect& view_rect,
                             EventHandler& event_handler) {
  Node* node = event_handler
                   .HitTestResultAtPoint(point, HitTestRequest::kReadOnly |
                                                    HitTestRequest::kActive)
                   .InnerNode();

  // If the node bounding box is sufficiently large, make a single attempt to
  // find a smaller node; the larger the node bounds, the greater the
  // variability under resize.
  const int max_node_area =
      view_rect.Width() * view_rect.Height() * kViewportToNodeMaxRelativeArea;
  if (node && node->BoundingBox().Width() * node->BoundingBox().Height() >
                  max_node_area) {
    IntSize point_offset = view_rect.Size();
    point_offset.Scale(kViewportAnchorRelativeEpsilon);
    node = event_handler
               .HitTestResultAtPoint(
                   point + point_offset,
                   HitTestRequest::kReadOnly | HitTestRequest::kActive)
               .InnerNode();
  }

  while (node && node->BoundingBox().IsEmpty())
    node = node->parentNode();

  return node;
}

void MoveToEncloseRect(IntRect& outer, const FloatRect& inner) {
  IntPoint minimum_position =
      CeiledIntPoint(inner.Location() + inner.Size() - FloatSize(outer.Size()));
  IntPoint maximum_position = FlooredIntPoint(inner.Location());

  IntPoint outer_origin = outer.Location();
  outer_origin = outer_origin.ExpandedTo(minimum_position);
  outer_origin = outer_origin.ShrunkTo(maximum_position);

  outer.SetLocation(outer_origin);
}

void MoveIntoRect(FloatRect& inner, const IntRect& outer) {
  FloatPoint minimum_position = FloatPoint(outer.Location());
  FloatPoint maximum_position = minimum_position + outer.Size() - inner.Size();

  // Adjust maximumPosition to the nearest lower integer because
  // VisualViewport::maximumScrollPosition() does the same.
  // The value of minumumPosition is already adjusted since it is
  // constructed from an integer point.
  maximum_position = FlooredIntPoint(maximum_position);

  FloatPoint inner_origin = inner.Location();
  inner_origin = inner_origin.ExpandedTo(minimum_position);
  inner_origin = inner_origin.ShrunkTo(maximum_position);

  inner.SetLocation(inner_origin);
}

}  // namespace

RotationViewportAnchor::RotationViewportAnchor(
    LocalFrameView& root_frame_view,
    VisualViewport& visual_viewport,
    const FloatSize& anchor_in_inner_view_coords,
    PageScaleConstraintsSet& page_scale_constraints_set)
    : root_frame_view_(&root_frame_view),
      visual_viewport_(&visual_viewport),
      anchor_in_inner_view_coords_(anchor_in_inner_view_coords),
      page_scale_constraints_set_(page_scale_constraints_set) {
  SetAnchor();
}

RotationViewportAnchor::~RotationViewportAnchor() {
  RestoreToAnchor();
}

void RotationViewportAnchor::SetAnchor() {
  RootFrameViewport* root_frame_viewport =
      root_frame_view_->GetRootFrameViewport();
  DCHECK(root_frame_viewport);

  old_page_scale_factor_ = visual_viewport_->Scale();
  old_minimum_page_scale_factor_ =
      page_scale_constraints_set_.FinalConstraints().minimum_scale;

  // Save the absolute location in case we won't find the anchor node, we'll
  // fall back to that.
  visual_viewport_in_document_ =
      FloatPoint(root_frame_viewport->VisibleContentRect().Location());

  anchor_node_.Clear();
  anchor_node_bounds_ = LayoutRect();
  anchor_in_node_coords_ = FloatSize();
  normalized_visual_viewport_offset_ = FloatSize();

  IntRect inner_view_rect = root_frame_viewport->VisibleContentRect();

  // Preserve origins at the absolute screen origin.
  if (inner_view_rect.Location() == IntPoint::Zero() ||
      inner_view_rect.IsEmpty())
    return;

  IntRect outer_view_rect =
      LayoutViewport().VisibleContentRect(kIncludeScrollbars);

  // Normalize by the size of the outer rect
  DCHECK(!outer_view_rect.IsEmpty());
  normalized_visual_viewport_offset_ = visual_viewport_->GetScrollOffset();
  normalized_visual_viewport_offset_.Scale(1.0 / outer_view_rect.Width(),
                                           1.0 / outer_view_rect.Height());

  // Note, we specifically use the unscaled visual viewport size here as the
  // conversion into content-space below will apply the scale.
  FloatPoint anchor_offset(visual_viewport_->Size());
  anchor_offset.Scale(anchor_in_inner_view_coords_.Width(),
                      anchor_in_inner_view_coords_.Height());

  // Note, we specifically convert to the rootFrameView contents here, rather
  // than the layout viewport. That's because hit testing works from the
  // LocalFrameView's content coordinates even if it's not the layout viewport.
  const FloatPoint anchor_point_in_contents = root_frame_view_->FrameToContents(
      visual_viewport_->ViewportToRootFrame(anchor_offset));

  Node* node = FindNonEmptyAnchorNode(
      FlooredIntPoint(anchor_point_in_contents), inner_view_rect,
      root_frame_view_->GetFrame().GetEventHandler());
  if (!node)
    return;

  anchor_node_ = node;
  anchor_node_bounds_ = node->BoundingBox();
  anchor_in_node_coords_ =
      anchor_point_in_contents - FloatPoint(anchor_node_bounds_.Location());
  anchor_in_node_coords_.Scale(1.f / anchor_node_bounds_.Width(),
                               1.f / anchor_node_bounds_.Height());
}

void RotationViewportAnchor::RestoreToAnchor() {
  float new_page_scale_factor =
      old_page_scale_factor_ / old_minimum_page_scale_factor_ *
      page_scale_constraints_set_.FinalConstraints().minimum_scale;
  new_page_scale_factor =
      page_scale_constraints_set_.FinalConstraints().ClampToConstraints(
          new_page_scale_factor);

  FloatSize visual_viewport_size(visual_viewport_->Size());
  visual_viewport_size.Scale(1 / new_page_scale_factor);

  IntPoint main_frame_origin;
  FloatPoint visual_viewport_origin;

  ComputeOrigins(visual_viewport_size, main_frame_origin,
                 visual_viewport_origin);

  LayoutViewport().SetScrollOffset(ToScrollOffset(main_frame_origin),
                                   kProgrammaticScroll);

  // Set scale before location, since location can be clamped on setting scale.
  visual_viewport_->SetScale(new_page_scale_factor);
  visual_viewport_->SetLocation(visual_viewport_origin);
}

ScrollableArea& RotationViewportAnchor::LayoutViewport() const {
  RootFrameViewport* root_frame_viewport =
      root_frame_view_->GetRootFrameViewport();
  DCHECK(root_frame_viewport);
  return root_frame_viewport->LayoutViewport();
}

void RotationViewportAnchor::ComputeOrigins(
    const FloatSize& inner_size,
    IntPoint& main_frame_offset,
    FloatPoint& visual_viewport_offset) const {
  IntSize outer_size = LayoutViewport().VisibleContentRect().Size();

  // Compute the viewport origins in CSS pixels relative to the document.
  FloatSize abs_visual_viewport_offset = normalized_visual_viewport_offset_;
  abs_visual_viewport_offset.Scale(outer_size.Width(), outer_size.Height());

  FloatPoint inner_origin = GetInnerOrigin(inner_size);
  FloatPoint outer_origin = inner_origin - abs_visual_viewport_offset;

  IntRect outer_rect = IntRect(FlooredIntPoint(outer_origin), outer_size);
  FloatRect inner_rect = FloatRect(inner_origin, inner_size);

  MoveToEncloseRect(outer_rect, inner_rect);

  outer_rect.SetLocation(IntPoint(
      LayoutViewport().ClampScrollOffset(ToIntSize(outer_rect.Location()))));

  MoveIntoRect(inner_rect, outer_rect);

  main_frame_offset = outer_rect.Location();
  visual_viewport_offset =
      FloatPoint(inner_rect.Location() - outer_rect.Location());
}

FloatPoint RotationViewportAnchor::GetInnerOrigin(
    const FloatSize& inner_size) const {
  if (!anchor_node_ || !anchor_node_->isConnected())
    return visual_viewport_in_document_;

  const LayoutRect current_node_bounds = anchor_node_->BoundingBox();
  if (anchor_node_bounds_ == current_node_bounds)
    return visual_viewport_in_document_;

  RootFrameViewport* root_frame_viewport =
      root_frame_view_->GetRootFrameViewport();
  const LayoutRect current_node_bounds_in_layout_viewport =
      root_frame_viewport->RootContentsToLayoutViewportContents(
          *root_frame_view_.Get(), current_node_bounds);

  // Compute the new anchor point relative to the node position
  FloatSize anchor_offset_from_node(
      current_node_bounds_in_layout_viewport.Size());
  anchor_offset_from_node.Scale(anchor_in_node_coords_.Width(),
                                anchor_in_node_coords_.Height());
  FloatPoint anchor_point =
      FloatPoint(current_node_bounds_in_layout_viewport.Location()) +
      anchor_offset_from_node;

  // Compute the new origin point relative to the new anchor point
  FloatSize anchor_offset_from_origin = inner_size;
  anchor_offset_from_origin.Scale(anchor_in_inner_view_coords_.Width(),
                                  anchor_in_inner_view_coords_.Height());
  return anchor_point - anchor_offset_from_origin;
}

}  // namespace blink
