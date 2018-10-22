// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_SPINNER_UTIL_H_
#define CHROME_BROWSER_UI_COCOA_SPINNER_UTIL_H_

#include "ui/gfx/geometry/angle_conversions.h"

namespace cocoa_spinner_util {

constexpr CGFloat kDegrees90 = gfx::DegToRad(90.0f);
constexpr CGFloat kDegrees135 = gfx::DegToRad(135.0f);
constexpr CGFloat kDegrees180 = gfx::DegToRad(180.0f);
constexpr CGFloat kDegrees270 = gfx::DegToRad(270.0f);
constexpr CGFloat kDegrees360 = gfx::DegToRad(360.0f);
constexpr CGFloat kSpinnerViewUnitWidth = 28.0;
constexpr CGFloat kSpinnerUnitInset = 2.0;
constexpr CGFloat kArcDiameter =
    (kSpinnerViewUnitWidth - kSpinnerUnitInset * 2.0);
constexpr CGFloat kArcRadius = kArcDiameter / 2.0;
constexpr CGFloat kArcLength =
    kDegrees135 * kArcDiameter;  // 135 degrees of circumference.
constexpr CGFloat kArcStrokeWidth = 3.0;
constexpr CGFloat kArcAnimationTime = 1.333;
constexpr CGFloat kRotationTime = 1.56863;
NSString* const kSpinnerAnimationName = @"SpinnerAnimationName";
NSString* const kRotationAnimationName = @"RotationAnimationName";
constexpr CGFloat kWaitingStrokeAlpha = 0.5;

}  // namespace cocoa_spinner_util

#endif  // CHROME_BROWSER_UI_COCOA_SPINNER_UTIL_H_
