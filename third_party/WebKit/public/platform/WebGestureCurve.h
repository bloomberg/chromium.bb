/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebGestureCurve_h
#define WebGestureCurve_h

#include "third_party/WebKit/public/platform/WebFloatSize.h"
#include "third_party/WebKit/public/platform/WebGestureCurveTarget.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {

// Abstract interface for curves used by ActivePlatformGestureAnimation. A
// WebGestureCurve defines the animation parameters as a function of time
// (zero-based), and applies the parameters directly to the target of the
// animation.
class WebGestureCurve {
 public:
  virtual ~WebGestureCurve() = default;

  // Returns false if curve has finished and can no longer be applied.
  // TODO(sahel): This will get removed once touchscreen and autoscroll flings
  // are handled on browser side (crbug.com/249063).
  bool AdvanceAndApplyToTarget(double time, WebGestureCurveTarget* target) {
    gfx::Vector2dF velocity, delta;
    bool still_active = Advance(time, velocity, delta);

    // As successive timestamps can be arbitrarily close (but monotonic!), don't
    // assume that a zero delta means the curve has terminated.
    if (delta.IsZero())
      return still_active;

    // scrollBy() could delete this curve if the animation is over, so don't
    // touch any member variables after making that call.
    bool did_scroll =
        target->ScrollBy(blink::WebFloatSize(delta.x(), delta.y()),
                         blink::WebFloatSize(velocity.x(), velocity.y()));
    return did_scroll && still_active;
  }

  // Returns false if curve has finished and can no longer advance.
  // This function is used for browser side fling.
  virtual bool Advance(double time,
                       gfx::Vector2dF& out_current_velocity,
                       gfx::Vector2dF& out_delta_to_scroll) = 0;
};

}  // namespace blink

#endif
