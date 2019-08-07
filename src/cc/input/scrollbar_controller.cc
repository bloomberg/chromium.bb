// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/scrollbar_layer_impl_base.h"

#include <algorithm>

#include "cc/base/math_util.h"
#include "cc/input/scrollbar.h"
#include "cc/input/scrollbar_controller.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {
ScrollbarController::ScrollbarController(
    LayerTreeHostImpl* layer_tree_host_impl)
    : layer_tree_host_impl_(layer_tree_host_impl),
      scrollbar_scroll_is_active_(false),
      thumb_drag_in_progress_(false),
      currently_captured_scrollbar_(nullptr),
      previous_pointer_position_(gfx::PointF(0, 0)) {}

// Performs hit test and prepares scroll deltas that will be used by GSB and
// GSU.
InputHandlerPointerResult ScrollbarController::HandleMouseDown(
    const gfx::PointF position_in_widget) {
  InputHandlerPointerResult scroll_result;
  LayerImpl* layer_impl = GetLayerHitByPoint(position_in_widget);

  // If a non-custom scrollbar layer was not found, we return early as there is
  // no point in setting additional state in the ScrollbarController.
  if (!(layer_impl && layer_impl->ToScrollbarLayer()))
    return scroll_result;

  currently_captured_scrollbar_ = layer_impl->ToScrollbarLayer();
  scroll_result.type = PointerResultType::kScrollbarScroll;
  layer_tree_host_impl_->active_tree()->UpdateScrollbarGeometries();
  scroll_result.scroll_offset =
      GetScrollDeltaFromPointerDown(position_in_widget);
  previous_pointer_position_ = position_in_widget;
  scrollbar_scroll_is_active_ = true;
  if (thumb_drag_in_progress_) {
    scroll_result.scroll_units =
        ui::input_types::ScrollGranularity::kScrollByPrecisePixel;
  } else {
    // TODO(arakeri): This needs to be updated to kLine once cc implements
    // handling it. crbug.com/959441
    scroll_result.scroll_units =
        ui::input_types::ScrollGranularity::kScrollByPixel;
  }

  return scroll_result;
}

// Performs hit test and prepares scroll deltas that will be used by GSU.
InputHandlerPointerResult ScrollbarController::HandleMouseMove(
    const gfx::PointF position_in_widget) {
  InputHandlerPointerResult scroll_result;
  if (!thumb_drag_in_progress_)
    return scroll_result;

  scroll_result.type = PointerResultType::kScrollbarScroll;
  const ScrollbarOrientation orientation =
      currently_captured_scrollbar_->orientation();
  if (orientation == ScrollbarOrientation::VERTICAL)
    scroll_result.scroll_offset.set_y(position_in_widget.y() -
                                      previous_pointer_position_.y());
  else
    scroll_result.scroll_offset.set_x(position_in_widget.x() -
                                      previous_pointer_position_.x());

  LayerImpl* owner_scroll_layer =
      layer_tree_host_impl_->active_tree()->ScrollableLayerByElementId(
          currently_captured_scrollbar_->scroll_element_id());

  // Calculating the delta by which the scroller layer should move when
  // dragging the thumb depends on the following factors:
  // - scrollbar_track_length
  // - scrollbar_thumb_length
  // - scroll_layer_length
  // - viewport_length
  // - device_scale_factor
  // - position_in_widget
  //
  // When a thumb drag is in progress, for every pixel that the pointer moves,
  // the delta for the corresponding scroll_layer needs to be scaled by the
  // following ratio:
  // scaled_scroller_to_scrollbar_ratio =
  //  (scroll_layer_length - viewport_length) /
  //   (scrollbar_track_length - scrollbar_thumb_length)
  //
  // |<--------------------- scroll_layer_length -------------------------->|
  //
  // +------------------------------------------------+......................
  // |                                                |                     .
  // |<-------------- viewport_length --------------->|                     .
  // |                                                |                     .
  // |                                                |                     .
  // |                                                |                     .
  // |                                                |                     .
  // |                                                |                     .
  // |                                                |                     .
  // |                                                |                     .
  // |                                                |                     .
  // |                                                |                     .
  // |  |<------- scrollbar_track_length --------->|  |                     .
  // |                                                |                     .
  // +--+-----+----------------------------+-------+--+......................
  // |<||     |############################|       ||>|
  // +--+-----+----------------------------+-------+--+
  //
  //          |<- scrollbar_thumb_length ->|
  //
  layer_tree_host_impl_->active_tree()->UpdateScrollbarGeometries();
  float scroll_layer_length =
      currently_captured_scrollbar_->scroll_layer_length();
  float scrollbar_track_length = currently_captured_scrollbar_->TrackLength();
  gfx::Rect thumb_rect(currently_captured_scrollbar_->ComputeThumbQuadRect());
  float scrollbar_thumb_length = orientation == ScrollbarOrientation::VERTICAL
                                     ? thumb_rect.height()
                                     : thumb_rect.width();

  float viewport_length =
      orientation == ScrollbarOrientation::VERTICAL
          ? owner_scroll_layer->scroll_container_bounds().height()
          : (owner_scroll_layer->scroll_container_bounds().width());
  float scaled_scroller_to_scrollbar_ratio =
      ((scroll_layer_length - viewport_length) /
       (scrollbar_track_length - scrollbar_thumb_length)) *
      layer_tree_host_impl_->active_tree()->device_scale_factor();
  scroll_result.scroll_offset.Scale(scaled_scroller_to_scrollbar_ratio);
  previous_pointer_position_ = position_in_widget;
  // Thumb drags have more granularity and are purely dependent on the pointer
  // movement. Hence we use kPrecisePixel when dragging the thumb.
  scroll_result.scroll_units =
      ui::input_types::ScrollGranularity::kScrollByPrecisePixel;

  return scroll_result;
}

// Performs hit test and prepares scroll deltas that will be used by GSE.
InputHandlerPointerResult ScrollbarController::HandleMouseUp(
    const gfx::PointF position_in_widget) {
  InputHandlerPointerResult scroll_result;
  if (scrollbar_scroll_is_active_) {
    scrollbar_scroll_is_active_ = false;
    scroll_result.type = PointerResultType::kScrollbarScroll;
  }
  thumb_drag_in_progress_ = false;
  return scroll_result;
}

// Returns the layer that is hit by the position_in_widget.
LayerImpl* ScrollbarController::GetLayerHitByPoint(
    const gfx::PointF position_in_widget) {
  LayerTreeImpl* active_tree = layer_tree_host_impl_->active_tree();
  gfx::Point viewport_point(position_in_widget.x(), position_in_widget.y());

  gfx::PointF device_viewport_point = gfx::ScalePoint(
      gfx::PointF(viewport_point), active_tree->device_scale_factor());
  LayerImpl* layer_impl =
      active_tree->FindFirstScrollingLayerOrScrollbarThatIsHitByPoint(
          device_viewport_point);

  return layer_impl;
}

int ScrollbarController::GetScrollDeltaForScrollbarPart(
    ScrollbarPart scrollbar_part) {
  int scroll_delta = 0;
  int viewport_length = 0;
  LayerImpl* owner_scroll_layer = nullptr;

  switch (scrollbar_part) {
    case ScrollbarPart::BACK_BUTTON:
    case ScrollbarPart::FORWARD_BUTTON:
      scroll_delta = kPixelsPerLineStep;
      break;
    case ScrollbarPart::BACK_TRACK:
    case ScrollbarPart::FORWARD_TRACK:
      owner_scroll_layer =
          layer_tree_host_impl_->active_tree()->ScrollableLayerByElementId(
              currently_captured_scrollbar_->scroll_element_id());
      viewport_length =
          currently_captured_scrollbar_->orientation() ==
                  ScrollbarOrientation::VERTICAL
              ? owner_scroll_layer->scroll_container_bounds().height()
              : (owner_scroll_layer->scroll_container_bounds().width());
      scroll_delta = viewport_length * kMinFractionToStepWhenPaging;
      break;
    default:
      scroll_delta = 0;
  }
  return scroll_delta *
         layer_tree_host_impl_->active_tree()->device_scale_factor();
}

// Determines the scroll offsets based on hit test results.
gfx::ScrollOffset ScrollbarController::GetScrollDeltaFromPointerDown(
    const gfx::PointF position_in_widget) {
  const ScrollbarOrientation orientation =
      currently_captured_scrollbar_->orientation();

  // position_in_widget needs to be transformed and made relative to the
  // scrollbar layer because hit testing assumes layer relative coordinates.
  ScrollbarPart scrollbar_part = ScrollbarPart::NO_PART;
  gfx::Transform inverse_screen_space_transform(
      gfx::Transform::kSkipInitialization);
  if (!currently_captured_scrollbar_->ScreenSpaceTransform().GetInverse(
          &inverse_screen_space_transform))
    return gfx::ScrollOffset(0, 0);

  bool clipped;
  gfx::PointF scroller_relative_position(MathUtil::ProjectPoint(
      inverse_screen_space_transform, position_in_widget, &clipped));

  if (clipped)
    return gfx::ScrollOffset(0, 0);

  scrollbar_part = currently_captured_scrollbar_->IdentifyScrollbarPart(
      scroller_relative_position);

  float scroll_delta = GetScrollDeltaForScrollbarPart(scrollbar_part);

  // See CreateScrollStateForGesture for more information on how these values
  // will be interpreted.
  if (scrollbar_part == ScrollbarPart::BACK_BUTTON) {
    return orientation == ScrollbarOrientation::VERTICAL
               ? gfx::ScrollOffset(0, -scroll_delta)   // Up arrow
               : gfx::ScrollOffset(-scroll_delta, 0);  // Left arrow
  } else if (scrollbar_part == ScrollbarPart::FORWARD_BUTTON) {
    return orientation == ScrollbarOrientation::VERTICAL
               ? gfx::ScrollOffset(0, scroll_delta)   // Down arrow
               : gfx::ScrollOffset(scroll_delta, 0);  // Right arrow
  } else if (scrollbar_part == ScrollbarPart::THUMB) {
    // Offsets are calculated in HandleMouseMove.
    thumb_drag_in_progress_ = true;
  } else if (scrollbar_part == ScrollbarPart::BACK_TRACK) {
    return orientation == ScrollbarOrientation::VERTICAL
               ? gfx::ScrollOffset(0, -scroll_delta)   // Track click up
               : gfx::ScrollOffset(-scroll_delta, 0);  // Track click left
  } else if (scrollbar_part == ScrollbarPart::FORWARD_TRACK) {
    return orientation == ScrollbarOrientation::VERTICAL
               ? gfx::ScrollOffset(0, scroll_delta)   // Track click down
               : gfx::ScrollOffset(scroll_delta, 0);  // Track click right
  }

  return gfx::ScrollOffset(0, 0);
}

}  // namespace cc
