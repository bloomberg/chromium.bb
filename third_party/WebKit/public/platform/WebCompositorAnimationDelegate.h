// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebCompositorAnimationDelegate_h
#define WebCompositorAnimationDelegate_h

#include "WebCommon.h"
#include "cc/animation/animation_curve.h"

namespace blink {

class BLINK_PLATFORM_EXPORT WebCompositorAnimationDelegate {
public:
    virtual ~WebCompositorAnimationDelegate() { }

    virtual void notifyAnimationStarted(double monotonicTime, int group) = 0;
    virtual void notifyAnimationFinished(double monotonicTime, int group) = 0;
    virtual void notifyAnimationAborted(double monotonicTime, int group) = 0;
    // In the current state of things, notifyAnimationTakeover only applies to
    // scroll offset animations since main thread scrolling reasons can be added
    // while the compositor is animating. Keeping this non-pure virtual since
    // it doesn't apply to CSS animations.
    virtual void notifyAnimationTakeover(
        double monotonicTime,
        double animationStartTime,
        scoped_ptr<cc::AnimationCurve> curve) { }
};

} // namespace blink

#endif // WebCompositorAnimationDelegate_h
