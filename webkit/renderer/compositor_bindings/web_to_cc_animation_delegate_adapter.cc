// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/renderer/compositor_bindings/web_to_cc_animation_delegate_adapter.h"

#include "third_party/WebKit/public/platform/WebAnimationDelegate.h"

namespace webkit {

WebToCCAnimationDelegateAdapter::WebToCCAnimationDelegateAdapter(
    blink::WebAnimationDelegate* delegate)
    : delegate_(delegate) {}

void WebToCCAnimationDelegateAdapter::NotifyAnimationStarted(
    double wall_clock_time,
    base::TimeTicks monotonic_time,
    cc::Animation::TargetProperty target_property) {
  delegate_->notifyAnimationStarted(
      (monotonic_time - base::TimeTicks()).InSecondsF(),
      static_cast<blink::WebAnimation::TargetProperty>(target_property));
}

void WebToCCAnimationDelegateAdapter::NotifyAnimationFinished(
    double wall_clock_time,
    base::TimeTicks monotonic_time,
    cc::Animation::TargetProperty target_property) {
  delegate_->notifyAnimationFinished(
      (monotonic_time - base::TimeTicks()).InSecondsF(),
      static_cast<blink::WebAnimation::TargetProperty>(target_property));
}

}  // namespace webkit
