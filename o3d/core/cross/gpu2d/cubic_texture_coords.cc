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

#include <math.h>

#include "base/logging.h"
#include "core/cross/gpu2d/cubic_math_utils.h"
#include "core/cross/gpu2d/cubic_texture_coords.h"

namespace o3d {
namespace gpu2d {

void CubicTextureCoords::Compute(const CubicClassifier::Result& c,
                                 bool fill_right_side,
                                 CubicTextureCoords::Result* result) {
  // Loop and Blinn's formulation states that the right side of the
  // curve is defined to be the inside (filled region), but for some
  // reason it looks like with the default orientation parameters we
  // are filling the left side of the curve. Regardless, because we
  // can receive arbitrarily oriented curves as input, we might have
  // to reverse the orientation of the cubic texture coordinates even
  // in cases where the paper doen't say it is necessary.
  bool reverse_orientation = false;
  const float kOneThird = 1.0f / 3.0f;
  const float kTwoThirds = 2.0f / 3.0f;
  CubicClassifier::CurveType curve_type = c.curve_type();

  result->is_line_or_point = false;
  result->has_rendering_artifact = false;
  result->subdivision_parameter_value = 0.0f;

  switch (curve_type) {
    case CubicClassifier::kSerpentine: {
      float t1 = sqrtf(9.0f * c.d2() * c.d2() - 12 * c.d1() * c.d3());
      float ls = 3.0f * c.d2() - t1;
      float lt = 6.0f * c.d1();
      float ms = 3.0f * c.d2() + t1;
      float mt = lt;
      float ltmls = lt - ls;
      float mtmms = mt - ms;
      result->coords[0] = Vector3(ls * ms,
                                  ls * ls * ls,
                                  ms * ms * ms);
      result->coords[1] = Vector3(kOneThird * (3.0f * ls * ms -
                                               ls * mt -
                                               lt * ms),
                                  ls * ls * (ls - lt),
                                  ms * ms * (ms - mt));
      result->coords[2] = Vector3(kOneThird * (lt * (mt - 2.0f * ms) +
                                               ls * (3.0f * ms - 2.0f * mt)),
                                  ltmls * ltmls * ls,
                                  mtmms * mtmms * ms);
      result->coords[3] = Vector3(ltmls * mtmms,
                                  -(ltmls * ltmls * ltmls),
                                  -(mtmms * mtmms * mtmms));
      if (c.d1() < 0.0f)
        reverse_orientation = true;
      break;
    }

    case CubicClassifier::kLoop: {
      float t1 = sqrtf(4.0f * c.d1() * c.d3() - 3.0f * c.d2() * c.d2());
      float ls = c.d2() - t1;
      float lt = 2.0f * c.d1();
      float ms = c.d2() + t1;
      float mt = lt;

      // Figure out whether there is a rendering artifact requiring
      // the curve to be subdivided by the caller.
      float ql = ls / lt;
      float qm = ms / mt;
      if (0.0f < ql && ql < 1.0f) {
        result->has_rendering_artifact = true;
        result->subdivision_parameter_value = ql;
        // TODO(kbr): may require more work?
        break;
      }

      if (0.0f < qm && qm < 1.0f) {
        result->has_rendering_artifact = true;
        result->subdivision_parameter_value = qm;
        // TODO(kbr): may require more work?
        break;
      }

      float ltmls = lt - ls;
      float mtmms = mt - ms;
      result->coords[0] = Vector3(ls * ms,
                                  ls * ls * ms,
                                  ls * ms * ms);
      result->coords[1] =
          Vector3(kOneThird * (-ls * mt - lt * ms + 3.0f * ls * ms),
                  -kOneThird * ls * (ls * (mt - 3.0f * ms) + 2.0f * lt * ms),
                  -kOneThird * ms * (ls * (2.0f * mt - 3.0f * ms) + lt * ms));
      result->coords[2] =
          Vector3(kOneThird * (lt * (mt - 2.0f * ms) +
                               ls * (3.0f * ms - 2.0f * mt)),
                  kOneThird * (lt - ls) * (ls * (2.0f * mt - 3.0f * ms) +
                                           lt * ms),
                  kOneThird * (mt - ms) * (ls * (mt - 3.0f * ms) +
                                           2.0f * lt * ms));
      result->coords[3] =
          Vector3(ltmls * mtmms,
                  -(ltmls * ltmls) * mtmms,
                  -ltmls * mtmms * mtmms);
      reverse_orientation =
              ((c.d1() > 0.0f && result->coords[0].getX() < 0.0f) ||
               (c.d1() < 0.0f && result->coords[0].getX() > 0.0f));
      break;
    }

    case CubicClassifier::kCusp: {
      float ls = c.d3();
      float lt = 3.0f * c.d2();
      float lsmlt = ls - lt;
      result->coords[0] = Vector3(ls,
                                  ls * ls * ls,
                                  1.0f);
      result->coords[1] = Vector3(ls - kOneThird * lt,
                                  ls * ls * lsmlt,
                                  1.0f);
      result->coords[2] = Vector3(ls - kTwoThirds * lt,
                                  lsmlt * lsmlt * ls,
                                  1.0f);
      result->coords[3] = Vector3(lsmlt,
                                  lsmlt * lsmlt * lsmlt,
                                  1.0f);
      break;
    }

    case CubicClassifier::kQuadratic: {
      result->coords[0] = Vector3(0.0f, 0.0f, 0.0f);
      result->coords[1] = Vector3(kOneThird, 0, kOneThird);
      result->coords[2] = Vector3(kTwoThirds, kOneThird, kTwoThirds);
      result->coords[3] = Vector3(1.0f, 1.0f, 1.0f);
      if (c.d3() < 0.0f)
        reverse_orientation = true;
      break;
    }

    case CubicClassifier::kLine:
    case CubicClassifier::kPoint:
      result->is_line_or_point = true;
      break;

    default:
      NOTREACHED();
      break;
  }

  if (fill_right_side) {
    reverse_orientation = !reverse_orientation;
  }

  if (reverse_orientation) {
    for (int i = 0; i < 4; i++) {
      result->coords[i].setX(-result->coords[i].getX());
      result->coords[i].setY(-result->coords[i].getY());
    }
  }
}

}  // namespace gpu2d
}  // namespace o3d

