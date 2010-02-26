/*
 * Copyright 2010, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef O3D_CORE_CROSS_GPU2D_CUBIC_MATH_UTILS_H_
#define O3D_CORE_CROSS_GPU2D_CUBIC_MATH_UTILS_H_

#include <math.h>

#include "core/cross/math_types.h"

namespace o3d {
namespace gpu2d {
namespace cubic {

// A roundoff factor in the cubic classification and texture
// coordinate generation algorithms. This primarily determines the
// handling of corner cases during the classification process. Be
// careful when adjusting this; it has been determined emprically to
// work well.
const float kEpsilon = 5.0e-4f;

// Returns zero if value is within +/-kEpsilon of zero.
inline float RoundToZero(float val) {
  if (val < kEpsilon && val > -kEpsilon)
    return 0;
  return val;
}

// Returns distance between two points in the 2D plane.
inline float Distance(float x0, float y0,
                      float x1, float y1) {
  float xd = x1 - x0;
  float yd = y1 - y0;
  return sqrtf(xd * xd + yd * yd);
}

// Returns true if the given points are within kEpsilon distance of
// each other.
inline bool ApproxEqual(const Vector3& v0, const Vector3& v1) {
  return lengthSqr(v0 - v1) < kEpsilon * kEpsilon;
}

// Returns true if the given 2D points are within kEpsilon distance of
// each other.
inline bool ApproxEqual(float x0, float y0,
                        float x1, float y1) {
  return Distance(x0, y0, x1, y1) < kEpsilon;
}

// Returns true if the given scalar values are within kEpsilon of each other.
inline bool ApproxEqual(float f0, float f1) {
  return fabsf(f0 - f1) < kEpsilon;
}

// Determines whether the line segment between (p1, q1) intersects
// that between (p2, q2).
bool LinesIntersect(float p1x, float p1y,
                    float q1x, float q1y,
                    float p2x, float p2y,
                    float q2x, float q2y);

// Determines whether the 2D point defined by (px, py) is inside the
// 2D triangle defined by vertices (ax, ay), (bx, by), and (cx, cy).
// This test defines that points exactly on an edge are not considered
// to be inside the triangle.
bool PointInTriangle(float px, float py,
                     float ax, float ay,
                     float bx, float by,
                     float cx, float cy);

// Determines whether the triangles defined by the points (a1, b1, c1)
// and (a2, b2, c2) overlap. The definition of this function is that
// if the two triangles only share an adjacent edge or vertex, they
// are not considered to overlap.
bool TrianglesOverlap(float a1x, float a1y,
                      float b1x, float b1y,
                      float c1x, float c1y,
                      float a2x, float a2y,
                      float b2x, float b2y,
                      float c2x, float c2y);

}  // namespace cubic
}  // namespace gpu2d
}  // namespace o3d

#endif  // O3D_CORE_CROSS_GPU2D_CUBIC_MATH_UTILS_H_

