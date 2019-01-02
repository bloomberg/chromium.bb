// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_BLINK_SCROLL_PREDICTOR_H_
#define UI_EVENTS_BLINK_SCROLL_PREDICTOR_H_

#include <vector>

#include "ui/events/base_event_utils.h"
#include "ui/events/blink/event_with_callback.h"
#include "ui/events/blink/prediction/input_predictor.h"

namespace ui {

namespace test {
class ScrollPredictorTest;
}

// This class handles resampling GestureScrollUpdate events on InputHandlerProxy
// at |BeginFrame| signal, before events been dispatched. The predictor use
// original events to update the prediction and align the aggregated event
// timestamp and delta_x/y to the VSync time.
class ScrollPredictor {
 public:
  ScrollPredictor();
  ~ScrollPredictor();

  void ResetOnGestureScrollBegin(const blink::WebGestureEvent& event);
  // Resampling GestureScrollUpdate events. Updates the prediction with events
  // in original events list, and apply the prediction to the aggregated GSU
  // event.
  void ResampleScrollEvents(
      const EventWithCallback::OriginalEventList& original_events,
      base::TimeTicks frame_time,
      blink::WebInputEvent* event);

  // Reset predictor and clear accumulated delta. This should be called on
  // GestureScrollBegin.
  void Reset();

 private:
  friend class test::InputHandlerProxyEventQueueTest;
  friend class test::ScrollPredictorTest;

  // Update the prediction with GestureScrollUpdate deltaX and deltaY
  void UpdatePrediction(const WebScopedInputEvent& event,
                        base::TimeTicks frame_time);

  // Apply resampled deltaX/deltaY to gesture events
  void ResampleEvent(base::TimeTicks frame_time, blink::WebInputEvent* event);

  std::unique_ptr<InputPredictor> predictor_;

  // Total scroll delta, used for prediction. Reset when GestureScrollBegin
  gfx::PointF current_accumulated_delta_;
  // Accumulated delta from last vsync, use to calculate delta_x and delta_y for
  // the aggregated event.
  gfx::PointF last_accumulated_delta_;

  bool should_resample_scroll_events_ = false;

  DISALLOW_COPY_AND_ASSIGN(ScrollPredictor);
};

}  // namespace ui

#endif  // UI_EVENTS_BLINK_SCROLL_PREDICTOR_H_
