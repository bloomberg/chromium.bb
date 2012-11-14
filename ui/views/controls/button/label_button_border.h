// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_BORDER_H_
#define UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_BORDER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/border_images.h"
#include "ui/views/controls/button/custom_button.h"

namespace views {

// A Border that paints a LabelButton's background frame.
class VIEWS_EXPORT LabelButtonBorder : public Border {
 public:
  LabelButtonBorder();
  virtual ~LabelButtonBorder();

  bool native_theme() const { return native_theme_; }
  void set_native_theme(bool native_theme) { native_theme_ = native_theme; }

  // Overridden from Border:
  virtual void Paint(const View& view, gfx::Canvas* canvas) OVERRIDE;
  virtual gfx::Insets GetInsets() const OVERRIDE;

  // Get or set the images shown for the specified button state.
  BorderImages* GetImages(CustomButton::ButtonState state);
  void SetImages(CustomButton::ButtonState state, const BorderImages& images);

 private:
  // The images shown for each button state.
  BorderImages images_[CustomButton::STATE_COUNT];

  // A flag controlling native (true) or Views theme styling; false by default.
  bool native_theme_;

  DISALLOW_COPY_AND_ASSIGN(LabelButtonBorder);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_BORDER_H_
