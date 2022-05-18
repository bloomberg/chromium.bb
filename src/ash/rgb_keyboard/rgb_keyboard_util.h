// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_RGB_KEYBOARD_RGB_KEYBOARD_UTIL_H_
#define ASH_RGB_KEYBOARD_RGB_KEYBOARD_UTIL_H_

#include "ash/ash_export.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"

namespace ash {

inline constexpr SkColor kInvalidColor = SK_ColorTRANSPARENT;

// Util method to convert the |BacklightColor| enum to a predefined SkColor
// which will be set by rgb keyboard manager to change the color of the keyboard
// backlight.
ASH_EXPORT SkColor ConvertBacklightColorToSkColor(
    personalization_app::mojom::BacklightColor backlight_color);

}  // namespace ash

#endif  // ASH_RGB_KEYBOARD_RGB_KEYBOARD_UTIL_H_
