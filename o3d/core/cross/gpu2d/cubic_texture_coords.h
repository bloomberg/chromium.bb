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

// Computes three-dimensional texture coordinates for the control
// points of a cubic curve for rendering via the shader in "Rendering
// Vector Art on the GPU" by Loop and Blinn, GPU Gems 3, Chapter 25.

#ifndef O3D_CORE_CROSS_GPU2D_CUBIC_TEXTURE_COORDS_H_
#define O3D_CORE_CROSS_GPU2D_CUBIC_TEXTURE_COORDS_H_

#include "base/basictypes.h"
#include "core/cross/math_types.h"
#include "core/cross/gpu2d/cubic_classifier.h"

namespace o3d {
namespace gpu2d {

// Computes texture coordinates for rendering cubic curves on the GPU.
class CubicTextureCoords {
 public:
  // Container for the cubic texture coordinates and other associated
  // information.
  struct Result {
    Result()
        : is_line_or_point(false),
          has_rendering_artifact(false),
          subdivision_parameter_value(0.0f) {
    }

    // The 3D texture coordinates that are to be associated with the
    // four control points of the cubic curve.
    Vector3 coords[4];

    // Indicates whether the curve is a line or a point, in which case
    // we do not need to add its triangles to the mesh.
    bool is_line_or_point;

    // For the loop case, indicates whether a rendering artifact was
    // detected, in which case the curve needs to be further
    // subdivided.
    bool has_rendering_artifact;

    // If a rendering artifact will occur for the given loop curve,
    // this is the parameter value (0 <= value <= 1) at which the
    // curve needs to be subdivided to fix the artifact.
    float subdivision_parameter_value;
  };

  // Computes the texture coordinates for a cubic curve segment's
  // control points, given the classification of the curve as well as
  // an indication of which side is to be filled.
  static void Compute(const CubicClassifier::Result& classification,
                      bool fill_right_side,
                      Result* result);

 private:
  // This class does not need to be instantiated.
  CubicTextureCoords() {}
  DISALLOW_COPY_AND_ASSIGN(CubicTextureCoords);
};

}  // namespace gpu2d
}  // namespace o3d

#endif  // O3D_CORE_CROSS_GPU2D_CUBIC_TEXTURE_COORDS_H_

