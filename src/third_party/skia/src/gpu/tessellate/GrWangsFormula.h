/*
 * Copyright 2020 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrWangsFormula_DEFINED
#define GrWangsFormula_DEFINED

#include "include/core/SkPoint.h"
#include "include/private/SkNx.h"
#include "src/gpu/tessellate/GrVectorXform.h"

// Wang's formulas for cubics and quadratics (1985) give us the minimum number of evenly spaced (in
// the parametric sense) line segments that a curve must be chopped into in order to guarantee all
// lines stay within a distance of "1/intolerance" pixels from the true curve.
namespace GrWangsFormula {

SK_ALWAYS_INLINE static float length(const Sk2f& n) {
    Sk2f nn = n*n;
    return std::sqrt(nn[0] + nn[1]);
}

// Returns the minimum number of evenly spaced (in the parametric sense) line segments that the
// quadratic must be chopped into in order to guarantee all lines stay within a distance of
// "1/intolerance" pixels from the true curve.
SK_ALWAYS_INLINE static float quadratic(float intolerance, const SkPoint pts[]) {
    Sk2f p0 = Sk2f::Load(pts);
    Sk2f p1 = Sk2f::Load(pts + 1);
    Sk2f p2 = Sk2f::Load(pts + 2);
    float k = intolerance * .25f;
    return SkScalarSqrt(k * length(p0 - p1*2 + p2));
}

// Returns the minimum number of evenly spaced (in the parametric sense) line segments that the
// cubic must be chopped into in order to guarantee all lines stay within a distance of
// "1/intolerance" pixels from the true curve.
SK_ALWAYS_INLINE static float cubic(float intolerance, const SkPoint pts[]) {
    Sk2f p0 = Sk2f::Load(pts);
    Sk2f p1 = Sk2f::Load(pts + 1);
    Sk2f p2 = Sk2f::Load(pts + 2);
    Sk2f p3 = Sk2f::Load(pts + 3);
    float k = intolerance * .75f;
    return SkScalarSqrt(k * length(Sk2f::Max((p0 - p1*2 + p2).abs(),
                                             (p1 - p2*2 + p3).abs())));
}

// Returns the log2 of the provided value, were that value to be rounded up to the next power of 2.
// Returns 0 if value <= 0:
// Never returns a negative number, even if value is NaN.
//
//     nextlog2((-inf..1]) -> 0
//     nextlog2((1..2]) -> 1
//     nextlog2((2..4]) -> 2
//     nextlog2((4..8]) -> 3
//     ...
SK_ALWAYS_INLINE static int nextlog2(float value) {
    uint32_t bits;
    memcpy(&bits, &value, 4);
    bits += (1u << 23) - 1u;  // Increment the exponent for non-powers-of-2.
    int exp = ((int32_t)bits >> 23) - 127;
    return exp & ~(exp >> 31);  // Return 0 for negative or denormalized floats, and exponents < 0.
}

// Returns the minimum log2 number of evenly spaced (in the parametric sense) line segments that the
// transformed quadratic must be chopped into in order to guarantee all lines stay within a distance
// of "1/intolerance" pixels from the true curve.
SK_ALWAYS_INLINE static int quadratic_log2(float intolerance, const SkPoint pts[],
                                           const GrVectorXform& vectorXform = GrVectorXform()) {
    Sk2f p0 = Sk2f::Load(pts);
    Sk2f p1 = Sk2f::Load(pts + 1);
    Sk2f p2 = Sk2f::Load(pts + 2);
    Sk2f v = p0 + p1*-2 + p2;
    v = vectorXform(v);
    Sk2f vv = v*v;
    float k = intolerance * .25f;
    float f = k*k * (vv[0] + vv[1]);
    return (nextlog2(f) + 3) >> 2;  // ceil(log2(sqrt(sqrt(f))))
}

// Returns the minimum log2 number of evenly spaced (in the parametric sense) line segments that the
// transformed cubic must be chopped into in order to guarantee all lines stay within a distance of
// "1/intolerance" pixels from the true curve.
SK_ALWAYS_INLINE static int cubic_log2(float intolerance, const SkPoint pts[],
                                       const GrVectorXform& vectorXform = GrVectorXform()) {
    Sk4f p01 = Sk4f::Load(pts);
    Sk4f p12 = Sk4f::Load(pts + 1);
    Sk4f p23 = Sk4f::Load(pts + 2);
    Sk4f v = p01 + p12*-2 + p23;
    v = vectorXform(v);
    Sk4f vv = v*v;
    vv = Sk4f::Max(vv, SkNx_shuffle<2,3,0,1>(vv));
    float k = intolerance * .75f;
    float f = k*k * (vv[0] + vv[1]);
    return (nextlog2(f) + 3) >> 2;  // ceil(log2(sqrt(sqrt(f))))
}

}  // namespace

#endif
