// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_STYLE_PLATFORM_STYLE_H_
#define UI_VIEWS_STYLE_PLATFORM_STYLE_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "ui/views/controls/button/button.h"

namespace views {

class Border;
class LabelButton;
class LabelButtonBorder;

// Cross-platform API for providing platform-specific styling for toolkit-views.
class PlatformStyle {
 public:
  // Creates the default label button border for the given |style|. Used when a
  // custom default border is not provided for a particular LabelButton class.
  static scoped_ptr<LabelButtonBorder> CreateLabelButtonBorder(
      Button::ButtonStyle style);

  // Applies the current system theme to the default border created by |button|.
  static scoped_ptr<Border> CreateThemedLabelButtonBorder(LabelButton* button);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PlatformStyle);
};

}  // namespace views

#endif  // UI_VIEWS_STYLE_PLATFORM_STYLE_H_
