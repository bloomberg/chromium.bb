// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/fling_curve_configuration.h"

#include "base/logging.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGestureCurve.h"
#include "webkit/glue/touch_fling_gesture_curve.h"

namespace webkit_glue {

FlingCurveConfiguration::FlingCurveConfiguration() { }

FlingCurveConfiguration::~FlingCurveConfiguration() { }

void FlingCurveConfiguration::SetCurveParameters(
    const std::vector<float>& new_touchpad,
    const std::vector<float>& new_touchscreen) {
  DCHECK(new_touchpad.size() >= 3);
  DCHECK(new_touchscreen.size() >= 3);
  base::AutoLock scoped_lock(lock_);
  touchpad_coefs_ = new_touchpad;
  touchscreen_coefs_ = new_touchscreen;
}

WebKit::WebGestureCurve* FlingCurveConfiguration::CreateCore(
    const std::vector<float>& coefs,
    const WebKit::WebFloatPoint& velocity,
    const WebKit::WebSize& cumulativeScroll) {
  float p0, p1, p2;

  {
    base::AutoLock scoped_lock(lock_);
    p0 = coefs[0];
    p1 = coefs[1];
    p2 = coefs[2];
  }

  return TouchFlingGestureCurve::Create(velocity, p0, p1, p2, cumulativeScroll);
}

WebKit::WebGestureCurve* FlingCurveConfiguration::CreateForTouchPad(
    const WebKit::WebFloatPoint& velocity,
    const WebKit::WebSize& cumulativeScroll) {
  return CreateCore(touchpad_coefs_, velocity, cumulativeScroll);
}

WebKit::WebGestureCurve* FlingCurveConfiguration::CreateForTouchScreen(
    const WebKit::WebFloatPoint& velocity,
    const WebKit::WebSize& cumulativeScroll) {
  return CreateCore(touchscreen_coefs_, velocity, cumulativeScroll);
}

} // namespace webkit_glue
