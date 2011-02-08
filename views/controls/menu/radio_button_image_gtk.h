// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_MENU_RADIO_BUTTON_IMAGE_H_
#define VIEWS_CONTROLS_MENU_RADIO_BUTTON_IMAGE_H_
#pragma once

#include "third_party/skia/include/core/SkBitmap.h"

namespace views {

// Return the RadioButton image for given state.
// It returns the "selected" image when |selected| is
// true, or the "unselected" image if false.
// The returned image is global object and should not be freed.
const SkBitmap* GetRadioButtonImage(bool selected);

} // namespace views

#endif  // VIEWS_CONTROLS_MENU_RADIO_BUTTON_IMAGE_H_
