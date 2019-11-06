// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/dark_mode_color_classifier.h"

namespace blink {

// Values below which a color is considered sufficiently transparent that a
// lighter color behind it would make the final color as seen by the user light.
// TODO(https://crbug.com/925949): This is a placeholder value. It should be
// replaced with a better researched value before launching dark mode.
const float kOpacityThreshold = 0.4;

// TODO(https://crbug.com/925949): Find a better algorithm for this.
bool IsLight(const Color& color, float opacity) {
  // Multiply the opacity by the alpha value to get a more accurate sense of how
  // transparent the element is.
  float real_opacity = opacity * (color.Alpha() / 255);
  // Assume the color is light if the background is sufficiently transparent.
  if (real_opacity < kOpacityThreshold) {
    return true;
  }
  double hue;
  double saturation;
  double lightness;
  color.GetHSL(hue, saturation, lightness);
  return lightness > 0.5;
}

}  // namespace blink
