// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TouchpadFlingPlatformGestureCurve_h
#define TouchpadFlingPlatformGestureCurve_h

#include "FloatPoint.h"
#include "PlatformGestureCurve.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class PlatformGestureCurveTarget;

// Implementation of PlatformGestureCurve suitable for touch pad/screen-based
// fling scroll. Starts with a flat velocity profile based on 'velocity', which
// tails off to zero. Time is scaled to that duration of the fling is proportional
// the initial velocity.
class TouchpadFlingPlatformGestureCurve : public PlatformGestureCurve {
public:
    static PassOwnPtr<PlatformGestureCurve> create(const FloatPoint& velocity, IntPoint cumulativeScroll = IntPoint());
    static PassOwnPtr<PlatformGestureCurve> create(const FloatPoint& velocity, float p0, float p1, float p2, float p3, float p4, float curveDuration, IntPoint cumulativeScroll = IntPoint());
    virtual ~TouchpadFlingPlatformGestureCurve();

    virtual const char* debugName() const OVERRIDE;
    virtual bool apply(double monotonicTime, PlatformGestureCurveTarget*) OVERRIDE;

private:
    TouchpadFlingPlatformGestureCurve(const FloatPoint& velocity, float p0, float p1, float p2, float p3, float p4, float curveDuration, const IntPoint& cumulativeScroll);

    FloatPoint m_displacementRatio;
    IntPoint m_cumulativeScroll;
    float m_coeffs[5];
    float m_timeOffset;
    float m_curveDuration;
    float m_positionOffset;

    static const int m_maxSearchIterations;
};

} // namespace WebCore

#endif
