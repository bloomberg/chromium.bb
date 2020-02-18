// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCHSCREEN_TAP_SUPPRESSION_CONTROLLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCHSCREEN_TAP_SUPPRESSION_CONTROLLER_H_

#include <memory>

#include "base/macros.h"
#include "content/browser/renderer_host/event_with_latency_info.h"
#include "content/browser/renderer_host/input/tap_suppression_controller.h"

namespace content {

// Controls the suppression of touchscreen taps immediately following the
// dispatch of a GestureFlingCancel event.
class TouchscreenTapSuppressionController : public TapSuppressionController {
 public:
  TouchscreenTapSuppressionController(
      const TapSuppressionController::Config& config);
  ~TouchscreenTapSuppressionController() override;

  // Should be called on arrival of any tap-related events. Returns true if the
  // caller should stop normal handling of the gesture.
  bool FilterTapEvent(const GestureEventWithLatencyInfo& event);

 private:

  DISALLOW_COPY_AND_ASSIGN(TouchscreenTapSuppressionController);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCHSCREEN_TAP_SUPPRESSION_CONTROLLER_H_
