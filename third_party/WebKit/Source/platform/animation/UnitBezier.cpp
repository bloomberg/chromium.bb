// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/UnitBezier.h"

namespace blink {

const double UnitBezier::kBezierEpsilon = 1e-7;

UnitBezier::UnitBezier(double p1x, double p1y, double p2x, double p2y)
{
    initCoefficients(p1x, p1y, p2x, p2y);
    initGradients(p1x, p1y, p2x, p2y);
    initRange(p1y, p2y);
}

void UnitBezier::initCoefficients(double p1x, double p1y, double p2x, double p2y)
{
    // Calculate the polynomial coefficients, implicit first and last control points are (0,0) and (1,1).
    cx = 3.0 * p1x;
    bx = 3.0 * (p2x - p1x) - cx;
    ax = 1.0 - cx -bx;

    cy = 3.0 * p1y;
    by = 3.0 * (p2y - p1y) - cy;
    ay = 1.0 - cy - by;
}

void UnitBezier::initGradients(double p1x, double p1y, double p2x, double p2y)
{
    // End-point gradients are used to calculate timing function results
    // outside the range [0, 1].
    //
    // There are three possibilities for the gradient at each end:
    // (1) the closest control point is not horizontally coincident with regard to
    //     (0, 0) or (1, 1). In this case the line between the end point and
    //     the control point is tangent to the bezier at the end point.
    // (2) the closest control point is coincident with the end point. In
    //     this case the line between the end point and the far control
    //     point is tangent to the bezier at the end point.
    // (3) the closest control point is horizontally coincident with the end
    //     point, but vertically distinct. In this case the gradient at the
    //     end point is Infinite. However, this causes issues when
    //     interpolating. As a result, we break down to a simple case of
    //     0 gradient under these conditions.

    if (p1x > 0)
        m_startGradient = p1y / p1x;
    else if (!p1y && p2x > 0)
        m_startGradient = p2y / p2x;
    else
        m_startGradient = 0;

    if (p2x < 1)
        m_endGradient = (p2y - 1) / (p2x - 1);
    else if (p2x == 1 && p1x < 1)
        m_endGradient = (p1y - 1) / (p1x - 1);
    else
        m_endGradient = 0;
}

void UnitBezier::initRange(double p1y, double p2y)
{
    m_rangeMin = 0;
    m_rangeMax = 1;
    if (0 <= p1y && p1y < 1 && 0 <= p2y && p2y <= 1)
        return;

    // Represent the function's derivative in the form at^2 + bt + c
    // as in sampleCurveDerivativeY.
    // (Technically this is (dy/dt)*(1/3), which is suitable for finding zeros
    // but does not actually give the slope of the curve.)
    const double a = 3.0 * ay;
    const double b = 2.0 * by;
    const double c = cy;

    // Check if the derivative is constant.
    if (std::abs(a) < kBezierEpsilon && std::abs(b) < kBezierEpsilon)
        return;

    // Zeros of the function's derivative.
    double t1 = 0;
    double t2 = 0;

    if (std::abs(a) < kBezierEpsilon) {
        // The function's derivative is linear.
        t1 = -c / b;
    } else {
        // The function's derivative is a quadratic. We find the zeros of this
        // quadratic using the quadratic formula.
        double discriminant = b * b - 4 * a * c;
        if (discriminant < 0)
            return;
        double discriminantSqrt = sqrt(discriminant);
        t1 = (-b + discriminantSqrt) / (2 * a);
        t2 = (-b - discriminantSqrt) / (2 * a);
    }

    double sol1 = 0;
    double sol2 = 0;

    if (0 < t1 && t1 < 1)
        sol1 = sampleCurveY(t1);

    if (0 < t2 && t2 < 1)
        sol2 = sampleCurveY(t2);

    m_rangeMin = std::min(std::min(m_rangeMin, sol1), sol2);
    m_rangeMax = std::max(std::max(m_rangeMax, sol1), sol2);
}

} // namespace blink
