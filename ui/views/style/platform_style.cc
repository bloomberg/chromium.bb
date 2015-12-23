// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/style/platform_style.h"

#include "build/build_config.h"
#include "ui/base/resource/material_design/material_design_controller.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"

namespace views {

#if !defined(OS_MACOSX)
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
#endif

#if !defined(OS_LINUX) || defined(OS_CHROMEOS)
// static
scoped_ptr<Border> PlatformStyle::CreateThemedLabelButtonBorder(
    LabelButton* button) {
  return button->CreateDefaultBorder();
}
#endif

}  // namespace views
