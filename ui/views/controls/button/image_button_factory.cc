// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/image_button_factory.h"

#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/painter.h"

namespace views {

ImageButton* CreateVectorImageButton(ButtonListener* listener) {
  ImageButton* button = new ImageButton(listener);
  button->SetInkDropMode(CustomButton::InkDropMode::ON);
  button->set_has_ink_drop_action_on_click(true);
  button->SetImageAlignment(ImageButton::ALIGN_CENTER,
                            ImageButton::ALIGN_MIDDLE);
  button->SetFocusPainter(nullptr);
  // Extra space around the buttons to increase their event target size.
  constexpr int kButtonExtraTouchSize = 4;
  button->SetBorder(CreateEmptyBorder(gfx::Insets(kButtonExtraTouchSize)));
  return button;
}

void SetImageFromVectorIcon(ImageButton* button,
                            const gfx::VectorIcon& icon,
                            SkColor related_text_color) {
  const SkColor icon_color =
      color_utils::DeriveDefaultIconColor(related_text_color);
  const SkColor disabled_color = SkColorSetA(icon_color, 0xff / 2);
  button->SetImage(CustomButton::STATE_NORMAL,
                   gfx::CreateVectorIcon(icon, icon_color));
  button->SetImage(CustomButton::STATE_DISABLED,
                   gfx::CreateVectorIcon(icon, disabled_color));
  button->set_ink_drop_base_color(icon_color);
}

}  // views
