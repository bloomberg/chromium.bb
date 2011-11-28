// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_IMAGE_UTIL_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_IMAGE_UTIL_H_
#pragma once

#include "third_party/skia/include/core/SkBitmap.h"

namespace views {

// Return the RadioButton image for given state.
// It returns the "selected" image when |selected| is
// true, or the "unselected" image if false.
// The returned image is global object and should not be freed.
const SkBitmap* GetRadioButtonImage(bool selected);

// Returns the image for submenu arrow for current RTL setting.
const SkBitmap* GetSubmenuArrowImage();

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_IMAGE_UTIL_H_
