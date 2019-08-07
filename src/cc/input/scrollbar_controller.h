// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_SCROLLBAR_CONTROLLER_H_
#define CC_INPUT_SCROLLBAR_CONTROLLER_H_

#include "cc/cc_export.h"
#include "cc/input/input_handler.h"
#include "cc/input/scrollbar.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/painted_scrollbar_layer_impl.h"

namespace cc {

// This class is responsible for hit testing composited scrollbars, event
// handling and creating gesture scroll deltas.
class CC_EXPORT ScrollbarController {
 public:
  explicit ScrollbarController(LayerTreeHostImpl*);
  virtual ~ScrollbarController() = default;

  InputHandlerPointerResult HandleMouseDown(
      const gfx::PointF position_in_widget);
  InputHandlerPointerResult HandleMouseMove(
      const gfx::PointF position_in_widget);
  InputHandlerPointerResult HandleMouseUp(const gfx::PointF position_in_widget);

 private:
  // Returns a gfx::ScrollOffset object which contains scroll deltas for the
  // synthetic Gesture events.
  gfx::ScrollOffset GetScrollDeltaFromPointerDown(
      const gfx::PointF position_in_widget);
  LayerImpl* GetLayerHitByPoint(const gfx::PointF position_in_widget);
  int GetScrollDeltaForScrollbarPart(ScrollbarPart scrollbar_part);

  LayerTreeHostImpl* layer_tree_host_impl_;

  // Used to safeguard against firing GSE without firing GSB and GSU. For
  // example, if mouse is pressed outside the scrollbar but released after
  // moving inside the scrollbar, a GSE will get queued up without this flag.
  bool scrollbar_scroll_is_active_;

  // Used to tell if the scrollbar thumb is getting dragged.
  bool thumb_drag_in_progress_;
  const ScrollbarLayerImplBase* currently_captured_scrollbar_;

  // This is relative to the RenderWidget's origin.
  gfx::PointF previous_pointer_position_;
};

}  // namespace cc

#endif  // CC_INPUT_SCROLLBAR_CONTROLLER_H_
