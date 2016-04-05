// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UnitBezier_h
#define UnitBezier_h

#include "ui/gfx/geometry/cubic_bezier.h"
#include "wtf/Allocator.h"

namespace blink {

// TODO(loyso): Erase blink::UnitBezier and use gfx::CubicBezier directly.
struct UnitBezier {
    USING_FAST_MALLOC(UnitBezier);
public:
    UnitBezier(double p1x, double p1y, double p2x, double p2y)
        : m_cubicBezier(p1x, p1y, p2x, p2y)
    {
    }

    double sampleCurveX(double t) const
    {
        return m_cubicBezier.SampleCurveX(t);
    }

    double sampleCurveY(double t) const
    {
        return m_cubicBezier.SampleCurveY(t);
    }

    // Evaluates y at the given x.
    double solve(double x) const
    {
        return m_cubicBezier.Solve(x);
    }

    // Evaluates y at the given x. The epsilon parameter provides a hint as to the required
    // accuracy and is not guaranteed.
    double solveWithEpsilon(double x, double epsilon) const
    {
        return m_cubicBezier.SolveWithEpsilon(x, epsilon);
    }

private:
    gfx::CubicBezier m_cubicBezier;
};

} // namespace blink

#endif // UnitBezier_h
