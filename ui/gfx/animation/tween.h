// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ANIMATION_TWEEN_H_
#define UI_GFX_ANIMATION_TWEEN_H_

#include "base/basictypes.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/transform.h"

namespace gfx {

class GFX_EXPORT Tween {
 public:
  enum Type {
    LINEAR,        // Linear.
    EASE_OUT,      // Fast in, slow out (default).
    EASE_IN,       // Slow in, fast out.
    EASE_IN_2,     // Variant of EASE_IN that starts out slower.
    EASE_IN_OUT,   // Slow in and out, fast in the middle.
    FAST_IN_OUT,   // Fast in and out, slow in the middle.
    EASE_OUT_SNAP, // Fast in, slow out, snap to final value.
    SMOOTH_IN_OUT, // Smooth, consistent speeds in and out (sine wave).
    ZERO,          // Returns a value of 0 always.
  };

  // Returns the value based on the tween type. |state| is from 0-1.
  static double CalculateValue(Type type, double state);

  // Conveniences for getting a value between a start and end point.
  static double DoubleValueBetween(double value, double start, double target);
  static float FloatValueBetween(double value, float start, float target);
  static int IntValueBetween(double value, int start, int target);
  static gfx::Rect RectValueBetween(double value,
                                    const gfx::Rect& start_bounds,
                                    const gfx::Rect& target_bounds);
  static gfx::Transform TransformValueBetween(
      double value,
      const gfx::Transform& start_transform,
      const gfx::Transform& target_transform);

 private:
  Tween();
  ~Tween();

  DISALLOW_COPY_AND_ASSIGN(Tween);
};

}  // namespace gfx

#endif  // UI_GFX_ANIMATION_TWEEN_H_
