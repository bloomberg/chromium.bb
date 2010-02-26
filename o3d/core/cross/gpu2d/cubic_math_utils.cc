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

#include "core/cross/gpu2d/cubic_math_utils.h"

namespace o3d {
namespace gpu2d {
namespace cubic {

namespace {

// Utility functions local to this file

int Orientation(float x1, float y1,
                float x2, float y2,
                float x3, float y3) {
  // p1 = (x1, y1)
  // p2 = (x2, y2)
  // p3 = (x3, y3)
  float cross_product = (y2 - y1) * (x3 - x2) - (y3 - y2) * (x2 - x1);
  return (cross_product < 0.0f) ? -1 : ((cross_product > 0.0f) ? 1 : 0);
}

bool EdgeEdgeTest(float ax, float ay,
                  float v0x, float v0y,
                  float u0x, float u0y,
                  float u1x, float u1y) {
  // This edge to edge test is based on Franlin Antonio's gem: "Faster
  // Line Segment Intersection", in Graphics Gems III, pp. 199-202.
  float bx = u0x - u1x;
  float by = u0y - u1y;
  float cx = v0x - u0x;
  float cy = v0y - u0y;
  float f = ay * bx - ax * by;
  float d = by * cx - bx * cy;
  if ((f > 0 && d >= 0 && d <= f) || (f < 0 && d <= 0 && d >= f)) {
    float e = ax * cy - ay * cx;

    // This additional test avoids reporting coincident edges, which
    // is the behavior we want.
    if (ApproxEqual(e, 0) || ApproxEqual(f, 0) || ApproxEqual(e, f)) {
      return false;
    }

    if (f > 0) {
      return e >= 0 && e <= f;
    } else {
      return e <= 0 && e >= f;
    }
  }
  return false;
}

bool EdgeAgainstTriEdges(float v0x, float v0y,
                         float v1x, float v1y,
                         float u0x, float u0y,
                         float u1x, float u1y,
                         float u2x, float u2y) {
  float ax, ay;
  ax = v1x - v0x;
  ay = v1y - v0y;
  // Test edge u0, u1 against v0, v1.
  if (EdgeEdgeTest(ax, ay, v0x, v0y, u0x, u0y, u1x, u1y)) {
    return true;
  }
  // Test edge u1, u2 against v0, v1.
  if (EdgeEdgeTest(ax, ay, v0x, v0y, u1x, u1y, u2x, u2y)) {
    return true;
  }
  // Test edge u2, u1 against v0, v1.
  if (EdgeEdgeTest(ax, ay, v0x, v0y, u2x, u2y, u0x, u0y)) {
    return true;
  }
  return false;
}

}  // anonymous namespace

bool LinesIntersect(float p1x, float p1y,
                    float q1x, float q1y,
                    float p2x, float p2y,
                    float q2x, float q2y) {
  return ((Orientation(p1x, p1y, q1x, q1y, p2x, p2y) !=
           Orientation(p1x, p1y, q1x, q1y, q2x, q2y)) &&
          (Orientation(p2x, p2y, q2x, q2y, p1x, p1y) !=
           Orientation(p2x, p2y, q2x, q2y, q1x, q1y)));
}

bool PointInTriangle(float px, float py,
                     float ax, float ay,
                     float bx, float by,
                     float cx, float cy) {
  // Algorithm from http://www.blackpawn.com/texts/pointinpoly/default.html
  float x0 = cx - ax;
  float y0 = cy - ay;
  float x1 = bx - ax;
  float y1 = by - ay;
  float x2 = px - ax;
  float y2 = py - ay;

  float dot00 = x0 * x0 + y0 * y0;
  float dot01 = x0 * x1 + y0 * y1;
  float dot02 = x0 * x2 + y0 * y2;
  float dot11 = x1 * x1 + y1 * y1;
  float dot12 = x1 * x2 + y1 * y2;
  float denom = dot00 * dot11 - dot01 * dot01;
  if (denom == 0.0f) {
    // Triangle is zero-area. Treat query point as not being inside.
    return false;
  }
  // Compute
  float invDenom = 1.0f / denom;
  float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
  float v = (dot00 * dot12 - dot01 * dot02) * invDenom;

  return (u > 0.0f) && (v > 0.0f) && (u + v < 1.0f);
}

bool TrianglesOverlap(float a1x, float a1y,
                      float b1x, float b1y,
                      float c1x, float c1y,
                      float a2x, float a2y,
                      float b2x, float b2y,
                      float c2x, float c2y) {
  // Derived from coplanar_tri_tri() at
  // http://jgt.akpeters.com/papers/ShenHengTang03/tri_tri.html ,
  // simplified for the 2D case and modified so that overlapping edges
  // do not report overlapping triangles.

  // Test all edges of triangle 1 against the edges of triangle 2.
  if (EdgeAgainstTriEdges(a1x, a1y, b1x, b1y, a2x, a2y, b2x, b2y, c2x, c2y))
    return true;
  if (EdgeAgainstTriEdges(b1x, b1y, c1x, c1y, a2x, a2y, b2x, b2y, c2x, c2y))
    return true;
  if (EdgeAgainstTriEdges(c1x, c1y, a1x, a1y, a2x, a2y, b2x, b2y, c2x, c2y))
    return true;
  // Finally, test if tri1 is totally contained in tri2 or vice versa.
  if (PointInTriangle(a1x, a1y, a2x, a2y, b2x, b2y, c2x, c2y))
    return true;
  if (PointInTriangle(a2x, a2y, a1x, a1y, b1x, b1y, c1x, c1y))
    return true;

  // Because we define that triangles sharing a vertex or edge don't
  // overlap, we must perform additional point-in-triangle tests to
  // see whether one triangle is contained in the other.
  if (PointInTriangle(b1x, b1y, a2x, a2y, b2x, b2y, c2x, c2y))
    return true;
  if (PointInTriangle(c1x, c1y, a2x, a2y, b2x, b2y, c2x, c2y))
    return true;
  if (PointInTriangle(b2x, b2y, a1x, a1y, b1x, b1y, c1x, c1y))
    return true;
  if (PointInTriangle(c2x, c2y, a1x, a1y, b1x, b1y, c1x, c1y))
    return true;
  return false;
}

}  // namespace cubic
}  // namespace gpu2d
}  // namespace o3d

