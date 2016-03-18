// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/style/platform_style.h"

#include "build/build_config.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/focusable_border.h"
#include "ui/views/controls/scrollbar/native_scroll_bar.h"

namespace views {

#if !defined(OS_MACOSX)
// static
scoped_ptr<FocusableBorder> PlatformStyle::CreateComboboxBorder() {
  return make_scoped_ptr(new FocusableBorder());
}

// static
scoped_ptr<LabelButtonBorder> PlatformStyle::CreateLabelButtonBorder(
    Button::ButtonStyle style) {
  if (!ui::MaterialDesignController::IsModeMaterial() ||
      style != Button::STYLE_TEXTBUTTON) {
    return make_scoped_ptr(new LabelButtonAssetBorder(style));
  }

  scoped_ptr<LabelButtonBorder> border(new views::LabelButtonBorder());
  border->set_insets(views::LabelButtonAssetBorder::GetDefaultInsetsForStyle(
      Button::STYLE_TEXTBUTTON));
  return border;
}

// static
scoped_ptr<ScrollBar> PlatformStyle::CreateScrollBar(bool is_horizontal) {
  return make_scoped_ptr(new NativeScrollBar(is_horizontal));
}
#endif

#if !defined(OS_LINUX) || defined(OS_CHROMEOS)
// static
scoped_ptr<Border> PlatformStyle::CreateThemedLabelButtonBorder(
    LabelButton* button) {
  return button->CreateDefaultBorder();
}
#endif

}  // namespace views
