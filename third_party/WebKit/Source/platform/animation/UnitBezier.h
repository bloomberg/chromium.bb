/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef UnitBezier_h
#define UnitBezier_h

#include "platform/PlatformExport.h"
#include "wtf/Allocator.h"
#include "wtf/Assertions.h"

#include <algorithm>
#include <cmath>

namespace blink {

struct PLATFORM_EXPORT UnitBezier {
    USING_FAST_MALLOC(UnitBezier);
public:
    UnitBezier(double p1x, double p1y, double p2x, double p2y);

    static const double kBezierEpsilon;

    double sampleCurveX(double t) const
    {
        // `ax t^3 + bx t^2 + cx t' expanded using Horner's rule.
        return ((ax * t + bx) * t + cx) * t;
    }

    double sampleCurveY(double t) const
    {
        return ((ay * t + by) * t + cy) * t;
    }

    double sampleCurveDerivativeX(double t) const
    {
        return (3.0 * ax * t + 2.0 * bx) * t + cx;
    }

    double sampleCurveDerivativeY(double t) const
    {
        return (3.0 * ay * t + 2.0 * by) * t + cy;
    }

    // Given an x value, find a parametric value it came from.
    double solveCurveX(double x, double epsilon) const
    {
        ASSERT(x >= 0.0);
        ASSERT(x <= 1.0);

        double t0;
        double t1;
        double t2;
        double x2;
        double d2;
        int i;

        // First try a few iterations of Newton's method -- normally very fast.
        for (t2 = x, i = 0; i < 8; i++) {
            x2 = sampleCurveX(t2) - x;
            if (fabs (x2) < epsilon)
                return t2;
            d2 = sampleCurveDerivativeX(t2);
            if (fabs(d2) < 1e-6)
                break;
            t2 = t2 - x2 / d2;
        }

        // Fall back to the bisection method for reliability.
        t0 = 0.0;
        t1 = 1.0;
        t2 = x;

        while (t0 < t1) {
            x2 = sampleCurveX(t2);
            if (fabs(x2 - x) < epsilon)
                return t2;
            if (x > x2)
                t0 = t2;
            else
                t1 = t2;
            t2 = (t1 - t0) * .5 + t0;
        }

        // Failure.
        return t2;
    }

    // Evaluates y at the given x.
    double solve(double x) const
    {
        return solveWithEpsilon(x, kBezierEpsilon);
    }

    // Evaluates y at the given x. The epsilon parameter provides a hint as to the required
    // accuracy and is not guaranteed.
    double solveWithEpsilon(double x, double epsilon) const
    {
        if (x < 0.0)
            return 0.0 + m_startGradient * x;
        if (x > 1.0)
            return 1.0 + m_endGradient * (x - 1.0);
        return sampleCurveY(solveCurveX(x, epsilon));
    }

    // Returns an approximation of dy/dx at the given x.
    double slope(double x) const
    {
        return slopeWithEpsilon(x, kBezierEpsilon);
    }

    double slopeWithEpsilon(double x, double epsilon) const
    {
        double t = solveCurveX(x, epsilon);
        double dx = sampleCurveDerivativeX(t);
        double dy = sampleCurveDerivativeY(t);
        return dy / dx;
    }

    // Sets |min| and |max| to the bezier's minimum and maximium y values in the
    // interval [0, 1].
    void range(double* min, double* max) const
    {
        *min = m_rangeMin;
        *max = m_rangeMax;
    }

private:
    void initCoefficients(double p1x, double p1y, double p2x, double p2y);
    void initGradients(double p1x, double p1y, double p2x, double p2y);
    void initRange(double p1y, double p2y);

    double ax;
    double bx;
    double cx;

    double ay;
    double by;
    double cy;

    double m_startGradient;
    double m_endGradient;

    double m_rangeMin;
    double m_rangeMax;
};

} // namespace blink

#endif // UnitBezier_h
