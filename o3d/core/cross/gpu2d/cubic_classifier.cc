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

#include "core/cross/gpu2d/cubic_classifier.h"

#include "core/cross/math_types.h"
#include "core/cross/gpu2d/cubic_math_utils.h"

namespace o3d {
namespace gpu2d {

using cubic::ApproxEqual;
using cubic::RoundToZero;

CubicClassifier::Result CubicClassifier::Classify(float b0x, float b0y,
                                                  float b1x, float b1y,
                                                  float b2x, float b2y,
                                                  float b3x, float b3y) {
  Vector3 b0(b0x, b0y, 1);
  Vector3 b1(b1x, b1y, 1);
  Vector3 b2(b2x, b2y, 1);
  Vector3 b3(b3x, b3y, 1);

  // Compute a1..a3
  float a1 = dot(b0, cross(b3, b2));
  float a2 = dot(b1, cross(b0, b3));
  float a3 = dot(b2, cross(b1, b0));

  // Compute d1..d3
  float d1 = a1 - 2 * a2 + 3 * a3;
  float d2 = -a2 + 3 * a3;
  float d3 = 3 * a3;

  // Experimentation has shown that the texture coordinates computed
  // from these values quickly become huge, leading to roundoff errors
  // and artifacts in the shader. It turns out that if we normalize
  // the vector defined by (d1, d2, d3), this fixes the problem of the
  // texture coordinates getting too large without affecting the
  // classification results.
  Vector3 nd = normalize(Vector3(d1, d2, d3));
  d1 = nd.getX();
  d2 = nd.getY();
  d3 = nd.getZ();

  // Compute the discriminant.
  // term0 is a common term in the computation which helps decide
  // which way to classify the cusp case: as serpentine or loop.
  float term0 = (3 * d2 * d2 - 4 * d1 * d3);
  float discr = d1 * d1 * term0;

  // Experimentation has also shown that when the classification is
  // near the boundary between one curve type and another, the shader
  // becomes numerically unstable, particularly with the cusp case.
  // Correct for this by rounding d1..d3 and the discriminant to zero
  // when they get near it.
  d1 = RoundToZero(d1);
  d2 = RoundToZero(d2);
  d3 = RoundToZero(d3);
  discr = RoundToZero(discr);

  // Do the classification
  if (ApproxEqual(b0, b1) &&
      ApproxEqual(b0, b2) &&
      ApproxEqual(b0, b3)) {
    return Result(kPoint, d1, d2, d3);
  }

  if (discr == 0) {
    if (d1 == 0 && d2 == 0) {
      if (d3 == 0)
        return Result(kLine, d1, d2, d3);
      return Result(kQuadratic, d1, d2, d3);
    }

    if (d1 == 0)
      return Result(kCusp, d1, d2, d3);

    // This is the boundary case described in Loop and Blinn's
    // SIGGRAPH '05 paper of a cusp with inflection at infinity.
    // Because term0 might not be exactly 0, we decide between using
    // the serpentine and loop cases depending on its sign to avoid
    // taking the square root of a negative number when computing the
    // cubic texture coordinates.
    if (term0 < 0)
      return Result(kLoop, d1, d2, d3);

    return Result(kSerpentine, d1, d2, d3);
  } else if (discr > 0) {
    return Result(kSerpentine, d1, d2, d3);
  } else {
    // discr < 0
    return Result(kLoop, d1, d2, d3);
  }
}

}  // namespace gpu2d
}  // namespace o3d

