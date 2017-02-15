// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COLOR_TRANSFORM_H_
#define UI_GFX_COLOR_TRANSFORM_H_

#include "base/macros.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

class GFX_EXPORT ColorTransform {
 public:
  enum class Intent { INTENT_ABSOLUTE, INTENT_PERCEPTUAL, TEST_NO_OPT };

  // TriStimulus is a color coordinate in any color space.
  // Channel order is XYZ, RGB or YUV.
  typedef Point3F TriStim;

  ColorTransform();
  virtual ~ColorTransform();

  // Perform transformation of colors, |colors| is both input and output.
  virtual void Transform(TriStim* colors, size_t num) = 0;

  virtual size_t NumberOfStepsForTesting() const = 0;

  static std::unique_ptr<ColorTransform> NewColorTransform(
      const ColorSpace& from,
      const ColorSpace& to,
      Intent intent);

 private:
  DISALLOW_COPY_AND_ASSIGN(ColorTransform);
};

}  // namespace gfx

#endif  // UI_GFX_COLOR_TRANSFORM_H_
