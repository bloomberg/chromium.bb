// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/compositor_bindings/web_animation_curve_common.h"

#include "cc/timing_function.h"

namespace WebKit {

scoped_ptr<cc::TimingFunction> createTimingFunction(
    WebAnimationCurve::TimingFunctionType type) {
  switch (type) {
    case WebAnimationCurve::TimingFunctionTypeEase:
      return cc::EaseTimingFunction::create();
    case WebAnimationCurve::TimingFunctionTypeEaseIn:
      return cc::EaseInTimingFunction::create();
    case WebAnimationCurve::TimingFunctionTypeEaseOut:
      return cc::EaseOutTimingFunction::create();
    case WebAnimationCurve::TimingFunctionTypeEaseInOut:
      return cc::EaseInOutTimingFunction::create();
    case WebAnimationCurve::TimingFunctionTypeLinear:
      return scoped_ptr<cc::TimingFunction>();
  }
  return scoped_ptr<cc::TimingFunction>();
}

}  // namespace WebKit
