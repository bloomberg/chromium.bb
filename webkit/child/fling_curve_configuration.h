// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_CHILD_FLING_CURVE_CONFIGURATION_H_
#define WEBKIT_CHILD_FLING_CURVE_CONFIGURATION_H_

#include <vector>

#include "base/synchronization/lock.h"
#include "third_party/WebKit/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/public/platform/WebSize.h"

namespace WebKit {
class WebGestureCurve;
}

namespace webkit_glue {

// A class to manage dynamically adjustable parameters controlling the
// shape of the fling deacceleration function.
class FlingCurveConfiguration {
 public:
  FlingCurveConfiguration();
  virtual ~FlingCurveConfiguration();

  // Create a touchpad fling curve using the current parameters.
  WebKit::WebGestureCurve* CreateForTouchPad(
      const WebKit::WebFloatPoint& velocity,
      const WebKit::WebSize& cumulativeScroll);

  // Create a touchscreen fling curve using the current parameters.
  WebKit::WebGestureCurve* CreateForTouchScreen(
      const WebKit::WebFloatPoint& velocity,
      const WebKit::WebSize& cumulativeScroll);

  // Set the curve parameters.
  void SetCurveParameters(
      const std::vector<float>& new_touchpad,
      const std::vector<float>& new_touchscreen);

 private:
  WebKit::WebGestureCurve* CreateCore(
    const std::vector<float>& coefs,
    const WebKit::WebFloatPoint& velocity,
    const WebKit::WebSize& cumulativeScroll);

  // Protect access to touchpad_coefs_ and touchscreen_coefs_.
  base::Lock lock_;
  std::vector<float> touchpad_coefs_;
  std::vector<float> touchscreen_coefs_;

  DISALLOW_COPY_AND_ASSIGN(FlingCurveConfiguration);
};

} // namespace webkit_glue

#endif // WEBKIT_CHILD_FLING_CURVE_CONFIGURATION_H_
