// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WheelFlingPlatformGestureCurve_h
#define WheelFlingPlatformGestureCurve_h

#include "FloatPoint.h"
#include "PlatformGestureCurve.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class PlatformGestureCurveTarget;

// Implementation of PlatformGestureCurve suitable for mouse wheel-based fling
// scroll. A Rayleigh distribtution curve is used to define the velocity profile,
// so velocity starts at zero, accelerates to a maximum proportional to 'velocity',
// then gently tails off to zero again.
class WheelFlingPlatformGestureCurve : public PlatformGestureCurve {
public:
    static PassOwnPtr<PlatformGestureCurve> create(const FloatPoint& velocity);
    virtual ~WheelFlingPlatformGestureCurve();

    virtual const char* debugName() const OVERRIDE;
    virtual bool apply(double time, PlatformGestureCurveTarget*) OVERRIDE;

private:
    explicit WheelFlingPlatformGestureCurve(const FloatPoint& velocity);

    FloatPoint m_velocity;
    IntPoint m_cumulativeScroll;
};

} // namespace WebCore

#endif
