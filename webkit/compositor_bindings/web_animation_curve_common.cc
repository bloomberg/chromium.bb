// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/compositor_bindings/web_animation_curve_common.h"

#include "cc/animation/timing_function.h"

namespace webkit {

scoped_ptr<cc::TimingFunction> CreateTimingFunction(
    WebKit::WebAnimationCurve::TimingFunctionType type) {
  switch (type) {
    case WebKit::WebAnimationCurve::TimingFunctionTypeEase:
      return cc::EaseTimingFunction::create();
    case WebKit::WebAnimationCurve::TimingFunctionTypeEaseIn:
      return cc::EaseInTimingFunction::create();
    case WebKit::WebAnimationCurve::TimingFunctionTypeEaseOut:
      return cc::EaseOutTimingFunction::create();
    case WebKit::WebAnimationCurve::TimingFunctionTypeEaseInOut:
      return cc::EaseInOutTimingFunction::create();
    case WebKit::WebAnimationCurve::TimingFunctionTypeLinear:
      return scoped_ptr<cc::TimingFunction>();
  }
  return scoped_ptr<cc::TimingFunction>();
}

}  // namespace WebKit
