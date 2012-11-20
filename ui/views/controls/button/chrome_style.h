// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_CHROME_STYLE_H_
#define UI_VIEWS_CONTROLS_BUTTON_CHROME_STYLE_H_

#include "ui/views/controls/button/text_button.h"

namespace views {

// Apply button settings to produce a style similar to Chrome WebUI.
VIEWS_EXPORT void ApplyChromeStyle(TextButton* button);

// Returns the number of pixels the shadow of the Chrome-style button extends
// beyond the bottom of the button.
VIEWS_EXPORT int GetChromeStyleButtonShadowMargin();


}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_CHROME_STYLE_H_
