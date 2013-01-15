// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for the TouchFlingGestureCurve.

#include "webkit/glue/touch_fling_gesture_curve.h"

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFloatPoint.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGestureCurve.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGestureCurveTarget.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebPoint.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"

using WebKit::WebFloatPoint;
using WebKit::WebGestureCurve;
using WebKit::WebGestureCurveTarget;
using WebKit::WebPoint;
using WebKit::WebSize;

namespace {

class MockGestureCurveTarget : public WebGestureCurveTarget {
 public:
  virtual void scrollBy(const WebPoint& delta) {
    cumulative_delta_.x += delta.x;
    cumulative_delta_.y += delta.y;
  }

  WebPoint cumulative_delta() const { return cumulative_delta_; }
  void resetCumulativeDelta() { cumulative_delta_ = WebPoint(); }

 private:
  WebPoint cumulative_delta_;
};

} // namespace anonymous

TEST(TouchFlingGestureCurve, flingCurveTouch)
{
  double initialVelocity = 5000;
  MockGestureCurveTarget target;

  scoped_ptr<WebGestureCurve> curve(webkit_glue::TouchFlingGestureCurve::Create(
      WebFloatPoint(initialVelocity, 0),
      -5.70762e+03f, 1.72e+02f, 3.7e+00f, WebSize()));

  // Note: the expectations below are dependent on the curve parameters hard
  // coded into the create call above.
  EXPECT_TRUE(curve->apply(0, &target));
  EXPECT_TRUE(curve->apply(0.25, &target));
  EXPECT_TRUE(curve->apply(0.45f, &target)); // Use non-uniform tick spacing.
  EXPECT_TRUE(curve->apply(1, &target));
  EXPECT_FALSE(curve->apply(1.5, &target));
  EXPECT_NEAR(target.cumulative_delta().x, 1193, 1);
  EXPECT_EQ(target.cumulative_delta().y, 0);
}

