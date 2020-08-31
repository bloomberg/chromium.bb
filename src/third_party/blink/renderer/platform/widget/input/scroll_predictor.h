// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_SCROLL_PREDICTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_SCROLL_PREDICTOR_H_

#include <vector>

#include "third_party/blink/public/platform/input/input_predictor.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/widget/input/event_with_callback.h"
#include "third_party/blink/renderer/platform/widget/input/prediction/filter_factory.h"
#include "third_party/blink/renderer/platform/widget/input/prediction/prediction_metrics_handler.h"

namespace blink {

namespace test {
class ScrollPredictorTest;
}

// This class handles resampling GestureScrollUpdate events on InputHandlerProxy
// at |BeginFrame| signal, before events been dispatched. The predictor use
// original events to update the prediction and align the aggregated event
// timestamp and delta_x/y to the VSync time.
class PLATFORM_EXPORT ScrollPredictor {
 public:
  // Select the predictor type from field trial params and initialize the
  // predictor.
  explicit ScrollPredictor();
  ~ScrollPredictor();

  // Reset the predictors on each GSB.
  void ResetOnGestureScrollBegin(const WebGestureEvent& event);

  // Resampling GestureScrollUpdate events. Updates the prediction with events
  // in original events list, and apply the prediction to the aggregated GSU
  // event if enable_resampling is true.
  std::unique_ptr<EventWithCallback> ResampleScrollEvents(
      std::unique_ptr<EventWithCallback> event_with_callback,
      base::TimeTicks frame_time);

 private:
  friend class test::InputHandlerProxyEventQueueTest;
  friend class test::ScrollPredictorTest;

  // Reset predictor and clear accumulated delta. This should be called on
  // GestureScrollBegin.
  void Reset();

  // Update the prediction with GestureScrollUpdate deltaX and deltaY
  void UpdatePrediction(const std::unique_ptr<WebInputEvent>& event,
                        base::TimeTicks frame_time);

  // Apply resampled deltaX/deltaY to gesture events
  void ResampleEvent(base::TimeTicks frame_time,
                     WebInputEvent* event,
                     ui::LatencyInfo* latency_info);

  // Reports metrics scores UMA histogram based on the metrics defined
  // in |PredictionMetricsHandler|
  void EvaluatePrediction();

  std::unique_ptr<InputPredictor> predictor_;
  std::unique_ptr<InputFilter> filter_;

  std::unique_ptr<FilterFactory> filter_factory_;

  // Whether predicted scroll events should be filtered or not
  bool filtering_enabled_ = false;

  // Total scroll delta from original scroll update events, used for calculating
  // predictions. Reset on GestureScrollBegin.
  gfx::PointF current_event_accumulated_delta_;
  // Predicted accumulated delta from last vsync, use for calculating delta_x
  // and delta_y for the resampled/predicted event.
  gfx::PointF last_predicted_accumulated_delta_;

  // Whether current scroll event should be resampled.
  bool should_resample_scroll_events_ = false;

  // Handler used for evaluating the prediction
  PredictionMetricsHandler metrics_handler_;

  DISALLOW_COPY_AND_ASSIGN(ScrollPredictor);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_SCROLL_PREDICTOR_H_
