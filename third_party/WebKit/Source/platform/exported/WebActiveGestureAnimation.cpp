/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/exported/WebActiveGestureAnimation.h"

#include <memory>
#include "platform/wtf/PtrUtil.h"
#include "public/platform/WebGestureCurve.h"
#include "public/platform/WebGestureCurveTarget.h"

namespace blink {

std::unique_ptr<WebActiveGestureAnimation>
WebActiveGestureAnimation::CreateWithTimeOffset(
    std::unique_ptr<WebGestureCurve> curve,
    WebGestureCurveTarget* target,
    double start_time) {
  return WTF::WrapUnique(
      new WebActiveGestureAnimation(std::move(curve), target, start_time));
}

WebActiveGestureAnimation::~WebActiveGestureAnimation() {}

WebActiveGestureAnimation::WebActiveGestureAnimation(
    std::unique_ptr<WebGestureCurve> curve,
    WebGestureCurveTarget* target,
    double start_time)
    : start_time_(start_time), curve_(std::move(curve)), target_(target) {}

bool WebActiveGestureAnimation::Animate(double time) {
  // All WebGestureCurves assume zero-based time, so we subtract
  // the animation start time before passing to the curve.
  return curve_->Apply(time - start_time_, target_);
}

}  // namespace blink
