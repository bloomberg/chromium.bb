// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COLOR_UTILS_H_
#define UI_GFX_COLOR_UTILS_H_
#pragma once

#include "third_party/skia/include/core/SkColor.h"

class SkBitmap;

namespace color_utils {

// Represents an HSL color.
struct HSL {
  double h;
  double s;
  double l;
};

// Calculated according to http://www.w3.org/TR/WCAG20/#relativeluminancedef
double RelativeLuminance(SkColor color);

// Note: these transformations assume sRGB as the source color space
void SkColorToHSL(SkColor c, HSL* hsl);
SkColor HSLToSkColor(const HSL& hsl, SkAlpha alpha);

// HSL-Shift an SkColor. The shift values are in the range of 0-1, with the
// option to specify -1 for 'no change'. The shift values are defined as:
// hsl_shift[0] (hue): The absolute hue value - 0 and 1 map
//    to 0 and 360 on the hue color wheel (red).
// hsl_shift[1] (saturation): A saturation shift, with the
//    following key values:
//    0 = remove all color.
//    0.5 = leave unchanged.
//    1 = fully saturate the image.
// hsl_shift[2] (lightness): A lightness shift, with the
//    following key values:
//    0 = remove all lightness (make all pixels black).
//    0.5 = leave unchanged.
//    1 = full lightness (make all pixels white).
SkColor HSLShift(SkColor color, const HSL& shift);

// Determine if a given alpha value is nearly completely transparent.
bool IsColorCloseToTransparent(SkAlpha alpha);

// Determine if a color is near grey.
bool IsColorCloseToGrey(int r, int g, int b);

// Gets a color representing a bitmap. The definition of "representing" is the
// average color in the bitmap. The color returned is modified to have the
// specified alpha.
SkColor GetAverageColorOfFavicon(SkBitmap* bitmap, SkAlpha alpha);

// Builds a histogram based on the Y' of the Y'UV representation of
// this image.
void BuildLumaHistogram(SkBitmap* bitmap, int histogram[256]);

// Returns a blend of the supplied colors, ranging from |background| (for
// |alpha| == 0) to |foreground| (for |alpha| == 255). The alpha channels of
// the supplied colors are also taken into account, so the returned color may
// be partially transparent.
SkColor AlphaBlend(SkColor foreground, SkColor background, SkAlpha alpha);

// Given a foreground and background color, try to return a foreground color
// that is "readable" over the background color by luma-inverting the foreground
// color and then picking whichever foreground color has higher contrast against
// the background color.
//
// NOTE: This won't do anything but waste time if the supplied foreground color
// has a luma value close to the midpoint (0.5 in the HSL representation).
SkColor GetReadableColor(SkColor foreground, SkColor background);

// Gets a Windows system color as a SkColor
SkColor GetSysSkColor(int which);

}  // namespace color_utils

#endif  // UI_GFX_COLOR_UTILS_H_
