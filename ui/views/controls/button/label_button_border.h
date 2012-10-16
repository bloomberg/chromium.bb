// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_BORDER_H_
#define UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_BORDER_H_

#include "ui/gfx/image/image_skia.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/custom_button.h"

namespace views {

class NativeThemeDelegate;

// A Border that paints a LabelButton's background frame.
class VIEWS_EXPORT LabelButtonBorder : public Border {
 public:
  explicit LabelButtonBorder(NativeThemeDelegate* delegate);
  virtual ~LabelButtonBorder();

  bool native_theme() const { return native_theme_; }
  void set_native_theme(bool native_theme) { native_theme_ = native_theme; }

  // Overridden from Border:
  virtual void Paint(const View& view, gfx::Canvas* canvas) const OVERRIDE;
  virtual void GetInsets(gfx::Insets* insets) const OVERRIDE;

 private:
  struct BorderImages {
    BorderImages();
    // |image_ids| must contain 9 image ids.
    explicit BorderImages(const int image_ids[]);
    ~BorderImages();

    gfx::ImageSkia top_left;
    gfx::ImageSkia top;
    gfx::ImageSkia top_right;
    gfx::ImageSkia left;
    gfx::ImageSkia center;
    gfx::ImageSkia right;
    gfx::ImageSkia bottom_left;
    gfx::ImageSkia bottom;
    gfx::ImageSkia bottom_right;
  };

  // Set the images shown for the specified button state.
  void SetImages(CustomButton::ButtonState state, const BorderImages& images);

  // Paint the view-style images for the specified button state.
  void PaintImages(const View& view,
                   gfx::Canvas* canvas,
                   CustomButton::ButtonState state) const;

  // Paint the native-style button border and background.
  void PaintNativeTheme(const View& view, gfx::Canvas* canvas) const;

  // The images shown for each button state.
  BorderImages images_[CustomButton::BS_COUNT];

  // A delegate and flag controlling the native/Views theme styling.
  NativeThemeDelegate* native_theme_delegate_;
  bool native_theme_;

  DISALLOW_COPY_AND_ASSIGN(LabelButtonBorder);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_BORDER_H_
