// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_TOUCH_FLING_GESTURE_CURVE_H_
#define WEBKIT_GLUE_TOUCH_FLING_GESTURE_CURVE_H_

#include "third_party/WebKit/Source/Platform/chromium/public/WebFloatPoint.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFloatSize.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGestureCurve.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "webkit/glue/webkit_glue_export.h"

namespace WebKit {
class WebGestureCurveTarget;
}

namespace webkit_glue {

// Implementation of WebGestureCurve suitable for touch pad/screen-based
// fling scroll. Starts with a flat velocity profile based on 'velocity', which
// tails off to zero. Time is scaled to that duration of the fling is
// proportional to the initial velocity.
class TouchFlingGestureCurve : public WebKit::WebGestureCurve {
 public:

  WEBKIT_GLUE_EXPORT static WebGestureCurve* Create(
      const WebKit::WebFloatPoint& initial_velocity,
      float p0, float p1, float p2,
      const WebKit::WebSize& cumulativeScroll);

 virtual bool apply(double monotonicTime,
                    WebKit::WebGestureCurveTarget*) OVERRIDE;

 private:
  TouchFlingGestureCurve(const WebKit::WebFloatPoint& initial_velocity,
                         float p0,
                         float p1,
                         float p2,
                         const WebKit::WebSize& cumulativeScroll);
  virtual ~TouchFlingGestureCurve();

  WebKit::WebFloatPoint displacement_ratio_;
  WebKit::WebFloatSize cumulative_scroll_;
  float coefficients_[3];
  float time_offset_;
  float curve_duration_;
  float position_offset_;

  DISALLOW_COPY_AND_ASSIGN(TouchFlingGestureCurve);
};

} // namespace webkit_glue

#endif // WEBKIT_GLUE_TOUCH_FLING_GESTURE_CURVE_H_
