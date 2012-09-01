// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PlatformGestureCurve_h
#define PlatformGestureCurve_h

namespace WebCore {

class PlatformGestureCurveTarget;

// Abstract interface for curves used by ActivePlatformGestureAnimation. A
// PlatformGestureCurve defines the animation parameters as a function of time
// (zero-based), and applies the parameters directly to the target of the
// animation.
class PlatformGestureCurve {
public:
    virtual ~PlatformGestureCurve() { }

    // Returns a name of the curve for use in debugging.
    virtual const char* debugName() const = 0;

    // Returns false if curve has finished and can no longer be applied.
    virtual bool apply(double time, PlatformGestureCurveTarget*) = 0;
};

} // namespace WebCore

#endif
