// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_GESTURES_BLINK_WEB_GESTURE_CURVE_IMPL_H_
#define UI_EVENTS_GESTURES_BLINK_WEB_GESTURE_CURVE_IMPL_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "third_party/WebKit/public/platform/WebGestureCurve.h"
#include "third_party/WebKit/public/platform/WebGestureDevice.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {
class WebGestureCurveTarget;
}

namespace ui {
class GestureCurve;

class WebGestureCurveImpl : public NON_EXPORTED_BASE(blink::WebGestureCurve) {
 public:
  static std::unique_ptr<blink::WebGestureCurve> CreateFromDefaultPlatformCurve(
      blink::WebGestureDevice device_source,
      const gfx::Vector2dF& initial_velocity,
      const gfx::Vector2dF& initial_offset,
      bool on_main_thread);
  static std::unique_ptr<blink::WebGestureCurve> CreateFromUICurveForTesting(
      std::unique_ptr<GestureCurve> curve,
      const gfx::Vector2dF& initial_offset);

  ~WebGestureCurveImpl() override;

  // WebGestureCurve implementation.
  bool Apply(double time, blink::WebGestureCurveTarget* target) override;

 private:
  enum class ThreadType {
    MAIN,
    IMPL,
    TEST
  };

  WebGestureCurveImpl(std::unique_ptr<GestureCurve> curve,
                      const gfx::Vector2dF& initial_offset,
                      ThreadType animating_thread_type);

  std::unique_ptr<GestureCurve> curve_;

  gfx::Vector2dF last_offset_;

  ThreadType animating_thread_type_;
  int64_t ticks_since_first_animate_;
  double first_animate_time_;
  double last_animate_time_;

  DISALLOW_COPY_AND_ASSIGN(WebGestureCurveImpl);
};

}  // namespace ui

#endif  // UI_EVENTS_GESTURES_BLINK_WEB_GESTURE_CURVE_IMPL_H_
