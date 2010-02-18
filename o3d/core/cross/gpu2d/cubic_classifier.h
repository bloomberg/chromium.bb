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

// Cubic curve classification algorithm from "Rendering Vector Art on
// the GPU" by Loop and Blinn, GPU Gems 3, Chapter 25.

#ifndef O3D_CORE_CROSS_GPU2D_CUBIC_CLASSIFIER_H_
#define O3D_CORE_CROSS_GPU2D_CUBIC_CLASSIFIER_H_

#include "base/basictypes.h"

namespace o3d {
namespace gpu2d {

// Classifies cubic curves into specific types.
class CubicClassifier {
 public:
  // The types of cubic curves.
  enum CurveType {
    kSerpentine,
    kCusp,
    kLoop,
    kQuadratic,
    kLine,
    kPoint
  };

  // The result of the classifier.
  class Result {
   public:
    Result(CurveType curve_type,
           float d1, float d2, float d3)
        : curve_type_(curve_type),
          d1_(d1),
          d2_(d2),
          d3_(d3) {
    }

    CurveType curve_type() const {
      return curve_type_;
    }

    float d1() const {
      return d1_;
    }

    float d2() const {
      return d2_;
    }

    float d3() const {
      return d3_;
    }

   private:
    CurveType curve_type_;
    float d1_;
    float d2_;
    float d3_;
  };

  // Classifies the given cubic bezier curve starting at (b0x, b0y),
  // ending at (b3x, b3y), and affected by control points (b1x, b1y)
  // and (b2x, b2y).
  static Result Classify(float b0x, float b0y,
                         float b1x, float b1y,
                         float b2x, float b2y,
                         float b3x, float b3y);

 private:
  // This class does not need to be instantiated.
  CubicClassifier() {}
  DISALLOW_COPY_AND_ASSIGN(CubicClassifier);
};

}  // namespace gpu2d
}  // namespace o3d

#endif  // O3D_CORE_CROSS_GPU2D_CUBIC_CLASSIFIER_H_

