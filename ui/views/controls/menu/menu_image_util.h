// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_IMAGE_UTIL_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_IMAGE_UTIL_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_skia.h"

namespace views {

// The width/height of the check and submenu arrows.
const int kMenuCheckSize = 16;
const int kSubmenuArrowSize = 8;

// Returns the Menu Check box image (always checked).
gfx::ImageSkia GetMenuCheckImage(SkColor icon_color);

// Return the RadioButton image for given state.
// It returns the "selected" image when |selected| is
// true, or the "unselected" image if false.
gfx::ImageSkia GetRadioButtonImage(bool selected);

// Returns the image for submenu arrow for current RTL setting.
gfx::ImageSkia GetSubmenuArrowImage(SkColor icon_color);

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_IMAGE_UTIL_H_
