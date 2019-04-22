// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_TARGET_MAC_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_TARGET_MAC_H_

#include "base/macros.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target_base.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/common/input/synthetic_gesture_params.h"

namespace content {

// SyntheticGestureTarget implementation for mac
class SyntheticGestureTargetMac : public SyntheticGestureTargetBase {
 public:
  SyntheticGestureTargetMac(RenderWidgetHostImpl* host,
                            RenderWidgetHostViewCocoa* cocoa_view);

  // SyntheticGestureTargetBase:
  void DispatchWebTouchEventToPlatform(
      const blink::WebTouchEvent& event,
      const ui::LatencyInfo& latency_info) override;
  void DispatchWebMouseWheelEventToPlatform(
      const blink::WebMouseWheelEvent& web_wheel,
      const ui::LatencyInfo& latency_info) override;
  void DispatchWebGestureEventToPlatform(
      const blink::WebGestureEvent& web_gesture,
      const ui::LatencyInfo& latency_info) override;
  void DispatchWebMouseEventToPlatform(
      const blink::WebMouseEvent& web_mouse,
      const ui::LatencyInfo& latency_info) override;

  // SyntheticGestureTarget:
  SyntheticGestureParams::GestureSourceType
  GetDefaultSyntheticGestureSourceType() const override;

  float GetTouchSlopInDips() const override;
  float GetSpanSlopInDips() const override;
  float GetMinScalingSpanInDips() const override;

 private:
  RenderWidgetHostViewMac* GetView() const;
  RenderWidgetHostViewCocoa* cocoa_view_;

  DISALLOW_COPY_AND_ASSIGN(SyntheticGestureTargetMac);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_TARGET_MAC_H_
